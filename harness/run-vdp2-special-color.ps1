# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 special colour (ratio, add, extended cascade) on Ymir: the
# chapter-12 refusals, the CRAM-mode-0 requirement, and the picture.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 300
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_special_color'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('ratio.png', 'add.png', 'ext.png', 'never.png', 'struct.bin', 'misaligned.txt')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}

# --screenshot-when is keyed on g_special_color.frames (word 12, offset 48),
# so the capture follows the scene's own mode clock instead of assuming the
# emulated and program frame counters stay in lockstep. The never.png probe
# proves a value the counter cannot reach does not fire.
# --watch-write starts at emulated frame 149, one frame before the RATIO
# switch at program frame 60 (90 BootFrames + 60), and records the CCCTL
# writes with their PC. Starting at the first program frame would spend the
# 64 stored hits on the per-frame zero writes before RATIO. The probe watches
# 27-bit bus addresses, so the SH-2 uncached alias 0x25F800EC is 0x05F800EC.
Invoke-ExampleTest `
    -Name 'specialcolor' -Example 'vdp2_special_color' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_special_color.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -ScreenshotWhenSymbol @(
        "_g_special_color:48:100+2:$(Join-Path $dir 'ratio.png')",
        "_g_special_color:48:150+2:$(Join-Path $dir 'add.png')",
        "_g_special_color:48:250+2:$(Join-Path $dir 'ext.png')",
        "_g_special_color:48:0xFFFFFFFF:$(Join-Path $dir 'never.png')") `
    -DumpMemSymbol @("_g_special_color:0:52:$(Join-Path $dir 'struct.bin')") `
    -WatchWrite '0x05F800EC:2:149' `
    -ExtraEnv "LIBSATURN_SPECIALCOLOR_SHOTS=$dir"
