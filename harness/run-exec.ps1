# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# sat_exec (examples/exec_demo) on Ymir. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 200
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
Invoke-ExampleTest `
    -Name 'exec' -Example 'exec_demo' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_exec_demo.py'
