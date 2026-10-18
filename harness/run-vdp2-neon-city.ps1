# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 neon city (E12): rotating RBG0 sign, streaming NBG1 street through two
# pattern-name rings, windows + line-colour light pool, colour offsets, and
# the final composition. -Long runs the 10-minute-class replay.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 900,
    [switch]$Long
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_neon_city'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('early.png', 'mid.png', 'late.png', 'never.png',
                     'struct.bin', 'misaligned.txt', 'wram.bin', 'probe.json')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
if ($Long) { $Frames = 3600 }
$longValue = if ($Long) { '1' } else { '0' }

Invoke-ExampleTest `
    -Name 'neoncity' -Example 'vdp2_neon_city' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_neon_city.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -Screenshot @(
        ('150:' + (Join-Path $dir 'early.png')),
        ('450:' + (Join-Path $dir 'mid.png')),
        ('800:' + (Join-Path $dir 'late.png'))) `
    -DumpMemSymbol @("_g_neon_city:0:128:$(Join-Path $dir 'struct.bin')") `
    -DumpVram @('131072:4096', '135168:4096') `
    -ExtraEnv "LIBSATURN_NEONCITY_SHOTS=$dir", "LIBSATURN_NEONCITY_LONG=$longValue"
