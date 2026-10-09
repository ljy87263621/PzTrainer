param([string]$JdkRoot = $(if ($env:PZ_JDK_ROOT) { $env:PZ_JDK_ROOT } else { 'D:\Develope\Dev_Env\JDK25' }))
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'trainer\src\java\pztrainer\player\aim'
$buildRoot = Join-Path $repoRoot 'obj\rage-fire-java'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$sources = Get-ChildItem -LiteralPath $sourceRoot -Filter '*.java' | Select-Object -ExpandProperty FullName
& (Join-Path $JdkRoot 'bin\javac.exe') --release 17 -encoding UTF-8 -d $buildRoot @sources
if ($LASTEXITCODE -ne 0) { throw 'Rage firing bridge compilation failed.' }
$jar = Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar'
& (Join-Path $JdkRoot 'bin\jar.exe') --update --file $jar -C $buildRoot pztrainer/player/aim
if ($LASTEXITCODE -ne 0) { throw 'Rage firing bridge packaging failed.' }
