# Using Wildbits File Manager

Disk and flash launches show the animated firebird title screen with Wildbits File Manager branding and the original author credit. Press any key to skip it, or let the animation finish automatically.

The active pane supplies the source for operations; the other pane is the destination. Switch panes with Tab or the left/right cursor keys. Move the selection with up/down and open a directory or launch a recognized file with Enter or `l`.

| Key | Action |
| --- | --- |
| `0` | External SD card |
| `1` | Internal micro SD card |
| `2` | IEC device 8 |
| `3` | IEC device 9 |
| `8`, `9` | RAM, flash |
| `R` | Refresh the active pane |
| `N`, `D`, `S`, `T` | Sort by name, date, size, type |
| `c` | Copy to the other pane |
| `p` | Duplicate in the current directory |
| `r` | Rename |
| Backspace or `x` | Delete, after confirmation |
| `m` | Create a directory |
| `h`, `t` | View as hex or text |
| `F`, `z` | Fill or clear the selected RAM bank |
| `f`, `g` | Search memory, find next |
| `M` | Open a Meatloaf URL on an IEC pane |
| `"` | Format the selected device, after confirmation |
| `C` | Set the real-time clock |
| `a` | About |
| `b`, `d` | Launch installed BASIC or DOS |
| `q` | Reset the machine |

Search text directly, or prefix hex bytes with `#`, for example `#00,FF,A1`. The clock dialog accepts `YY-MM-DD HH:MM` in 24-hour notation. Escape cancels dialogs; viewers also accept Run/Stop or `q` at page prompts.

Copying a file to an occupied name chooses a numbered suffix. Files are copied individually; recursive directory copying is not implemented. Saving a memory bank to a filename writes that file. Memory destinations occupied by the running application, its overlays, strings, filename storage or directory records are protected. Flash cannot be written through the memory pane.

## Current limits

- At most 255 directory entries per pane. Each pane has independent banked record storage; loading one directory does not reduce the other pane's capacity. Longer listings produce an explicit capacity warning. The synthetic home entry on Meatloaf counts toward this limit.
- Filenames are limited to 31 characters; longer names are skipped with a warning, rather than truncated for file operations.
- Paths must fit a 255-byte buffer including their terminator.
- File viewing is limited by the scratch region before filename storage: 224 pages (57,344 bytes). The loader also has an absolute 255-page limit. The final displayed page can include zero padding. Loading a file into a selected RAM bank is limited to 8 KiB; a failed oversized load can leave that bank partially modified.
- SD sizes are estimates from 256-byte blocks; IEC sizes use 254-byte blocks. Directory dates are unavailable from the current three-byte kernel size record, so date sorting has no useful distinction.
- The layout uses an 80-by-60 text screen and the banked MicroKernel ABI.
- Music and text helper programs must be installed at the paths in `strings/strings.txt`. Application launching depends on the installed loader. Launching another program replaces the manager.
