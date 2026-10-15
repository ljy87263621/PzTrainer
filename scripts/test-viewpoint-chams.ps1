param(
    [Parameter(Mandatory = $true)][string]$ViewpointJar,
    [string]$GameRoot = $env:PZ_GAME_ROOT,
    [string]$JdkRoot = $env:PZ_JDK_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $GameRoot) { throw 'Set PZ_GAME_ROOT or pass -GameRoot.' }
if (-not $JdkRoot) { throw 'Set PZ_JDK_ROOT or pass -JdkRoot (JDK 25 or newer).' }
$repoRoot = Split-Path -Parent $PSScriptRoot
$gameRoot = $GameRoot
$buildRoot = Join-Path $repoRoot 'obj\viewpoint-chams-java'
& (Join-Path $PSScriptRoot 'build-viewpoint-chams.ps1') -GameRoot $GameRoot -JdkRoot $JdkRoot
$classPath = "$buildRoot;$(Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar');$(Join-Path $gameRoot 'projectzomboid.jar');$ViewpointJar"
$sources = @('NativeModelCaptureTest.java', 'ViewpointFrameHookTest.java', 'ViewpointModelChamsTest.java', 'ViewpointChamsGlStateTest.java', 'ViewpointChamsRenderTest.java') | ForEach-Object { Join-Path $repoRoot "trainer\tests\$_" }
& (Join-Path $JdkRoot 'bin\javac.exe') --release 17 -encoding UTF-8 -cp $classPath -d $buildRoot @sources
if ($LASTEXITCODE -ne 0) { throw 'Viewpoint chams test compilation failed.' }
$vmOptions = @('-Xverify:all', "-Duser.home=$buildRoot\profile", "-Dorg.lwjgl.system.SharedLibraryExtractPath=$buildRoot\natives", '--enable-native-access=ALL-UNNAMED', '-cp', $classPath)
foreach ($test in @('NativeModelCaptureTest', 'ViewpointFrameHookTest', 'ViewpointModelChamsTest', 'ViewpointChamsGlStateTest')) {
    & (Join-Path $gameRoot 'jre64\bin\java.exe') @vmOptions $test
    if ($LASTEXITCODE -ne 0) { throw "Viewpoint chams test failed: $test" }
}

$hookOutput=Join-Path $repoRoot 'obj\viewpoint-frame-hook'
New-Item -ItemType Directory -Force -Path $hookOutput | Out-Null
Push-Location $repoRoot
try {
    & 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /MD /O1 /Gy /LD /Itrainer\src /Itrainer\third_party\imgui /Itrainer\third_party\openjdk\include trainer\tests\viewpoint_frame_hook_jvm_test.cpp "/Fo$hookOutput\" "/Fe$hookOutput\viewpoint_frame_hook_test.dll" /link /OPT:REF
    if ($LASTEXITCODE -ne 0) {throw 'Frame hook JNI fixture compilation failed.'}
    & (Join-Path $gameRoot 'jre64\bin\java.exe') @vmOptions ViewpointFrameHookTest "$hookOutput\viewpoint_frame_hook_test.dll" (Join-Path $repoRoot 'trainer\src\runtime\resources\pztrainer-player-overrides.jar')
    if ($LASTEXITCODE -ne 0) {throw 'Production frame hook JNI/JVMTI test failed.'}
} finally {Pop-Location}
