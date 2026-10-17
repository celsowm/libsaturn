# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 hardware horizontal blur (BOKEN/BOKN) on Ymir: the chapter-12
# refusals, the CC-vs-blur A/B and the sharp right-half control.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 260
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_hardware_blur'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('plain.png', 'cc.png', 'blur.png', 'solo.png', 'never.png', 'struct.bin', 'misaligned.txt')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}

# --screenshot-when is keyed on g_blur.frames (word 14, offset 56), so the
# captures follow the scene's mode clock. The never.png probe proves a value
# the counter cannot reach does not fire.
# --watch-write starts at emulated frame 209, one frame before the BLUR switch
# at program frame 120 (90 BootFrames + 120). The probe watches 27-bit bus
# addresses, so the SH-2 uncached CCCTL alias 0x25F800EC is 0x05F800EC.
Invoke-ExampleTest `
    -Name 'hardwareblur' -Example 'vdp2_hardware_blur' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_hardware_blur.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -ScreenshotWhenSymbol @(
        "_g_blur:56:30+2:$(Join-Path $dir 'plain.png')",
        "_g_blur:56:100+2:$(Join-Path $dir 'cc.png')",
        "_g_blur:56:160+2:$(Join-Path $dir 'blur.png')",
        "_g_blur:56:250+2:$(Join-Path $dir 'solo.png')",
        "_g_blur:56:0xFFFFFFFF:$(Join-Path $dir 'never.png')") `
    -DumpMemSymbol @("_g_blur:0:60:$(Join-Path $dir 'struct.bin')") `
    -WatchWrite '0x05F800EC:2:209' `
    -ExtraEnv "LIBSATURN_HARDWAREBLUR_SHOTS=$dir"
