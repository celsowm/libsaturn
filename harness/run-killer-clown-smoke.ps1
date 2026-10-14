[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Bios)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $root
try {
    & .\build-example.ps1 killer_clown_demo
    if($LASTEXITCODE){throw 'Killer Clown build failed.'}
    $prefix='harness/build/clown_default'
    Copy-Item -LiteralPath build/killer_clown_demo.map -Destination ($prefix+'.map')
    @('0 NONE','40 START','60 RIGHT','100 X','120 RIGHT','140 A','240 C','360 B','420 A','520 C','620 NONE') |
        Set-Content -LiteralPath ($prefix+'.pad')
    & .\harness\run-harness.ps1 killer_clown_demo -Bios $Bios -Frames 700 -RamCart none `
        -PadScript ($prefix+'.pad') `
        -Screenshot @("120:${prefix}_idle.png","150:${prefix}_jump.png","300:${prefix}_lighting.png","699:${prefix}_final.png") `
        -DumpWramHigh ($prefix+'_high.bin') -Out ($prefix+'.json') `
        -DumpMem @("0x25C00000:524288:${prefix}_vdp1.bin","0x25F80000:512:${prefix}_vdp2.bin") `
        -DumpAudio ($prefix+'.raw') -DumpSoundRam ($prefix+'_sound.bin')
    if($LASTEXITCODE){throw 'Killer Clown smoke emulation failed.'}
    & python harness/tests/killer_clown_runtime.py $prefix --smoke
    if($LASTEXITCODE){throw 'Killer Clown smoke validation failed.'}
} finally { Pop-Location }
