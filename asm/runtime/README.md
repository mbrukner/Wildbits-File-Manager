# Assembly runtime provenance

These assembly helpers and their required include files come from the official cc65 V2.19 source tree, commit `5552824`. Their original author comments and [license](LICENSE) are retained. Whitespace is normalized; helper logic is unchanged. Only modules needed by this application are vendored; `tools/build.py` assembles them into a local archive on every build.

`tables.s` preserves the ASCII data from cc65's `_hextab.c` and `_longminstr.c` as assembly directives. Application startup is implemented separately in `../startup.s`. There is no dependency on the former binary `wildbits.lib`.
