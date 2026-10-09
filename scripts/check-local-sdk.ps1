param(
    [string]$GameRoot = '',
    [string]$JdkRoot = '',
    [switch]$Json
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

function First-ExistingPath([string[]]$Candidates) {
    foreach ($candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return [IO.Path]::GetFullPath($candidate)
        }
    }
    return $null
}

$gameCandidates = @(
    $GameRoot,
    $env:PZ_GAME_ROOT,
    $env:PROJECT_ZOMBOID_ROOT,
    'D:\Apps\Steam\steamapps\common\ProjectZomboid',
    (Join-Path ${env:ProgramFiles(x86)} 'Steam\steamapps\common\ProjectZomboid'),
    (Join-Path $env:ProgramFiles 'Steam\steamapps\common\ProjectZomboid')
)
$jdkCandidates = @(
    $JdkRoot,
    $env:PZ_JDK_ROOT,
    'D:\Develope\Dev_Env\JDK25',
    $env:JAVA_HOME,
    'C:\Program Files\Java\jdk-25'
)

$resolvedGameRoot = First-ExistingPath $gameCandidates
$resolvedJdkRoot = First-ExistingPath $jdkCandidates
$javacPath = if ($resolvedJdkRoot) { Join-Path $resolvedJdkRoot 'bin\\javac.exe' } else { $null }
$jdkVersion = if ($javacPath -and (Test-Path -LiteralPath $javacPath)) { (& $javacPath -version 2>&1 | Out-String).Trim() } else { $null }
$jdkMajor = if ($jdkVersion -match '([0-9]+)') { [int]$Matches[1] } else { 0 }
$msvcWrapper = First-ExistingPath @(
    $env:PZ_MSVC_WRAPPER,
    'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd'
)
$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vswhereAvailable = Test-Path -LiteralPath $vswhere

$checks = [ordered]@{
    Repository = $repoRoot
    GameRoot = $resolvedGameRoot
    GameJar = if ($resolvedGameRoot) { Test-Path (Join-Path $resolvedGameRoot 'projectzomboid.jar') } else { $false }
    GameNative = if ($resolvedGameRoot) { Test-Path (Join-Path $resolvedGameRoot 'PZBullet64.dll') } else { $false }
    GameJre = if ($resolvedGameRoot) { Test-Path (Join-Path $resolvedGameRoot 'jre64\bin\java.exe') } else { $false }
    JdkRoot = $resolvedJdkRoot
    JdkVersion = $jdkVersion
    Jdk25 = $jdkMajor -ge 25
    Javac = if ($resolvedJdkRoot) { Test-Path (Join-Path $resolvedJdkRoot 'bin\javac.exe') } else { $false }
    Jar = if ($resolvedJdkRoot) { Test-Path (Join-Path $resolvedJdkRoot 'bin\jar.exe') } else { $false }
    Java = if ($resolvedJdkRoot) { Test-Path (Join-Path $resolvedJdkRoot 'bin\java.exe') } else { $false }
    JniHeaders = Test-Path (Join-Path $repoRoot 'trainer\third_party\openjdk\include\jni.h')
    JvmtiHeaders = Test-Path (Join-Path $repoRoot 'trainer\third_party\openjdk\include\jvmti.h')
    MsvcWrapper = $null -ne $msvcWrapper
    MsbuildOnPath = $null -ne $msbuild
    Vswhere = $vswhereAvailable
}

$required = @('GameRoot', 'GameJar', 'JdkRoot', 'Jdk25', 'Javac', 'Jar', 'Java', 'JniHeaders', 'JvmtiHeaders')
$missing = @($required | Where-Object { -not $checks[$_] })
$checks.Required = ($missing.Count -eq 0)
$checks.Missing = $missing

if ($Json) {
    $checks | ConvertTo-Json -Depth 4
} else {
    $checks.GetEnumerator() | ForEach-Object {
        $value = if ($null -eq $_.Value) { '<missing>' } else { $_.Value }
        '{0,-18} {1}' -f $_.Key, $value
    }
    if ($checks.Required) {
        Write-Output 'Local SDK check passed.'
    } else {
        Write-Output ('Local SDK incomplete. Missing: ' + ($missing -join ', '))
        Write-Output 'Set PZ_GAME_ROOT and PZ_JDK_ROOT or pass -GameRoot/-JdkRoot.'
    }
}

if (-not $checks.Required) { exit 1 }
