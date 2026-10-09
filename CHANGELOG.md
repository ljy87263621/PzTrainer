# Unreleased source baseline

- Added responsive item quantity controls with bounded step buttons, Ctrl fast-step support, and offline layout/interaction coverage.
- Added light and media payload routes with Java/C++ bridge validation and inventory-only destination handling.
- Added a local SDK check script and unified `PZ_GAME_ROOT`/`PZ_JDK_ROOT` discovery for Java and Viewpoint scripts.

# v1.0.0-beta.2

Second public beta release, including the merged Viewpoint 3D compatibility work and configurable rage weapon modifiers.

- Added optional Viewpoint 3D ESP for zombies, players, animals, and vehicles, with 3D model Chams and separate 3D Legit/Rage aiming toggles.
- Embedded the Viewpoint bridge and model-rendering Java helper in the trainer build; the feature activates when a compatible Viewpoint mod is present in the game.
- Added per-weapon-group switches for accuracy, minimum-damage, and maximum-damage modifiers while preserving legacy configuration behavior.
- Upgraded configuration storage to format 19 with migration support for Viewpoint v18, both v17 variants, and earlier files.
- The published Windows x64 Portable package embeds the VMProtect-processed trainer DLL; end users do not need to install the VMProtect SDK.

Known limitations for this beta:

- Viewpoint 3D features require a compatible Viewpoint mod and matching Project Zomboid build; the portable package does not redistribute that third-party mod.
- The launcher and trainer require a compatible x64 Project Zomboid build.
- Game-in-process injection, 3D rendering, and UI behavior still require manual validation on the target machine.
- Source builds that enable VMProtect markers require a separately installed VMProtect SDK; the published Portable package already contains the processed trainer image.

## v1.0.0-beta.1

Initial public beta release of the combined PzTrainer trainer and launcher repository.

- Combined the trainer DLL and launcher into one buildable repository.
- Added a portable x64 release build that embeds the matching trainer DLL in the launcher.
- Removed generated build trees and machine-specific build defaults from the repository scope.
- Kept VMProtect markers optional and configurable through MSBuild properties.

Known limitations:

- The launcher and trainer require a compatible x64 Project Zomboid build.
- Game-in-process injection and UI behavior still require manual validation on the target machine.
- VMProtect-protected builds require a separately installed VMProtect SDK.
