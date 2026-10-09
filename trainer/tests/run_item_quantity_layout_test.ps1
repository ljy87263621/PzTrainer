param([string] $MsvcWrapper = 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd')

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$buildPath = Join-Path $projectRoot 'obj\item-quantity-layout-test'
New-Item -ItemType Directory -Path $buildPath -Force | Out-Null
$sources = @(
    'trainer/tests/item_quantity_layout_test.cpp',
    'trainer/third_party/imgui/imgui.cpp',
    'trainer/third_party/imgui/imgui_draw.cpp',
    'trainer/third_party/imgui/imgui_tables.cpp',
    'trainer/third_party/imgui/imgui_widgets.cpp',
    'trainer/src/ui/theme.cpp',
    'trainer/src/ui/animation.cpp',
    'trainer/src/ui/animated_dropdown.cpp',
    'trainer/src/ui/components.cpp',
    'trainer/src/ui/controls/action_controls.cpp',
    'trainer/src/ui/item_generator.cpp',
    'trainer/src/ui/item_generator_text.cpp',
    'trainer/src/ui/corpse_payload_panel.cpp',
    'trainer/src/settings/localization.cpp'
)
$arguments = @('/nologo', '/std:c++17', '/EHsc', '/utf-8', '/MD', '/O1', '/Gy',
    '/DIMGUI_ENABLE_TEST_ENGINE', '/DIMGUI_DISABLE_DEMO_WINDOWS',
    '/Itrainer/src', '/Itrainer/third_party/imgui', '/Itrainer/third_party/openjdk/include',
    "/Fo$buildPath\", "/Fe$buildPath\item_quantity_layout_test.exe") + $sources + @('/link', '/OPT:REF')

Push-Location $projectRoot
try {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        & cl.exe @arguments
    } elseif (Test-Path -LiteralPath $MsvcWrapper) {
        & $MsvcWrapper cl @arguments
    } else {
        throw 'Run in a Visual Studio developer shell, or pass -MsvcWrapper with your compiler wrapper.'
    }
    if ($LASTEXITCODE -ne 0) { throw 'Layout test compilation failed.' }
    & (Join-Path $buildPath 'item_quantity_layout_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Layout regression detected.' }
} finally {
    Pop-Location
}
