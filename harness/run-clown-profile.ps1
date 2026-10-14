[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Bios,
      [string]$OutPrefix='harness/build/clown_profile',
      [ValidateSet('idle','walk','combat','ambient')]
      [string[]]$Workloads=@('idle','walk','combat'),
      [switch]$SkipNoRecord,
      [ValidateSet('Parallel','Serial','Verify')]
      [string]$GeometryMode='Parallel')
$ErrorActionPreference='Stop'
if(-not $SkipNoRecord -and $Workloads -notcontains 'idle') {
    throw 'The no-record comparison requires an idle workload. Use -SkipNoRecord.'
}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$previousProfile=$env:CLOWN_PROFILE
$previousGeometry=$env:CLOWN_PARALLEL_GEOMETRY
$previousVerify=$env:CLOWN_VERIFY_GEOMETRY
$env:CLOWN_PARALLEL_GEOMETRY=if($GeometryMode -eq 'Serial'){'0'}else{'1'}
$env:CLOWN_VERIFY_GEOMETRY=if($GeometryMode -eq 'Verify'){'1'}else{'0'}
$env:CLOWN_PROFILE='1'
Push-Location $root
try {
    & .\build-example.ps1 killer_clown_demo
    if($LASTEXITCODE){throw 'Killer Clown build failed.'}
    foreach($workload in $Workloads) {
        $prefix="${OutPrefix}_$workload"
        Copy-Item -LiteralPath build/killer_clown_demo.map -Destination ($prefix+'.map')
        $pad=@('0 NONE','40 START','80 NONE')
        if($workload -eq 'walk'){$pad+=@('160 RIGHT','460 LEFT','760 RIGHT')}
        if($workload -eq 'combat'){$pad+=@('160 A','300 C','440 X','580 B','720 A','860 C')}
        $fields=1000
        $dumpRanges=@("0x00200000:1048576:${prefix}_low.bin")
        if($workload -eq 'ambient') {
            $pad=@('0 NONE','40 DOWN','60 NONE','80 DOWN','100 NONE','120 A','140 NONE',
                   '160 DOWN','180 NONE','200 A','220 NONE','240 B','260 NONE','280 START','320 NONE')
            $fields=1200
            $dumpRanges+="0x25C00000:524288:${prefix}_vdp1.bin"
        }
        $pad | Set-Content -LiteralPath ($prefix+'.pad')
        & .\harness\run-harness.ps1 killer_clown_demo -Bios $Bios -Frames $fields -RamCart none `
            -PadScript ($prefix+'.pad') -DumpWramHigh ($prefix+'_high.bin') `
            -DumpMem $dumpRanges `
            -Screenshot @("$($fields-1):${prefix}_final.png") -Out ($prefix+'.json')
        if($LASTEXITCODE){throw "Emulation failed: $workload"}
        if($workload -eq 'ambient'){ & python harness/tests/clown_profile.py $prefix --ambient }
        else { & python harness/tests/clown_profile.py $prefix }
        if($LASTEXITCODE){throw "Profile validation failed: $workload"}
    }
    if(-not $SkipNoRecord) {
    $env:CLOWN_PROFILE='0'
    & .\build-example.ps1 killer_clown_demo
    if($LASTEXITCODE){throw 'No-record comparison build failed.'}
    $prefix=$OutPrefix+'_no_record'
    Copy-Item -LiteralPath build/killer_clown_demo.map -Destination ($prefix+'.map')
    & .\harness\run-harness.ps1 killer_clown_demo -Bios $Bios -Frames 1000 -RamCart none `
        -PadScript ($OutPrefix+'_idle.pad') -DumpWramHigh ($prefix+'_high.bin') `
        -DumpMem @("0x00200000:1048576:${prefix}_low.bin") -Out ($prefix+'.json')
    if($LASTEXITCODE){throw 'No-record comparison emulation failed.'}
    & python harness/tests/clown_profile.py $prefix --no-record
    if($LASTEXITCODE){throw 'Instrumentation comparison failed.'}
    $env:CLOWN_PROFILE='1'
    & .\build-example.ps1 killer_clown_demo
    if($LASTEXITCODE){throw 'Profile build restoration failed.'}
    }
} finally {
    $env:CLOWN_PROFILE=$previousProfile
    $env:CLOWN_PARALLEL_GEOMETRY=$previousGeometry
    $env:CLOWN_VERIFY_GEOMETRY=$previousVerify
    Pop-Location
}
