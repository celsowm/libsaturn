[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Bios,
    [ValidateSet('minimal','full')][string]$TestProfile='minimal',
    [ValidateSet('yaul','sbl','native')][string]$BootProfile='yaul',
    [ValidateSet('injection','bios')][string]$BootMode='injection',
    [switch]$RunCdTests,
    [string]$CdbRom,
    [string]$PackageDirectory,
    [string]$Prefix='harness/build/optical-tester'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
Push-Location $repo
$previous=$env:OPTICAL_TEST_PROFILE
try {
    if($RunCdTests -and $TestProfile -ne 'full'){throw 'CD tests require the full profile'}
    if(-not $PackageDirectory) {
        $env:OPTICAL_TEST_PROFILE=$TestProfile
        & ./build-example.ps1 optical_emulators_tester -IpTemplate $BootProfile
        if($LASTEXITCODE){throw 'Optical tester build failed'}
        $PackageDirectory='build/examples'
        $map='build/optical_emulators_tester.map'
    } else {$map=Join-Path $PackageDirectory 'optical_emulators_tester.map'}
    Copy-Item $map ($Prefix+'.map') -Force
    $marker=Select-String -Path $map -Pattern '^\s*0x([0-9A-Fa-f]+)\s+_sat_boot_trace_record$' | Select-Object -First 1
    if(-not $marker){throw 'Missing boot trace symbol'}
    $markerAddress='0x'+$marker.Matches[0].Groups[1].Value
    $args=@{Bios=$Bios;BootMode=$BootMode;Frames=900;BootMarker=$markerAddress;
        DumpWramHigh=($Prefix+'.ram');Out=($Prefix+'.json');
        DiscImage=(Join-Path $PackageDirectory 'optical_emulators_tester.iso');
        ApplicationBinary=(Join-Path $PackageDirectory 'optical_emulators_tester.app.bin');
        Screenshot=@("899:${Prefix}.png")}
    if($CdbRom){$args.CdbRom=$CdbRom}
    if($RunCdTests) {
        @('0 NONE','300 A','310 NONE','380 DOWN','390 NONE','400 A','410 NONE',
          '480 DOWN','490 NONE','500 A','510 NONE','580 DOWN','590 NONE','600 A','610 NONE',
          '680 DOWN','690 NONE','700 A','710 NONE') | Set-Content ($Prefix+'.pad')
        $args.PadScript=$Prefix+'.pad'
    }
    & ./harness/run-harness.ps1 optical_emulators_tester @args
    if($LASTEXITCODE){throw 'Optical emulation failed'}
    $python=if(Test-Path 'C:/Python/python.exe'){'C:/Python/python.exe'}else{'python'}
    $check=@($Prefix)
    if($RunCdTests){$check+='--require-cd-tests'}
    & $python harness/tests/test_optical_emulators_tester.py @check
    if($LASTEXITCODE){throw 'Optical result was not PASS; inspect the analysis report'}
} finally {
    if($null -eq $previous){Remove-Item Env:OPTICAL_TEST_PROFILE -ErrorAction SilentlyContinue}
    else{$env:OPTICAL_TEST_PROFILE=$previous}
    Pop-Location
}
