# Dynamic moveable item catalogue

**Goal:** Include furniture pickup items such as cubicle dividers in the item generator.

**Architecture:** Supplement ScriptManager's ordinary definitions with a snapshot of the loaded IsoSpriteManager map. Accept sprites marked IsMoveAble only after the game's Moveable parser validates them. Use the engine's `Moveables.<sprite>` factory identifier and display name/icon, retain each sprite/part, deduplicate by identifier and expose game-native generation. Keep hidden/obsolete script filtering intact.

**Tech stack:** Existing C++17/JNI bridge and ImGui list. No additional Java runtime helper or new installation.

- [x] Verify cubicle properties from the installed tile definitions and the game's factory bytecode.
- [x] Add failing catalogue/search tests for dynamic sprite entries, duplicate identifiers and cubicle aliases.
- [x] Implement the independent moveable catalogue collector and shared search helper. Add furniture categorization and refresh diagnostics.
- [x] Verify the collector against the game JVM with fixture sprites and the actual factory, and verify ordinary item search behavior.
- [x] Build DLL/launcher, verify embedded resources and package a local portable validation build. Preserve all previous ESP work; report live-game verification limits.
