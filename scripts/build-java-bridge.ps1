param(
    [string]$GameRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$JdkRoot = $env:JAVA_HOME
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $JdkRoot) { throw 'Set JAVA_HOME or pass -JdkRoot (JDK 25 or newer).' }
$javac = Join-Path $JdkRoot 'bin\javac.exe'
$jar = Join-Path $JdkRoot 'bin\jar.exe'
$gameJar = Join-Path $GameRoot 'projectzomboid.jar'
foreach ($required in @($javac, $jar, $gameJar)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing build dependency: $required" }
}
$sourceRoot = Join-Path $repoRoot 'trainer\src\java'
$buildRoot = Join-Path $repoRoot ('obj\java-bridge\' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$sources = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -Filter '*.java' | ForEach-Object FullName)
& $javac -encoding UTF-8 --release 25 -classpath $gameJar -d $buildRoot @sources
if ($LASTEXITCODE -ne 0) { throw 'Java bridge compilation failed.' }
Copy-Item -LiteralPath (Join-Path $sourceRoot 'pztrainer\extensions\character_creation.lua') -Destination (Join-Path $buildRoot 'pztrainer\extensions\character_creation.lua')
$output = Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-lua-bridge.jar'
& $jar --create --file $output -C $buildRoot pztrainer/extensions -C $buildRoot pztrainer/lua
if ($LASTEXITCODE -ne 0) { throw 'Java bridge packaging failed.' }
Write-Output "Embedded bridge rebuilt: $output"
$playerOutput = Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar'
& $jar --create --file $playerOutput -C $buildRoot pztrainer/player
if ($LASTEXITCODE -ne 0) { throw 'Player overrides packaging failed.' }
Write-Output "Embedded player overrides rebuilt: $playerOutput"
