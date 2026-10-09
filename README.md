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
.\scripts\build-portable-release.ps1 -Version '1.0.0-beta.2'
```

The script builds the trainer first, builds the launcher with that DLL embedded as a resource, writes SHA-256 sums, and produces a portable ZIP under `artifacts\`.

The script is the default marker-disabled developer build. The published beta.2 package was produced from a VMProtect-processed trainer DLL and then rebuilt through PzLauncher; the protected-release handoff is documented in [docs/RELEASE.md](docs/RELEASE.md). End users of the published package do not need the VMProtect SDK.

The trainer includes optional Viewpoint 3D compatibility. When a compatible Viewpoint mod is installed in the target Project Zomboid build, the visual page exposes 3D ESP for zombies, players, animals, and vehicles, and the aim pages expose separate 3D Legit/Rage switches. The portable package embeds the bridge and Java helper but does not redistribute the third-party Viewpoint mod.

The current source baseline also contains responsive item quantity controls and the light/media payload routes. These changes are in the source tree and validation builds; they are separate from the already published beta.2 artifact until a new release is cut.

## Local SDK

Keep the game installation and JDK outside the repository. Set `PZ_GAME_ROOT` to the Project Zomboid directory containing `projectzomboid.jar` and set `PZ_JDK_ROOT` to a JDK 25 directory containing `javac.exe` and `jar.exe`. Check the complete local setup with:

```powershell
.\scripts\check-local-sdk.ps1
```

The repository contains JNI/JVMTI headers under `trainer\third_party\openjdk\include`. Do not copy the proprietary game JAR, game DLLs, or a Viewpoint mod into the repository. The build scripts use `PZ_GAME_ROOT` and `PZ_JDK_ROOT`, with the current machine's conventional locations as fallbacks; see [docs/LOCAL_SDK.md](docs/LOCAL_SDK.md).

## Usage boundary

The launcher is intended for the user's own local x64 Project Zomboid process and single-player testing. It requires administrator privileges because manual mapping writes into another process. Do not use it against systems or processes without authorization, and expect game updates or anti-cheat software to make a build incompatible.

The beta release has not been validated against every Project Zomboid version or in every Windows configuration. Read the release notes and verify the hash before running a downloaded artifact.
