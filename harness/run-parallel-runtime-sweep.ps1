[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Bios,

    [int]$Frames = 120,
    [int]$BootFrames = 90,
    [int[]]$Workloads = @(12, 48, 96),
    [ValidateSet('MASTER', 'SLAVE', 'AUTO')]
    [string[]]$Modes = @('MASTER', 'SLAVE', 'AUTO'),
    [string]$Msys2Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $repoRoot 'build-example.ps1'
$runScript = Join-Path $PSScriptRoot 'run-harness.ps1'
$biosPath = (Resolve-Path $Bios).Path

foreach ($workload in $Workloads) {
    if ($workload -notin @(12, 48, 96)) {
        throw "Unsupported parallel_runtime workload: $workload (use 12, 48, or 96)."
    }
}

try {
    foreach ($mode in $Modes) {
        foreach ($workload in $Workloads) {
            $tag = "{0}.{1}" -f $mode.ToLowerInvariant(), $workload
            Write-Host "[parallel-sweep] Building workload=$workload mode=$mode"
            $buildArgs = @{
                Example = 'parallel_runtime'
                GeometryObjects = $workload
                ParallelMode = $mode
                ForceRebuild = $true
            }
            if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
            & $buildScript @buildArgs
            if ($LASTEXITCODE -ne 0) {
                throw "parallel_runtime build failed for $tag"
            }

            $runArgs = @{
                Example = 'parallel_runtime'
                Bios = $biosPath
                Frames = $Frames
                BootFrames = $BootFrames
                ProfileInstructions = (Join-Path $repoRoot "build\parallel_runtime.$tag.instructions.csv")
                ProfileCycles = (Join-Path $repoRoot "build\parallel_runtime.$tag.cycles.csv")
                ProfileTransfers = (Join-Path $repoRoot "build\parallel_runtime.$tag.transfers.csv")
                Out = (Join-Path $repoRoot "build\parallel_runtime.$tag.probe.json")
            }
            if ($Msys2Root) { $runArgs.Msys2Root = $Msys2Root }
            & $runScript @runArgs
            if ($LASTEXITCODE -ne 0) {
                throw "parallel_runtime harness failed for $tag"
            }
        }
    }
}
finally {
    # Leave the checkout on the documented default artifact.
    Write-Host '[parallel-sweep] Restoring default 12-object AUTO artifact'
    $restoreArgs = @{
        Example = 'parallel_runtime'
        GeometryObjects = 12
        ParallelMode = 'AUTO'
        ForceRebuild = $true
    }
    if ($Msys2Root) { $restoreArgs.Msys2Root = $Msys2Root }
    & $buildScript @restoreArgs
}
