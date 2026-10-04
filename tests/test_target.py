"""Execute linked application code, including cc65-generated code and RAM overlays.

Directory fixtures model the MicroKernel vector ABI, including its user-LUT RAM
alias. Disk/IRQ hardware is not emulated here. Other fixtures start after I/O.
Target structure sizes/offsets come from the assembly ABI in asm/layout.inc.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from py65.devices.mpu65c02 import MPU

ROOT = Path(__file__).resolve().parents[1]
FIELDS = {'struct dirent': ['d_name', 'd_blocks', 'd_type'],
 'WB2KList': ['next_item_', 'prev_item_', 'payload_'],
 'WB2KFileObject': ['size_', 'panel_id_', 'id_', 'file_type_', 'selected_', 'row_', 'display_row_'],
 'WB2KFolderObject': ['list_', 'file_name_', 'file_path_', 'file_count_', 'cur_row_', 'panel_id_'],
 'WB2KViewPanel': ['root_folder_',
                   'id_',
                   'x_',
                   'y_',
                   'width_',
                   'height_',
                   'active_',
                   'for_disk_',
                   'sort_compare_function_',
                   'content_top_',
                   'num_rows_',
                   'device_number_',
                   'memory_system_'],
 'FMMemorySystem': ['is_flash_', 'bank_', 'cur_row_'],
 'FMBankObject': ['bank_num_']}



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

    def call(self, name, argument=0, stack_args=b'', limit=8_000_000):
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
        # Layout constants are also included by the native directory routines.
        constants = {}
        for line in (ROOT / 'asm/layout.inc').read_text().splitlines():
            if '=' in line and not line.startswith(';'):
                name, value = line.split('=')
                constants[name.strip()] = int(value.strip())
        cls.layout = {}
        for struct, fields in FIELDS.items():
            for key in [struct] + [struct + '.' + field for field in fields]:
                cls.layout[key] = constants[key.replace('struct ', '').replace('.', '_')]

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

    def install_directory_kernel(self, machine, names):
        # Model the real vector boundary, not _Kernel_OpenDir/ReadDir/CloseDir.
        # The kernel writes its physical RAM, then NextEvent/ReadData/ReadExt
        # access that RAM through $C000 in the *user's* active LUT.
        alias_bank = machine.luts[machine.control & 3][6]
        args = self.labels['_args']
        cursor, drive = 0, 0
        event, name = bytes(7), b''
        pending = False
        empty_poll = False

        def physical(offset, data):
            start = alias_bank * 8192 + offset
            machine.ram[start:start+len(data)] = data

        def enqueue(kind, filename=b''):
            nonlocal event, name, pending, empty_poll
            name = filename
            event = bytes([kind, 1, 0, 0, 0, len(name), 0])
            physical(0x300, event)
            physical(0x400, name)
            physical(0x600, b'\1\0\0')
            pending, empty_poll = True, True

        def kernel_call(vector):
            nonlocal cursor, drive, pending, empty_poll
            self.assertEqual(machine.luts[machine.control & 3][6], alias_bank,
                             f'${vector:04X} cannot access the kernel alias')
            old_io = machine.io_control
            machine.io_control = 4
            machine.cpu.p &= ~machine.cpu.CARRY
            result = 0
            if vector == 0xFF78:
                drive = machine[args+3]
                self.assertIn(drive, names)
                cursor = 0
                # Directory.Open imports the path via the same alias, too.
                source, length = machine.word(args+11), machine[args+13]
                for i in range(length):
                    machine[0xC800+i] = machine[source+i]
                self.assertEqual(bytes(machine.ram[alias_bank*8192+0x800:alias_bank*8192+0x800+length]),
                                 bytes(machine[source+i] for i in range(length)))
                enqueue(0x3C)
                result = 1
            elif vector == 0xFF7C:
                if cursor == 0:
                    enqueue(0x3E, f'{drive}:'.encode())
                elif cursor <= len(names[drive]):
                    enqueue(0x40, names[drive][cursor-1].encode())
                else:
                    enqueue(0x44)
                cursor += 1
            elif vector == 0xFF80:
                enqueue(0x46)
            elif vector == 0xFF00:
                # Exercise polling while the asynchronous request is pending.
                if empty_poll or not pending:
                    machine.cpu.p |= machine.cpu.CARRY
                    empty_poll = False
                else:
                    destination = machine.word(args)
                    for i in range(7):
                        machine[destination+i] = machine[0xC300+i]
                    pending = False
            elif vector in (0xFF04, 0xFF08):
                source = 0xC400 if vector == 0xFF04 else 0xC600
                destination, length = machine.word(args+11), machine[args+13]
                for i in range(length):
                    machine[destination+i] = machine[source+i]
            machine.io_control = old_io
            return result

        for vector in (0xFF78, 0xFF7C, 0xFF80, 0xFF00, 0xFF04, 0xFF08, 0xFF0C):
            machine[vector] = 0x60   # vector hook stands in for the kernel ROM
            machine.hooks[vector] = lambda vector=vector: kernel_call(vector)

    def test_kernel_bridge_preserves_registers_flags_and_mapping(self):
        vectors = sorted(name for name in self.labels if name.startswith('_KernelCall_'))
        self.assertTrue(vectors)
        for lut in range(4):
            machine = BankedMachine(self.pgz, self.labels, lut)
            # Capture a non-default alias, with an unrelated LUT selected for edit.
            machine.luts[lut][6] = 31
            machine.control = 0xA0 | lut
            machine.io_control = 2
            machine.call('_KernelBridge_Init')
            for pane in range(2):
                machine.call('_Directory_Select', pane)
                for name in vectors:
                    vector = int(name[-4:], 16)
                    machine[vector] = 0x60
                    for incoming, outgoing in [(0xC9, 0x06), (0x06, 0xC9)]:
                        for io in (0, 4, 7):
                            with self.subTest(lut=lut, pane=pane, vector=name, io=io, flags=incoming):
                                machine.io_control = io
                                mappings = [row[:] for row in machine.luts]
                                def kernel():
                                    self.assertEqual(machine.luts[lut][6], 31)
                                    self.assertEqual(machine.control, 0xA0 | lut)
                                    self.assertEqual((machine.cpu.a, machine.cpu.x, machine.cpu.y), (0x12, 0x34, 0x56))
                                    self.assertEqual(machine.cpu.p & 0xCF, incoming)
                                    machine.cpu.y = 0xAB
                                    machine.cpu.p = outgoing
                                    # Verify restoration even if a vector changes the I/O page.
                                    machine.io_control = 3
                                    return 0xCDEF
                                machine.hooks[vector] = kernel
                                machine.cpu.y, machine.cpu.p = 0x56, incoming
                                machine.call(name, 0x3412)
                                self.assertEqual((machine.cpu.a, machine.cpu.x, machine.cpu.y), (0xEF, 0xCD, 0xAB))
                                self.assertEqual(machine.cpu.p & 0xCF, outgoing)
                                self.assertEqual(machine.luts, mappings)
                                self.assertEqual(machine.control, 0xA0 | lut)
                                self.assertEqual(machine.io_control, io)
            machine.call('_KernelBridge_Restore')
            self.assertEqual(machine.luts[lut][6], 31)
            self.assertEqual(machine.io_control, 2)

    def banked_directory_pair(self, count):
        machine = BankedMachine(self.pgz, self.labels)
        machine.luts[0][5] = 11
        machine.call('_Startup_LoadString')
        machine.call('_Buffer_Initialize')
        # Match the runtime's heap initialization, retaining its full stack reserve.
        machine.word(self.labels['__heapend'], self.labels['__STACKSTART__'])
        machine.luts[0][5] = 9
        names = {pane: [f'{pane}-{count-i:03d}' for i in range(count)] for pane in range(2)}
        machine.call('_KernelBridge_Init')
        machine.call('_kernel_init')
        self.install_directory_kernel(machine, names)
        panels = []
        for panel_id in range(2):
            for i, value in enumerate(f'{panel_id}:\0'.encode()):
                machine[0xE100+i] = value
            machine.luts[0][5] = 9
            machine.call('_Folder_NewOrReset', 0xE100, bytes([panel_id, 0, 0]))
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
            self.assertEqual(machine.word(folder + self.layout['WB2KFolderObject.file_count_']), min(count, 255),
                             f'Pane {panel_id} truncated the directory')
        # Refresh either pane without destroying or corrupting the other one.
        for folder, panel in panels:
            machine.call('_Panel_Refresh', panel)
            self.assertEqual(machine.word(folder + self.layout['WB2KFolderObject.file_count_']), min(count, 255))
        for pane_id, x in enumerate((1, 46)):
            for row in range(min(41, count)):
                offset = (8 + row) * 80 + x
                self.assertEqual(machine.io[2][offset:offset+5], f'{pane_id}-{row+max(1,count-254):03d}'.encode())

        return machine, panels

    def test_two_directory_panes_use_independent_banks(self):
        self.banked_directory_pair(41)

    def test_full_directory_banks_scroll_and_switch(self):
        machine, panels = self.banked_directory_pair(255)
        # Jump to the final entry in each pane, then switch back and verify that
        # the other pane's records and scroll position were not substituted.
        snapshots = [bytes(machine.ram[bank*8192:(bank+1)*8192]) for bank in (29, 30)]
        for pane_id, (folder, panel) in enumerate(panels):
            args = (254).to_bytes(2, 'little') + panel.to_bytes(2, 'little')
            machine.call('_Panel_SetFileSelectionByRow', 1, args)
            self.assertEqual(machine.word(folder + self.layout['WB2KFolderObject.cur_row_']), 254)
            offset = 48 * 80 + (1 if pane_id == 0 else 46)
            self.assertEqual(machine.io[2][offset:offset+5], f'{pane_id}-255'.encode())
            other = 1 - pane_id
            self.assertEqual(bytes(machine.ram[(29+other)*8192:(30+other)*8192]), snapshots[other])
            snapshots[pane_id] = bytes(machine.ram[(29+pane_id)*8192:(30+pane_id)*8192])
        for folder, panel in panels:
            machine.call('_Panel_RenderContents', panel)
        for pane_id, x in enumerate((1, 46)):
            offset = 48 * 80 + x
            self.assertEqual(machine.io[2][offset:offset+5], f'{pane_id}-255'.encode())

    def test_directory_capacity_is_explicit(self):
        machine, _ = self.banked_directory_pair(256)
        self.assertIn(b'Directory limited to 255 entries.', bytes(machine.io[2]))
        self.assertNotIn(b'low memory', bytes(machine.io[2]))

    def test_assembly_startup_initializes_heap_and_stack(self):
        machine = BankedMachine(self.pgz, self.labels)
        start = self.labels['__BSS_RUN__']
        for address in range(start, start + self.labels['__BSS_SIZE__']):
            machine[address] = 0xA5
        machine.cpu.sp = 0xFD
        machine.word(0x1FE, 0x6FF)
        machine.cpu.pc = 0x799
        for _ in range(100_000):
            if machine.cpu.pc == self.labels['_main']:
                break
            machine.cpu.step()
        else:
            self.fail('Assembly startup did not reach main')
        self.assertEqual(machine.word(self.labels['sp']), 0x9FFF)
        self.assertEqual(machine.word(self.labels['__heapend']), self.labels['__STACKSTART__'])
        self.assertEqual(machine.word(self.labels['__heapptr']), start + self.labels['__BSS_SIZE__'])
        # Simulate main returning after it has selected a directory bank.
        machine.luts[0][6] = 30
        machine.io_control = 4
        machine.cpu.pc = machine.cpu.stPopWord() + 1
        for _ in range(10_000):
            if machine.cpu.pc == 0x700:
                break
            machine.cpu.step()
        else:
            self.fail('Startup did not return to its loader')
        self.assertEqual(machine.cpu.sp, 0xFF)
        self.assertEqual(machine.luts[0][6], 6)
        self.assertEqual(machine.io_control, 0)

    def test_native_filename_slots_and_mmu_restoration(self):
        for lut in range(4):
            machine = BankedMachine(self.pgz, self.labels, lut)
            machine.control = 0xA0 | lut
            for pane in range(2):
                machine.call('_Directory_Select', pane)
                for slot, name in [(0, b''), (1, b'a'), (127, b'a'*31),
                                   (254, b'b'*32), (255, b'end.txt')]:
                    machine.call('_Directory_RecordAddress', slot)
                    record = machine.cpu.a + 256 * machine.cpu.x
                    machine[record + self.layout['WB2KFileObject.panel_id_']] = pane
                    machine[record + self.layout['WB2KFileObject.id_']] = slot
                    for i, value in enumerate(name + b'\0'):
                        machine[0xE100+i] = value
                    machine.call('_App_SetFilenameInEM', 0xE100, record.to_bytes(2, 'little'))
                    machine.call('_App_GetFilenameFromEM', record)
                    result = machine.cpu.a + 256 * machine.cpu.x
                    expected = name[:31] + b'\0'
                    self.assertEqual(bytes(machine[result+i] for i in range(len(expected))), expected)
                    self.assertEqual(machine.luts[lut][6], 29 + pane)
                    self.assertEqual(machine.io_control, 4)
                    self.assertEqual(machine.control, 0xA0 | lut)

    def test_directory_banks_are_write_protected(self):
        machine = BankedMachine(self.pgz, self.labels)
        machine.luts[0][5] = 12
        memsys = self.labels['__BSS_RUN__'] + self.labels['__BSS_SIZE__']
        machine.word(memsys + self.layout['FMMemorySystem.cur_row_'], 0)
        bank = memsys + self.layout['FMMemorySystem.bank_'] + self.layout['FMBankObject.bank_num_']
        protected = set(range(13)) | {18, 27, 28, 29, 30}
        for number in range(64):
            machine[bank] = number
            machine.call('_MemSys_BankIsWriteable', memsys)
            self.assertEqual(bool(machine.cpu.a), number not in protected, f'Bank {number}')
        machine[memsys + self.layout['FMMemorySystem.is_flash_']] = 1
        machine.call('_MemSys_BankIsWriteable', memsys)
        self.assertEqual(machine.cpu.a, 0)

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
