# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 raster effects on Ymir: the cycle-pattern allocator's result, the
# refusals, and the picture. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 200
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_effects_demo'
Invoke-ExampleTest `
    -Name 'effects' -Example 'vdp2_effects_demo' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_effects.py' `
    -Screenshot @(('100:' + (Join-Path $dir 'e.png'))) `
    -ExtraEnv "LIBSATURN_EFFECTS_SHOTS=$dir"
