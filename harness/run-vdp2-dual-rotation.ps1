# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# vdp2_dual_rotation on Ymir: RBG1, the second rotation screen, beside RBG0.
# The demo cycles three modes (A-ONLY, B-on-RBG0, DUAL) every 120 frames,
# exercises the live sat_vdp2_rotation_set_parameter_mode and
# sat_vdp2_rotation_rbg1_set_enabled paths and records six refused illegal
# combinations; the test reads g_dual_rotation through
# build/vdp2_dual_rotation.map and checks the screenshots. Requires the
# user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 420
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/vdp2_dual_rotation'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads
# the struct through build/vdp2_dual_rotation.map, which must match.
$buildArgs = @{ Example = 'vdp2_dual_rotation' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build vdp2_dual_rotation' }

$dump = Join-Path $dir 'wram.bin'
foreach ($stale in @('struct.bin', 'f60.png', 'f180.png', 'f300.png', 'f380.png',
                      'misaligned.txt', 'probe.json')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
# One screenshot per mode, taken mid-scene: the modes advance at frames
# 121/241/361, so 60 is A-ONLY, 180 B-on-RBG0, 300 DUAL (the capture
# frame), 380 A-ONLY again after the wrap.
$shots = @(("60:{0}" -f (Join-Path $dir 'f60.png')),
           ("180:{0}" -f (Join-Path $dir 'f180.png')),
           ("300:{0}" -f (Join-Path $dir 'f300.png')),
           ("380:{0}" -f (Join-Path $dir 'f380.png')))
$map = Join-Path $repo 'build/vdp2_dual_rotation.map'
$symbol = Select-String -Path $map -Pattern '^\s*0x([0-9A-Fa-f]+)\s+_g_dual_rotation\s*$' | Select-Object -First 1
if (-not $symbol) { throw "_g_dual_rotation is absent from $map" }
$struct = [Convert]::ToUInt32($symbol.Matches[0].Groups[1].Value, 16)
$argsForHarness = @{
    Example = 'vdp2_dual_rotation'
    Frames = $Frames
    Screenshot = $shots
    DumpMem = @(("0x{0:X8}:144:{1}" -f $struct, (Join-Path $dir 'struct.bin')))
    DumpWramHigh = $dump
    MisalignedReport = (Join-Path $dir 'misaligned.txt')
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir vdp2_dual_rotation probe failed' }
if (-not (Test-Path $dump)) { throw 'No Work RAM dump' }

$env:LIBSATURN_DROT_DUMP = [string]$dump
$env:LIBSATURN_DROT_MAP = Join-Path $repo 'build/vdp2_dual_rotation.map'
$env:LIBSATURN_DROT_SHOTS = [string]$dir
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_vdp2_dual_rotation.py -v
if ($LASTEXITCODE -ne 0) { throw "VDP2 dual rotation acceptance failed; inspect $dir" }
Write-Host "[dual-rotation] Passed. Capture: $dir"
