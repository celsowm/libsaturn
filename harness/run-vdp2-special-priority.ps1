# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 special priority / special colour calculation on Ymir: the 2-word
# pattern-name bits, SFCODE dot selection, the CRAM MSB mode and the refuses.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 300
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_special_priority'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('screen.png', 'char.png', 'dot.png', 'msb.png', 'never.png',
                     'struct.bin', 'misaligned.txt')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}

# --screenshot-when is keyed on g_special_priority.frames (word 12, offset 48),
# so the captures follow the scene's mode clock. The never.png probe proves a
# value the counter cannot reach does not fire.
# --watch-write starts at emulated frame 269, one frame before the MSB switch
# at program frame 180 (90 BootFrames + 180). The probe watches 27-bit bus
# addresses, so the SH-2 uncached SFCCMD alias 0x25F800EE is 0x05F800EE.
Invoke-ExampleTest `
    -Name 'specialpriority' -Example 'vdp2_special_priority' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_special_priority.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -ScreenshotWhenSymbol @(
        "_g_special_priority:48:40+2:$(Join-Path $dir 'screen.png')",
        "_g_special_priority:48:100+2:$(Join-Path $dir 'char.png')",
        "_g_special_priority:48:160+2:$(Join-Path $dir 'dot.png')",
        "_g_special_priority:48:240+2:$(Join-Path $dir 'msb.png')",
        "_g_special_priority:48:0xFFFFFFFF:$(Join-Path $dir 'never.png')") `
    -DumpMemSymbol @("_g_special_priority:0:52:$(Join-Path $dir 'struct.bin')") `
    -WatchWrite '0x05F800EE:2:269' `
    -ExtraEnv "LIBSATURN_SPECIALPRIORITY_SHOTS=$dir"
