from pathlib import Path
import importlib.util
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build', ROOT / 'tools/build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class BuildTests(unittest.TestCase):
    def test_string_table_and_terminator(self):
        data = build.encode_strings((ROOT / 'strings/strings.txt').read_text())
        count = int(re.search(r'#define NUM_STRINGS (\d+)', (ROOT / 'strings.h').read_text())[1])
        offset = 0
        for ident in range(count):
            self.assertEqual(data[offset], ident)
            size = data[offset + 1]
            self.assertGreater(size, 0)
            offset += 2 + size
        self.assertEqual(data[offset:], b'\0\0')
        for invalid in ['1\t1\tx', '0\t2\tx', '0\t0\t', '0\t255\t' + 'x'*255]:
            with self.assertRaises(ValueError):
                build.encode_strings(invalid)
        self.assertEqual(build.encode_strings('0\t3\t#~x'), b'\x00\x03#\xf8x\x00\x00')

    def test_pgz_records(self):
        data = build.pack_pgz([(0x799,b'abc'), (0x24000,b'xy')])
        self.assertEqual(data, b'Z\x99\x07\0\x03\0\0abc\0\x40\x02\x02\0\0xy\x99\x07\0\0\0\0')
        for segment in [(-1,b'x'),(0xFFFFFF,b'xx'),(0,b'')]:
            with self.assertRaises(ValueError):
                build.pack_pgz([segment])

    def test_kup_container_metadata(self):
        header = (ROOT / 'fm_firmware_header.bin').read_bytes()
        self.assertEqual(header[:3], bytes([0xF2, 0x56, 8]))
        self.assertEqual(len(header), 1536)
        self.assertEqual(header[10:14], b'fm\0\0')
        self.assertTrue(header[14:256].startswith(b'Wildbits File Manager\0'))

    def test_drive_layout(self):
        text = (ROOT / 'app.h').read_text()
        for name, value in [('EXTERNAL_SD',0),('INTERNAL_SD',1),('IEC_8',2),('IEC_9',3)]:
            self.assertRegex(text, rf'DEVICE_{name}\s*=\s*{value}\b')
            self.assertRegex(text, rf'ACTION_SWITCH_TO_{name}\s+\'{value}\'')


if __name__ == '__main__':
    unittest.main()
