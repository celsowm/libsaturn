# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Scene cache + occlusion demo (examples/scene_cache_occlusion) on Ymir.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 120
)
. (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
$dir = Join-Path $PSScriptRoot 'build/scene_cache_occlusion'
Invoke-ExampleTest `
    -Name 'cache' -Example 'scene_cache_occlusion' `
    -Bios:$Bios -Msys2Root:$Msys2Root `
    -Frames $Frames `
    -TestFile 'test_scene_cache_occlusion.py' `
    -Screenshot @(('100:' + (Join-Path $dir 'screen.png'))) `
    -ExtraEnv "LIBSATURN_CACHE_SHOTS=$dir"
