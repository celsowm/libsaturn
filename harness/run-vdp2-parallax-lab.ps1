# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 parallax lab: four hand-painted layers, two 8bpp bitmap planes and two
# cell planes (8bpp sky, 4bpp mountains), panning at 8/24/52/100 percent.
# -Long runs four pan cycles. Requires the user's own Saturn BIOS.
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
$dir = Join-Path $PSScriptRoot $(if ($palMode) { 'build/vdp2_parallax_lab_pal' } else { 'build/vdp2_parallax_lab' })
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('early.png', 'mid.png', 'late.png', 'struct.bin',
                     'misaligned.txt', 'wram.bin', 'probe.json')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
if ($Long) { $Frames = 2400 }
$longValue = if ($Long) { '1' } else { '0' }

Invoke-ExampleTest `
    -Name 'plab' -Example 'vdp2_parallax_lab' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -Pal:$palMode `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_parallax_lab.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -Screenshot @(
        ('120:' + (Join-Path $dir 'early.png')),
        ('360:' + (Join-Path $dir 'mid.png')),
        ('700:' + (Join-Path $dir 'late.png'))) `
    -DumpMemSymbol @("_g_parallax_lab:0:68:$(Join-Path $dir 'struct.bin')") `
    -DumpVram @('0:16', '131072:16', '139264:16', '196608:16') `
    -ExtraEnv "LIBSATURN_PLAB_SHOTS=$dir", "LIBSATURN_PLAB_LONG=$longValue"
