param([string]$GameRoot=$env:PZ_GAME_ROOT,[string]$JdkRoot=$env:PZ_JDK_ROOT)
$ErrorActionPreference='Stop'
if(-not $GameRoot -or -not $JdkRoot) {throw 'Pass GameRoot and JdkRoot.'}
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot 'obj/prepared-route-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $repoRoot
try {
    $sources=@('trainer/src/java/pztrainer/player/items/RecordedMediaItems.java',
        'trainer/src/java/pztrainer/player/items/PreparedSpawnItems.java',
        'trainer/src/java/pztrainer/items/MediaItemPayload.java','trainer/src/java/pztrainer/items/LightItemPayload.java',
        'trainer/tests/PreparedItemRoutesTest.java','trainer/tests/RecordedMediaItemsTest.java','trainer/tests/MoveableCatalogFixture.java')
    & "$JdkRoot/bin/javac.exe" --release 17 -encoding UTF-8 -cp "$GameRoot/projectzomboid.jar" -d $output @sources
    if($LASTEXITCODE -ne 0) {throw 'Prepared-item Java compilation failed.'}
    & "$GameRoot/jre64/bin/java.exe" -Xverify:all --enable-native-access=ALL-UNNAMED "-Duser.home=$output/profile" -cp "$output;$GameRoot/projectzomboid.jar" PreparedItemRoutesTest
    if($LASTEXITCODE -ne 0) {throw 'Prepared-item engine regression failed.'}
    & "$JdkRoot/bin/jar.exe" --create --file "$output/prepared-helper.jar" -C $output pztrainer/player/items
    if($LASTEXITCODE -ne 0) {throw 'Prepared test helper packaging failed.'}
    & "$JdkRoot/bin/jar.exe" --create --file "$output/media-route-helper.jar" -C $output pztrainer/items
    if($LASTEXITCODE -ne 0) {throw 'Media test helper packaging failed.'}
    & "$JdkRoot/bin/jar.exe" --create --file "$output/fixtures-only.jar" -C $output PreparedItemRoutesTest.class -C $output RecordedMediaItemsTest.class -C $output MoveableCatalogFixture.class
    if($LASTEXITCODE -ne 0) {throw 'Isolated loader fixture packaging failed.'}
    & "$GameRoot/jre64/bin/java.exe" -Xverify:all --enable-native-access=ALL-UNNAMED "-Duser.home=$output/profile" -cp "$output/fixtures-only.jar;$GameRoot/projectzomboid.jar" PreparedItemRoutesTest "$output/prepared-helper.jar" "$output/media-route-helper.jar"
    if($LASTEXITCODE -ne 0) {throw 'Separate helper classloader regression failed.'}
    & 'D:/Develope/Tools/CodexToolchain/codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /W4 /Itrainer/src /Itrainer/third_party/imgui /Itrainer/third_party/openjdk/include /Iprotection trainer/tests/prepared_corpse_route_jvm_test.cpp trainer/src/bridge/recorded_media_catalog.cpp "/Fo$output\" "/Fe$output/prepared_corpse_route_jvm_test.exe"
    if($LASTEXITCODE -ne 0) {throw 'Prepared corpse dispatch compilation failed.'}
    & "$output/prepared_corpse_route_jvm_test.exe" $GameRoot $output "$output/prepared-helper.jar"
    if($LASTEXITCODE -ne 0) {throw 'Production prepared corpse dispatch regression failed.'}
} finally {Pop-Location}
