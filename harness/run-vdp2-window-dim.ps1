# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Darkening limited to a window: a black colour-calculation NBG shown only outside two line-window
# beams (the VDP2 colour offset cannot be limited to an area), on Ymir: the library's verdicts, the
# beam tables and two pictures at different ratios. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 130
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/vdp2_window_dim'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads the struct through
# build/vdp2_window_dim.map, which must match the ISO.
$buildArgs = @{ Example = 'vdp2_window_dim' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build vdp2_window_dim' }

$dump = Join-Path $dir 'wram.bin'
foreach ($stale in @('a.png', 'b.png', 'last.png', 'misaligned.txt', 'wram.bin')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
$map = Join-Path $repo 'build/vdp2_window_dim.map'
if (-not (Select-String -Path $map -Pattern '^\s*0x([0-9A-Fa-f]+)\s+_g_dim_demo\s*$' | Select-Object -First 1)) {
    throw "_g_dim_demo is absent from $map"
}
$argsForHarness = @{
    Example = 'vdp2_window_dim'
    Frames = $Frames
    # the beams hold from frame 50 and the ratio drops at frame 90: one picture on each side
    Screenshot = @(("75:{0}" -f (Join-Path $dir 'a.png')), ("{0}:{1}" -f ($Frames - 5), (Join-Path $dir 'b.png')))
    DumpWramHigh = $dump
    MisalignedReport = (Join-Path $dir 'misaligned.txt')
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir vdp2_window_dim probe failed' }
if (-not (Test-Path $dump)) { throw 'No Work RAM dump' }

$env:LIBSATURN_DIM_DUMP = [string]$dump
$env:LIBSATURN_DIM_MAP = $map
$env:LIBSATURN_DIM_SHOTS = [string]$dir
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_vdp2_window_dim.py -v
if ($LASTEXITCODE -ne 0) { throw "VDP2 window dim acceptance failed; inspect $dir" }
Write-Host "[window-dim] Passed. Capture: $dir"
