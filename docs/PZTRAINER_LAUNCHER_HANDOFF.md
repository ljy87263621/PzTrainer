# PzTrainer Launcher handoff

The launcher is now part of the root PzTrainer repository. Its source lives in `launcher/`, while the trainer project lives in `trainer/`. The root `pztrainer_launcher.vcxproj` builds the trainer project first and embeds `bin\x64\Release\pztrainer.dll` through `launcher\launcher_resources.rc`.

The runtime flow is:

1. `launcher/main.cpp` resolves the game directory and creates the Win32 UI.
2. `launcher/injector.cpp` starts Steam AppID 108600 when needed and waits for `ProjectZomboid64.exe`.
3. The launcher reads the embedded trainer resource.
4. `launcher/manual_mapper.cpp` maps the x64 image into the authorized local game process.

Build the complete pair from the repository root:

```powershell
.\scripts\build-portable-release.ps1 -Version '1.0.0-beta.1'
```

The default build does not require VMProtect. Protected marker builds require a separately installed SDK and `-EnableVmProtectMarkers -VmProtectRoot <path>`.

The manual mapper remains a high-risk runtime boundary. Do not alter its PE relocation, import, TLS, exception, or section-protection logic without a dedicated test plan and an in-process validation pass. The launcher currently identifies the target by `ProjectZomboid64.exe`; game updates or multiple matching processes can make injection fail or require further hardening.

The portable release is a beta artifact for authorized single-player use. It has not been validated against every Project Zomboid update and should be hash-checked before execution.
