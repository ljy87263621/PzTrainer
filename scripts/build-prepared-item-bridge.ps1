param([string]$GameRoot=$env:PZ_GAME_ROOT,[string]$JdkRoot=$env:PZ_JDK_ROOT)
$ErrorActionPreference='Stop'
if(-not $GameRoot -or -not $JdkRoot) {throw 'Pass GameRoot and JdkRoot.'}
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot 'obj/prepared-item-bridge'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $repoRoot
try {
    $sources=@('trainer/src/java/pztrainer/player/items/RecordedMediaItems.java',
        'trainer/src/java/pztrainer/player/items/PreparedSpawnItems.java',
        'trainer/src/java/pztrainer/items/MediaItemPayload.java','trainer/src/java/pztrainer/items/LightItemPayload.java')
    & "$JdkRoot/bin/javac.exe" --release 17 -encoding UTF-8 -cp "$GameRoot/projectzomboid.jar" -d $output @sources
    if($LASTEXITCODE -ne 0) {throw 'Prepared-item helper compilation failed.'}
    & "$JdkRoot/bin/jar.exe" --update --file trainer/src/runtime/resources/pztrainer-player-overrides.jar -C $output pztrainer/player/items
    if($LASTEXITCODE -ne 0) {throw 'Prepared-item player helper packaging failed.'}
    & "$JdkRoot/bin/jar.exe" --update --file trainer/src/runtime/resources/pztrainer-lua-bridge.jar -C $output pztrainer/items
    if($LASTEXITCODE -ne 0) {throw 'Prepared-item payload helper packaging failed.'}
} finally {Pop-Location}
