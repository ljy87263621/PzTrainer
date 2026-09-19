# PzTrainer

PzTrainer is a Windows x64 Project Zomboid single-player assistance DLL and its companion launcher. The launcher starts the Steam game when needed and embeds the exact trainer DLL produced by the same build.

This repository is organized as one build unit:

- `trainer/` contains the DLL, JNI bridge, ImGui/MinHook sources, tests, and embedded Lua resources.
- `launcher/` contains the Win32 launcher and manual mapper.
- `pztrainer_launcher.vcxproj` builds the launcher after the trainer project has produced `bin\x64\Release\pztrainer.dll`.

## Build prerequisites

- Windows x64
- Visual Studio C++ build tools with the Windows SDK and the `v145` toolset used by the projects
- PowerShell 5+
- VMProtect is optional for the default build. Set `VMP_ROOT` or pass `-VmProtectRoot` only when building protected markers.

From the repository root, run:

```powershell
.\scripts\build-portable-release.ps1 -Version '1.0.0-beta.1'
```

The script builds the trainer first, builds the launcher with that DLL embedded as a resource, writes SHA-256 sums, and produces a portable ZIP under `artifacts\`.

## Usage boundary

The launcher is intended for the user's own local x64 Project Zomboid process and single-player testing. It requires administrator privileges because manual mapping writes into another process. Do not use it against systems or processes without authorization, and expect game updates or anti-cheat software to make a build incompatible.

The beta release has not been validated against every Project Zomboid version or in every Windows configuration. Read the release notes and verify the hash before running a downloaded artifact.
