[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Example,

    [ValidateSet('mednafen', 'kronos')]
    [string]$Emulator = 'mednafen',

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
$isoPath = Join-Path $RepoRoot ("build\examples\{0}.iso" -f $safeName)

if ($BuildFirst -or -not (Test-Path $isoPath)) {
    $buildScript = Join-Path $RepoRoot 'build-example.ps1'
    if (-not (Test-Path $buildScript)) {
        throw "Script de build nao encontrado: $buildScript"
    }

    $buildArgs = @{
        Example = $normalizedExample
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

Write-Host "[run-example] Executando $normalizedExample em $Emulator"
& $launcher -GamePath $isoPath -ExtraArgs $ExtraArgs
exit $LASTEXITCODE
