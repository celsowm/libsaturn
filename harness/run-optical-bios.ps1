[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PackageDirectory,
    [string]$Prefix='harness/build/optical-bios-mednafen',
    [switch]$RunCdTests,
    [switch]$SustainedRead,
    [int]$BootSeconds=8
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$package=[IO.Path]::GetFullPath((Join-Path $repo $PackageDirectory))
$cue=Join-Path $package 'optical_emulators_tester.cue'
if(-not (Test-Path $cue)){throw "Missing packaged disc: $cue"}
$prefixPath=[IO.Path]::GetFullPath((Join-Path $repo $Prefix))
New-Item -ItemType Directory -Force (Split-Path -Parent $prefixPath) | Out-Null
$existing=Get-CimInstance Win32_Process -Filter "Name = 'mednafen.exe'"
if($existing){throw 'A Mednafen instance is already running; close the prior test before starting another.'}
$launcher=Join-Path $repo 'emulators/mednafen/run-mednafen.ps1'
$child=Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList @(
    '-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$launcher+'"'),
    '-GamePath',('"'+$cue+'"'),'-Cart','none') `
    -RedirectStandardOutput ($prefixPath+'.log') -RedirectStandardError ($prefixPath+'.errors.log')
try {
    $limit=[DateTime]::UtcNow.AddSeconds(15)
    $emulator=$null
    while([DateTime]::UtcNow -lt $limit) {
        $emulator=Get-CimInstance Win32_Process -Filter "Name = 'mednafen.exe'" |
            Where-Object {$_.CommandLine -like ('*'+$cue+'*')} | Select-Object -First 1
        if($emulator -and (Get-Process -Id $emulator.ProcessId).MainWindowHandle -ne 0){break}
        Start-Sleep -Milliseconds 200
    }
    if(-not $emulator){throw 'Mednafen window did not open'}
    $emulatorId=$emulator.ProcessId
    Start-Sleep -Seconds $BootSeconds
    & (Join-Path $repo 'tools/capture-mednafen.ps1') -ProcessId $emulatorId -Output ($prefixPath+'-idle.png')
    if($RunCdTests) {
        & (Join-Path $repo 'tools/send-mednafen-key.ps1') -ProcessId $emulatorId -Key Enter -HoldMilliseconds 250
        Start-Sleep -Milliseconds 500
        foreach($step in 0..4) {
            if($step){& (Join-Path $repo 'tools/send-mednafen-key.ps1') -ProcessId $emulatorId -Key Down -HoldMilliseconds 250}
            & (Join-Path $repo 'tools/send-mednafen-key.ps1') -ProcessId $emulatorId -Key J -HoldMilliseconds 250
            Start-Sleep -Seconds 2
            & (Join-Path $repo 'tools/capture-mednafen.ps1') -ProcessId $emulatorId -Output ($prefixPath+"-step$step.png")
        }
        if($SustainedRead) {
            & (Join-Path $repo 'tools/send-mednafen-key.ps1') -ProcessId $emulatorId -Key Down -HoldMilliseconds 250
            & (Join-Path $repo 'tools/send-mednafen-key.ps1') -ProcessId $emulatorId -Key J -HoldMilliseconds 250
            Start-Sleep -Seconds 35
            & (Join-Path $repo 'tools/capture-mednafen.ps1') -ProcessId $emulatorId -Output ($prefixPath+'-sustained.png')
        }
        & (Join-Path $repo 'tools/send-mednafen-key.ps1') -ProcessId $emulatorId -Key L -HoldMilliseconds 250
        & (Join-Path $repo 'tools/capture-mednafen.ps1') -ProcessId $emulatorId -Output ($prefixPath+'-registers.png')
    }
    $report=[ordered]@{schema_version=1;mode='bios';engine='mednafen';injected=$false;
        image_sha256=(Get-FileHash (Join-Path $package 'optical_emulators_tester.iso') -Algorithm SHA256).Hash.ToLower();
        cd_tests_requested=[bool]$RunCdTests;sustained_read_requested=[bool]$SustainedRead;
        status='CAPTURED_REQUIRES_VISUAL_REVIEW';pc='unknown';physical_ode='pending'}
    $report | ConvertTo-Json | Set-Content ($prefixPath+'.json') -Encoding UTF8
    Write-Host '[optical-bios] Captures saved; inspect them before recording a boot or CD pass.'
} finally {
    if($emulator){Stop-Process -Id $emulator.ProcessId -ErrorAction SilentlyContinue}
    $child.WaitForExit(5000) | Out-Null
    if(-not $child.HasExited){Stop-Process -Id $child.Id -ErrorAction SilentlyContinue}
}
