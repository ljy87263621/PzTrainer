# Repository workflow

- `main` is the Stable line. Only promote or commit to `main` when the user explicitly requests a Stable version/promotion.
- `beta` is the default development line. Without an explicit Stable request, put source commits and integration work on `beta`; start temporary `codex/*` branches from `beta` and merge them back into `beta`.
- Keep the GitHub default branch as `main`. Finish Stable preparation with the working directory checked out on `beta` for subsequent development.
- A version preparation, source push or tag does not authorize a GitHub Release. Publish a Release only when requested; Beta releases are prereleases, Stable releases are regular releases.
- Current baseline: **v1.0.1 Stable**, based on user-confirmed beta.9 behavior. The current phase prepares only the VMProtect marker input DLL. Wait for the user's processed DLL before compiling/packaging its Launcher or creating a Release/tag.
- Use `scripts/build-vmp-marker.ps1` for trainer-only input preparation. Never pass an unprocessed marker DLL to the portable packager or distribute the SDK DLL to recipients.
- Preserve processed DLLs: rebuilding Trainer invalidates their relationship to the source/input hash. When embedding a processed DLL, build Launcher with `BuildProjectReferences=false` and verify its embedded DLL bytes match exactly.
- Keep per-frame rendering/ESP/capture, OpenGL hook entry points, JNI identity lookups and JVMTI callbacks outside VMP regions. Initialization ready/retry guards stay outside new regions. Update `protection/markers.json` and run `scripts/verify-vmp-markers.py` when changing trainer markers.
- Read `trainer/src/ui/AGENTS.md` before editing trainer UI. Preserve user changes and use scoped verification appropriate to the modified paths.
