[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Bios,
    [ValidateSet('serial','paired')][string]$Geometry='paired'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$examplesRoot=[IO.Path]::GetFullPath((Join-Path $root 'examples'))+[IO.Path]::DirectorySeparatorChar
$name='_character_geometry_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$temporary=[IO.Path]::GetFullPath((Join-Path $examplesRoot $name))
if(-not $temporary.StartsWith($examplesRoot,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Study directory escaped the examples workspace.'
}
if(Test-Path -LiteralPath $temporary){throw 'Study directory already exists.'}
$oldMetrics=$env:SAT_PROFILE_METRICS
$oldGeometry=$env:CHARACTER_DEMO_PARALLEL_GEOMETRY
$oldVerify=$env:CHARACTER_DEMO_VERIFY_GEOMETRY
Push-Location $root
try {
    $env:SAT_PROFILE_METRICS='1'
    $env:CHARACTER_DEMO_VERIFY_GEOMETRY='0'
    New-Item -ItemType Directory -Path $temporary | Out-Null
    $env:CHARACTER_DEMO_PARALLEL_GEOMETRY=if($Geometry -eq 'paired'){'1'}else{'0'}
    @"
#include "saturn/saturn.h"
#include "examples/characters/demo.h"
int main(void) {
    sat_dma_set_command_list(0);
    return character_demo_run(CHARACTER_DEMO_STRESS);
}
"@ | Set-Content -LiteralPath (Join-Path $temporary 'main.c')
    Copy-Item -LiteralPath 'examples/skinning_stress_3d/Makefile.inc' `
        -Destination (Join-Path $temporary 'Makefile.inc')
    & .\build-example.ps1 $name
    if($LASTEXITCODE){throw 'Study build failed.'}
    $prefix='harness/build/character_geometry_'+$Geometry
    Copy-Item -LiteralPath ('build/'+$name+'.map') -Destination ($prefix+'.map')
    @('0 NONE','100 A','140 NONE','500 Z','540 NONE','900 Z','940 NONE') |
        Set-Content -LiteralPath ($prefix+'.pad')
    & .\harness\run-harness.ps1 $name -Bios $Bios -Frames 1400 -PadScript ($prefix+'.pad') `
        -Screenshot @("399:${prefix}_two_auto.png") -DumpWramHigh ($prefix+'_high.bin') `
        -DumpMem @("0x00200000:1048576:${prefix}_low.bin") -Out ($prefix+'.json') `
        2>&1 | Tee-Object -FilePath ($prefix+'_run.log')
    if($LASTEXITCODE){throw 'Study emulation failed.'}
    & python harness/tests/character_preparation.py $prefix
    if($LASTEXITCODE){throw 'Study analysis failed.'}
} finally {
    $env:SAT_PROFILE_METRICS=$oldMetrics
    $env:CHARACTER_DEMO_PARALLEL_GEOMETRY=$oldGeometry
    $env:CHARACTER_DEMO_VERIFY_GEOMETRY=$oldVerify
    # Remove only this freshly created directory, checked again before deletion.
    $resolved=[IO.Path]::GetFullPath($temporary)
    if(-not $resolved.StartsWith($examplesRoot,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Study cleanup escaped the examples workspace.'
    }
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
    Pop-Location
}
