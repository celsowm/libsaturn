# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Backup Memory cartridge acceptance on Ymir: one throwaway 4 Mbit cartridge
# image and one internal Backup RAM image are reused across two fresh emulator
# processes. The guest writes a record to the cartridge through the BIOS Backup
# Library; the record must persist and count up while the internal image stays
# untouched. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 90
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/save_cartridge'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$cart = Join-Path $dir 'cartridge.bup'
$internal = Join-Path $dir 'internal.bup'
Remove-Item $cart, $internal -ErrorAction SilentlyContinue

$buildArgs = @{ Example = 'save_cartridge_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Failed to build save_cartridge_demo' }

function Invoke-Pass([string]$Tag) {
    $args2 = @{
        Example = 'save_cartridge_demo'
        Frames = $Frames
        BackupRam = $internal
        BackupCartFixture = $cart
        DumpWramHigh = (Join-Path $dir "wram_$Tag.bin")
        Out = (Join-Path $dir "$Tag.json")
    }
    if ($Bios) { $args2.Bios = $Bios }
    if ($Msys2Root) { $args2.Msys2Root = $Msys2Root }
    & (Join-Path $PSScriptRoot 'run-harness.ps1') @args2 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "The Ymir save_cartridge_demo probe ($Tag) failed" }
}

Write-Host '[cartridge] pass 1: fresh cartridge, guest writes the record'
Invoke-Pass 'first'
Write-Host '[cartridge] pass 2: same images, new process'
Invoke-Pass 'second'

$env:LIBSATURN_CART_FIRST_JSON = Join-Path $dir 'first.json'
$env:LIBSATURN_CART_SECOND_JSON = Join-Path $dir 'second.json'
$env:LIBSATURN_CART_FIRST_DUMP = Join-Path $dir 'wram_first.bin'
$env:LIBSATURN_CART_SECOND_DUMP = Join-Path $dir 'wram_second.bin'
$env:LIBSATURN_CART_MAP = Join-Path $repo 'build/save_cartridge_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_save_cartridge.py -v
if ($LASTEXITCODE -ne 0) { throw "Save cartridge acceptance failed; inspect $dir" }
Write-Host "[cartridge] Passed. Capture: $dir"
