# PzTrainer development handoff

## Repository layout

- `trainer/src/`: DLL runtime, JNI bridges, features, settings, and UI.
- `trainer/third_party/`: vendored ImGui, MinHook, and OpenJDK headers.
- `trainer/pztrainer.vcxproj`: Windows x64 DLL project.
- `launcher/`: Win32 launcher, manual mapper, resources, and UI.
- `pztrainer_launcher.vcxproj`: launcher project at repository root.
- `scripts/build-portable-release.ps1`: trainer-first portable build.

## Build contract

Both projects use C++17 and the `v145` x64 toolset. The trainer output is written to `bin\x64\Release\pztrainer.dll`; the launcher resource compiler reads that exact file and produces `bin\x64\Release\pztrainer_launcher.exe`.

VMProtect markers are opt-in. The SDK path is supplied with `VMP_ROOT` or the `VmProtectRoot` MSBuild property. No user-specific path is committed to the project files.

## Runtime contract

The trainer attaches to a compatible x64 Project Zomboid JVM process, installs the OpenGL/Win32 overlay hooks, and gates game writes behind its single-player safety checks. The launcher starts Steam AppID 108600 when necessary and manual-maps the embedded DLL into `ProjectZomboid64.exe`.

The manual mapper and JNI signatures are coupled to the target game build. Any game update can require a compatibility review. Game-in-process injection, shutdown behavior, and anti-cheat interactions remain manual validation items for each release.

## Verification checklist

1. Build the portable package from a clean checkout.
2. Verify the ZIP contents and SHA-256 manifest.
3. Confirm the launcher starts with an already running compatible game.
4. Confirm Steam launch detection and error display when no game is running.
5. Confirm the trainer menu opens with Insert in an authorized single-player session.
6. Record the tested game version and known limitations in the release notes.
