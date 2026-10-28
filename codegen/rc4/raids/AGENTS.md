# Raids C

- Codegen/export wiring: `Assets/Forge/Scripts/CustomMode/RaidsModeData.cs`. Matching runtime/server code: `../horizon-deadlocked-patch/raids` and `../horizon-deadlocked-plugin`. These paths are relative to the Forge repo root.
- Guard `MapConfig.State` and bank readiness (`bankGetHas...`). Reuse bank helpers for XP, levels, rarity, and class restrictions. Hero, Weapon, and Class XP are independent; Hero levels display as `bankGetLevel() + 1`, classes use `bankGetClassLevel()` and Roman suffixes from `bankGetClassLevelSuffix()`.
- Use `menu.h` drawing helpers with `Window_t`; use `windowDrawTextWindow()` for wrapped paragraphs. Use `shouldCloseMenu()` or hub-only `shouldCloseHubMenu()`, calling the owning menu's Close function to restore input. Preserve forced Planet Select opening after mission failure; mark it handled only once visible.
- Split screen is disabled. Use local player slot 0. Hub network IDs start at 190; 200+ is patch-reserved. Check existing enums before assigning IDs.
