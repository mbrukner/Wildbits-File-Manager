# Wildbits File Manager

A dual-panel file and memory manager for Wildbits, using the banked 65C02 memory map and MicroKernel API.

| Key / drive | Device |
| --- | --- |
| `0` / `0:` | External SD card |
| `1` / `1:` | Internal micro SD card |
| `2` / `2:` | IEC device 8 |
| `3` / `3:` | IEC device 9 |
| `8` | RAM banks |
| `9` | Flash banks (read only) |

Browse, sort, copy, rename and delete files; create directories; view text and hex; launch programs; inspect, copy and search memory banks; and set the clock. Directory copies are not recursive.

## Build

Use Python 3 and the official **cc65 V2.19** source release. Set `CC65_HOME` to its checkout after building its tools (`make -j4 bin`). That release identifies itself as V2.18. Newer compilers generate helpers absent from the inherited runtime library, so use the pinned version for this baseline.

```sh
CC65_HOME=/path/to/cc65 ./build.sh
CC65_HOME=/path/to/cc65 ./build.sh --debug
```

The release output is `build_cc65/release/wildbits-fm.pgz`; the same directory contains a ZIP, linker map, labels, and an eight-bank KUP image. The debug build enables UART error logging at 115200 baud and has substantially less free heap. Building does not access hardware.

## Verification

```sh
python3 -m venv .venv
.venv/bin/pip install -r tests/requirements.txt
CC65_HOME=/path/to/cc65 .venv/bin/python -m unittest discover -s tests -v
```

Tests need a host C compiler with AddressSanitizer and UndefinedBehaviorSanitizer. They exercise portable application functions, the native text/MMU routines, and the linked release build's directory sorting, rendering and selection in a 65C02 emulator with modeled banked RAM and I/O. Target structure layouts are obtained from cc65. These tests do not emulate the complete machine or its disk devices. Hardware validation is still required.

See [installation](documentation/installing.md), [usage](documentation/using.md), and the [review record](documentation/review.md).

## History and credit

Wildbits File Manager is derived from [Micah Bly (WartyMN)'s original file manager](https://github.com/WartyMN/F256-FileManager). Micah's source author credits, copyright notice in the application's About display, and the original [GNU GPL version 3 license](LICENSE) are retained. This is a renamed and modified fork, maintained by [mbrukner](https://github.com/mbrukner). The bundled cc65 material retains its [separate license](config_cc65/include/___READ_ME_CC65/LICENSE).

The original Git history and GitHub fork relationship are preserved. The `upstream` remote points to Micah's repository; `local/debug-checkpoint` preserves Martin Brukner's local debugging changes before the Wildbits review.

Development lives at [mbrukner/Wildbits-File-Manager](https://github.com/mbrukner/Wildbits-File-Manager). The reviewed C/assembly baseline is the reference for the subsequent assembler conversion.
