# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# vdp2_rotation_coefficients on Ymir: RBG0 parameter sets A and B, the
# coefficient table contract and the per-line A/B switching, on a live
# scene. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 300
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/vdp2_rotation_coefficients'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads
# the struct through build/vdp2_rotation_coefficients.map, which must match.
$buildArgs = @{ Example = 'vdp2_rotation_coefficients' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build vdp2_rotation_coefficients' }

$dump = Join-Path $dir 'wram.bin'
foreach ($stale in @('struct.bin', 'f50.png', 'f110.png', 'f170.png', 'f230.png',
                      'misaligned.txt', 'probe.json')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
# One screenshot per scene, taken mid-scene so the animation has settled:
# 50 SWELL, 110 WAVE, 170 YAW B, 230 MIX.
$shots = @(("50:{0}" -f (Join-Path $dir 'f50.png')),
           ("110:{0}" -f (Join-Path $dir 'f110.png')),
           ("170:{0}" -f (Join-Path $dir 'f170.png')),
           ("230:{0}" -f (Join-Path $dir 'f230.png')))
$map = Join-Path $repo 'build/vdp2_rotation_coefficients.map'
$symbol = Select-String -Path $map -Pattern '^\s*0x([0-9A-Fa-f]+)\s+_g_rotation_coefficients\s*$' | Select-Object -First 1
if (-not $symbol) { throw "_g_rotation_coefficients is absent from $map" }
$struct = [Convert]::ToUInt32($symbol.Matches[0].Groups[1].Value, 16)
$argsForHarness = @{
    Example = 'vdp2_rotation_coefficients'
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
if ($LASTEXITCODE -ne 0) { throw 'The Ymir vdp2_rotation_coefficients probe failed' }
if (-not (Test-Path $dump)) { throw 'No Work RAM dump' }

$env:LIBSATURN_ROTC_DUMP = [string]$dump
$env:LIBSATURN_ROTC_MAP = Join-Path $repo 'build/vdp2_rotation_coefficients.map'
$env:LIBSATURN_ROTC_SHOTS = [string]$dir
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_vdp2_rotation_coefficients.py -v
if ($LASTEXITCODE -ne 0) { throw "VDP2 rotation coefficients acceptance failed; inspect $dir" }
Write-Host "[rotation-coefficients] Passed. Capture: $dir"
