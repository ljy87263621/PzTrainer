# Model capture lifecycle and explicit recorded media selection

**Goal:** Restore native and first-person model effects, expose named VHS/CD selection, and document the automatic-stop issue on hover. Preserve the user-confirmed silent-aim fix and automatic-stop implementation.

**Architecture:** Follow the bone ESP snapshot identities into the exact live model slot; initialise ModelSlotRenderData using the engine two-stage protocol and balance render references. For ViewPoint, publish entity/effect selections from the overlay and render them during WorldRenderer.finish before ModelPass.endFrame and frame retirement. Validate that method with JVMTI and ClassFile before modifying it. Model ownership remains explicit, with no screen-box guesses. Add a VHS/CD navigation page that reuses item generator controls/dispatch and displays only registered programme entries with VHS/CD category filters. Normal items keep their existing flow. Add a custom RoundedTooltip to automatic stop.

## Tasks and checks
1. Reproduce the screenshot exception against the installed game class (direct alloc().init(slot) leaves character null). Add constructor-order/refcount and slot-ownership regressions; initialise model data before frame data and isolate per-entity failure.
2. Verify installed ViewPoint frame retirement/reset timing; test an end-of-frame hook before retirement, preserve render state, disable stale published selections, and prove callback insertion against the actual installed classes. Replay fill, rim, both, first-person/Panini, and reset scene data between frames.
3. Add recorded-media base type metadata to ItemCatalogEntry and native collector. Add VHS/CD page/navigation/search entry and filter programme rows only. Reuse native backpack/ground quantity/execute path; show explicit unavailable/refresh information for empty registry. Add category/selection regression tests.
4. Add automatic-stop tooltip with symptom and unequip/re-equip workaround; add localized strings. Do not modify rage_aim.cpp.
5. Run targeted real JVM and OpenGL tests plus item/layout/config regressions; build updated embedded helper, DLL and launcher, verify exact nested resources/static CRT and portable manifest. Update notes with confirmed silent-aim result and remaining live-game boundaries. Commit and publish source plus portable beta.7 under the continuing delivery authorization.
