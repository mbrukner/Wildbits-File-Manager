#!/usr/bin/env python3
"""Assemble Wildbits File Manager from assembly sources; package without touching hardware."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MODULES = 'kernel app bank comm_buffer debug file folder general keyboard list list_panel memsys overlay_em overlay_startup screen sys text'.split()


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
    parser.add_argument('--build-dir', type=Path)
    options = parser.parse_args()
    build = (options.build_dir or ROOT / 'build_asm' / 'release').resolve()
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
    include = ROOT / 'asm/runtime/include'
    runtime = []
    for source in sorted((ROOT / 'asm/runtime').glob('*.s')):
        obj = build / ('rt_' + source.stem + '.o')
        run(tool('ca65'), '--cpu', '65C02', '-t', 'none', '-I', include, '-I', ROOT / 'asm', source, '-o', obj)
        runtime.append(obj)
    library = build / 'runtime.lib'
    if library.exists():
        library.unlink()
    run(tool('ar65'), 'a', library, *runtime)
    objects = []
    sources = [ROOT / 'asm/imported' / (m + '.s') for m in MODULES]
    sources += [ROOT / (m + '.asm') for m in ['memory', 'text_ml']]
    sources += [ROOT / 'asm' / (m + '.s') for m in ['startup', 'directory', 'filenames', 'kernel_bridge']]
    for source in sources:
        obj = build / (source.stem + '.o')
        run(tool('ca65'), '--cpu', '65C02', '-t', 'none', '-I', include, '-I', ROOT / 'asm', source, '-o', obj)
        objects.append(obj)
    run(tool('ld65'), '-C', ROOT / 'asm/wildbits.cfg', '-o', build / 'wildbits.rom',
        *objects, library, '-m', build / 'wildbits.map', '-Ln', build / 'labels.lbl')
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
        for doc in ['README.md', 'documentation/installing.md', 'documentation/using.md', 'documentation/assembly.md', 'documentation/review.md', 'asm/API-NOTICE', 'asm/runtime/README.md', 'LICENSE']:
            archive.write(ROOT / doc, doc)
        cc65_license = 'asm/runtime/LICENSE'
        archive.write(ROOT / cc65_license, cc65_license)
    print(f'Built {build / "wildbits-fm.pgz"}: {len(pgz)} bytes')


if __name__ == '__main__':
    main()
