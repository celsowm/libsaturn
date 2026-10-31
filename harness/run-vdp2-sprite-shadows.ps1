# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 sprite shadows on Ymir: normal-shadow and transparent-MSB-shadow words
# over a scrolling NBG0 ground, a VDP1 MSB-on sprite self-shadow, a VDP1
# shadow control panel, and the SDCTL target/ownership refusals.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 320
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_sprite_shadows'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('normal.png', 'msb.png', 'vdp1.png', 'blocked.png', 'off.png',
                     'never.png', 'struct.bin', 'misaligned.txt')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}

# --screenshot-when is keyed on g_sprite_shadows.frames (word 12, offset 48).
# The extra 2 frames let the mode's register commit and the next VDP2 frame
# settle. The never.png probe proves an unreachable counter value cannot fire.
# --watch-write starts at emulated frame 149: program frame 60 (90 boot frames
# + 59) is one frame before the NORMAL -> MSB switch, whose SDCTL write must
# carry TPSDSL (0x0101). The probe watches 27-bit bus addresses, so the SH-2
# uncached SDCTL alias 0x25F800E2 is 0x05F800E2.
Invoke-ExampleTest `
    -Name 'spriteshadows' -Example 'vdp2_sprite_shadows' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_sprite_shadows.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -ScreenshotWhenSymbol @(
        "_g_sprite_shadows:48:42+2:$(Join-Path $dir 'normal.png')",
        "_g_sprite_shadows:48:102+2:$(Join-Path $dir 'msb.png')",
        "_g_sprite_shadows:48:162+2:$(Join-Path $dir 'vdp1.png')",
        "_g_sprite_shadows:48:222+2:$(Join-Path $dir 'blocked.png')",
        "_g_sprite_shadows:48:282+2:$(Join-Path $dir 'off.png')",
        "_g_sprite_shadows:48:0xFFFFFFFF:$(Join-Path $dir 'never.png')") `
    -DumpMemSymbol @("_g_sprite_shadows:0:52:$(Join-Path $dir 'struct.bin')") `
    -WatchWrite '0x05F800E2:2:149' `
    -ExtraEnv "LIBSATURN_SPRITESHADOWS_SHOTS=$dir"
