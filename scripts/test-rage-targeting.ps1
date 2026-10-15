$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $repoRoot 'obj\rage-policy'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source=Get-Content (Join-Path $repoRoot 'trainer\src\features\aim\rage_aim.cpp') -Raw
$start=$source.IndexOf('bool RageAimNeedsZombieData()')
$end=$source.IndexOf('const RageAimStatus& GetRageAimStatus()',$start)
if ($start -lt 0 -or $end -lt 0) { throw 'Production collection routines were not found.' }
$collection='#include <algorithm>' + "`n" + '#include "features/aim/rage_targeting_policy.hpp"' + "`nnamespace pztrainer::features::aim {`n" + $source.Substring($start,$end-$start) + "`n}"
[IO.File]::WriteAllText((Join-Path $output 'production_collection.cpp'),$collection)
if (-not $source.Contains('if (ShouldAdjustRageAim(*preset, aiming))')) { throw 'Runtime aiming dispatch is not using the tested policy.' }
Push-Location $repoRoot
try {
    & 'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd' cl /nologo /std:c++17 /EHsc /utf-8 /Itrainer\src trainer\tests\rage_targeting_policy_test.cpp "$output\production_collection.cpp" "/Fo$output\" "/Fe$output\rage_targeting_policy_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Rage policy compilation failed.' }
    & "$output\rage_targeting_policy_test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Rage policy matrix failed.' }
} finally { Pop-Location }
