# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# SCU interrupt acceptance on Ymir. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 400
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
Invoke-ExampleTest `
    -Name 'irq' -Example 'irq_demo' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -Screenshot @('380:' + (Join-Path $PSScriptRoot 'build/irq_demo/screen.png'))
