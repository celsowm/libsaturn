[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Example,

    [ValidateSet('mednafen', 'kronos', 'yabasanshiro')]
    [string]$Emulator = 'mednafen',

    [ValidateSet('auto', 'na', 'jp', 'eu')]
    [string]$BiosProfile = 'auto',

    [ValidateSet('current', 'safe')]
    [string]$IpProfile = 'current',

    [ValidateSet('yaul', 'sbl', 'minimal', 'yaul_fixed', 'region_free', 'minimal_boot', 'correct', 'final')]
    [string]$IpTemplate = 'yaul',

    [switch]$BuildFirst,
    [string]$Msys2Root,
    [string[]]$ExtraArgs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = $PSScriptRoot
$normalizedExample = $Example.Trim().Trim('\', '/')
if ($normalizedExample -match '^(examples[\\/])(.+)$') {
    $normalizedExample = $matches[2]
}

$safeName = ($normalizedExample -replace '[\\/]', '_')
$cuePath = Join-Path $RepoRoot ("build\examples\{0}.cue" -f $safeName)
$isoPath = Join-Path $RepoRoot ("build\examples\{0}.iso" -f $safeName)

if ($BuildFirst -or -not (Test-Path $isoPath)) {
    $buildScript = Join-Path $RepoRoot 'build-example.ps1'
    if (-not (Test-Path $buildScript)) {
        throw "Build script not found: $buildScript"
    }

    $buildArgs = @{
        Example = $normalizedExample
        IpProfile = $IpProfile
        IpTemplate = $IpTemplate
    }
    if ($BuildFirst) {
        $buildArgs.ForceRebuild = $true
    }
    if ($Msys2Root) {
        $buildArgs.Msys2Root = $Msys2Root
    }

    & $buildScript @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build example: $normalizedExample"
    }
}

if (-not (Test-Path $isoPath)) {
    throw "Example ISO not found: $isoPath"
}

$launcher = switch ($Emulator) {
    'mednafen' { Join-Path $RepoRoot 'emulators\mednafen\run-mednafen.ps1' }
    'kronos' { Join-Path $RepoRoot 'emulators\kronos\run-kronos.ps1' }
    'yabasanshiro' { Join-Path $RepoRoot 'emulators\YabaSanshiro\run-yabasanshiro.ps1' }
}

if (-not (Test-Path $launcher)) {
    throw "Emulator launcher not found: $launcher. Run .\scripts\download-emulators.ps1."
}

$gamePath = if (($Emulator -eq 'mednafen' -or $Emulator -eq 'yabasanshiro') -and (Test-Path $cuePath)) { $cuePath } else { $isoPath }
Write-Host "[run-example] Running $normalizedExample on $Emulator"
if ($Emulator -eq 'mednafen') {
    Write-Host "[run-example] BIOS profile: $BiosProfile"
    & $launcher -GamePath $gamePath -Region $BiosProfile -ExtraArgs $ExtraArgs
}
else {
    & $launcher -GamePath $gamePath -ExtraArgs $ExtraArgs
}
if (Test-Path Variable:LASTEXITCODE) { exit $LASTEXITCODE }
