[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Bios)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $root
try {
    & .\build-example.ps1 dino_demo
    if($LASTEXITCODE){throw 'Dino build failed.'}
    $prefix='harness/build/dino_runtime'
    Copy-Item -LiteralPath build/dino_demo.map -Destination ($prefix+'.map')
    @('0 NONE','150 START','170 NONE','200 A','220 NONE','300 A','320 NONE',
      '360 R','420 NONE','460 X','520 NONE','560 C','580 NONE','620 Z','640 NONE',
      '700 Z','720 NONE','800 B','820 NONE') | Set-Content -LiteralPath ($prefix+'.pad')
    & .\harness\run-harness.ps1 dino_demo -Bios $Bios -Frames 1200 `
        -PadScript ($prefix+'.pad') -Screenshot @("299:${prefix}_paused.png",
        "599:${prefix}_camera.png","1199:${prefix}_final.png") `
        -DumpWramHigh ($prefix+'_high.bin') -Out ($prefix+'.json') `
        2>&1 | Tee-Object -FilePath ($prefix+'_run.log')
    if($LASTEXITCODE){throw 'Dino emulation failed.'}
    & python harness/tests/dino_runtime.py $prefix
    if($LASTEXITCODE){throw 'Dino runtime validation failed.'}
} finally { Pop-Location }
