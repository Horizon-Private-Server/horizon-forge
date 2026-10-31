# Custom map C

- Edit templates here or original map sources in `Assets/Maps/<map>/ModCode/rc4`, not generated build copies. Register new modules and lifecycle hooks through the existing codegen/CustomCodeGen configuration.
- Keep changes minimal and preserve surrounding formatting and line endings. Typically: two-space indentation, function braces on their own line, control-flow opening braces on the same line.
- Match the surrounding comment style: short lowercase `//` notes above logical blocks, usually without a trailing period (e.g. `// find free slot`). Use a second short line for an engine quirk or ordering constraint when needed. Avoid formal paragraphs, redundant narration, and unrelated history. Preserve file headers and function separators.
- No file-scope `static`. Use function-local `static` only when state must persist between calls. Prefer fixed-size tables/buffers and bounded strings.
- Use PS2 types from `<tamtypes.h>` and APIs from `<libdl/...>`. Clone `https://github.com/Dnawrkshp/libdl.git` into a temporary folder for reference; check its `include/` and `src/` and include each API's declaring header.
- Reuse `base/` and `common/` helpers.
- Hooks at instruction addresses inside the game's map code overlay reset when the overlay reloads on map changes. Install them directly in Init without installed flags, saved instructions, or cleanup restoration. Check the hook site's address against the map's `code.*.def` ranges; the callback's location does not determine the hook's lifetime.
- Cleanup runs when the map unloads. Mobys, their Gubers/PVars, and map overlay state are automatically unloaded; do not destroy mobys or reset overlay-owned state in Cleanup. Use Cleanup only to undo hooks/registrations stored outside the code overlay that persist after map unload (including network handlers and shared callbacks), restoring hooks only if still owned by the map, or to free memory allocated with `malloc`.
- Treat shared structs/messages/function tables as ABI: synchronize matching C/C# definitions, preserve field order/types, and append function-table entries. Keep custom message payloads within 504 bytes; paginate larger data.
- Leave builds and Unity Editor automation to the user unless explicitly requested; ask them to rebuild and verify affected maps.
