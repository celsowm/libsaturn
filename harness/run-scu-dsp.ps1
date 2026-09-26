# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# SCU DSP on Ymir: the batch transform against the SH-2 fixed-point path, the
# DSP moving its own data, overlap with SH-2 work and a DMA address-addition
# probe. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 120
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/scu_dsp'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

$buildArgs = @{ Example = 'scu_dsp_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Failed to build scu_dsp_demo' }

$dump = Join-Path $dir 'wram.bin'
$args2 = @{
    Example = 'scu_dsp_demo'
    Frames = $Frames
    DumpWramHigh = $dump
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $args2.Bios = $Bios }
if ($Msys2Root) { $args2.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @args2 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir scu_dsp_demo probe failed' }

$env:LIBSATURN_DSP_DUMP = [string]$dump
$env:LIBSATURN_DSP_MAP = Join-Path $repo 'build/scu_dsp_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_scu_dsp.py -v
if ($LASTEXITCODE -ne 0) { throw "SCU DSP acceptance failed; inspect $dir" }
Write-Host "[dsp] Passed. Capture: $dir"
