param(
    [string]$GameRoot = $env:PZ_GAME_ROOT,
    [string]$JdkRoot = $env:PZ_JDK_ROOT,
    [string]$ScriptPath = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $GameRoot -or -not $JdkRoot) { throw 'Set PZ_GAME_ROOT and PZ_JDK_ROOT, or pass -GameRoot and -JdkRoot.' }
if (-not $ScriptPath) { $ScriptPath = Join-Path $repoRoot 'trainer\lua\examples\item_esp.lua' }
$ScriptPath = (Resolve-Path -LiteralPath $ScriptPath).Path
$gameJar = Join-Path $GameRoot 'projectzomboid.jar'
$output = Join-Path $repoRoot 'obj\item-esp-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& (Join-Path $JdkRoot 'bin\javac.exe') -encoding UTF-8 -cp $gameJar -d $output `
    (Join-Path $repoRoot 'trainer\tests\ItemEspTest.java') `
    (Join-Path $repoRoot 'trainer\src\java\pztrainer\lua\Utf8ByteReader.java')
if ($LASTEXITCODE -ne 0) { throw 'Item ESP test compilation failed.' }
# The game's J2SEPlatform resolves stdlib.lua relative to its working directory.
Push-Location $GameRoot
try {
    & (Join-Path $JdkRoot 'bin\java.exe') -cp ($output + ';' + $gameJar) ItemEspTest `
        $ScriptPath (Join-Path $repoRoot 'trainer\tests\item_esp_test.lua')
    if ($LASTEXITCODE -ne 0) { throw 'Item ESP regression tests failed.' }
} finally { Pop-Location }
