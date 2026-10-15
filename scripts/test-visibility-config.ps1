param([string[]]$ConfigurationFixtures=@())
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot 'obj\visibility-config-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$compiler='D:\Develope\Tools\CodexToolchain\codex-msvc.cmd'
$common=@('/nologo','/std:c++17','/EHsc','/utf-8','/DNOMINMAX','/O1','/Gy','/Itrainer/src','/Itrainer/third_party/imgui')
Push-Location $repoRoot
try {
    & $compiler cl @common trainer/tests/visibility_options_test.cpp "/Fo$output\visibility_options_test.obj" "/Fe$output\visibility_options_test.exe"
    if($LASTEXITCODE -ne 0){throw 'Visibility policy compilation failed.'}
    & "$output\visibility_options_test.exe"
    if($LASTEXITCODE -ne 0){throw 'Visibility policy regression failed.'}
    & $compiler cl @common trainer/tests/default_configuration_test.cpp trainer/src/settings/ui_preferences.cpp "/Fo$output\" "/Fe$output\default_configuration_test.exe" /link shell32.lib ole32.lib
    if($LASTEXITCODE -ne 0){throw 'Default preference compilation failed.'}
    & "$output\default_configuration_test.exe"
    if($LASTEXITCODE -ne 0){throw 'Default preference regression failed.'}
    & "$output\default_configuration_test.exe" reload
    if($LASTEXITCODE -ne 0){throw 'Default preference reload regression failed.'}
    $sources=@('trainer/tests/configuration_upgrade_test.cpp','trainer/src/features/aim/aim_settings.cpp',
        'trainer/src/features/visual/visual_settings.cpp','trainer/src/features/visual/player_visual_settings.cpp',
        'trainer/src/features/visual/animal_visual_settings.cpp','trainer/src/features/visual/vehicle_visual_settings.cpp',
        'trainer/src/settings/ui_preferences.cpp','trainer/src/settings/localization.cpp')
    & $compiler cl @common @sources "/Fo$output\" "/Fe$output\configuration_upgrade_test.exe" /link /OPT:REF shell32.lib ole32.lib
    if($LASTEXITCODE -ne 0){throw 'Configuration codec compilation failed.'}
    & "$output\configuration_upgrade_test.exe"
    if($LASTEXITCODE -ne 0){throw 'Configuration codec regression failed.'}
    foreach($fixture in $ConfigurationFixtures) {
        & "$output\configuration_upgrade_test.exe" $fixture
        if($LASTEXITCODE -ne 0){throw "Saved configuration copy regression failed: $fixture"}
    }
} finally {Pop-Location}
