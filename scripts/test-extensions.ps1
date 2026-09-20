param(
    [string]$GameRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$JdkRoot = $env:JAVA_HOME
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'build-java-bridge.ps1') -GameRoot $GameRoot -JdkRoot $JdkRoot
$tests = Join-Path $repoRoot 'obj\extension-tests'
New-Item -ItemType Directory -Force -Path $tests | Out-Null
$classpath = (Join-Path $GameRoot 'projectzomboid.jar') + ';' + (Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-lua-bridge.jar')
$classpath += ';' + (Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar')
& (Join-Path $JdkRoot 'bin\javac.exe') -encoding UTF-8 -cp $classpath -d $tests (Join-Path $repoRoot 'trainer\tests\CharacterCreationTest.java') (Join-Path $repoRoot 'trainer\tests\ExtensionAccessTest.java') (Join-Path $repoRoot 'trainer\tests\VehiclePhysicsStateTest.java') (Join-Path $repoRoot 'trainer\tests\PlayerOverridesTest.java')
if ($LASTEXITCODE -ne 0) { throw 'Character creation test compilation failed.' }
& (Join-Path $JdkRoot 'bin\java.exe') -cp ($tests + ';' + $classpath) pztrainer.extensions.VehiclePhysicsStateTest
if ($LASTEXITCODE -ne 0) { throw 'Vehicle physics state tests failed.' }
& (Join-Path $JdkRoot 'bin\java.exe') -cp ($tests + ';' + $classpath) PlayerOverridesTest (Join-Path $GameRoot 'projectzomboid.jar')
if ($LASTEXITCODE -ne 0) { throw 'Player override tests failed.' }
& (Join-Path $PSScriptRoot 'test-player-overrides-jvm.cmd') $JdkRoot $GameRoot
if ($LASTEXITCODE -ne 0) { throw 'Player override JVM integration failed.' }
& (Join-Path $JdkRoot 'bin\java.exe') -cp ($tests + ';' + $classpath) pztrainer.extensions.ExtensionAccessTest
if ($LASTEXITCODE -ne 0) { throw 'Extension session access tests failed.' }
Push-Location $GameRoot
try {
    & (Join-Path $JdkRoot 'bin\java.exe') -cp ($tests + ';' + $classpath) CharacterCreationTest (Join-Path $repoRoot 'trainer\src\java\pztrainer\extensions\character_creation.lua') (Join-Path $repoRoot 'trainer\tests\character_creation_test.lua')
    if ($LASTEXITCODE -ne 0) { throw 'Character creation tests failed.' }
} finally { Pop-Location }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild was not found.' }
& $msbuild (Join-Path $repoRoot 'trainer\tests\extension_tests.vcxproj') /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Native test compilation failed.' }
& (Join-Path $tests 'vehicle_ghost_test.exe') (Join-Path $GameRoot 'PZBullet64.dll')
if ($LASTEXITCODE -ne 0) { throw 'Vehicle physics tests failed.' }
& (Join-Path $PSScriptRoot 'test-vehicle-ghost-mapper.cmd')
if ($LASTEXITCODE -ne 0) { throw 'Manual-map vehicle callback tests failed.' }
