#!/usr/bin/env python3
"""Measure linked viewer CPU cycles, excluding file I/O and key-wait time.

Run with the tests' Python environment. An optional build directory allows
comparison against a separately built revision without changing this checkout.
"""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from test_target import BankedMachine


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build_dir', nargs='?', type=Path, default=ROOT / 'build_asm/release')
    build = parser.parse_args().build_dir
    labels = {name.lstrip('.'): int(address, 16) for _, address, name in
              (line.split() for line in (build / 'labels.lbl').read_text().splitlines())}
    pgz = (build / 'wildbits-fm.pgz').read_bytes()
    for mode in ('Text', 'Hex'):
        machine = BankedMachine(pgz, labels)
        machine.luts[0][5] = 11
        machine.call('_Startup_LoadString')
        machine.call('_Directory_Select', 0)
        machine.luts[0][5] = 10
        data = (b'Words for a viewer speed test, with wrapping and line endings.\r\n' * 120
                if mode == 'Text' else bytes(range(256)) * 8)
        pages = (len(data) + 255) // 256
        machine.ram[20*8192:20*8192+pages*256] = data.ljust(pages*256, b'\0')
        for index, value in enumerate(b'bench.txt\0'):
            machine[0xE100+index] = value
        cycles = []
        previous = machine.cpu.processorCycles
        def key():
            nonlocal previous
            now = machine.cpu.processorCycles
            cycles.append(now - previous)
            previous = now
            return ord(' ') if len(cycles) < 3 else ord('q')
        machine.hooks[labels['_Keyboard_GetChar']] = key
        machine.call('_EM_DisplayAs' + mode, 0xE100, bytes([pages, 20]), limit=20_000_000)
        print(json.dumps({'viewer': mode, 'cycles_per_screen': cycles, 'total': sum(cycles)}))


if __name__ == '__main__':
    main()
