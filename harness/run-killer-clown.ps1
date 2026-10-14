[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Bios)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $root
try {
    & .\build-example.ps1 killer_clown_demo
    if($LASTEXITCODE){throw 'Killer Clown build failed.'}
    $prefix='harness/build/killer_clown_fight'
    Copy-Item -LiteralPath build/killer_clown_demo.map -Destination ($prefix+'.map')
    @('0 NONE','40 DOWN','60 NONE','80 DOWN','100 NONE','120 A','140 NONE','160 DOWN','180 NONE','200 A','220 NONE','240 A','260 NONE','280 B','300 NONE','320 UP','340 NONE','360 A','380 NONE','400 B','420 NONE','440 START','460 RIGHT','540 A','580 A+Z','620 A','660 C','840 B','860 B+Z','900 B','1060 C+Z','1100 C','1260 C+Z','1300 C','1420 NONE','1500 Y','1520 NONE','1540 RIGHT','1660 NONE','1700 START','1720 NONE','1800 B','1820 NONE','1860 A','1960 C','2140 NONE') | Set-Content -LiteralPath ($prefix+'.pad')
    @('0 2 buttons=NONE','1540 2 buttons=LEFT','1660 2 buttons=B',
      '1960 2 buttons=C','2140 2 buttons=NONE') | Set-Content -LiteralPath ($prefix+'.devices')
    & .\harness\run-harness.ps1 killer_clown_demo -Bios $Bios -Frames 2300 `
        -PadScript ($prefix+'.pad') -RamCart none -PortDevice @('2:pad') `
        -DeviceScript ($prefix+'.devices') `
        -Screenshot @("60:${prefix}_menu.png","220:${prefix}_options.png","380:${prefix}_controls.png","560:${prefix}_idle.png","760:${prefix}_kick.png",
        "960:${prefix}_solid.png","1160:${prefix}_cutaway.png", "1360:${prefix}_auto.png",
        "1750:${prefix}_paused_a.png","1780:${prefix}_paused_b.png",
        "1920:${prefix}_two_player.png","2299:${prefix}_final.png") `
        -DumpMem @("0x25C00000:524288:${prefix}_vdp1.bin","0x25F80000:512:${prefix}_vdp2.bin") `
        -DumpAudio ($prefix+'.raw') -DumpSoundRam ($prefix+'_sound.bin') `
        -DumpWramHigh ($prefix+'_high.bin') -Out ($prefix+'.json') `
        2>&1 | Tee-Object -FilePath ($prefix+'_run.log')
    if($LASTEXITCODE){throw 'Killer Clown emulation failed.'}
    & python harness/tests/killer_clown_runtime.py $prefix
    if($LASTEXITCODE){throw 'Killer Clown runtime validation failed.'}
} finally { Pop-Location }
