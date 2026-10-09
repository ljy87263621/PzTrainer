param(
    [Parameter(Mandatory = $true)][string]$ViewpointJar,
    [string]$GameRoot = $(if ($env:PZ_GAME_ROOT) { $env:PZ_GAME_ROOT } else { 'D:\Apps\Steam\steamapps\common\ProjectZomboid' }),
    [string]$JdkRoot = $(if ($env:PZ_JDK_ROOT) { $env:PZ_JDK_ROOT } else { 'D:\Develope\Dev_Env\JDK25' })
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$gameRoot = $GameRoot
$buildRoot = Join-Path $repoRoot 'obj\viewpoint-chams-java'
& (Join-Path $PSScriptRoot 'build-viewpoint-chams.ps1') -GameRoot $GameRoot -JdkRoot $JdkRoot
$classPath = "$buildRoot;$(Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar');$(Join-Path $gameRoot 'projectzomboid.jar');$ViewpointJar"
$sources = @('ViewpointModelChamsTest.java', 'ViewpointChamsGlStateTest.java', 'ViewpointChamsRenderTest.java') | ForEach-Object { Join-Path $repoRoot "trainer\tests\$_" }
& (Join-Path $JdkRoot 'bin\javac.exe') --release 17 -encoding UTF-8 -cp $classPath -d $buildRoot @sources
if ($LASTEXITCODE -ne 0) { throw 'Viewpoint chams test compilation failed.' }
$vmOptions = @('-Xverify:all', "-Duser.home=$buildRoot\profile", "-Dorg.lwjgl.system.SharedLibraryExtractPath=$buildRoot\natives", '--enable-native-access=ALL-UNNAMED', '-cp', $classPath)
foreach ($test in @('ViewpointModelChamsTest', 'ViewpointChamsGlStateTest')) {
    & (Join-Path $gameRoot 'jre64\bin\java.exe') @vmOptions $test
    if ($LASTEXITCODE -ne 0) { throw "Viewpoint chams test failed: $test" }
}
