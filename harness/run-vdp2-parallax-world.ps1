# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 parallax world (E10): four NBGs plus RBG0, per-line sky and line
# colour, line scroll, a window, add-mode colour calculation, and the VDP1
# pre-rendered reference path toggled automatically at frames 600-660.
# -Long runs the 60-second replay requested by the plan.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 750,
    [switch]$Long,
    [switch]$Pal
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$palMode = $Pal -or $env:LIBSATURN_PAL -eq '1'
$dir = Join-Path $PSScriptRoot $(if ($palMode) { 'build/vdp2_parallax_world_pal' } else { 'build/vdp2_parallax_world' })
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('early.png', 'mid.png', 'late.png', 'reference.png', 'never.png',
                     'struct.bin', 'misaligned.txt', 'wram.bin', 'probe.json')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
if ($Long) { $Frames = 3600 }
$longValue = if ($Long) { '1' } else { '0' }

Invoke-ExampleTest `
    -Name 'parallax' -Example 'vdp2_parallax_world' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -Pal:$palMode `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_parallax_world.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -Screenshot @(
        ('120:' + (Join-Path $dir 'early.png')),
        ('300:' + (Join-Path $dir 'mid.png')),
        ('700:' + (Join-Path $dir 'late.png'))) `
    -ScreenshotWhenSymbol @(
        "_g_parallax:40:1+15:$(Join-Path $dir 'reference.png')",
        "_g_parallax:40:0xFFFFFFFF:$(Join-Path $dir 'never.png')") `
    -DumpMemSymbol @("_g_parallax:0:96:$(Join-Path $dir 'struct.bin')") `
    -ExtraEnv "LIBSATURN_PARALLAX_SHOTS=$dir", "LIBSATURN_PARALLAX_LONG=$longValue"
