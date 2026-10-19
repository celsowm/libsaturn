# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# vdp2_vram_stress on Ymir: the transactional resource plan's success and
# every documented refusal path, on a live four-layer picture. Requires the
# user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 260
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/vdp2_vram_stress'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads
# the struct through build/vdp2_vram_stress.map, which must match the ISO.
$buildArgs = @{ Example = 'vdp2_vram_stress' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build vdp2_vram_stress' }

$dump = Join-Path $dir 'wram.bin'
foreach ($stale in @('struct.bin', 'f80.png', 'f195.png', 'f255.png', 'misaligned.txt')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
# Frames 80 (layout A), 195 (after every refusal scenario, before the
# reconfiguration) and 255 (after it): the bands must be identical across
# the refusals and the bank bars must show the reclaimed layout at the end.
$shots = @(("80:{0}" -f (Join-Path $dir 'f80.png')),
           ("195:{0}" -f (Join-Path $dir 'f195.png')),
           ("255:{0}" -f (Join-Path $dir 'f255.png')))
$map = Join-Path $repo 'build/vdp2_vram_stress.map'
$symbol = Select-String -Path $map -Pattern '^\s*0x([0-9A-Fa-f]+)\s+_g_vram_stress\s*$' | Select-Object -First 1
if (-not $symbol) { throw "_g_vram_stress is absent from $map" }
$struct = [Convert]::ToUInt32($symbol.Matches[0].Groups[1].Value, 16)
$argsForHarness = @{
    Example = 'vdp2_vram_stress'
    Frames = $Frames
    Screenshot = $shots
    DumpMem = @(("0x{0:X8}:256:{1}" -f $struct, (Join-Path $dir 'struct.bin')))
    DumpWramHigh = $dump
    MisalignedReport = (Join-Path $dir 'misaligned.txt')
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir vdp2_vram_stress probe failed' }
if (-not (Test-Path $dump)) { throw 'No Work RAM dump' }

$env:LIBSATURN_VRS_DUMP = [string]$dump
$env:LIBSATURN_VRS_MAP = Join-Path $repo 'build/vdp2_vram_stress.map'
$env:LIBSATURN_VRS_SHOTS = [string]$dir
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_vdp2_vram_stress.py -v
if ($LASTEXITCODE -ne 0) { throw "VDP2 vram stress acceptance failed; inspect $dir" }
Write-Host "[vram-stress] Passed. Capture: $dir"
