# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# sat_exec round trip (examples/exec_demo) on Ymir. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 900
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/exec_demo'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads
# the struct through build/exec_demo.map, which must match the ISO.
$buildArgs = @{ Example = 'exec_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build exec_demo' }

$dump = Join-Path $dir 'wram_high.bin'
$argsForHarness = @{
    Example = 'exec_demo'
    Frames = $Frames
    Screenshot = @(("{0}:{1}" -f ($Frames - 20), (Join-Path $dir 'screen.png')))
    DumpWramHigh = $dump
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness
if ($LASTEXITCODE -ne 0) { throw 'The Ymir exec_demo probe failed' }

# The test skips without a dump, and a skipped run would pass here.
if (-not (Test-Path $dump)) { throw "The probe wrote no Work RAM dump: $dump" }
$env:LIBSATURN_EXEC_DUMP = $dump
$env:LIBSATURN_EXEC_MAP = Join-Path $repo 'build/exec_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_exec_demo.py -v
if ($LASTEXITCODE -ne 0) { throw "sat_exec acceptance failed; inspect $dir" }
Write-Host "[exec] Passed. Capture: $dir"
