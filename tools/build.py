#!/usr/bin/env python3
"""Build Wildbits File Manager with cc65 2.19; package without touching hardware."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MODULES = 'kernel app bank comm_buffer debug file folder general keyboard list list_panel memsys overlay_em overlay_startup screen sys text'.split()
OVERLAYS = dict(bank='MEMSYS', memsys='MEMSYS', file='DISKSYS', folder='DISKSYS', overlay_em='EM', overlay_startup='STARTUP', screen='SCREEN')


def encode_strings(source):
    result = bytearray()
    expected = 0
    for line in source.splitlines():
        if not line or line.startswith('#'):
            continue
        ident, length, value = line.split('\t', 2)
        data = value.replace('~', chr(248)).encode('latin1')
        if int(ident) != expected or len(data) != int(length) or not 0 < len(data) <= 254:
            raise ValueError(f'Invalid string record {ident}: IDs must be sequential and lengths exact')
        result.extend(bytes((expected, len(data))) + data)
        expected += 1
    # Loader replaces the following record's ID with the preceding string's NUL.
    result.extend(b'\0\0')
    if len(result) > 8192:
        raise ValueError('Strings exceed their 8 KiB bank')
    return bytes(result)


def pack_pgz(segments, entry=0x799):
    result = bytearray(b'Z')
    for address, data in segments:
        if not data or address < 0 or address + len(data) > 0x1000000:
            raise ValueError('Invalid PGZ segment')
        result.extend(address.to_bytes(3, 'little') + len(data).to_bytes(3, 'little') + data)
    result.extend(entry.to_bytes(3, 'little') + b'\0\0\0')
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--debug', action='store_true', help='Enable error logging via UART at 115200 baud')
    parser.add_argument('--build-dir', type=Path)
    options = parser.parse_args()
    build = (options.build_dir or ROOT / 'build_cc65' / ('debug' if options.debug else 'release')).resolve()
    build.mkdir(parents=True, exist_ok=True)
    cc_home = os.environ.get('CC65_HOME')
    def tool(name):
        candidate = Path(cc_home) / 'bin' / name if cc_home else None
        found = str(candidate) if candidate and candidate.is_file() else shutil.which(name)
        if not found:
            raise SystemExit(f'{name} not found; install cc65 2.19 or set CC65_HOME to its source checkout')
        return found
    def run(*args):
        subprocess.run([str(x) for x in args], cwd=ROOT, check=True)
    compiler = tool('cc65')
    version = subprocess.run([compiler, '--version'], capture_output=True, text=True, check=True)
    # Official V2.19 sources report V2.18; record the exact compiler identification.
    (build / 'compiler.txt').write_text(version.stdout + version.stderr)
    for module in MODULES:
        flags = ['--cpu', '65C02', '-t', 'none', '-I', ROOT / 'config_cc65', '-D_TRY_TO_WRITE_TO_DISK', '-T']
        if module != 'memsys':
            flags += ['-Os']
        if module in OVERLAYS:
            flags += ['--code-name', 'OVERLAY_' + OVERLAYS[module]]
        if options.debug:
            flags += ['-DLOG_LEVEL_1', '-DUSE_SERIAL_LOGGING']
        run(compiler, *flags, ROOT / (module + '.c'), '-o', build / (module + '.s'))
        run(tool('ca65'), '--cpu', '65C02', '-t', 'none', build / (module + '.s'), '-o', build / (module + '.o'))
    for module in ['memory', 'text_ml']:
        run(tool('ca65'), '--cpu', '65C02', '-t', 'none', ROOT / (module + '.asm'), '-o', build / (module + '.o'))
    run(tool('ld65'), '-C', ROOT / 'config_cc65/wildbits.cfg', '-o', build / 'wildbits.rom',
        *[build / (m + '.o') for m in MODULES + ['memory', 'text_ml']], ROOT / 'config_cc65/lib/wildbits.lib',
        '-m', build / 'wildbits.map', '-Ln', build / 'labels.lbl')
    strings = encode_strings((ROOT / 'strings/strings.txt').read_text())
    (build / 'strings.bin').write_bytes(strings)
    segments = [(0x799, (build / 'wildbits.rom').read_bytes())]
    segments += [(0x10000 + i * 8192, (build / f'wildbits.rom.{i+1}').read_bytes()) for i in range(5)]
    segments.append((0x24000, strings))
    pgz = pack_pgz(segments)
    (build / 'wildbits-fm.pgz').write_bytes(pgz)
    # Existing KUP loader ABI reserves eight banks. Fail instead of truncating it.
    firmware = (ROOT / 'fm_firmware_header.bin').read_bytes() + pgz
    if len(firmware) > 65536:
        raise SystemExit('Firmware exceeds eight banks; update the KUP loader before flashing')
    (build / 'wildbits-fm.bin').write_bytes(firmware.ljust(65536, b'\0'))
    with zipfile.ZipFile(build / 'wildbits-file-manager.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('disk/wildbits-fm.pgz', pgz)
        archive.writestr('flash/wildbits-fm.bin', firmware.ljust(65536, b'\0'))
        for i in range(8):
            archive.writestr(f'flash/fm.{i:02d}', firmware[i*8192:(i+1)*8192].ljust(8192, b'\0'))
        for doc in ['README.md', 'documentation/installing.md', 'documentation/using.md', 'LICENSE']:
            archive.write(ROOT / doc, Path(doc).name)
    print(f'Built {build / "wildbits-fm.pgz"}: {len(pgz)} bytes')


if __name__ == '__main__':
    main()
