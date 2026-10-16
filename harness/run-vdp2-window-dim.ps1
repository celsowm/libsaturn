# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Darkening limited to a window on Ymir. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 130
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_window_dim'
Invoke-ExampleTest `
    -Name 'dim' -Example 'vdp2_window_dim' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_window_dim.py' `
    -Screenshot @(('75:' + (Join-Path $dir 'a.png')),
                  ('95:' + (Join-Path $dir 'b.png'))) `
    -ExtraEnv "LIBSATURN_DIM_SHOTS=$dir"
