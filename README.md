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
.\scripts\build-portable-release.ps1 -Version '1.0.0-beta.9'
```

The script builds the trainer first, builds the launcher with that DLL embedded as a resource, writes SHA-256 sums, and produces a portable ZIP under `artifacts\`.

The beta.9 portable package uses the marker-disabled build with the static MSVC runtime. Its launcher embeds the matching DLL and Java helpers; end users do not need Visual Studio, a separate VC runtime, JDK, or the VMProtect SDK. A compatible Project Zomboid installation is required. Earlier beta.2 protection details remain documented in [docs/RELEASE.md](docs/RELEASE.md).

The trainer automatically follows the active game renderer. Vanilla top-down view uses the isometric ESP/model-capture path; an enabled compatible ViewPoint renderer uses its current 3D camera for ESP and aiming. Manual projection/compatibility switches have been removed, and old saved flags no longer override camera selection. During 3D camera startup the overlay waits for a valid camera. The portable package embeds the bridge and Java helper but does not redistribute the third-party ViewPoint mod.

Beta.9 moves native model submissions inside the active world frame, establishes each model's camera, removes legacy outline/alpha writes from ESP collection, and corrects capture replay at scaled world framebuffer sizes. See [native world-frame effects](docs/NATIVE_WORLD_FRAME_EFFECTS.md) for the observed game errors, production capture tests and live-game validation limits. Automatic projection, named VHS/CD programs in the ordinary Spawner, independent fill/rim controls, per-feature visibility filters and default configuration loading remain available.

## Local SDK

Keep the game installation and JDK outside the repository. Set `PZ_GAME_ROOT` to the Project Zomboid directory containing `projectzomboid.jar` and set `PZ_JDK_ROOT` to a JDK 25 directory containing `javac.exe` and `jar.exe`. Check the complete local setup with:

```powershell
.\scripts\check-local-sdk.ps1
```

The repository contains JNI/JVMTI headers under `trainer\third_party\openjdk\include`. Do not copy the proprietary game JAR, game DLLs, or a Viewpoint mod into the repository. The build scripts use `PZ_GAME_ROOT` and `PZ_JDK_ROOT`; see [docs/LOCAL_SDK.md](docs/LOCAL_SDK.md).

## Usage boundary

The launcher is intended for the user's own local x64 Project Zomboid process and single-player testing. It requires administrator privileges because manual mapping writes into another process. Do not use it against systems or processes without authorization, and expect game updates or anti-cheat software to make a build incompatible.

The beta release has not been validated against every Project Zomboid version or in every Windows configuration. Read the release notes and verify the hash before running a downloaded artifact.
