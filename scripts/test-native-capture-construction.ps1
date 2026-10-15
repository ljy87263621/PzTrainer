param([string]$GameRoot=$env:PZ_GAME_ROOT,[string]$JdkRoot=$env:PZ_JDK_ROOT)
$ErrorActionPreference='Stop'
$jar=Join-Path (Split-Path -Parent $PSScriptRoot) 'trainer\src\runtime\resources\pztrainer-player-overrides.jar'
$bytecode=(& (Join-Path $JdkRoot 'bin\javap.exe') -p -c -classpath $jar pztrainer.player.visual.NativeModelCapture) -join "`n"
$first=$bytecode.IndexOf('ModelSlotRenderData.initModel:')
$second=$bytecode.IndexOf('ModelSlotRenderData.init:')
$lights=$bytecode.IndexOf('ModelInstance.updateLights:')
if ($lights -lt 0 -or $lights -gt $first) { throw 'Explicit native capture must initialize engine playerData before frame construction.' }
if ($first -lt 0 -or $second -lt $first) { throw 'Missing engine two-stage ModelSlotRenderData construction; character remains null.' }
if (-not $bytecode.Contains('ModelManager$ModelSlot.renderRefCount')) { throw 'Missing model slot reference ownership.' }
Write-Output 'Native capture bytecode: updateLights before initModel/init and slot reference ownership verified.'
