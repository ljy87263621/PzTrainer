param([string]$GameRoot=$env:PZ_GAME_ROOT,[string]$JdkRoot=$env:PZ_JDK_ROOT)
$ErrorActionPreference='Stop'
if (-not $GameRoot -or -not $JdkRoot) { throw 'Set PZ_GAME_ROOT/PZ_JDK_ROOT, or pass -GameRoot/-JdkRoot.' }
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot 'obj\moveable-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $repoRoot
try {
    & (Join-Path $JdkRoot 'bin\javac.exe') -encoding UTF-8 -cp (Join-Path $GameRoot 'projectzomboid.jar') -d $output 'trainer\tests\MoveableCatalogFixture.java'
    if ($LASTEXITCODE -ne 0) { throw 'Moveable fixture compilation failed.' }
    foreach ($test in @('item_catalog_search_test','item_generator_text_test','moveable_item_catalog_jvm_test')) {
        $sources=@("trainer\tests\$test.cpp")
        if ($test -eq 'moveable_item_catalog_jvm_test') { $sources+='trainer\src\bridge\moveable_item_catalog.cpp' }
        if ($test -eq 'item_generator_text_test') { $sources+=@('trainer\src\ui\item_generator_text.cpp','trainer\src\settings\localization.cpp') }
        & 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /W4 /Itrainer\src /Itrainer\third_party\openjdk\include @sources "/Fo$output\" "/Fe$output\$test.exe"
        if ($LASTEXITCODE -ne 0) { throw "Moveable test compilation failed: $test" }
        if ($test -eq 'moveable_item_catalog_jvm_test') { & (Join-Path $output "$test.exe") $GameRoot $output }
        else { & (Join-Path $output "$test.exe") }
        if ($LASTEXITCODE -ne 0) { throw "Moveable test failed: $test" }
    }
} finally { Pop-Location }
