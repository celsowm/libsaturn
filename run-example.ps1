[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Example,

    [ValidateSet('mednafen', 'kronos')]
    [string]$Emulator = 'mednafen',

    [ValidateSet('auto', 'na', 'jp', 'eu')]
    [string]$BiosProfile = 'auto',

    [ValidateSet('current', 'safe')]
    [string]$IpProfile = 'current',

    [ValidateSet('yaul', 'sbl')]
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
        throw "Script de build nao encontrado: $buildScript"
    }

    $buildArgs = @{
        Example = $normalizedExample
        IpProfile = $IpProfile
        IpTemplate = $IpTemplate
    }
    if ($Msys2Root) {
        $buildArgs.Msys2Root = $Msys2Root
    }

    & $buildScript @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Falha ao buildar example: $normalizedExample"
    }
}

if (-not (Test-Path $isoPath)) {
    throw "ISO do example nao encontrada: $isoPath"
}

$launcher = switch ($Emulator) {
    'mednafen' { Join-Path $RepoRoot 'emulators\mednafen\run-mednafen.ps1' }
    'kronos' { Join-Path $RepoRoot 'emulators\kronos\run-kronos.ps1' }
}

if (-not (Test-Path $launcher)) {
    throw "Launcher do emulador nao encontrado: $launcher. Rode .\scripts\download-emulators.ps1."
}

$gamePath = if ($Emulator -eq 'mednafen' -and (Test-Path $cuePath)) { $cuePath } else { $isoPath }
Write-Host "[run-example] Executando $normalizedExample em $Emulator"
if ($Emulator -eq 'mednafen') {
    Write-Host "[run-example] BIOS profile: $BiosProfile"
    & $launcher -GamePath $gamePath -Region $BiosProfile -ExtraArgs $ExtraArgs
}
else {
    & $launcher -GamePath $gamePath -ExtraArgs $ExtraArgs
}
exit $LASTEXITCODE
