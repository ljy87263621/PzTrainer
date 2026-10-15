param([string]$GameRoot=$env:PZ_GAME_ROOT,[string]$JdkRoot=$env:PZ_JDK_ROOT,[string]$ViewpointJar=$env:PZ_VIEWPOINT_JAR)
$ErrorActionPreference='Stop'
if (-not $GameRoot -or -not $JdkRoot -or -not $ViewpointJar) { throw 'Pass -GameRoot, -JdkRoot and -ViewpointJar.' }
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot 'obj\model-colour-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $repoRoot
try {
    python scripts/test-native-model-ownership.py
    if ($LASTEXITCODE -ne 0) { throw 'Native ownership regression failed.' }
    foreach ($test in @('model_visual_policy_test','model_outline_gl_test')) {
        & 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /DNOMINMAX /W4 /Itrainer\src /Itrainer\third_party\imgui "trainer\tests\$test.cpp" "/Fo$output\$test.obj" "/Fe$output\$test.exe" /link opengl32.lib user32.lib gdi32.lib
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $test" }
        & (Join-Path $output "$test.exe")
        if ($LASTEXITCODE -ne 0) { throw "Model colour test failed: $test" }
    }
    $routeSources=@('trainer\tests\model_viewpoint_route_test.cpp','trainer\src\features\visual\native_model_chams.cpp',
        'trainer\src\features\visual\visual_settings.cpp','trainer\src\features\visual\player_visual_settings.cpp',
        'trainer\src\features\visual\animal_visual_settings.cpp','trainer\src\features\visual\vehicle_visual_settings.cpp',
        'trainer\third_party\imgui\imgui.cpp','trainer\third_party\imgui\imgui_draw.cpp',
        'trainer\third_party\imgui\imgui_tables.cpp','trainer\third_party\imgui\imgui_widgets.cpp')
    & 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /DNOMINMAX /O1 /Gy /MP /Itrainer\src /Itrainer\third_party\imgui /Itrainer\third_party\openjdk\include /Itrainer\third_party\minhook\include /Iprotection @routeSources "/Fo$output\" "/Fe$output\model_viewpoint_route_test.exe" /link /OPT:REF opengl32.lib user32.lib gdi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Production ViewPoint route test compilation failed.' }
    & (Join-Path $output 'model_viewpoint_route_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Production ViewPoint route test failed.' }
    & '.\scripts\test-viewpoint-chams.ps1' -GameRoot $GameRoot -JdkRoot $JdkRoot -ViewpointJar $ViewpointJar
    & '.\scripts\test-native-capture-construction.ps1' -GameRoot $GameRoot -JdkRoot $JdkRoot
} finally { Pop-Location }
