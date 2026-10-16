param(
    [string]$GameRoot = $env:PZ_GAME_ROOT,
    [string]$JdkRoot = $env:PZ_JDK_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $GameRoot) { throw 'Set PZ_GAME_ROOT or pass -GameRoot.' }
if (-not $JdkRoot) { throw 'Set PZ_JDK_ROOT or pass -JdkRoot (JDK 25 or newer).' }
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot 'obj\viewpoint-chams-java'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
& (Join-Path $JdkRoot 'bin\javac.exe') --release 17 -encoding UTF-8 -cp (Join-Path $GameRoot 'projectzomboid.jar') -d $buildRoot (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\visual\ViewpointModelChams.java') (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\visual\ModelMaterial.java') (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\visual\ModelScreenPass.java') (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\visual\NativeModelCapture.java') (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\visual\ViewpointChamsTransforms.java') (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\visual\NativeCaptureTransforms.java')
if ($LASTEXITCODE -ne 0) { throw 'Viewpoint model chams compilation failed.' }
$thermalHeader = Get-Content -LiteralPath (Join-Path $repoRoot 'trainer\src\features\visual\thermal_material.hpp') -Raw
$thermalBody = [regex]::Match($thermalHeader, '(?s)R"PZTHERMAL\((.*?)\)PZTHERMAL"').Groups[1].Value
if (-not $thermalBody) { throw 'Shared thermal GLSL body was not found.' }
[System.IO.File]::WriteAllText((Join-Path $buildRoot 'pztrainer\player\visual\thermal_material.glsl'), $thermalBody, (New-Object System.Text.UTF8Encoding($false)))
$compositeHeader = Get-Content -LiteralPath (Join-Path $repoRoot 'trainer\src\features\visual\model_composite_shader.hpp') -Raw
foreach ($stage in @(@('VERT','vert'),@('FRAG','frag'))) {
    $body = [regex]::Match($compositeHeader, ('(?s)R"PZCOMP_' + $stage[0] + '\((.*?)\)PZCOMP_' + $stage[0] + '"')).Groups[1].Value
    if (-not $body) { throw "Shared model composite shader missing: $($stage[0])" }
    [System.IO.File]::WriteAllText((Join-Path $buildRoot "pztrainer\player\visual\model_composite.$($stage[1])"), $body, (New-Object System.Text.UTF8Encoding($false)))
}
& (Join-Path $JdkRoot 'bin\javac.exe') --release 17 -encoding UTF-8 -cp (Join-Path $GameRoot 'projectzomboid.jar') -d $buildRoot (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\items\RecordedMediaItems.java')
if ($LASTEXITCODE -ne 0) { throw 'Recorded media helper compilation failed.' }
& (Join-Path $JdkRoot 'bin\jar.exe') --update --file (Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar') -C $buildRoot pztrainer/player/visual -C $buildRoot pztrainer/player/items
if ($LASTEXITCODE -ne 0) { throw 'Viewpoint model chams packaging failed.' }
