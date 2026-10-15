param([string]$GameRoot=$env:PZ_GAME_ROOT,[string]$JdkRoot=$env:PZ_JDK_ROOT)
$ErrorActionPreference='Stop'
if (-not $GameRoot -or -not $JdkRoot) { throw 'Pass -GameRoot and -JdkRoot, or set PZ_GAME_ROOT/PZ_JDK_ROOT.' }
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot 'obj\media-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& (Join-Path $JdkRoot 'bin\javac.exe') --release 17 -encoding UTF-8 -cp (Join-Path $GameRoot 'projectzomboid.jar') -d $output (Join-Path $repoRoot 'trainer\src\java\pztrainer\player\items\RecordedMediaItems.java') (Join-Path $repoRoot 'trainer\tests\RecordedMediaItemsTest.java')
if ($LASTEXITCODE -ne 0) { throw 'Recorded media fixture compilation failed.' }
& (Join-Path $GameRoot 'jre64\bin\java.exe') --enable-native-access=ALL-UNNAMED "-Duser.home=$output\profile" -cp "$output;$GameRoot\projectzomboid.jar" RecordedMediaItemsTest
if ($LASTEXITCODE -ne 0) { throw 'Recorded media tests failed.' }

& (Join-Path $JdkRoot 'bin\jar.exe') --create --file "$output\media-helper.jar" -C $output pztrainer/player/items
if ($LASTEXITCODE -ne 0) { throw 'Media test helper packaging failed.' }
Push-Location $repoRoot
try {
    & 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /Itrainer\src /Itrainer\third_party\imgui /Itrainer\third_party\openjdk\include trainer\tests\recorded_media_catalog_jvm_test.cpp trainer\src\bridge\recorded_media_catalog.cpp "/Fo$output\" "/Fe$output\recorded_media_catalog_jvm_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Native recorded media catalog compilation failed.' }
    & "$output\recorded_media_catalog_jvm_test.exe" $GameRoot $output "$output\media-helper.jar"
    if ($LASTEXITCODE -ne 0) { throw 'Native recorded media catalog failed.' }
} finally { Pop-Location }
