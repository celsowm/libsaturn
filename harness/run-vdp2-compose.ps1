# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 windows, mosaic and colour calculation on Ymir: the cycle-pattern allocator's result, the
# refusals, and the picture. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 300
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/vdp2_compose'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads the
# struct through build/vdp2_compose_demo.map, which must match the ISO.
$buildArgs = @{ Example = 'vdp2_compose_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build vdp2_compose_demo' }

$dump = Join-Path $dir 'wram.bin'
$shots = @(("100:{0}" -f (Join-Path $dir 'c.png')), ("250:{0}" -f (Join-Path $dir 'c2.png')))
$argsForHarness = @{
    Example = 'vdp2_compose_demo'
    Frames = $Frames
    Screenshot = $shots
    DumpWramHigh = $dump
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir vdp2_compose_demo probe failed' }
if (-not (Test-Path $dump)) { throw 'No Work RAM dump' }

$env:LIBSATURN_COMPOSE_DUMP = [string]$dump
$env:LIBSATURN_COMPOSE_MAP = Join-Path $repo 'build/vdp2_compose_demo.map'
$env:LIBSATURN_COMPOSE_SHOTS = [string]$dir
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_vdp2_compose.py -v
if ($LASTEXITCODE -ne 0) { throw "VDP2 compose acceptance failed; inspect $dir" }
Write-Host "[compose] Passed. Capture: $dir"
