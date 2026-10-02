# Installing Wildbits File Manager

Build the project as described in the README. Copy `build_cc65/release/wildbits-fm.pgz` to either SD card and launch it through the installed PGZ loader. The application expects the Wildbits kernel drive mapping: external SD `0:`, internal micro SD `1:`, IEC device 8 `2:`, IEC device 9 `3:`.

The ZIP also contains `flash/wildbits-fm.bin`, an eight-bank (64 KiB) KUP container, and eight individual 8 KiB chunks. Its launch name remains `fm`; its displayed description is Wildbits File Manager. Installation placement depends on the board's existing firmware layout. The old machine-specific bulk-upload presets have been removed because they also replaced kernel and BASIC images.

This review has built and checked the packages locally. Neither the PGZ nor the KUP package has been validated on the current board yet. Use the PGZ for initial hardware validation before installing the KUP image.
