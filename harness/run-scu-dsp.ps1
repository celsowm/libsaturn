# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# SCU DSP on Ymir: the batch transform against the SH-2 fixed-point path, the
# DSP moving its own data, overlap with SH-2 work and a DMA address-addition
# probe. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 120
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
Invoke-ExampleTest `
    -Name 'dsp' -Example 'scu_dsp_demo' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_scu_dsp.py'
