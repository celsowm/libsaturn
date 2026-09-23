[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Bios,

    [int]$Frames = 360,
    [int]$DiagnosticFrames = 120,
    [int]$BootFrames = 90,
    [string[]]$GeometryCases = @('12:1', '48:1', '96:1', '192:1',
        '48:2', '96:2', '192:2', '24:4', '48:4', '96:4'),
    [ValidateSet('MASTER', 'SLAVE', 'AUTO')]
    [string[]]$Modes = @('MASTER', 'SLAVE', 'AUTO'),
    [string]$Msys2Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $repoRoot 'build-example.ps1'
$runScript = Join-Path $PSScriptRoot 'run-harness.ps1'
$biosPath = (Resolve-Path -LiteralPath $Bios).Path

foreach ($case in $GeometryCases) {
    if ($case -notmatch '^(12|24|48|96|192):(1|2|4)$') {
        throw "Unsupported geometry case '$case' (use OBJECTS:FACES_PER_OBJECT from the documented matrix)."
    }
}

$reportArgs = @()
$normalReportArgs = @()
try {
    foreach ($mode in $Modes) {
        foreach ($case in $GeometryCases) {
            $parts = $case.Split(':')
            $workload = [int]$parts[0]
            $faces = [int]$parts[1]
            $tag = "{0}.{1}x{2}" -f $mode.ToLowerInvariant(), $workload, $faces
            $normalTag = "$tag.normal"
            Write-Host "[parallel-sweep] Normal frame benchmark workload=$workload faces=$faces mode=$mode ($Frames frames)"
            $normalBuildArgs = @{
                Example = 'parallel_runtime'
                GeometryObjects = $workload
                GeometryFacesPerObject = $faces
                ParallelMode = $mode
            }
            if ($Msys2Root) { $normalBuildArgs.Msys2Root = $Msys2Root }
            & $buildScript @normalBuildArgs
            if ($LASTEXITCODE -ne 0) { throw "normal parallel_runtime build failed for $normalTag" }
            $normalRunArgs = @{
                Example = 'parallel_runtime'
                Bios = $biosPath
                Frames = $Frames
                BootFrames = $BootFrames
                ProfileInstructions = (Join-Path $repoRoot "build\parallel_runtime.$normalTag.instructions.csv")
                ProfileCycles = (Join-Path $repoRoot "build\parallel_runtime.$normalTag.cycles.csv")
                ProfileTransfers = (Join-Path $repoRoot "build\parallel_runtime.$normalTag.transfers.csv")
                Out = (Join-Path $repoRoot "build\parallel_runtime.$normalTag.probe.json")
            }
            if ($Msys2Root) { $normalRunArgs.Msys2Root = $Msys2Root }
            & $runScript @normalRunArgs
            if ($LASTEXITCODE -ne 0) { throw "normal parallel_runtime harness failed for $normalTag" }
            $normalReportArgs += @('--normal-input', "${mode},${workload},${faces},${BootFrames},$(Join-Path $repoRoot "build\parallel_runtime.$normalTag.cycles.csv"),$(Join-Path $repoRoot "build\parallel_runtime.$normalTag.instructions.csv")")

            Write-Host "[parallel-sweep] Building workload=$workload faces=$faces mode=$mode (validation profile)"
            $buildArgs = @{
                Example = 'parallel_runtime'
                GeometryObjects = $workload
                GeometryFacesPerObject = $faces
                ParallelMode = $mode
                ParallelRuntimeValidation = $true
            }
            if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
            & $buildScript @buildArgs
            if ($LASTEXITCODE -ne 0) {
                throw "parallel_runtime build failed for $tag"
            }

            $runArgs = @{
                Example = 'parallel_runtime'
                Bios = $biosPath
                Frames = $DiagnosticFrames
                BootFrames = $BootFrames
                ProfileInstructions = (Join-Path $repoRoot "build\parallel_runtime.$tag.instructions.csv")
                ProfileCycles = (Join-Path $repoRoot "build\parallel_runtime.$tag.cycles.csv")
                ProfileTransfers = (Join-Path $repoRoot "build\parallel_runtime.$tag.transfers.csv")
                ProfileParallelRuntimeTelemetry = $true
                ParallelRuntimeTelemetryCsv = (Join-Path $repoRoot "build\parallel_runtime.$tag.telemetry.csv")
                Out = (Join-Path $repoRoot "build\parallel_runtime.$tag.probe.json")
            }
            if ($Msys2Root) { $runArgs.Msys2Root = $Msys2Root }
            & $runScript @runArgs
            if ($LASTEXITCODE -ne 0) {
                throw "parallel_runtime harness failed for $tag"
            }
            $reportArgs += @('--input', "${mode}:${workload}:${faces}:$(Join-Path $repoRoot "build\parallel_runtime.$tag.telemetry.csv")")
        }
    }
    $reportPath = Join-Path $repoRoot 'build\parallel_runtime.validation_summary.json'
    & python (Join-Path $PSScriptRoot 'tests\parallel_runtime_report.py') `
        --out $reportPath @reportArgs @normalReportArgs
    if ($LASTEXITCODE -ne 0) { throw 'parallel_runtime telemetry validation/report failed.' }
}
finally {
    # Leave the checkout on the documented default artifact.
    Write-Host '[parallel-sweep] Restoring default 12-object AUTO artifact'
    $restoreArgs = @{
        Example = 'parallel_runtime'
        GeometryObjects = 12
        GeometryFacesPerObject = 1
        ParallelMode = 'AUTO'
    }
    if ($Msys2Root) { $restoreArgs.Msys2Root = $Msys2Root }
    & $buildScript @restoreArgs
}
