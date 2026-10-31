# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 video modes (E09): 320/352/640/704 wide, 224/240/256 lines,
# non-interlaced and single-density interlaced. The example changes mode by a
# full sat_shutdown/sat_init, so the run captures each supported mode through
# its telemetry `mode` word.
#
# NTSC (default): the five 224/240-line modes. PAL (-Pal with a PAL BIOS):
# all eight, including the 256-line modes and 512-line interlaced fields.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 700,
    [switch]$Pal
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$palMode = $Pal -or $env:LIBSATURN_PAL -eq '1'
$dir = Join-Path $PSScriptRoot $(if ($palMode) { 'build/vdp2_video_modes_pal' } else { 'build/vdp2_video_modes' })
if ($palMode -and $Frames -eq 700) { $Frames = 900 }
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('m0.png', 'm1.png', 'm2.png', 'm3.png', 'm4.png',
                     'm5.png', 'm6.png', 'm7.png', 'never.png',
                     'struct.bin', 'misaligned.txt', 'wram.bin', 'probe.json')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}

# Telemetry word 9 (offset 36) is the mode index; each index holds for 60
# fields, so +20 lands on a stable picture.
$shots = @()
for ($i = 0; $i -lt 8; $i++) {
    $shots += "_g_video_modes:36:${i}+20:$(Join-Path $dir ('m{0}.png' -f $i))"
}
$shots += "_g_video_modes:36:0xFFFFFFFF:$(Join-Path $dir 'never.png')"

Invoke-ExampleTest `
    -Name 'videomodes' -Example 'vdp2_video_modes' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames -Pal:$palMode `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_video_modes.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -ScreenshotWhenSymbol $shots `
    -DumpMemSymbol @("_g_video_modes:0:96:$(Join-Path $dir 'struct.bin')") `
    -DumpVram @('0:16384') `
    -ExtraEnv "LIBSATURN_VIDEOMODES_SHOTS=$dir", "LIBSATURN_VIDEOMODES_PAL=$($palMode ? 1 : 0)"
