<#
.SYNOPSIS
    Rebuilds city_walk, runs its four harness scenarios and the acceptance tests.

.DESCRIPTION
    walk    4 MB cart, run across several chunks so the residency rings page
    wall    4 MB cart, walk into a building
    nocart  no cartridge: the program must refuse, not crash
    onemeg  1 MB cartridge: the program must refuse, not load halfway

    Each scenario keeps its own probe report, Work RAM dump and screenshots, so
    the tests never read another scenario's output. The example is rebuilt
    first: the harness reuses an existing ISO, and a stale one would test the
    wrong program. Any skipped test fails the run.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Bios
)
# Native tools (xorriso) print banners on stderr: check exit codes explicitly instead.
$ErrorActionPreference = 'Continue'
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
$Build = Join-Path $RepoRoot 'harness\build'
New-Item -ItemType Directory -Force $Build | Out-Null

& (Join-Path $RepoRoot 'build-example.ps1') city_walk 2>&1 | Out-Host
if ($global:LASTEXITCODE -ne 0) { throw 'build-example.ps1 city_walk failed' }

$scenarios = @(
    @{ Name = 'walk';   Cart = '4m';   Frames = 1300; Pad = 'city_walk_walk.pad'; Shots = @("820:$Build\city_walk_walk_mid.png", "1250:$Build\city_walk_walk_end.png") },
    @{ Name = 'wall';   Cart = '4m';   Frames = 1800; Pad = 'city_walk_wall.pad'; Shots = @("1750:$Build\city_walk_wall_end.png") },
    @{ Name = 'nocart'; Cart = 'none'; Frames = 260;  Pad = $null; Shots = @("240:$Build\city_walk_nocart.png") },
    @{ Name = 'onemeg'; Cart = '1m';   Frames = 260;  Pad = $null; Shots = @("240:$Build\city_walk_onemeg.png") }
)
foreach ($s in $scenarios) {
    Write-Host "== city_walk scenario: $($s.Name) (RAM cart $($s.Cart))"
    # A hashtable splat binds named parameters; an array splat would pass "-Bios"
    # as a positional string.
    $harnessArgs = @{
        Example      = 'city_walk'
        Bios         = $Bios
        Frames       = $s.Frames
        RamCart      = $s.Cart
        DumpWramHigh = (Join-Path $Build "city_walk_$($s.Name)_wramh.bin")
    }
    if ($s.Pad) {
        $harnessArgs.PadScript = Join-Path $RepoRoot "harness\scripts\$($s.Pad)"
        $harnessArgs.PadScriptGameFrame = $true
    }
    if ($s.Shots.Count -gt 0) { $harnessArgs.Screenshot = $s.Shots }
    & (Join-Path $RepoRoot 'harness\run-harness.ps1') @harnessArgs | Out-Host
    if ($global:LASTEXITCODE -ne 0) { throw "run-harness failed for scenario $($s.Name)" }
    Copy-Item (Join-Path $Build 'probe.json') (Join-Path $Build "city_walk_$($s.Name).json") -Force
}

Write-Host '== acceptance tests'
$output = & python -m unittest discover -s (Join-Path $RepoRoot 'harness\tests') -p 'test_city_walk.py' -v 2>&1
$code = $global:LASTEXITCODE
$output | Out-Host
if ($code -ne 0) { throw 'city_walk acceptance tests failed' }
# A wrapper must never report success when its tests were skipped.
if ($output -match 'skipped') { throw 'city_walk acceptance tests were skipped: the run proved nothing' }
Write-Host 'city_walk checks passed.'
