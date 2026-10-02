"""Execute the actual ca65 routines on a 65C02 with modeled MMU/I/O pages."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from py65.devices.mpu65c02 import MPU

ROOT = Path(__file__).resolve().parents[1]


class Memory:
    def __init__(self):
        self.ram = [0] * 65536
        self.pages = [[0] * 8192 for _ in range(4)]
        self.luts = [[bank + 16 * lut for bank in range(8)] for lut in range(4)]

    def lut(self):
        ctrl = self.ram[0]
        return ((ctrl >> 4) if ctrl & 128 else ctrl) & 3

    def __getitem__(self, address):
        if 8 <= address < 16:
            return self.luts[self.lut()][address - 8]
        if 0xC000 <= address < 0xE000 and not self.ram[1] & 4:
            return self.pages[self.ram[1] & 3][address - 0xC000]
        return self.ram[address]

    def __setitem__(self, address, value):
        if 8 <= address < 16:
            self.luts[self.lut()][address - 8] = value
        elif 0xC000 <= address < 0xE000 and not self.ram[1] & 4:
            self.pages[self.ram[1] & 3][address - 0xC000] = value
        else:
            self.ram[address] = value


class AssemblyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        directory = Path(cls.temp.name)
        def tool(name):
            home = os.getenv('CC65_HOME')
            return str(Path(home) / 'bin' / name) if home else name
        config = directory / 'test.cfg'
        config.write_text('MEMORY { ZP: start=$30, size=$D0, type=rw; RAM: start=$2000, size=$6000, file=%O; }\nSEGMENTS { ZEROPAGE: load=ZP, type=zp; CODE: load=RAM, type=rw; }')
        objects = []
        for module in ['memory', 'text_ml']:
            obj = directory / (module + '.o')
            subprocess.run([tool('ca65'), str(ROOT / (module + '.asm')), '-o', str(obj)], check=True)
            objects.append(str(obj))
        subprocess.run([tool('ld65'), '-C', str(config), *objects, '-o', str(directory / 'test.bin'), '-Ln', str(directory / 'test.lbl')], check=True)
        cls.code = (directory / 'test.bin').read_bytes()
        cls.labels = {name.lstrip('.'): int(addr, 16) for _, addr, name in (line.split() for line in (directory / 'test.lbl').read_text().splitlines())}

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.mem = Memory()
        self.mem.ram[0x2000:0x2000 + len(self.code)] = self.code
        self.cpu = MPU(memory=self.mem)

    def set(self, label, value):
        self.mem[self.labels[label]] = value & 255

    def word(self, label, value):
        address = self.labels[label]
        self.mem[address] = value & 255
        self.mem[address + 1] = value >> 8

    def getword(self, label):
        address = self.labels[label]
        return self.mem[address] + 256 * self.mem[address + 1]

    def call(self, label, argument=0):
        self.cpu.pc = self.labels[label]
        self.cpu.a, self.cpu.x = argument & 255, argument >> 8
        self.cpu.sp = 0xFD
        self.mem.ram[0x1FE:0x200] = [0xFF, 0x0F]  # RTS to $1000
        for _ in range(300000):
            self.cpu.step()
            if self.cpu.pc == 0x1000:
                self.assertEqual(self.cpu.sp, 0xFF)
                return
        self.fail('Routine failed to return')

    def xy(self, x, y):
        self.set('_zp_x', x)
        self.set('_zp_y', y)
        self.call('_Text_SetMemLocForXY')
        self.assertEqual(self.getword('_zp_vram_ptr'), 0xC000 + 80*y + x)

    def test_mmu_all_luts_and_interrupt_states(self):
        for lut in range(4):
            for interrupt in [0, 4]:
                for edit in [0, 0xB0]:
                    ctrl = lut | edit
                    self.mem[0] = ctrl
                    self.cpu.p = 0x21 | interrupt
                    old = self.mem.luts[lut][5]
                    before = [x[:] for x in self.mem.luts]
                    self.set('_zp_bank_num', 99)
                    self.call('_Memory_SwapInNewBank', 5)
                    self.assertEqual(self.cpu.a, old)
                    self.assertEqual(self.cpu.p & 4, interrupt)
                    self.assertEqual(self.mem[0], ctrl)
                    before[lut][5] = 99
                    self.assertEqual(self.mem.luts, before)
                    self.call('_Memory_GetMappedBankNum', 5)
                    self.assertEqual(self.cpu.a, 99)
                    self.call('_Memory_RestorePreviousBank', 5)
                    self.assertEqual(self.mem.luts[lut][5], old)
                    self.assertEqual(self.mem[0], ctrl)
                    self.assertEqual(self.cpu.p & 4, interrupt)

    def test_character_wrap_and_last_cell(self):
        for x, y, expected_x, expected_y in [(0, 0, 1, 0), (79, 2, 0, 3), (79, 59, 79, 59)]:
            for io in [0, 1, 3, 4]:
                self.mem[1] = io
                self.xy(x, y)
                self.call('_Text_SetChar', ord('W'))
                self.assertEqual(self.mem.pages[2][80*y+x], ord('W'))
                self.assertEqual(self.mem[self.labels['_zp_x']], expected_x)
                self.assertEqual(self.mem[self.labels['_zp_y']], expected_y)
                self.assertEqual(self.mem[1], io)

    def test_draw_and_invert_length_boundaries(self):
        for count in [0, 1, 255, 256, 257, 511, 512, 1024]:
            self.mem[1] = 1
            self.xy(3, 2)
            self.mem.pages[2] = [0xCC] * 8192
            self.mem.pages[3] = [0x12] * 8192
            data = [(i*7) & 255 for i in range(count)]
            self.mem.ram[0x6000:0x6000+count] = data
            self.word('_zp_ptr', 0x6000)
            self.call('_Text_DrawChars', count)
            self.assertEqual(self.mem.pages[2][163:163+count], data)
            self.assertEqual(self.mem.pages[2][162], 0xCC)
            self.assertEqual(self.mem.pages[2][163+count], 0xCC)
            self.assertEqual(self.mem[1], 1)
            self.xy(3, 2)
            self.call('_Text_Invert', count)
            self.assertEqual(self.mem.pages[3][163:163+count], [0x21]*count)
            self.assertEqual(self.mem.pages[3][162], 0x12)
            self.assertEqual(self.mem.pages[3][163+count], 0x12)
            self.assertEqual(self.mem[1], 1)

    def test_scroll_preserves_io_and_interrupt_mask(self):
        for routine, row, source in [('_Text_ScrollTextUp', 2, 2), ('_Text_ScrollTextDown', 59, 58)]:
            self.mem.pages[2] = [i//80 % 256 for i in range(8192)]
            self.mem[1] = 4
            self.cpu.p |= 4
            self.xy(3, row)
            self.set('_zp_y_cnt', 1)
            self.call(routine, 30)
            target = row-1 if routine.endswith('Up') else row
            self.assertEqual(self.mem.pages[2][target*80+3:target*80+33], [source]*30)
            self.assertEqual(self.mem[1], 4)
            self.assertEqual(self.cpu.p & 4, 4)


if __name__ == '__main__':
    unittest.main()
