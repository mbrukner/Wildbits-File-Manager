"""Execute linked application code, including cc65-generated code and RAM overlays.

Directory fixtures inject entries at the kernel API boundary or start after I/O.
Disk/IRQ hardware is not emulated here.
Target structure sizes/offsets come from cc65 compiling the application's headers.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from py65.devices.mpu65c02 import MPU

ROOT = Path(__file__).resolve().parents[1]
FIELDS = {
    'struct dirent': ['d_name', 'd_blocks', 'd_type'],
    'WB2KList': ['next_item_', 'prev_item_', 'payload_'],
    'WB2KFileObject': ['size_', 'panel_id_', 'id_', 'file_type_', 'selected_', 'row_', 'display_row_'],
    'WB2KFolderObject': ['list_', 'file_name_', 'file_path_', 'file_count_', 'cur_row_'],
    'WB2KViewPanel': ['root_folder_', 'id_', 'x_', 'y_', 'width_', 'height_', 'active_', 'for_disk_', 'sort_compare_function_'],
}


def tool(name):
    home = os.getenv('CC65_HOME')
    return str(Path(home) / 'bin' / name) if home else name


class BankedMachine:
    def __init__(self, pgz, labels, active_lut=0):
        self.ram = bytearray(128 * 8192)
        self.io = [bytearray(8192) for _ in range(4)]
        self.control = active_lut
        self.io_control = 0
        self.luts = [list(range(8)) for _ in range(4)]
        self.labels = labels
        self.hooks = {}
        offset = 1
        assert pgz[0] == ord('Z')
        while offset < len(pgz):
            address = int.from_bytes(pgz[offset:offset+3], 'little')
            size = int.from_bytes(pgz[offset+3:offset+6], 'little')
            offset += 6
            if not size:
                break
            self.ram[address:address+size] = pgz[offset:offset+size]
            offset += size
        self.cpu = MPU(memory=self)

    def editing_lut(self):
        return ((self.control >> 4) if self.control & 128 else self.control) & 3

    def __getitem__(self, address):
        if address == 0:
            return self.control
        if address == 1:
            return self.io_control
        if 8 <= address < 16:
            return self.luts[self.editing_lut()][address-8]
        if 0xC000 <= address < 0xE000 and not self.io_control & 4:
            return self.io[self.io_control & 3][address-0xC000]
        bank = self.luts[self.control & 3][address >> 13]
        return self.ram[bank * 8192 + (address & 8191)]

    def __setitem__(self, address, value):
        if address == 0:
            self.control = value
        elif address == 1:
            self.io_control = value
        elif 8 <= address < 16:
            if value >= 128:
                raise AssertionError(f'Invalid RAM/flash bank {value} written at ${address:04X}')
            self.luts[self.editing_lut()][address-8] = value
        elif 0xC000 <= address < 0xE000 and not self.io_control & 4:
            self.io[self.io_control & 3][address-0xC000] = value
        else:
            bank = self.luts[self.control & 3][address >> 13]
            self.ram[bank * 8192 + (address & 8191)] = value

    def word(self, address, value=None):
        if value is None:
            return self[address] + 256 * self[address+1]
        self[address] = value & 255
        self[address+1] = value >> 8

    def call(self, name, argument=0, stack_args=b'', limit=2_000_000):
        # The resident stack is below the $A000 overlay window. RTS stops at $0700.
        stack = 0xA000 - len(stack_args)
        self.word(self.labels['sp'], stack)
        for i, value in enumerate(stack_args):
            self[stack+i] = value
        self.cpu.sp = 0xFD
        self.word(0x1FE, 0x6FF)
        self.cpu.pc = self.labels[name]
        self.cpu.a, self.cpu.x = argument & 255, argument >> 8
        for _ in range(limit):
            if self.cpu.pc == 0x700:
                assert self.cpu.sp == 0xFF, 'Unbalanced hardware stack'
                assert self.word(self.labels['sp']) == 0xA000, 'Unbalanced software stack'
                return
            if self[self.cpu.pc] == 0:
                raise AssertionError(f'{name} reached BRK at ${self.cpu.pc:04X}')
            if self.cpu.pc in self.hooks:
                result = self.hooks[self.cpu.pc]()
                self.cpu.a, self.cpu.x = result & 255, result >> 8
                self.cpu.pc = (self.cpu.stPopWord() + 1) & 65535
            else:
                self.cpu.step()
        raise AssertionError(f'{name} failed to return; PC=${self.cpu.pc:04X}')


class TargetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        work = Path(cls.temp.name)
        subprocess.run([sys.executable, str(ROOT / 'tools/build.py'), '--build-dir', str(work / 'build')], check=True, stdout=subprocess.DEVNULL)
        cls.pgz = (work / 'build/wildbits-fm.pgz').read_bytes()
        cls.labels = {name.lstrip('.'): int(address, 16) for _, address, name in
                      (line.split() for line in (work / 'build/labels.lbl').read_text().splitlines())}
        # Use target sizes, enum width and packing rather than assuming host layout.
        expressions = {}
        for struct, fields in FIELDS.items():
            expressions[struct] = f'sizeof({struct})'
            for field in fields:
                expressions[struct + '.' + field] = f'offsetof({struct}, {field})'
        source = '#include "list_panel.h"\n#include "dirent.h"\n#include <stddef.h>\nconst unsigned int layout[] = {\n'
        source += ',\n'.join(expressions.values()) + '\n};\n'
        (work / 'layout.c').write_text(source)
        subprocess.run([tool('cc65'), '--cpu', '65C02', '-t', 'none', '-I', str(ROOT), '-I', str(ROOT / 'config_cc65'), str(work / 'layout.c'), '-o', str(work / 'layout.s')], check=True)
        subprocess.run([tool('ca65'), str(work / 'layout.s'), '-o', str(work / 'layout.o')], check=True)
        (work / 'layout.cfg').write_text('MEMORY { RAM: start=$0, size=$1000, file=%O; } SEGMENTS { RODATA: load=RAM, type=ro; }')
        subprocess.run([tool('ld65'), '-C', str(work / 'layout.cfg'), str(work / 'layout.o'), '-o', str(work / 'layout.bin')], check=True)
        data = (work / 'layout.bin').read_bytes()
        cls.layout = {key: int.from_bytes(data[i*2:i*2+2], 'little') for i, key in enumerate(expressions)}

    def directory(self, names, panel_id=0, lut=0):
        machine = BankedMachine(self.pgz, self.labels, lut)
        # Set up the same string table used by file rendering.
        machine.luts[lut][5] = 11
        machine.call('_Startup_LoadString')
        machine.luts[lut][5] = 9
        next_free = self.labels['__BSS_RUN__'] + self.labels['__BSS_SIZE__']
        def allocate(size):
            nonlocal next_free
            address = next_free
            next_free += size
            assert next_free < 0x9800, 'Fixture collides with software stack'
            return address
        def put(address, struct, field, value, width=1):
            offset = self.layout[struct + '.' + field]
            for i in range(width):
                machine[address + offset + i] = (value >> (8*i)) & 255
        def get(address, struct, field, width=1):
            offset = self.layout[struct + '.' + field]
            return sum(machine[address + offset + i] << (8*i) for i in range(width))
        head = allocate(2)
        nodes, objects = [], []
        for ident, name in enumerate(names):
            node, obj = allocate(self.layout['WB2KList']), allocate(self.layout['WB2KFileObject'])
            nodes.append(node)
            objects.append(obj)
            put(node, 'WB2KList', 'payload_', obj, 2)
            put(obj, 'WB2KFileObject', 'panel_id_', panel_id)
            put(obj, 'WB2KFileObject', 'id_', ident)
            put(obj, 'WB2KFileObject', 'file_type_', 0x11)
            put(obj, 'WB2KFileObject', 'size_', ident * 256, 4)
            encoded = name.encode('ascii') + b'\0'
            address = (27+panel_id)*8192 + ident*32
            machine.ram[address:address+len(encoded)] = encoded
        for i, node in enumerate(nodes):
            put(node, 'WB2KList', 'next_item_', nodes[i+1] if i+1<len(nodes) else 0, 2)
            put(node, 'WB2KList', 'prev_item_', nodes[i-1] if i else 0, 2)
        machine.word(head, nodes[0] if nodes else 0)
        folder, panel = allocate(self.layout['WB2KFolderObject']), allocate(self.layout['WB2KViewPanel'])
        title = allocate(4)
        for i, value in enumerate(b'0:\0\0'):
            machine[title+i] = value
        put(folder, 'WB2KFolderObject', 'list_', head, 2)
        put(folder, 'WB2KFolderObject', 'file_name_', title, 2)
        put(folder, 'WB2KFolderObject', 'file_path_', title, 2)
        put(folder, 'WB2KFolderObject', 'file_count_', len(names), 2)
        put(panel, 'WB2KViewPanel', 'root_folder_', folder, 2)
        for field, value in [('id_', panel_id), ('x_', 1 if panel_id==0 else 46), ('y_', 8), ('width_', 33), ('height_', 41), ('active_', 1), ('for_disk_', 1)]:
            put(panel, 'WB2KViewPanel', field, value)
        put(panel, 'WB2KViewPanel', 'sort_compare_function_', self.labels['_File_CompareName'], 2)
        return machine, head, nodes, objects, folder, panel, get

    def assert_list(self, machine, head, nodes, objects, names, get):
        node, previous = machine.word(head), 0
        visited, result = set(), []
        while node:
            self.assertIn(node, nodes, 'Sort wrote a pointer outside the directory list')
            self.assertNotIn(node, visited, 'Sort created a cycle')
            visited.add(node)
            self.assertEqual(get(node, 'WB2KList', 'prev_item_', 2), previous)
            obj = get(node, 'WB2KList', 'payload_', 2)
            self.assertIn(obj, objects)
            result.append(names[get(obj, 'WB2KFileObject', 'id_')])
            previous, node = node, get(node, 'WB2KList', 'next_item_', 2)
        self.assertEqual(len(visited), len(names))
        self.assertEqual([name.lower() for name in result], sorted(name.lower() for name in names))

    def test_compiled_sort_with_banked_names(self):
        cases = [[], ['only'], ['b', 'a'], ['a', 'b'], ['same', 'SAME'],
                 [f'file-{41-i:03d}' for i in range(41)],
                 [f'file-{i*17%41:03d}' for i in range(41)]]
        for names in cases:
            for panel_id, lut in [(0, 0), (1, 3)]:
                with self.subTest(count=len(names), first=names[:2], panel=panel_id, lut=lut):
                    machine, head, nodes, objects, _, _, get = self.directory(names, panel_id, lut)
                    machine.call('_List_InitMergeSort', self.labels['_File_CompareName'], head.to_bytes(2, 'little'))
                    self.assert_list(machine, head, nodes, objects, names, get)
                    self.assertEqual(machine.control, lut)
                    self.assertEqual(machine.luts[lut][5:7], [9, 6])

    def test_two_directory_panes_share_real_heap(self):
        machine = BankedMachine(self.pgz, self.labels)
        machine.luts[0][5] = 11
        machine.call('_Startup_LoadString')
        machine.call('_Buffer_Initialize')
        # Match the runtime's heap initialization, retaining its full stack reserve.
        machine.word(self.labels['__heapend'], self.labels['__STACKSTART__'])
        machine.luts[0][5] = 9
        names = [f'file-{41-i:03d}' for i in range(41)]
        cursor = 0
        def open_dir():
            nonlocal cursor
            cursor = 0
            return 0xE100
        def read_dir():
            nonlocal cursor
            if cursor > len(names):
                return 0
            address = 0xE200
            for offset in range(self.layout['struct dirent']):
                machine[address + offset] = 0
            # Include a volume label, as the kernel does for an SD directory.
            name = '0:' if cursor == 0 else names[cursor-1]
            for i, value in enumerate(name.encode() + b'\0'):
                machine[address + self.layout['struct dirent.d_name'] + i] = value
            machine[address + self.layout['struct dirent.d_type']] = 2 if cursor == 0 else 0
            machine.word(address + self.layout['struct dirent.d_blocks'], 1)
            cursor += 1
            return address
        machine.hooks[self.labels['_Kernel_OpenDir']] = open_dir
        machine.hooks[self.labels['_Kernel_ReadDir']] = read_dir
        machine.hooks[self.labels['_Kernel_CloseDir']] = lambda: 0
        panels = []
        for panel_id in range(2):
            for i, value in enumerate(b'0:\0'):
                machine[0xE100+i] = value
            machine.luts[0][5] = 9
            machine.call('_Folder_NewOrReset', 0xE100, b'\0\0\0')
            folder = machine.cpu.a + 256 * machine.cpu.x
            self.assertNotEqual(folder, 0)
            machine.call('_calloc', self.layout['WB2KViewPanel'], b'\1\0')
            panel = machine.cpu.a + 256 * machine.cpu.x
            self.assertNotEqual(panel, 0)
            machine.word(panel + self.layout['WB2KViewPanel.root_folder_'], folder)
            machine.word(panel + self.layout['WB2KViewPanel.sort_compare_function_'], self.labels['_File_CompareName'])
            for field, value in [('id_', panel_id), ('x_', 1 if panel_id == 0 else 46),
                                 ('y_', 8), ('width_', 33), ('height_', 41),
                                 ('active_', panel_id == 0), ('for_disk_', 1)]:
                machine[panel + self.layout['WB2KViewPanel.' + field]] = int(value)
            panels.append((folder, panel))
            machine.call('_Panel_Refresh', panel)
            self.assertEqual(machine.word(folder + self.layout['WB2KFolderObject.file_count_']), 41,
                             f'Pane {panel_id} truncated the directory')
        # Refresh either pane without destroying or corrupting the other one.
        for folder, panel in panels:
            machine.call('_Panel_Refresh', panel)
            self.assertEqual(machine.word(folder + self.layout['WB2KFolderObject.file_count_']), 41)
        for x in (1, 46):
            for row in range(41):
                offset = (8 + row) * 80 + x
                self.assertEqual(machine.io[2][offset:offset+8], f'file-{row+1:03d}'.encode())

    def test_41_file_sort_render_and_selection(self):
        names = [f'file-{41-i:03d}' for i in range(41)]
        machine, head, nodes, objects, folder, panel, get = self.directory(names)
        machine.call('_Panel_SortAndDisplay', panel)
        self.assert_list(machine, head, nodes, objects, names, get)
        self.assertEqual(get(folder, 'WB2KFolderObject', 'cur_row_', 2), 0)
        self.assertEqual(sum(get(obj, 'WB2KFileObject', 'selected_') for obj in objects), 1)
        first = get(machine.word(head), 'WB2KList', 'payload_', 2)
        self.assertEqual(get(first, 'WB2KFileObject', 'selected_'), 1)
        for row in range(41):
            offset = (8+row)*80+1
            self.assertEqual(machine.io[2][offset:offset+8], f'file-{row+1:03d}'.encode())


if __name__ == '__main__':
    unittest.main()
