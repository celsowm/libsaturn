# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# More than 512 distinct 8bpp tiles on one VDP2 scroll screen (a second NBG
# reads the window that follows the first), beside a third 256-colour layer,
# on Ymir. The struct dump in `struct.bin` is a side product the test compares
# against the live WRAM read. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 120
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_tile_overflow'
# The probe writes its screenshots and dumps here; a fresh clone has no
# harness/build/<example> yet (run-vdp2-layers.ps1 creates its own the same way).
New-Item -ItemType Directory -Force -Path $dir | Out-Null
# vdp2_tile_overflow needs the harness to dump the struct bytes from a known
# address. We can't reach run-harness.ps1's -DumpMem from the helper, so the
# few extra lines stay here.
$buildArgs = @{ Example = 'vdp2_tile_overflow' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path (Split-Path -Parent $PSScriptRoot) 'build-example.ps1') @buildArgs
if ($LASTEXITCODE -ne 0) { throw 'Failed to build vdp2_tile_overflow' }
foreach ($stale in @('last.png', 'struct.bin', 'misaligned.txt', 'wram.bin')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
$map = Join-Path (Split-Path -Parent $PSScriptRoot) 'build/vdp2_tile_overflow.map'
$symbol = Select-String -Path $map -Pattern '^\s*0x([0-9A-Fa-f]+)\s+_g_overflow_demo\s*$' | Select-Object -First 1
if (-not $symbol) { throw "_g_overflow_demo is absent from $map" }
$struct = [Convert]::ToUInt32($symbol.Matches[0].Groups[1].Value, 16)
$argsForHarness = @{
    Example = 'vdp2_tile_overflow'
    Frames = $Frames
    Screenshot = @(('{0}:{1}' -f ($Frames - 5), (Join-Path $dir 'last.png')))
    DumpMem = @('0x{0:X8}:72:{1}' -f $struct, (Join-Path $dir 'struct.bin'))
    DumpWramHigh = (Join-Path $dir 'wram.bin')
    MisalignedReport = (Join-Path $dir 'misaligned.txt')
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @argsForHarness | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir vdp2_tile_overflow probe failed' }

$env:LIBSATURN_OVERFLOW_DUMP = (Join-Path $dir 'wram.bin')
$env:LIBSATURN_OVERFLOW_MAP = $map
$env:LIBSATURN_OVERFLOW_SHOTS = $dir
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_vdp2_tile_overflow.py -v
if ($LASTEXITCODE -ne 0) { throw "VDP2 tile overflow acceptance failed; inspect $dir" }
Write-Host "[tile-overflow] Passed. Capture: $dir"
