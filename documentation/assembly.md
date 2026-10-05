# Assembly implementation

The application and runtime build from assembly source. `asm/imported/` holds the initial compiler-output conversion of Micah Bly's implementation plus the reviewed Wildbits changes. Its explicit provenance is intentional: this is an assembly-only build, not a claim that every routine has been manually rewritten. Native replacements live beside that directory, with the existing native text/MMU routines at repository root.

## Directory storage

| Physical RAM banks | Use |
| --- | --- |
| 0–7 | Resident application, heap and software stack |
| 8–12 | UI, disk, viewer, startup and memory-system overlays |
| 18 | Resource strings |
| 20–26 | File-viewer scratch space |
| 27, 28 | Left/right filenames, 32 bytes per slot |
| 29, 30 | Left/right file records, 32 bytes per slot |

A record slot contains an 18-byte file object and a 6-byte list node. The pane's folder object, list-head pointer, path and label remain resident. Native constructors derive record addresses from the entry ID; clearing a directory resets its resident head and counters. Files and list nodes do not use `malloc` or `free`.

Only one record bank is mapped at a time, into CPU slot 6 ($C000–$DFFF). The I/O control register is set to 4 while records are accessed. Panel rendering/sorting, folder traversal and file creation select the owning pane first. Filename and resource transfers save/restore slot 6, and display routines save/restore the I/O page. Every MicroKernel API call goes through `asm/kernel_bridge.s`: the kernel requires its own RAM alias at $C000 in the user LUT when importing buffers or exporting events and data. Startup captures that alias before any mapping changes. The bridge restores it for the call, then restores the pane bank and I/O page while preserving A/X/Y and status (including carry). Bridge calls are foreground-only; application IRQ callbacks must not use the shared saved mapping. A record pointer must not be retained across selecting another pane. Both record banks are protected against file loads, fills, clears and memory copies.

The directory tests model kernel vector calls and their user-LUT alias, including asynchronous polling and buffer imports/exports. They use distinct names in each pane to detect accidental bank substitution. They exercise both banks at 255 entries, scrolling to the final row, redraws, refreshes and an explicit overflow warning. Startup and native filename tests additionally check the stack/heap setup, all active MMU LUTs, boundary slots and truncation/termination.

## Banked data transfers

`asm/em_copy.s` transfers a 256-byte page through the existing resident interbank buffer at $0400. The caller's buffer can live inside an overlay at $A000–$BFFF: reads stage the bank data before restoring that overlay and filling the buffer; writes stage the caller's data before replacing the overlay with the destination bank. This preserves the current directory mapping and avoids copying through an overlay address while the wrong bank is visible. Both viewers and memory search use this helper.

Target regressions load a known file through the kernel vector model and check its actual text and hex output, including multiple pages and the final partial page. Transfer tests cover both directions, resident and overlay buffers, page/bank boundaries, all active LUTs, and mapping restoration.

## Further optimization

The initial native replacements remove the directory heap bottleneck and reduce filename-copy overhead. Most UI and filesystem control flow still uses the imported routines and the existing calling convention. Any further hand optimization should preserve their behavior with target tests and report separate CPU-cycle and binary-size measurements. Disk timing must be measured on hardware.
