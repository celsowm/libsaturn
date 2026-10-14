[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$DiscImage,
    [ValidateRange(10,3600)][int]$Seconds=300,
    [string]$Prefix='harness/build/disc-stability'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$image=[IO.Path]::GetFullPath((Join-Path $repo $DiscImage))
$prefixPath=[IO.Path]::GetFullPath((Join-Path $repo $Prefix))
if(Get-Process mednafen -ErrorAction SilentlyContinue){throw 'Another Mednafen instance is running'}
New-Item -ItemType Directory -Force (Split-Path -Parent $prefixPath) | Out-Null
$launcher=Join-Path $repo 'emulators/mednafen/run-mednafen.ps1'
$child=Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList @(
    '-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$launcher+'"'),
    '-GamePath',('"'+$image+'"'),'-Cart','none') `
    -RedirectStandardOutput ($prefixPath+'.log') -RedirectStandardError ($prefixPath+'.errors.log')
$observations=@()
$emulator=$null
try {
    $limit=[DateTime]::UtcNow.AddSeconds(15)
    while([DateTime]::UtcNow -lt $limit) {
        $emulator=Get-CimInstance Win32_Process -Filter "Name = 'mednafen.exe'" |
            Where-Object {$_.CommandLine -like ('*'+$image+'*')} | Select-Object -First 1
        if($emulator -and (Get-Process -Id $emulator.ProcessId).MainWindowHandle -ne 0){break}
        Start-Sleep -Milliseconds 200
    }
    if(-not $emulator){throw 'Mednafen did not open the requested disc'}
    Start-Sleep -Seconds 8
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $nextCapture=0
    while($watch.Elapsed.TotalSeconds -lt $Seconds) {
        $process=Get-Process -Id $emulator.ProcessId -ErrorAction Stop
        if($watch.Elapsed.TotalSeconds -ge $nextCapture) {
            $shot=$prefixPath+('-{0:000}.png' -f [int]$watch.Elapsed.TotalSeconds)
            & (Join-Path $repo 'tools/capture-mednafen.ps1') -ProcessId $process.Id -Output $shot
            $observations+=@{elapsed_seconds=$watch.Elapsed.TotalSeconds;screenshot=[IO.Path]::GetFileName($shot);
                sha256=(Get-FileHash $shot -Algorithm SHA256).Hash.ToLower()}
            Write-Host ('[stability] Process alive, capture at {0:N0} seconds' -f $watch.Elapsed.TotalSeconds)
            $nextCapture+=60
        }
        Start-Sleep -Seconds 10
    }
    & (Join-Path $repo 'tools/capture-mednafen.ps1') -ProcessId $emulator.ProcessId -Output ($prefixPath+'-final.png')
    [ordered]@{schema_version=1;mode='bios';engine='mednafen';injected=$false;
        status='CAPTURED_REQUIRES_VISUAL_REVIEW';seconds=$watch.Elapsed.TotalSeconds;
        image_sha256=(Get-FileHash $image -Algorithm SHA256).Hash.ToLower();observations=$observations;
        physical_ode='pending'} | ConvertTo-Json -Depth 5 | Set-Content ($prefixPath+'.json') -Encoding UTF8
} finally {
    if($emulator){Stop-Process -Id $emulator.ProcessId -ErrorAction SilentlyContinue}
    $child.WaitForExit(5000) | Out-Null
    if(-not $child.HasExited){Stop-Process -Id $child.Id -ErrorAction SilentlyContinue}
}
