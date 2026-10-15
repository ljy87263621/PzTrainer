param([string]$GameRoot = $env:PZ_GAME_ROOT)
$ErrorActionPreference = 'Stop'
if (-not $GameRoot) { throw 'Set PZ_GAME_ROOT or pass -GameRoot.' }
$repoRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $repoRoot 'obj\thermal-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $repoRoot
try {
    foreach ($test in @('thermal_material_test','thermal_native_gl_test')) {
        & 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /DNOMINMAX /Itrainer\src /Itrainer\third_party\imgui `
            "trainer\tests\$test.cpp" "/Fo$output\$test.obj" "/Fe$output\$test.exe" /link opengl32.lib user32.lib gdi32.lib
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $test" }
        if ($test -eq 'thermal_native_gl_test') { & (Join-Path $output "$test.exe") $GameRoot }
        else { & (Join-Path $output "$test.exe") }
        if ($LASTEXITCODE -ne 0) { throw "Thermal material test failed: $test" }
    }
} finally { Pop-Location }
