param([string]$GameRoot=$env:PZ_GAME_ROOT,[string]$JdkRoot=$env:PZ_JDK_ROOT)
$ErrorActionPreference='Stop'
if (-not $GameRoot -or -not $JdkRoot) {throw 'Pass GameRoot and JdkRoot.'}
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot 'obj\native-capture-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $repoRoot
try {
    python scripts/test-esp-read-only.py
    if ($LASTEXITCODE -ne 0) {throw 'Read-only collection regression failed.'}
    $jar=Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar'
    $cp="$output;$jar;$GameRoot\projectzomboid.jar"
    & "$JdkRoot\bin\javac.exe" --release 17 -encoding UTF-8 -cp $cp -d $output trainer/tests/NativeCaptureFrameTest.java trainer/tests/NativeCaptureGlTest.java
    if ($LASTEXITCODE -ne 0) {throw 'Native capture Java test compilation failed.'}
    & "$GameRoot\jre64\bin\java.exe" -Xverify:all -cp $cp NativeCaptureFrameTest
    if ($LASTEXITCODE -ne 0) {throw 'Native world-frame transform test failed.'}
    $sources=@('trainer/tests/native_capture_gl_jvm_test.cpp',
        'trainer/src/features/visual/visual_settings.cpp','trainer/src/features/visual/player_visual_settings.cpp',
        'trainer/src/features/visual/animal_visual_settings.cpp','trainer/src/features/visual/vehicle_visual_settings.cpp',
        'trainer/third_party/imgui/imgui.cpp','trainer/third_party/imgui/imgui_draw.cpp','trainer/third_party/imgui/imgui_tables.cpp','trainer/third_party/imgui/imgui_widgets.cpp',
        'trainer/third_party/minhook/src/buffer.c','trainer/third_party/minhook/src/hook.c','trainer/third_party/minhook/src/trampoline.c','trainer/third_party/minhook/src/hde/hde64.c')
    & 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /MD /O1 /Gy /MP /LD /DNOMINMAX /Itrainer/src /Itrainer/third_party/imgui /Itrainer/third_party/openjdk/include /Itrainer/third_party/minhook/include /Iprotection @sources "/Fo$output\" "/Fe$output\native_capture_test.dll" /link /OPT:REF opengl32.lib user32.lib gdi32.lib
    if ($LASTEXITCODE -ne 0) {throw 'Native capture DLL fixture build failed.'}
    & "$GameRoot\jre64\bin\java.exe" -Xverify:all --enable-native-access=ALL-UNNAMED "-Duser.home=$output\profile" "-Dorg.lwjgl.system.SharedLibraryExtractPath=$output\natives" -cp $cp NativeCaptureGlTest "$output\native_capture_test.dll" $jar $GameRoot
    if ($LASTEXITCODE -ne 0) {throw 'Production native capture GPU regression failed.'}
} finally {Pop-Location}
