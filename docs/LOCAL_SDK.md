# Local SDK

PzTrainer uses two external SDK inputs and one repository-vendored header set. They stay outside Git because the game files and third-party runtime files are not project source.

| Input | Required files | Used by |
| --- | --- | --- |
| Project Zomboid game root | `projectzomboid.jar`, `PZBullet64.dll`, and optionally `jre64\bin\java.exe` | Java bridge compilation, native vehicle checks, JVM tests |
| JDK 25 | `bin\javac.exe`, `bin\jar.exe`, `bin\java.exe` | Java bridge compilation and packaging |
| Repository headers | `trainer\third_party\openjdk\include\jni.h`, `jvmti.h` | C++ JNI/JVMTI compilation |
| Visual Studio | v145 C++ toolset, Windows SDK, MSBuild | DLL and launcher builds |
| Viewpoint mod (optional) | A compatible Viewpoint JAR | Viewpoint chams tests only |

Set the two portable environment variables before running scripts:

```powershell
$env:PZ_GAME_ROOT = 'C:\Games\ProjectZomboid'
$env:PZ_JDK_ROOT = 'C:\Program Files\Java\jdk-25'
.\scripts\check-local-sdk.ps1
```

All Java and Viewpoint scripts accept explicit `-GameRoot` and `-JdkRoot` parameters. The default lookup also understands the local convention used by this workstation (`D:\Apps\Steam\steamapps\common\ProjectZomboid` and `D:\Develope\Dev_Env\JDK25`), while environment variables take precedence.

The standard checks are:

```powershell
.\scripts\build-java-bridge.ps1
.\scripts\test-extensions.ps1
.\scripts\build-portable-release.ps1 -Version '1.0.0-beta.2'
```

The Java bridge build compiles all Java sources against the selected game JAR. If the current game API has drifted, the script reports the exact unresolved methods; do not replace the game JAR with a copied or patched file. The native C++ build can still be validated independently with the repository's MSVC wrapper.

The current local audit confirmed the JDK 25 and game JAR inputs, compiled the two new light/media payload classes independently, and built the native Release DLL and launcher. The full Java bridge remains subject to the existing `GameClient.sendPlayerDamage(IsoPlayer)` API mismatch in `InventoryTools.java` and `CarryOverrides.java` until those callers are adapted to the installed game API.
