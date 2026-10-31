# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 dynamic tile/map streaming (E07): two pattern-name rings, stage_map2
# double buffering, VBlank base/scroll publication, CRC and DMA/CPU telemetry.
# The default run is the normal acceptance; -Stress runs the 10-minute-class
# 36,000-frame Ymir stress and freezes the scene at 33,000 for the final CRC.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 2400,
    [switch]$Stress
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/vdp2_dynamic_tile_stream'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('early.png', 'mid.png', 'late.png', 'wrap.png', 'long.png', 'never.png',
                     'struct.bin', 'misaligned.txt', 'wram.bin', 'probe.json')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}
if ($Stress) {
    $Frames = 36000
    $stressValue = '1'
    Write-Host '[tilestream] stress mode: 36,000 emulated frames, freeze at 33,000'
} else {
    $stressValue = '0'
}

# frames is word 9 (offset 36); the struct is 32 words / 128 bytes. MPABN0 is
# the NBG0 plane A/B register at 0x180040; the uncached alias is 0x05F80040.
# The scene alternates rings every frame, so both 0x0000 and 0x0202 appear.
Invoke-ExampleTest `
    -Name 'tilestream' -Example 'vdp2_dynamic_tile_stream' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_dynamic_tile_stream.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -ScreenshotWhenSymbol @(
        "_g_tile_stream:36:600+2:$(Join-Path $dir 'early.png')",
        "_g_tile_stream:36:1200+2:$(Join-Path $dir 'mid.png')",
        "_g_tile_stream:36:1800+2:$(Join-Path $dir 'late.png')",
        "_g_tile_stream:36:8190+2:$(Join-Path $dir 'wrap.png')",
        "_g_tile_stream:36:33000+2:$(Join-Path $dir 'long.png')",
        "_g_tile_stream:36:0xFFFFFFFF:$(Join-Path $dir 'never.png')") `
    -DumpMemSymbol @("_g_tile_stream:0:128:$(Join-Path $dir 'struct.bin')") `
    -DumpVram @('0:4096', '8192:4096') `
    -WatchWrite '0x05F80040:2:1289' `
    -ExtraEnv "LIBSATURN_TILESTREAM_SHOTS=$dir", "LIBSATURN_TILESTREAM_STRESS=$stressValue"
