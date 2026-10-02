"""Run portable application functions unchanged with host I/O stubs and sanitizers.

Function extraction avoids replacing the target's cc65 headers or inline assembly.
It does not validate cc65 code generation; assembly tests cover the native routines.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(file, name):
    text = (ROOT / file).read_text()
    match = re.search(r'^[^/\n]*\b' + name + r'\([^;\n]*\)\n\{.*?^\}', text, re.M | re.S)
    if not match:
        raise ValueError(name)
    return match.group()


class HostTests(unittest.TestCase):
    def test_portable_regressions(self):
        source = (ROOT / 'tests/host_prefix.c').read_text()
        for file, names in {
            'general.c': ['General_Strnlen', 'General_Strlcpy', 'General_PathPart', 'General_CreateFilePathFromFolderAndFile', 'General_ExtractFileExtensionFromFilename'],
            'list.c': ['List_MergeSortedList', 'List_SplitList', 'List_MergeSort', 'List_RepairPrevLinks'],
            'screen.c': ['ScreenConvertHexCharToByteValue', 'ScreenEvaluateUserStringForHexSeries'],
            'comm_buffer.c': ['Buffer_ScrollUp', 'Buffer_NewMessage'],
            'folder.c': ['Folder_CopyFileBytes'],
            'text.c': ['Text_GetStringFromUser'],
            'overlay_em.c': ['EM_WrapAndDisplayString'],
        }.items():
            source += '\n'.join(function(file, name) for name in names) + '\n'
        source += (ROOT / 'tests/host_cases.c').read_text()
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            (path / 'test.c').write_text(source)
            subprocess.run(['cc', '-std=c99', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True, timeout=15)


if __name__ == '__main__':
    unittest.main()
