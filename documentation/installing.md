# Installing Wildbits File Manager

Build the project as described in the README. Copy `build_asm/release/wildbits-fm.pgz` to either SD card and launch it through the installed PGZ loader. The application expects the Wildbits kernel drive mapping: external SD `0:`, internal micro SD `1:`, IEC device 8 `2:`, IEC device 9 `3:`.

The assembler build writes ROM parts `fm.00` through `fm.07` directly into `build_asm/release/`, each exactly 8 KiB. The ZIP includes the same parts under `flash/`, alongside `flash/wildbits-fm.bin`, the complete eight-bank (64 KiB) KUP container. Its launch name remains `fm`; its displayed description is Wildbits File Manager. Installation placement depends on the board's existing firmware layout. The old machine-specific bulk-upload presets have been removed because they also replaced kernel and BASIC images.

The C baseline was tested on hardware. This assembly build has been checked in emulation but still needs testing on the current board. Use the PGZ for initial hardware validation before installing the KUP image.
