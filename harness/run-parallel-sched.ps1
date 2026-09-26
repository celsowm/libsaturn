# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Slave SH-2 scheduling on Ymir's two real CPUs: priorities, dependencies,
# batched dispatch and sat_parallel_for. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 120
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/parallel_sched'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

$buildArgs = @{ Example = 'parallel_sched_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Failed to build parallel_sched_demo' }

$dump = Join-Path $dir 'wram.bin'
$args2 = @{
    Example = 'parallel_sched_demo'
    Frames = $Frames
    DumpWramHigh = $dump
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $args2.Bios = $Bios }
if ($Msys2Root) { $args2.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @args2 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir parallel_sched_demo probe failed' }

$env:LIBSATURN_SCHED_DUMP = [string]$dump
$env:LIBSATURN_SCHED_MAP = Join-Path $repo 'build/parallel_sched_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_parallel_sched.py -v
if ($LASTEXITCODE -ne 0) { throw "Slave scheduling acceptance failed; inspect $dir" }
Write-Host "[sched] Passed. Capture: $dir"
