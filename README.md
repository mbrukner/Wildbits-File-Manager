# Wildbits File Manager

A dual-panel file and memory manager for Wildbits, using the banked 65C02 memory map and MicroKernel API. Current version: **1.2**.

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

Use Python 3 and the **ca65 assembler, ld65 linker and ar65 librarian** from the cc65 toolchain (tested with V2.19). No target C compiler or prebuilt runtime library is used. `CC65_HOME` is optional when these three tools are on `PATH`.

```sh
CC65_HOME=/path/to/cc65 ./build.sh
```

Output is `build_asm/release/wildbits-fm.pgz`; the same directory contains a ZIP, linker map, labels, an eight-bank KUP image, and ROM parts `fm.00` through `fm.07`. Every ROM part is exactly 8 KiB. Building does not access hardware.

## Assembly conversion

All application modules and runtime helpers now assemble from source. `asm/imported/` preserves cc65-generated assembly for existing features; these routines are not yet all hand-optimized. Native routines in `asm/directory.s` and `asm/filenames.s` replace heap-based file records and filename transfers. Each pane has a separate 8 KiB record bank and a separate filename bank, supporting 255 entries independently of the shared heap. Startup opens the file manager directly. The four-drive mapping and existing operations are retained.

The complete C version is preserved at tag `wildbits-c-final`. Small C excerpts under `tests/reference/` serve only as test oracles. The earlier C build's UART debug option belongs to that tag; the current assembly build has a single configuration.

## Verification

```sh
python3 -m venv .venv
.venv/bin/pip install -r tests/requirements.txt
CC65_HOME=/path/to/cc65 .venv/bin/python -m unittest discover -s tests -v
```

Tests execute the linked assembly build in a 65C02 emulator with modeled banked RAM and I/O, including MicroKernel event/buffer aliasing, two full 255-entry panes, scrolling, refreshes, filename transfers and memory protection. The preserved C reference cases additionally need a host C compiler with AddressSanitizer and UndefinedBehaviorSanitizer; this compiler is not used to build the application. Structure offsets are shared with the assembler through `asm/layout.inc`. The complete machine and its disk devices are not emulated, so hardware validation is still required.

See the [assembly implementation](documentation/assembly.md), [installation](documentation/installing.md), [usage](documentation/using.md), and the [review record](documentation/review.md).

## History and credit

Wildbits File Manager is derived from [Micah Bly (WartyMN)'s F256 f/manager](https://github.com/WartyMN/F256-FileManager). Micah's source author credits, copyright notice in the application's About display, and the original [GNU GPL version 3 license](LICENSE) are retained. This is a renamed and modified fork, maintained by [mbrukner](https://github.com/mbrukner). The bundled cc65 material retains its [separate license](asm/runtime/LICENSE).

The original Git history and GitHub fork relationship are preserved. The `upstream` remote points to Micah's repository; `local/debug-checkpoint` preserves Martin Brukner's local debugging changes before the Wildbits review.

Development lives at [mbrukner/Wildbits-File-Manager](https://github.com/mbrukner/Wildbits-File-Manager). Tag `wildbits-c-final` is the reference for the assembly conversion. The inherited MicroKernel API notice is preserved in [asm/API-NOTICE](asm/API-NOTICE).
