# Review record — October 2026

## Git lineage

The upstream baseline is `cd81d75` (November 2025). The local checkout originally dated from July 2025 and was updated in March 2026. The pre-review local modifications were committed as `27c1b83` and preserved on `local/debug-checkpoint`. The fork's `wildbits/review` branch builds on that checkpoint; the original repository is unchanged.

## Review scope

The review covered the active application modules, native memory/text routines, kernel adapter, linker layout, string resources, build and packaging. It is a static review plus targeted execution tests, not a claim that every hardware behavior has been exercised. Historical disabled examples and commented-out code are not supported application features.

The fixes address:

- Hard-coded zero-page addresses that disagreed with linker allocation; MMU routines now select the active LUT and restore MMU control and the caller's interrupt mask.
- Last-column text recursion, zero-length and multi-page text operations, I/O-page preservation, rectangular fills and input editing bounds.
- Reads of 256 bytes into 204-byte buffers, extension overflows, unsafe filename copies, dialog cancellation losing a permanent buffer pointer, and long unbroken status messages.
- File descriptor collisions with stdin/stdout, read/close error propagation, directory record width, directory error events, invalid drive prefixes, path truncation and launch argument termination.
- Empty files, failed opens, short writes, failed closes, copy-name collisions, selected-bank protection and bounded memory loads.
- Directory allocation cleanup, excessive listing counts, sorting stack usage, stale selection after deletion and page jumps.
- SD directory attribute interpretation, size estimates for both SD devices, all four device selectors and disabled keyboard aliases.
- Search boundary conditions, KUP metadata bounds, clock validation/scheduling, progress scaling, serial logging and firmware bank count.
- Strict string lengths/IDs and repeatable release/debug packaging.

Obsolete screenshots, spreadsheets, prebuilt string data, demo code and flash presets were removed from the active tree. They remain recoverable through Git history. Original licensing and authorship were retained.

## Tests and limitations

`tests/test_host.py` extracts the actual portable functions into a host harness with injected I/O failures and runs them under AddressSanitizer and UndefinedBehaviorSanitizer. Cases include paths, extensions, 255-node sorting, malformed hex, empty and page-aligned copies, write failures, long messages, line endings and bounded editing.

`tests/test_assembly.py` assembles the actual native routines and executes them with py65. It checks active LUT selection, register/interrupt restoration, last-column wrapping, zero/255/256/257/511/512/1024 lengths and scrolling. `tests/test_build.py` checks resource and package structure.

Both cc65 release and UART debug configurations must build. No board, SD card, IEC device, UART or flash programming has been exercised as part of this review. Hardware acceptance should cover startup with each device present/absent, SD and IEC navigation/copy/rename/delete, cancellation, viewers, memory operations, helper launching and clock refresh after disk activity. Verify copied files byte-for-byte using disposable test files.

## Assembler conversion

The reviewed baseline should remain available for comparison. Conversion must retain the four-drive mapping, kernel event handling, memory protection and existing operations. Establish code-size and cycle measurements before claiming improvements; disk latency cannot be inferred from CPU-only tests. Preserve the baseline and regression cases while moving implementation to native assembly on the fork.

## October 3: startup hang after directory scan

Hardware testing reported a hang after “41 files found”, with both PGZ and flash launches. The iterative merge introduced during the review used `*tail = list1 != NULL ? list1 : list2`. The pinned cc65 compiler emitted a load from software-stack offset 11 for the non-NULL arm, although `list1` was at offset 9. This linked an unrelated pointer into the file list, producing a cycle or memory corruption. The host C tests could not detect this target-code-generation defect.

Reproducing the compiled sort in banked-memory emulation failed even for a two-entry reverse-ordered directory. Replacing the conditional assignment with explicit `if`/`else` arms fixes the generated load. The new `tests/test_target.py` builds and executes the actual linked release code, using structure offsets compiled from the application headers. It checks empty, singleton, ascending, descending, duplicate and shuffled directories, both filename banks and multiple active LUTs. A 41-file case exercises sorting, rendering every filename and selecting the first entry. The corrected PGZ and flash package still need confirmation on the board.

## October 4: restored title screen

Restored Micah Bly's original firebird character artwork and cycling palette from the preserved checkout, with Wildbits File Manager title/version text and original author credit. Both disk and flash launches show the animation, which ends automatically or on any keypress. Splash data stays in the startup overlay; the routine saves and restores both text palettes instead of reconfiguring the machine.

Release and debug builds pass, as do the 11 existing regression groups. An additional local py65 check executed the linked splash routine with mocked kernel key queries for both immediate skip and full timeout, verifying the captions, balanced stacks, restored palettes, I/O page and MMU mappings. Animation appearance and timing still need confirmation on hardware.
