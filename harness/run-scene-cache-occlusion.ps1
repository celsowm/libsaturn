# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Requires the user's own Saturn BIOS and a runnable pinned Ymir probe.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$BootFrames = 90
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$dir = Join-Path $PSScriptRoot 'build/scene_cache_occlusion'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$far = Join-Path $dir 'far_55.png'
$near = Join-Path $dir 'near_105.png'
$camera = Join-Path $dir 'camera_145.png'
$zoom = Join-Path $dir 'zoom_175.png'
$probe = Join-Path $dir 'probe.json'
$run = Join-Path $PSScriptRoot 'run-harness.ps1'
$argsForHarness = @{
    Example = 'scene_cache_occlusion'
    Frames = 190
    BootFrames = $BootFrames
    PadScript = (Join-Path $PSScriptRoot 'scripts/scene_cache_occlusion.pad')
    Screenshot = @("55:$far", "105:$near", "145:$camera", "175:$zoom")
    Out = $probe
}
if ($Bios) { $argsForHarness.Bios = $Bios }
if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
& $run @argsForHarness
if ($LASTEXITCODE -ne 0) { throw "The Ymir occlusion probe failed" }
$env:LIBSATURN_OCCLUSION_DIR = $dir
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_scene_cache_occlusion.py -v
if ($LASTEXITCODE -ne 0) { throw "Camera-cache occlusion regression failed; inspect captures in $dir" }
Write-Host "[occlusion] Passed. Inspect original captures: $dir"
