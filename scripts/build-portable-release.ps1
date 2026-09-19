param(
    [ValidatePattern('^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$')]
    [string]$Version = '1.0.0-beta.1',
    [ValidateSet('Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    [string]$VmProtectRoot = '',
    [switch]$EnableVmProtectMarkers
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$trainerProject = Join-Path $repoRoot 'trainer\pztrainer.vcxproj'
$launcherProject = Join-Path $repoRoot 'pztrainer_launcher.vcxproj'
$trainerDll = Join-Path $repoRoot "bin\x64\$Configuration\pztrainer.dll"
$launcherExe = Join-Path $repoRoot "bin\x64\$Configuration\pztrainer_launcher.exe"

function Resolve-MSBuild {
    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
        if ($path) { return $path }
    }
    $managedInstall = 'D:\Develope\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'
    if (Test-Path $managedInstall) { return $managedInstall }
    throw 'MSBuild.exe was not found. Install Visual Studio C++ build tools or add MSBuild to PATH.'
}

if (-not (Test-Path $trainerProject)) { throw "Trainer project not found: $trainerProject" }
if (-not (Test-Path $launcherProject)) { throw "Launcher project not found: $launcherProject" }
$msbuild = Resolve-MSBuild
$markerValue = if ($EnableVmProtectMarkers) { 'true' } else { 'false' }
$properties = @(
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:EnableVmProtectMarkers=$markerValue"
)
if ($VmProtectRoot) { $properties += "/p:VmProtectRoot=$VmProtectRoot" }

Push-Location $repoRoot
try {
    & $msbuild $trainerProject /t:Rebuild /m @properties
    if ($LASTEXITCODE -ne 0) { throw "Trainer build failed with exit code $LASTEXITCODE" }
    if (-not (Test-Path $trainerDll)) { throw "Trainer build completed without $trainerDll" }

    & $msbuild $launcherProject /t:Rebuild /m @properties
    if ($LASTEXITCODE -ne 0) { throw "Launcher build failed with exit code $LASTEXITCODE" }
    if (-not (Test-Path $launcherExe)) { throw "Launcher build completed without $launcherExe" }

    $artifactRoot = Join-Path $repoRoot "artifacts\pztrainer-v$Version-windows-x64-portable"
    if (Test-Path $artifactRoot) { Remove-Item $artifactRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $artifactRoot | Out-Null
    Copy-Item $launcherExe (Join-Path $artifactRoot 'pztrainer_launcher.exe')
    Copy-Item (Join-Path $repoRoot 'README.md') (Join-Path $artifactRoot 'README.md')
    Copy-Item (Join-Path $repoRoot 'CHANGELOG.md') (Join-Path $artifactRoot 'CHANGELOG.md')

    $hashLines = Get-ChildItem $artifactRoot -File | Sort-Object Name | ForEach-Object {
        $hash = Get-FileHash $_.FullName -Algorithm SHA256
        "$($hash.Hash.ToLowerInvariant())  $($_.Name)"
    }
    $hashLines | Set-Content (Join-Path $artifactRoot 'SHA256SUMS.txt') -Encoding ascii
    $zip = "$artifactRoot.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path (Join-Path $artifactRoot '*') -DestinationPath $zip
    Write-Host "Portable artifact: $zip"
    Write-Host ($hashLines -join [Environment]::NewLine)
}
finally {
    Pop-Location
}
