# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# SCU DMA acceptance on Ymir. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 400
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/dma_demo'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads
# the struct through build/dma_demo.map, which must match the ISO.
$buildArgs = @{ Example = 'dma_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build dma_demo' }

$dump = Join-Path $dir 'wram_high.bin'
$argsForHarness = @{
    Example = 'dma_demo'
    Frames = $Frames
    Screenshot = @(("{0}:{1}" -f ($Frames - 20), (Join-Path $dir 'screen.png')))
    DumpWramHigh = $dump
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness
if ($LASTEXITCODE -ne 0) { throw 'The Ymir dma_demo probe failed' }

$env:LIBSATURN_DMA_DUMP = $dump
$env:LIBSATURN_DMA_MAP = Join-Path $repo 'build/dma_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_dma_demo.py -v
if ($LASTEXITCODE -ne 0) { throw "SCU DMA acceptance failed; inspect $dir" }
Write-Host "[dma] Passed. Capture: $dir"
