# Custom map C

- Edit templates here or original map sources in `Assets/Maps/<map>/ModCode/rc4`, not generated build copies. Register new modules and lifecycle hooks through the existing codegen/CustomCodeGen configuration.
- Keep changes minimal and preserve surrounding formatting and line endings. Typically: two-space indentation, function braces on their own line, control-flow opening braces on the same line.
- No file-scope `static`. Use function-local `static` only when state must persist between calls. Prefer fixed-size tables/buffers and bounded strings.
- Use PS2 types from `<tamtypes.h>` and APIs from `<libdl/...>`. Clone `https://github.com/Dnawrkshp/libdl.git` into a temporary folder for reference; check its `include/` and `src/` and include each API's declaring header.
- Reuse `base/` and `common/` helpers. Uninstall map-specific network handlers through the Cleanup hook before map code unloads.
- Treat shared structs/messages/function tables as ABI: synchronize matching C/C# definitions, preserve field order/types, and append function-table entries. Keep custom message payloads within 504 bytes; paginate larger data.
- Leave builds and Unity Editor automation to the user unless explicitly requested; ask them to rebuild and verify affected maps.
