# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Input devices and SMPC services on Ymir: a 3D Control Pad and a Shuttle
# Mouse driven by a device script, the same pad in digital mode, and the RTC,
# SMEM and reset-enable round trips. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 420,
    [switch]$ForceRebuildHarness
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/input_devices'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# Always rebuild: run-harness only builds a missing ISO, and the test reads the
# struct through build/input_devices_demo.map, which must match the ISO.
$buildArgs = @{ Example = 'input_devices_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build input_devices_demo' }

# Scenario A: 3D pad in analog mode on port 1 (axes move at frame 100), mouse
# on port 2 (moves from frame 100), START+A on port 1 at frame 300 runs the
# RTC/SMEM round trips.
$scriptA = Join-Path $dir 'device_a.txt'
@'
# frame port fields
0   1 buttons=NONE analog=1 x=0x80 y=0x80 l=0 r=0
0   2 dx=0 dy=0 left=0
100 1 x=0xF0 y=0x10 l=0xC8 r=0x64
100 2 dx=5 dy=-3 left=1
300 1 buttons=START+A
'@ | Set-Content -Encoding ASCII $scriptA

# Scenario B: the 3D pad in digital mode, port 2 empty.
$scriptB = Join-Path $dir 'device_b.txt'
@'
0   1 buttons=B analog=0
'@ | Set-Content -Encoding ASCII $scriptB

function Invoke-Scenario($name, $script, [string[]]$ports) {
    $dump = Join-Path $dir "wram_$name.bin"
    $argsForHarness = @{
        Example = 'input_devices_demo'
        Frames = $Frames
        PortDevice = $ports
        DeviceScript = $script
        Screenshot = @(("{0}:{1}" -f ($Frames - 10), (Join-Path $dir "screen_$name.png")))
        DumpWramHigh = $dump
        Out = (Join-Path $dir "probe_$name.json")
    }
    if ($Bios) { $argsForHarness.Bios = $Bios }
    if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
    if ($ForceRebuildHarness -and $name -eq 'a') { $argsForHarness.ForceRebuildHarness = $true }
    & (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "The Ymir input_devices_demo probe failed ($name)" }
    if (-not (Test-Path $dump)) { throw "No Work RAM dump for scenario $name" }
    return [string]$dump
}

$dumpA = Invoke-Scenario 'a' $scriptA @('1:analog', '2:mouse')
$dumpB = Invoke-Scenario 'b' $scriptB @('1:analog', '2:none')

$env:LIBSATURN_INPUT_DUMP_A = $dumpA
$env:LIBSATURN_INPUT_DUMP_B = $dumpB
$env:LIBSATURN_INPUT_MAP = Join-Path $repo 'build/input_devices_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_input_devices.py -v
if ($LASTEXITCODE -ne 0) { throw "Input device acceptance failed; inspect $dir" }
Write-Host "[input] Passed. Capture: $dir"
