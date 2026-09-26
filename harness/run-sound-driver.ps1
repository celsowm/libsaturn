# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# The resident 68000 sound driver on Ymir: timed events, tick rate and SCSP slot
# state. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 300
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/sound_driver'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

$buildArgs = @{ Example = 'sound_driver_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Failed to build sound_driver_demo' }

$args2 = @{
    Example = 'sound_driver_demo'
    Frames = $Frames
    DumpWramHigh = (Join-Path $dir 'wram.bin')
    DumpSoundRam = (Join-Path $dir 'sram.bin')
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $args2.Bios = $Bios }
if ($Msys2Root) { $args2.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @args2 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir sound_driver_demo probe failed' }

$env:LIBSATURN_DRIVER_DIR = [string]$dir
$env:LIBSATURN_DRIVER_MAP = Join-Path $repo 'build/sound_driver_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_sound_driver.py -v
if ($LASTEXITCODE -ne 0) { throw "Sound driver acceptance failed; inspect $dir" }
Write-Host "[driver] Passed. Capture: $dir"
