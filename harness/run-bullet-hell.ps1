# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Ymir acceptance for examples/bullet_hell_stress (the shmup stress demo): two runs of the real
# program, each from a fresh build because the test reads the telemetry struct through that
# build's link map.
#   play   default build + a pad script that skips to stage 2 at game frame 400: disc streaming of
#          the next stage's sprites and background, the stage swap, music underruns
#   bench  BH_AUTOBENCH build: 128/256/512/1024 simultaneous bullets x 300 frames, per-stage FRT ticks
# Ymir has no VDP1 fill-rate model; the tick counts are the CPU side only (docs/SHMUP_BULLET_HELL_SUPPORT.md).
# Requires the user's own BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/bullet_hell_check'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
Get-ChildItem $dir -File | Remove-Item -Force

function Invoke-Run([string]$Tag, [bool]$Autobench, [int]$Frames, [string[]]$Shots, [string]$PadScript) {
    # build-example.ps1 reads these when it composes the CFLAGS, and a different value rebuilds.
    if ($Autobench) { $env:BH_AUTOBENCH = '1' } else { Remove-Item Env:\BH_AUTOBENCH -ErrorAction SilentlyContinue }
    try {
        $buildArgs = @{ Example = 'bullet_hell_stress' }
        if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
        & (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'Failed to build bullet_hell_stress' }
    } finally {
        Remove-Item Env:\BH_AUTOBENCH -ErrorAction SilentlyContinue
    }
    Copy-Item -Force (Join-Path $repo 'build/bullet_hell_stress.map') (Join-Path $dir "$Tag.map")
    $harnessArgs = @{
        Example = 'bullet_hell_stress'
        Frames = $Frames
        Screenshot = @($Shots | ForEach-Object { $f, $n = $_ -split ':'; "{0}:{1}" -f $f, (Join-Path $dir "${Tag}_$n.png") })
        DumpWramHigh = (Join-Path $dir "$Tag.wram")
        Out = (Join-Path $dir "$Tag.json")
    }
    if ($PadScript) { $harnessArgs.PadScript = $PadScript; $harnessArgs.PadScriptGameFrame = $true }
    if ($Bios) { $harnessArgs.Bios = $Bios }
    if ($Msys2Root) { $harnessArgs.Msys2Root = $Msys2Root }
    & (Join-Path $PSScriptRoot 'run-harness.ps1') @harnessArgs | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "The Ymir bullet_hell_stress '$Tag' probe failed" }
    if (-not (Test-Path (Join-Path $dir "$Tag.wram"))) { throw "No Work RAM dump for '$Tag'" }
}

# Screenshot frames are emulated fields counted from the probe's start (the BIOS boot is part of them).
Invoke-Run 'play' $false 1100 @('600:s1', '1000:s2') (Join-Path $PSScriptRoot 'scripts/bullet_hell_stage.pad')
Invoke-Run 'bench' $true 2600 @('1900:mid', '2500:end') $null

$env:LIBSATURN_BH_DIR = [string]$dir
Push-Location $repo
try { python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_bullet_hell.py -v } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw "bullet_hell_stress acceptance failed; inspect $dir" }
Write-Host "[bullet-hell] Passed. Capture: $dir"
