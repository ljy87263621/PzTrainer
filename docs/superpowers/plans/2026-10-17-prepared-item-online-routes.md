# Prepared item online routes repair

**Goal:** Repair prepared-item routes and deliver a v1.0.1 beta1 VMP Marker DLL for the user's manual protection step.

**Architecture:** Share C++ prepared-item capability policy between catalogs and execution. Corpse insertion creates concrete items on the main thread before serialization; media payloads carry the selected recording's real base type and index. Leave string-only retrieval paths unsupported where the game cannot reconstruct dynamic furniture or programme state.

**Tech stack:** Existing C++17/JNI asynchronous queue, Java 17-compatible helper code, installed Project Zomboid JVM/API, MSVC x64 static CRT, PowerShell tests.

## Constraints

- Continue independently on `beta`; retain the v1.0.1 Stable protected DLL, symbols, Launcher and portable package unchanged.
- No Release, Stable promotion, automatic VMProtect processing, Launcher build/package or live server interaction this round. Wait for the user's processed beta1 DLL before Launcher packaging.
- Furniture: Game, CorpsePayload, LightPayload, MediaPayload. Named/generic recorded media: Game, CorpsePayload, MediaPayload.
- Preserve exact furniture sprite/orientation and media programme identity; never silently convert a programme key to a blank base VHS/CD.
- Claims of multiplayer support are limited to route/API/serialization regressions until a real server scenario is tested.

## Implementation and verification

- [x] Reproduce native-only catalog masks in existing JVM/C++ fixtures before changing them; add policy matrix tests for every spawn method.
- [x] Add `PreparedSpawnItems.java` with validated concrete prepared-item container insertion; route corpse prepared items through it while preserving ordinary-item insertion. Test independent instances, exact media/sprite state, container ownership, invalid input rejection and engine serialization round trips.
- [x] Extend `MediaItemPayload.java` to resolve programme keys into base type plus real media index, and count selected programmes correctly. Test with actual DeviceData media removal and recorded-media registry.
- [x] Apply masks and shared execution guards in moveable/recorded catalogs and item bridge. Explain query matches excluded by the current scheme in the existing custom UI.
- [x] Build the updated helper resources, run targeted JVM/native catalog and UI/layout regressions, then compile a marker-enabled Trainer into `artifacts/pztrainer-v1.0.1-beta.1-vmp/input`. Prepare Launcher source display version `1.0.1 beta1` without building it. Verify all 20 marker regions, both embedded helpers, source diff and preserved Stable hashes.
- [x] Document the compatibility matrix and remaining server validation; commit the repair to beta only.
