# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# MPEG card probe and UART open (examples/expansion_probe_demo) on Ymir, which
# has neither. Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 120
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
Invoke-ExampleTest `
    -Name 'expansion' -Example 'expansion_probe_demo' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_expansion_probe.py' `
    -ExtraEnv "LIBSATURN_EXPANSION_DIR=$((Join-Path $PSScriptRoot 'build/expansion_probe_demo'))"
