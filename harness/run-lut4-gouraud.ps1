[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Bios)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $root
try {
    & .\build-example.ps1 lut4_gouraud
    if($LASTEXITCODE){throw 'LUT4 Gouraud build failed.'}
    $prefix='harness/build/lut4_gouraud'
    Copy-Item -LiteralPath build/lut4_gouraud.map -Destination ($prefix+'.map')
    @('0 NONE','80 A','100 NONE','180 A','200 NONE') | Set-Content -LiteralPath ($prefix+'.pad')
    & .\harness\run-harness.ps1 lut4_gouraud -Bios $Bios -RamCart none -Frames 300 `
        -PadScript ($prefix+'.pad') `
        -Screenshot @("60:${prefix}_animated.png","130:${prefix}_paused_a.png","160:${prefix}_paused_b.png","299:${prefix}_final.png") `
        -DumpWramHigh ($prefix+'_high.bin') -Out ($prefix+'.json') `
        -DumpMem @("0x25C00000:65536:${prefix}_vdp1.bin","0x25C10000:8:${prefix}_table.bin")
    if($LASTEXITCODE){throw 'LUT4 Gouraud emulation failed.'}
    & python harness/tests/lut4_gouraud_runtime.py $prefix
    if($LASTEXITCODE){throw 'LUT4 Gouraud validation failed.'}
} finally { Pop-Location }
