param(
    [ValidatePattern('^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$')]
    [string]$Version = '1.0.1-beta.1',
    [string]$DisplayName = '',
    [string]$VmProtectRoot = $env:VMP_ROOT
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $VmProtectRoot) { throw 'Pass -VmProtectRoot pointing to the VMProtect SDK root.' }
foreach ($relative in @('Include\C\VMProtectSDK.h', 'Lib\Windows\VMProtectSDK64.lib')) {
    if (-not (Test-Path -LiteralPath (Join-Path $VmProtectRoot $relative))) { throw "Missing SDK file: $relative" }
}
$command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
$msbuild = if ($command) { $command.Source } else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
    }
}
if (-not $msbuild) { throw 'MSBuild.exe not found. Install Visual Studio C++ build tools.' }
$handoff = Join-Path $repoRoot "artifacts\pztrainer-v$Version-vmp"
$inputDir = Join-Path $handoff 'input'
$outputDir = Join-Path $handoff 'protected'
$dll = Join-Path $inputDir 'pztrainer.dll'
if ((Test-Path -LiteralPath $dll) -or (Test-Path -LiteralPath (Join-Path $outputDir 'pztrainer.dll'))) {
    throw "Existing VMP input/output preserved: $handoff. Archive it before rebuilding."
}
New-Item -ItemType Directory -Force -Path $inputDir, $outputDir | Out-Null
$intermediate = Join-Path $repoRoot "obj\vmp-v$Version"
Push-Location $repoRoot
try {
    & $msbuild trainer/pztrainer.vcxproj /t:Rebuild /m /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:EnableVmProtectMarkers=true /p:BuildProjectReferences=false "/p:VmProtectRoot=$VmProtectRoot" "/p:OutDir=$inputDir\" "/p:IntDir=$intermediate\"
    if ($LASTEXITCODE -ne 0) { throw "Marker trainer build failed: $LASTEXITCODE" }
    if (-not (Test-Path -LiteralPath $dll)) { throw 'Build did not produce the input DLL.' }
    $manifest = [ordered]@{
        version = $Version
        displayName = if ($DisplayName) { $DisplayName } elseif ($Version -match '-') { "v$Version" } else { "v$Version Stable" }
        phase = 'marker-input-awaiting-user-vmprotect'
        sourceCommit = (& git rev-parse HEAD)
        sourceDirty = [bool](& git status --porcelain --untracked-files=normal)
        dll = 'input/pztrainer.dll'
        dllSha256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
        playerHelperSha256 = (Get-FileHash -LiteralPath 'trainer/src/runtime/resources/pztrainer-player-overrides.jar' -Algorithm SHA256).Hash.ToLowerInvariant()
        luaBridgeSha256 = (Get-FileHash -LiteralPath 'trainer/src/runtime/resources/pztrainer-lua-bridge.jar' -Algorithm SHA256).Hash.ToLowerInvariant()
        expectedProcessedDll = 'protected/pztrainer.dll'
        launcherBuilt = $false
        releasePublished = $false
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $handoff 'input-manifest.json') -Encoding UTF8
    Copy-Item -LiteralPath 'protection/markers.json', 'docs/VMP_HANDOFF.md' -Destination $handoff
    Write-Host "VMP input DLL: $dll"
    Write-Host "SHA256: $($manifest.dllSha256)"
    Write-Host "Save the user-processed DLL to: $(Join-Path $outputDir 'pztrainer.dll')"
} finally { Pop-Location }
