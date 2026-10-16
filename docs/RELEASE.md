# Release

Current Release x64 builds use the static MSVC runtime (`/MT`) in both Trainer and Launcher. They do not need separate `msvcp140.dll` or `vcruntime140*.dll` files. Rebuilding Trainer changes the protection input: process the newly built marker-enabled DLL in VMProtect again before embedding it in a protected Launcher. A marker-enabled DLL that has not been processed still imports `VMProtectSDK64.dll` and must not be distributed as a protected release.

## v1.0.1 Hotfix1 Stable promotion

Promote v1.0.1 beta1 to `main`; retain `beta` as the default development line. The display name is `v1.0.1 Hotfix1`, channel Stable, artifact version `1.0.1-hotfix.1`. No Release/tag is created by this promotion.

The Trainer, embedded helpers and Lua are unchanged from beta1. Reuse its user-processed DLL (`SHA-256: 7ec823eaa7d39f1f9a2bfe6b06fe1e4eee1fcdeef7f589118ea6dd5a273354b6`) and rebuild only Launcher with `BuildProjectReferences=false` and `EnableVmProtectMarkers=false`. Verify the embedded bytes, four localized version labels and portable checksums. Preserve prior Stable/Beta artifacts. Prepared-item engine/JNI tests are separate from live multiplayer server acceptance, which remains outstanding; see [prepared item routes](PREPARED_ITEM_ROUTES.md).

## v1.0.1 Stable preparation

The user confirmed the beta.9 native top-down model fix. `main` now represents Stable; `beta` is the default development line unless the user explicitly requests Stable promotion. This phase produces only the marker-enabled trainer input using `scripts/build-vmp-marker.ps1`, with isolated output/intermediates. No Stable tag/Release or rebuilt Launcher is produced. Return the VMProtect-processed DLL before Launcher embedding and portable packaging; see [VMP handoff](VMP_HANDOFF.md) for paths, marker decisions and the subsequent verification gate.

## v1.0.0-beta.9

Moves native character capture into the active world frame, initializes/restores the per-model camera, removes legacy outline registry writes from snapshot collection, and keeps replay in the destination mask viewport when the world buffer is scaled. Production JNI/JVMTI, MinHook and actual game shader pixel regressions cover both zombie/player ownership, independent fill/rim modes, local-player exclusion and three buffer scales. See [native world-frame effects](NATIVE_WORLD_FRAME_EFFECTS.md).

Restart the game to replace the loaded helper/frame callback and discard old outline state. This Portable prerelease embeds the matching DLL/helper and uses the static CRT; no external JDK, VC runtime or VMProtect SDK is required by the recipient. Live save-game rendering and error-counter validation remain pending.

## v1.0.0-beta.8

Repairs native per-player model lighting initialization and automatically routes ESP, model effects, Lua item ESP and aim projection according to the active ViewPoint renderer. Removes the manual projection switches and the separate VHS/CD page; named media remain in the ordinary Spawner. See [automatic projection and native capture](AUTO_PROJECTION_NATIVE_CAPTURE.md) for the reproduced exception, test scope and remaining live-game validation.

The marker-disabled Portable build uses static CRT and embeds the matching trainer DLL and helper JAR. No separate JDK, VC runtime, or VMProtect SDK is required by the recipient. The game and optional ViewPoint mod remain external prerequisites.

## v1.0.0-beta.7

Updates native ModelSlotRenderData initialization/reference ownership, hooks model effects before ViewPoint frame retirement, and adds explicit named VHS/CD selection in its own page. Automatic stop receives a known-issue tooltip; its implementation and the confirmed silent-aim fix remain unchanged. Use the ordinary marker-disabled Portable build with the static CRT. See [capture and media UI](MODEL_CAPTURE_MEDIA_UI.md) for validation and live-game limits.

## v1.0.0-beta.6

Ordinary marker-disabled x64 Portable prerelease, using static CRT in both executables. The launcher embeds the newly built trainer DLL, including the updated recorded-media Java helper; no external developer runtime is required. The package includes Lua examples, feature notes and SHA256SUMS. Project Zomboid itself and the optional ViewPoint mod are not redistributed.

The trainer is built first; rebuild the launcher with `/p:BuildProjectReferences=false` to retain that exact trainer image. Verify the embedded RCDATA DLL and helper JAR match the source build, and inspect PE imports for absent VC runtime / VMProtect SDK dependencies before packaging.

Engine/JVM, native ballistics, GPU and configuration regressions are documented in the feature notes. Actual in-game playback and the reported aim scenario remain unverified, so this version is published as a prerelease.

## v1.0.0-beta.2

This release is a Windows x64 portable package containing `pztrainer_launcher.exe`. The trainer DLL is embedded in the launcher and is built from the same source revision. It includes the merged Viewpoint 3D compatibility path for ESP, model Chams, and optional 3D Legit/Rage aiming.

The published package was built from the x64 VMProtect-marker trainer, processed with the local VMProtect GUI, and then embedded by PzLauncher. The final launcher resource contains the VMProtect-processed trainer image, so a target user does not need the VMProtect SDK or `VMProtectSDK64.dll`.

Build from the repository root:

```powershell
.\scripts\build-portable-release.ps1 -Version '1.0.0-beta.2'
```

The script requires Visual Studio C++ build tools, the Windows SDK, and the `v145` toolset. VMProtect markers are disabled for ordinary portable packaging. Use `scripts/build-vmp-marker.ps1 -Version '1.0.1' -VmProtectRoot '<SDK root>'` for a separate protected-build input; the portable script now rejects marker-enabled packaging.

The repository script is the ordinary marker-disabled build path. For a protected release, build the marker-enabled trainer, process that DLL in VMProtect, stage the resulting protected DLL at `bin\\x64\\Release\\pztrainer.dll`, and rebuild the launcher with `/p:BuildProjectReferences=false` so the project reference cannot replace the protected image before resource compilation.

The ZIP contains the launcher, README, changelog, and `SHA256SUMS.txt`. It is not a standalone game, JVM, or Viewpoint mod distribution. The target machine must have a compatible x64 Steam Project Zomboid installation. Viewpoint 3D features additionally require a compatible Viewpoint mod in that installation. Administrator privileges are required for the manual mapper.

Validate the downloaded package before running it:

```powershell
Get-FileHash .\pztrainer-v1.0.0-beta.2-windows-x64-portable.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

This is a beta release. Compatibility with every game update, security product, and Windows configuration is not guaranteed.
