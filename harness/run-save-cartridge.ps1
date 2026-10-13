# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Two-process persistence (examples/save_cartridge_demo) on Ymir. Requires
# the user's own Saturn BIOS. The wrapper runs the demo twice (first and
# second run) so the test can compare both dumps.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 90
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$harness = $PSScriptRoot
$repo = Split-Path -Parent $harness
$dir = Join-Path $harness 'build/save_cartridge'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$fixture = Join-Path $dir 'cartridge.bup'
Remove-Item $fixture -ErrorAction SilentlyContinue

$buildArgs = @{ Example = 'save_cartridge_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Failed to build save_cartridge_demo' }

foreach ($phase in 'first', 'second') {
    $argsForHarness = @{
        Example = 'save_cartridge_demo'
        Frames = $Frames
        DumpWramHigh = (Join-Path $dir "wram_$phase.bin")
        Out = (Join-Path $dir "$phase.json")
    }
    if ($phase -eq 'second') { $argsForHarness.BackupCartFixture = $fixture }
    if ($Bios) { $argsForHarness.Bios = $Bios }
    if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
    Write-Host "[save-cartridge] $phase"
    & (Join-Path $harness 'run-harness.ps1') @argsForHarness | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "The Ymir save_cartridge_demo probe ($phase) failed" }
}

$env:LIBSATURN_CART_FIRST_JSON = (Join-Path $dir 'first.json')
$env:LIBSATURN_CART_SECOND_JSON = (Join-Path $dir 'second.json')
$env:LIBSATURN_CART_FIRST_DUMP = (Join-Path $dir 'wram_first.bin')
$env:LIBSATURN_CART_SECOND_DUMP = (Join-Path $dir 'wram_second.bin')
$env:LIBSATURN_CART_MAP = (Join-Path $repo 'build/save_cartridge_demo.map')
python -m unittest discover (Join-Path $harness 'tests') -p test_save_cartridge.py -v
if ($LASTEXITCODE -ne 0) { throw "Save cartridge acceptance failed; inspect $dir" }
Write-Host "[save-cartridge] Passed. Capture: $dir"
