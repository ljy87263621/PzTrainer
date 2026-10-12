# ViewPoint bridge test fixture

These classes implement only the fields consumed by the trainer's JNI bridge.
They let `viewpoint_bridge_test.cpp` check detection, camera projection and aiming,
including preservation of `View.enabled` when trainer compatibility mode changes.
They do not render the game and are not a replacement for testing the real Mod.

From the repository root, with a JDK capable of reading the game's class files
(JDK 25 for the current local game build):

```powershell
$gameRoot = 'D:\Apps\Steam\steamapps\common\ProjectZomboid'
$jdkBin = 'D:\Develope\Dev_Env\JDK25\bin'
$sources = git ls-files --cached --others --exclude-standard -- 'trainer/tests/fixtures/viewpoint/*.java'
& "$jdkBin/javac.exe" --release 17 -cp "$gameRoot/projectzomboid.jar" -d obj/viewpoint-fixture-classes $sources
& "$jdkBin/jar.exe" --create --file obj/viewpoint-fixtures.jar -C obj/viewpoint-fixture-classes .
# Compile viewpoint_bridge_test.cpp with viewpoint_bridge.cpp, then run both paths:
obj/viewpoint-tests/viewpoint_bridge_test.exe $gameRoot
obj/viewpoint-tests/viewpoint_bridge_test.exe $gameRoot "$PWD/obj/viewpoint-fixtures.jar"
```
