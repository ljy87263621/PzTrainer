# Release

## v1.0.0-beta.2

This release is a Windows x64 portable package containing `pztrainer_launcher.exe`. The trainer DLL is embedded in the launcher and is built from the same source revision. It includes the merged Viewpoint 3D compatibility path for ESP, model Chams, and optional 3D Legit/Rage aiming.

The published package was built from the x64 VMProtect-marker trainer, processed with the local VMProtect GUI, and then embedded by PzLauncher. The final launcher resource contains the VMProtect-processed trainer image, so a target user does not need the VMProtect SDK or `VMProtectSDK64.dll`.

Build from the repository root:

```powershell
.\scripts\build-portable-release.ps1 -Version '1.0.0-beta.2'
```

The script requires Visual Studio C++ build tools, the Windows SDK, and the `v145` toolset. VMProtect markers are disabled by default. Set `VMP_ROOT` or pass `-VmProtectRoot` with `-EnableVmProtectMarkers` only when the separately installed SDK is available.

The repository script is the ordinary marker-disabled build path. For a protected release, build the marker-enabled trainer, process that DLL in VMProtect, stage the resulting protected DLL at `bin\\x64\\Release\\pztrainer.dll`, and rebuild the launcher with `/p:BuildProjectReferences=false` so the project reference cannot replace the protected image before resource compilation.

The ZIP contains the launcher, README, changelog, and `SHA256SUMS.txt`. It is not a standalone game, JVM, or Viewpoint mod distribution. The target machine must have a compatible x64 Steam Project Zomboid installation. Viewpoint 3D features additionally require a compatible Viewpoint mod in that installation. Administrator privileges are required for the manual mapper.

Validate the downloaded package before running it:

```powershell
Get-FileHash .\pztrainer-v1.0.0-beta.2-windows-x64-portable.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

This is a beta release. Compatibility with every game update, security product, and Windows configuration is not guaranteed.
