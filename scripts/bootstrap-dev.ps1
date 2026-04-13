[CmdletBinding()]
param(
    [string]$Msys2Root,
    [switch]$NoInstall,
    [string]$LogPath,
    [switch]$SkipEmulators
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$BootstrapScript = Join-Path $PSScriptRoot 'bootstrap-msys2.ps1'
$DownloadEmulatorsScript = Join-Path $PSScriptRoot 'download-emulators.ps1'

if (-not (Test-Path $BootstrapScript)) {
    throw "Script not found: $BootstrapScript"
}

if (-not (Test-Path $DownloadEmulatorsScript)) {
    throw "Script not found: $DownloadEmulatorsScript"
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$StepName,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    Write-Host "[bootstrap-dev] Starting step: $StepName"
    & $Action
    Write-Host "[bootstrap-dev] Step completed: $StepName"
}

$bootstrapArgs = @{}
if ($Msys2Root) {
    $bootstrapArgs['Msys2Root'] = $Msys2Root
}
if ($NoInstall.IsPresent) {
    $bootstrapArgs['NoInstall'] = $true
}
if ($LogPath) {
    $bootstrapArgs['LogPath'] = $LogPath
}

$downloadArgs = @{}
if ($Msys2Root) {
    $downloadArgs['Msys2Root'] = $Msys2Root
}

Invoke-Step -StepName 'bootstrap host' -Action {
    & $BootstrapScript host @bootstrapArgs
}

Invoke-Step -StepName 'bootstrap full' -Action {
    & $BootstrapScript full @bootstrapArgs
}

if ($SkipEmulators.IsPresent) {
    Write-Host '[bootstrap-dev] Emulators step skipped due to -SkipEmulators.'
}
else {
    Invoke-Step -StepName 'download emulators' -Action {
        & $DownloadEmulatorsScript @downloadArgs
    }
}

Write-Host '[bootstrap-dev] Environment ready.'
