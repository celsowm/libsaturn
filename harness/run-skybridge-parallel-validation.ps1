[CmdletBinding()]
param(
    [string]$Bios,
    [int]$Frames = 900,
    [int]$BootFrames = 90,
    [switch]$SkipFaultInjection
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$OutputRoot = Join-Path $RepoRoot 'build\skybridge_parallel_validation'
if (-not (Test-Path $OutputRoot)) {
    New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
}
$PadScript = Join-Path $PSScriptRoot 'scripts\skybridge_parallel_validation.pad'
$script:SkybridgeFirstBuild = $true
$script:SkybridgeFirstHarnessRun = $true

function Invoke-SkybridgeProfile {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Mode,
        [switch]$ForceGemSplit,
        [string]$Fault = 'None',
        [int]$RunFrames = $Frames
    )

    $buildArgs = @{
        Example = 'skybridge_3d'
        SkybridgeValidation = $true
        ParallelMode = $Mode
    }
    if ($script:SkybridgeFirstBuild) {
        $buildArgs.ForceRebuild = $true
        $script:SkybridgeFirstBuild = $false
    }
    if ($ForceGemSplit) { $buildArgs.ForceGemSplit = $true }
    if ($Fault -ne 'None') { $buildArgs.SkybridgeFault = $Fault }
    Write-Host "[skybridge-validation] Building $Name ($Mode, split=$ForceGemSplit, fault=$Fault)"
    & (Join-Path $RepoRoot 'build-example.ps1') @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Skybridge build failed for $Name" }

    $json = Join-Path $OutputRoot "$Name.json"
    $instructions = Join-Path $OutputRoot "${Name}_instructions.csv"
    $cycles = Join-Path $OutputRoot "${Name}_cycles.csv"
    $telemetry = Join-Path $OutputRoot "${Name}_telemetry.csv"
    $runArgs = @{
        Example = 'skybridge_3d'
        Frames = $RunFrames
        BootFrames = $BootFrames
        PadScript = $PadScript
        PadScriptGameFrame = $true
        Out = $json
        ProfileInstructions = $instructions
        ProfileCycles = $cycles
        ProfileSkybridgeTelemetry = $true
        SkybridgeTelemetryCsv = $telemetry
    }
    if ($script:SkybridgeFirstHarnessRun) {
        $runArgs.ForceRebuildHarness = $true
        $script:SkybridgeFirstHarnessRun = $false
    }
    if ($Bios) { $runArgs.Bios = $Bios }
    Write-Host "[skybridge-validation] Running $Name ($BootFrames BIOS + $RunFrames emulated frames)"
    & (Join-Path $PSScriptRoot 'run-harness.ps1') @runArgs
    if ($LASTEXITCODE -ne 0) { throw "Skybridge harness run failed for $Name" }
}

foreach ($mode in @('MASTER', 'SLAVE', 'AUTO')) {
    Invoke-SkybridgeProfile -Name "bench_$mode" -Mode $mode
}

foreach ($mode in @('MASTER', 'SLAVE', 'AUTO')) {
    Invoke-SkybridgeProfile -Name "split_$mode" -Mode $mode -ForceGemSplit
}

if (-not $SkipFaultInjection) {
    foreach ($fault in @('SubmitReject', 'WorkerError', 'TimeoutAbort', 'AbortFailure', 'ReleaseFailure')) {
        $faultFrames = if ($fault -eq 'AbortFailure') { [Math]::Max($Frames, 180) } else { $Frames }
        Invoke-SkybridgeProfile -Name "fault_$fault" -Mode 'SLAVE' `
            -ForceGemSplit -Fault $fault -RunFrames $faultFrames
    }
}

$reportArgs = @((Join-Path $PSScriptRoot 'tests\skybridge_parallel_report.py'),
               '--root', $OutputRoot, '--frames', '360')
if ($SkipFaultInjection) { $reportArgs += '--skip-faults' }
& python @reportArgs
$reportExitCode = $LASTEXITCODE

Write-Host '[skybridge-validation] Restoring the normal AUTO build (no validation hooks)'
& (Join-Path $RepoRoot 'build-example.ps1') skybridge_3d -ParallelMode AUTO
if ($LASTEXITCODE -ne 0) { throw 'Could not restore the normal Skybridge AUTO build.' }
if ($reportExitCode -ne 0) { throw 'Skybridge validation report rejected one or more profiles.' }
