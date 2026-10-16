# v1.0.1 Stable and VMProtect marker preparation

**Goal:** Prepare the user-confirmed baseline as v1.0.1 Stable and deliver only the marker-enabled trainer DLL for manual VMProtect processing.

**Architecture:** Keep `main` as the Stable baseline and create `beta` from the same commit. Future work starts on `beta`; promoting to `main` requires an explicit Stable request. Build the existing MSVC x64 static-CRT trainer into an isolated versioned directory, leaving the existing launcher and ordinary DLL untouched.

**Tech stack:** MSVC v145, MSBuild, JNI/JVMTI, VMProtect C SDK, PowerShell, Python PE/disassembly verification.

## Constraints

- Work independently in the current session, as requested by the user.
- Do not publish a Release, create a release tag, invoke VMProtect compilation, or build/package the Launcher in this phase.
- Preserve the user-confirmed rendering, automatic projection, media generation and aiming behavior.
- Keep per-frame ESP, GL hooks, capture/replay, JNI identity lookup and JVMTI callbacks outside protected regions.
- Deliver an input DLL, SHA-256, marker inventory and instructions for returning the processed DLL.

## Tasks

- [x] Audit source markers against actual initialization/hot call paths; add mutation markers to the native/ViewPoint model, Rage fire and recorded-media initialization paths. Keep ready/retry guards outside those markers, add noinline boundaries, and remove the duplicate bootstrap End.
- [x] Add a marker inventory/verifier; demonstrate that the pre-change inventory fails the intended contract, then verify source and compiled PE marker imports/references and embedded helper resources.
- [x] Add an isolated trainer-only build script that refuses to overwrite an existing handoff DLL and records input hashes. Build with `/p:EnableVmProtectMarkers=true`, `/p:BuildProjectReferences=false` and separate OutDir/IntDir; do not invoke the portable packaging script.
- [x] Set the display version to `1.0.1 Stable`, document Stable/Beta rules and VMP handoff, and make ordinary packaging versions explicit rather than silently reusing an old Beta version.
- [ ] Run appropriate native render/aim/media regressions and inspect the final diff; commit the explicitly requested Stable preparation to main, synchronize the remote without force, create beta at that baseline and finish on beta.
- [ ] Verify remote branch heads/default, no new Release/tag, preserved previous binaries, and handoff DLL hash. Final Launcher and protected-runtime verification remain pending the user's VMProtect output.
