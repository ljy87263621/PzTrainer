# Recorded media and silent aim repair

**Goal:** Spawn playable named VHS/CD recordings from the loaded game registry and make manual silent aim collect and redirect targets in vanilla and ViewPoint modes; publish source and a portable prerelease under the existing GitHub authorization.

**Architecture:** A small Java RecordedMediaItems helper reads the active registry (including registered mods), exposes named entries, and creates real InventoryItems with setRecordedMediaData before insertion. Native item tasks use a dedicated media creation stage on the game main thread and retain the created object for backpack/ground insertion. Catalog keys use RecordedMedia:<id>, and only the local native creation route supports those keys. Generic VHS/CD entries resolve a playable recording in their own category; blank registry/missing IDs fail explicitly. Existing configuration layouts remain unchanged. Rage collection and redirection use shared policy predicates covering silent aim independently of automatic aim/fire.

**Stack:** Existing C++17/JNI main-thread task queue, Java 17-compatible helper in the embedded player JAR, real Project Zomboid JVM fixtures, MSVC x64 static CRT portable launcher.

## Task 1 — Silent aim settings and dispatch
- Reproduce silent-only collection failure with a C++ fixture compiled against the current production collection functions.
- Add trainer/src/features/aim/rage_targeting_policy.hpp. HasRageTargetingMode(preset) includes automatic_aim, automatic_fire, silent_aim, magic_bullet. ShouldAdjustRageAim(preset,aiming) requires aiming and automatic_aim/silent_aim/magic_bullet.
- Replace all three collection gates and UpdateRageAim dispatch gate with those predicates. Improve manual silent-aim status text. Clear overrides when not aiming so an old lock cannot persist.
- Run the policy matrix, existing native ballistics test and actual JVM firing-transform tests. Cover disabled presets, empty bones, target categories, manual aim, automatic fire and both projection modes.

## Task 2 — Playable media catalog and creation
- Add trainer/tests/RecordedMediaItemsTest.java with real engine script definitions and RecordedMedia register fixtures; confirm base factory gives index -1 and old creation fails playable expectations.
- Add trainer/src/java/pztrainer/player/items/RecordedMediaItems.java with catalog() returning flattened key/display/category/search/base-type rows and create(String) returning a validated InventoryItem. Enumerate media types 0 and 1; accept only CDs/Retail-VHS/Home-VHS with lines and valid indexes. Search translated and English title/subtitle/author plus media ID and VHS/录像带 aliases. Reject unknown keys and absent category recordings.
- Add trainer/src/bridge/recorded_media_catalog.hpp/.cpp to load the embedded helper, append/deduplicate rows, reuse base textures, and expose its class for the main-thread creation stage. Register the CPP in trainer/pztrainer.vcxproj.
- Extend item_catalog_search.hpp dynamic-key predicates and item_bridge.cpp catalog status/creation stage. For media entries queue create(key), keep the real object, then AddItem(object) or AddWorldInventoryItem(object). Reject unsupported alternate payload routes and batch media keys.
- Add scripts/test-recorded-media.ps1, compile helper/tests against the local game JAR, run isolated profile, check exact index/type/name/lines, multiple instances, unknown/empty records, category mapping, search and backpack insertion. Never distribute game data.
- Add helper compilation to scripts/build-viewpoint-chams.ps1 and update embedded JAR.

## Task 3 — Delivery and regression
- Run the new tests and the relevant earlier item/model/config regression scripts. Build the trainer, then rebuild launcher with BuildProjectReferences=false so it embeds the exact trainer DLL.
- Verify PE imports have no VC runtime dependency; compare embedded JAR/DLL bytes; package Lua, docs and SHA256SUMS.
- Update CHANGELOG/README and docs/RECORDED_MEDIA_SILENT_AIM.md with findings, verified boundaries and reproduction commands. Review staged files for proprietary data, credentials and unrelated artifacts.
- Fetch once, preserve existing commits and user edits, commit the authorized session changes, push without force. Create a new beta prerelease and upload portable ZIP if the user has not selected source-only. Verify remote commit/tag/assets and report actual URLs. No live-game success claim without a live test.
