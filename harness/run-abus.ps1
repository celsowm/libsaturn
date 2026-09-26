# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# A-Bus slot acceptance on Ymir: the same guest runs with an empty slot, the
# 1 MiB and 4 MiB RAM expansions and a Backup Memory cartridge fixture.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 60
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/abus'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$fixture = Join-Path $dir 'cartridge.bup'
Remove-Item $fixture -ErrorAction SilentlyContinue

$buildArgs = @{ Example = 'abus_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Failed to build abus_demo' }

foreach ($mode in 'none', '1m', '4m', 'backup') {
    $args2 = @{
        Example = 'abus_demo'
        Frames = $Frames
        DumpWramHigh = (Join-Path $dir "wram_$mode.bin")
        Out = (Join-Path $dir "$mode.json")
    }
    if ($mode -eq 'backup') { $args2.BackupCartFixture = $fixture }
    elseif ($mode -ne 'none') { $args2.RamCart = $mode }
    if ($Bios) { $args2.Bios = $Bios }
    if ($Msys2Root) { $args2.Msys2Root = $Msys2Root }
    Write-Host "[abus] $mode"
    & (Join-Path $PSScriptRoot 'run-harness.ps1') @args2 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "The Ymir abus_demo probe ($mode) failed" }
}

$env:LIBSATURN_ABUS_DIR = [string]$dir
$env:LIBSATURN_ABUS_MAP = Join-Path $repo 'build/abus_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_abus.py -v
if ($LASTEXITCODE -ne 0) { throw "A-Bus acceptance failed; inspect $dir" }
Write-Host "[abus] Passed. Capture: $dir"
