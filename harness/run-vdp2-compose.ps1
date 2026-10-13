# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 windows, mosaic and colour calculation on Ymir: the cycle-pattern
# allocator's result, the refusals, and the picture. Requires the user's
# own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 300
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_compose'
Invoke-ExampleTest `
    -Name 'compose' -Example 'vdp2_compose_demo' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -Screenshot @('100:' + (Join-Path $dir 'c.png'),
                  '250:' + (Join-Path $dir 'c2.png')) `
    -ExtraEnv "LIBSATURN_COMPOSE_SHOTS=$dir"
