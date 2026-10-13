# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# A red light on hi-res (640x224, 8-bit) VDP1 polygons done by the VDP2: line colour screen under the
# sprite layer, add-mode colour calculation in an ellipse window, lit only on codes 128-255. On Ymir:
# the demo's verdicts and two pictures (spot holding, spot sweeping). Requires the user's own BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 400
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/vdp2_hires_light'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads the struct through
# build/vdp2_hires_light.map, which must match the ISO.
$buildArgs = @{ Example = 'vdp2_hires_light' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build vdp2_hires_light' }

$dump = Join-Path $dir 'wram.bin'
foreach ($stale in @('a.png', 'b.png', 'misaligned.txt', 'wram.bin')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
$map = Join-Path $repo 'build/vdp2_hires_light.map'
if (-not (Select-String -Path $map -Pattern '^\s*0x([0-9A-Fa-f]+)\s+_g_light_demo\s*$' | Select-Object -First 1)) {
    throw "_g_light_demo is absent from $map"
}
$argsForHarness = @{
    Example = 'vdp2_hires_light'
    Frames = $Frames
    # the spot holds until frame 240, then sweeps: one picture on each side
    Screenshot = @(("100:{0}" -f (Join-Path $dir 'a.png')), ("{0}:{1}" -f ($Frames - 10), (Join-Path $dir 'b.png')))
    DumpWramHigh = $dump
    MisalignedReport = (Join-Path $dir 'misaligned.txt')
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir vdp2_hires_light probe failed' }
if (-not (Test-Path $dump)) { throw 'No Work RAM dump' }

$env:LIBSATURN_LIGHT_DUMP = [string]$dump
$env:LIBSATURN_LIGHT_MAP = $map
$env:LIBSATURN_LIGHT_SHOTS = [string]$dir
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_vdp2_hires_light.py -v
if ($LASTEXITCODE -ne 0) { throw "VDP2 hi-res light acceptance failed; inspect $dir" }
Write-Host "[hires-light] Passed. Capture: $dir"
