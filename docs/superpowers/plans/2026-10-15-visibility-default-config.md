# Visibility and default configuration implementation plan

**Goal:** Independent actual-visibility filters for ESP/material features, reliable model ownership and first-person composition, and automatic loading/upgrading of a chosen saved configuration.

**Architecture:** Keep existing binary configuration payload layouts frozen; append versioned visibility options. Store the default configuration name in UI preferences and load it once on an initialized JNI/render thread. Replace implicit native outline capture with explicitly owned model rendering where the game API supports it. Preserve game GL state around 3D composition.

**Tech Stack:** Existing C++/MSVC, JNI/Java 17, ImGui custom controls, game and ViewPoint APIs, LWJGL GPU regression tests.

**Constraints:** Work independently in the current checkout; preserve previous uncommitted work. Actual game visibility means occlusion/fog, not the facing heuristic. Each feature has an independent filter, initially disabled. Model fill and rim remain independent. Skip the local player. Configuration upgrades preserve existing options; missing new options are disabled. Portable release embeds Java helpers.

- [x] Reproduce and fix inherited GL state rejecting ViewPoint composition; assert pixels and state restoration.
- [x] Implement independent visibility options and actual-game visibility snapshots; apply to vanilla and 3D ESP and model fill/rim separately.
- [x] Implement explicit owned native model capture; verify owner isolation, unseen-object selection, self-pixel exclusion and alpha-cutout preservation. Live game submission remains part of the documented game acceptance check.
- [x] Add default configuration preference and custom per-row toggle; report missing files and clear deleted/default-directory references and load once after JNI initialization.
- [x] Append options to configuration version 21; test old-version migration, atomic rewrite, defaults and corrupt-file handling.
- [x] Run relevant CPU/JVM/GPU checks, rebuild embedded resources and portable launcher, verify exact embedded JAR/DLL and ZIP manifest, document live-game verification limits.

Portable artifact: `artifacts/visibility-default-config-20261015-windows-x64-portable.zip`
SHA256: `004cf422162e12f7a24416250325e21949c4828b42da3559c17db31c44bf3457`

Live game acceptance is explicitly pending; no running game process was available for it.
