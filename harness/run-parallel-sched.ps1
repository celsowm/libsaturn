# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Slave SH-2 scheduling on Ymir's two real CPUs: priorities, dependencies,
# batched dispatch and sat_parallel_for. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 120
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
Invoke-ExampleTest `
    -Name 'sched' -Example 'parallel_sched_demo' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_parallel_sched.py'
