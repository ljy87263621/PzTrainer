param(
    [string]$GameRoot = $(if ($env:PZ_GAME_ROOT) { $env:PZ_GAME_ROOT } else { 'D:\Apps\Steam\steamapps\common\ProjectZomboid' }),
    [string]$JdkRoot = $(if ($env:PZ_JDK_ROOT) { $env:PZ_JDK_ROOT } else { 'D:\Develope\Dev_Env\JDK25' })
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot 'obj\viewpoint-chams-java'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
& (Join-Path $JdkRoot 'bin\javac.exe') --release 17 -encoding UTF-8 -cp (Join-Path $GameRoot 'projectzomboid.jar') -d $buildRoot (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\visual\ViewpointModelChams.java')
if ($LASTEXITCODE -ne 0) { throw 'Viewpoint model chams compilation failed.' }
& (Join-Path $JdkRoot 'bin\jar.exe') --update --file (Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar') -C $buildRoot pztrainer/player/visual
if ($LASTEXITCODE -ne 0) { throw 'Viewpoint model chams packaging failed.' }
