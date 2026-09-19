# v1.0.0-beta.1

Initial public beta release of the combined PzTrainer trainer and launcher repository.

- Combined the trainer DLL and launcher into one buildable repository.
- Added a portable x64 release build that embeds the matching trainer DLL in the launcher.
- Removed generated build trees and machine-specific build defaults from the repository scope.
- Kept VMProtect markers optional and configurable through MSBuild properties.

Known limitations:

- The launcher and trainer require a compatible x64 Project Zomboid build.
- Game-in-process injection and UI behavior still require manual validation on the target machine.
- VMProtect-protected builds require a separately installed VMProtect SDK.
