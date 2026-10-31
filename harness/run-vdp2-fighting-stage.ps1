# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# VDP2 fighting stage (E11): the normal two-fighter character demo with the
# E11 four-NBG stage behind it. The example hides/shows the stage at demo
# frames 70 and 120; the on capture is at probe frame 150 and the off/on
# captures are triggered by the stage telemetry toggle count, so PAL round
# timing cannot shift them onto the wrong state.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 650,
    [switch]$Pal
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$palMode = $Pal -or $env:LIBSATURN_PAL -eq '1'
$dir = Join-Path $PSScriptRoot $(if ($palMode) { 'build/vdp2_fighting_stage_pal' } else { 'build/vdp2_fighting_stage' })
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($stale in @('on.png', 'off.png', 'on2.png', 'stage.bin', 'metrics.bin',
                     'misaligned.txt', 'wram.bin', 'probe.json')) {
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dir $stale)
}

Invoke-ExampleTest `
    -Name 'fightingstage' -Example 'vdp2_fighting_stage' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -Pal:$palMode `
    -DumpName 'wram.bin' `
    -TestFile 'test_vdp2_fighting_stage.py' `
    -MisalignedReport (Join-Path $dir 'misaligned.txt') `
    -Screenshot @(
        ('150:' + (Join-Path $dir 'on.png'))) `
    -ScreenshotWhenSymbol @(
        "_g_vdp2_stage:36:1+30:$(Join-Path $dir 'off.png')",
        "_g_vdp2_stage:36:2+30:$(Join-Path $dir 'on2.png')") `
    -DumpMemSymbol @(
        "_g_vdp2_stage:0:80:$(Join-Path $dir 'stage.bin')",
        "_g_character_demo:0:200:$(Join-Path $dir 'metrics.bin')") `
    -ExtraEnv "LIBSATURN_FIGHTINGSTAGE_SHOTS=$dir"
