# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# SCU DMA acceptance on Ymir. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 400
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/dma_demo'
Invoke-ExampleTest `
    -Name 'dma' -Example 'dma_demo' `
    -Bios $Bios -Msys2Root $Msys2Root `
    -Frames $Frames `
    -Screenshot @("$($Frames - 20):$($dir)/screen.png") `
    -MapSuffix '.map' `
    -TestFile 'test_dma_demo.py'
