[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Bios)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $root
try {
    & .\build-example.ps1 colored_light_cubes
    if($LASTEXITCODE){throw 'Colored light cubes build failed.'}
    $prefix='harness/build/light_cubes'
    Copy-Item -LiteralPath build/colored_light_cubes.map -Destination ($prefix+'.map')
    @('0 NONE','80 A','100 NONE','180 A','200 NONE','220 B','230 NONE','250 B','260 NONE','270 C','280 NONE','290 C','295 NONE') |
        Set-Content -LiteralPath ($prefix+'.pad')
    & .\harness\run-harness.ps1 colored_light_cubes -Bios $Bios -RamCart none -Frames 360 `
        -PadScript ($prefix+'.pad') `
        -Screenshot @("60:${prefix}_animated.png","130:${prefix}_paused_a.png","160:${prefix}_paused_b.png","240:${prefix}_unlit.png","279:${prefix}_no_pool.png","359:${prefix}_final.png") `
        -DumpWramHigh ($prefix+'_high.bin') -Out ($prefix+'.json') `
        -DumpMem @("0x25C00000:524288:${prefix}_vdp1.bin","0x25F80000:512:${prefix}_vdp2.bin")
    if($LASTEXITCODE){throw 'Colored light cubes emulation failed.'}
    & python harness/tests/colored_light_cubes_runtime.py $prefix
    if($LASTEXITCODE){throw 'Colored light cubes validation failed.'}
} finally { Pop-Location }
