# Release

## v1.0.0-beta.1

This release is a Windows x64 portable package containing `pztrainer_launcher.exe`. The trainer DLL is embedded in the launcher and is built from the same source revision.

Build from the repository root:

```powershell
.\scripts\build-portable-release.ps1 -Version '1.0.0-beta.1'
```

The script requires Visual Studio C++ build tools, the Windows SDK, and the `v145` toolset. VMProtect markers are disabled by default. Set `VMP_ROOT` or pass `-VmProtectRoot` with `-EnableVmProtectMarkers` only when the separately installed SDK is available.

The ZIP contains the launcher, README, changelog, and `SHA256SUMS.txt`. It is not a standalone game or JVM distribution. The target machine must have a compatible x64 Steam Project Zomboid installation. Administrator privileges are required for the manual mapper.

Validate the downloaded package before running it:

```powershell
Get-FileHash .\pztrainer-v1.0.0-beta.1-windows-x64-portable.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

This is a beta release. Compatibility with every game update, security product, and Windows configuration is not guaranteed.
