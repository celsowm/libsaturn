[CmdletBinding()]
param(
    [string]$Msys2Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RequestedRoot = if ($Msys2Root) { $Msys2Root } elseif ($env:LIBSATURN_MSYS2_ROOT) { $env:LIBSATURN_MSYS2_ROOT } else { 'C:\msys64' }

$EmulatorsRoot = Join-Path $RepoRoot 'emulators'
$MednafenDir = Join-Path $EmulatorsRoot 'mednafen'
$KronosDir = Join-Path $EmulatorsRoot 'kronos'

function Convert-ToMsysPath {
    param([Parameter(Mandatory = $true)][string]$WindowsPath)
    if ($WindowsPath -match '^([A-Za-z]):\\(.*)$') {
        $drive = $matches[1].ToLowerInvariant()
        $rest = ($matches[2] -replace '\\', '/')
        return "/$drive/$rest"
    }
    return ($WindowsPath -replace '\\', '/')
}

function Find-Msys2Shell {
    param(
        [Parameter(Mandatory = $true)][string[]]$RootCandidates
    )

    foreach ($candidate in $RootCandidates) {
        if (-not $candidate) {
            continue
        }
        $shellPath = Join-Path ([System.IO.Path]::GetFullPath($candidate)) 'msys2_shell.cmd'
        if (Test-Path $shellPath) {
            return $shellPath
        }
    }
    return $null
}

function Invoke-Msys2Command {
    param(
        [Parameter(Mandatory = $true)][string]$ShellPath,
        [Parameter(Mandatory = $true)][string]$ScriptCommand
    )

    $wrappedCommand = "rm -f /var/lib/pacman/db.lck >/dev/null 2>&1 || true; $ScriptCommand"
    Write-Host "[download-emulators] $ScriptCommand"
    & $ShellPath -defterm -no-start -ucrt64 -here -c $wrappedCommand
    if ($LASTEXITCODE -ne 0) {
        throw "MSYS2 command failed (code $LASTEXITCODE): $ScriptCommand"
    }
}

function Find-MednafenExe {
    param(
        [Parameter(Mandatory = $true)][string]$ResolvedMsysRoot
    )

    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    $wingetPackagesRoot = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    $candidates = @(
        (Join-Path $ResolvedMsysRoot 'ucrt64\bin\mednafen.exe'),
        (Join-Path $env:ProgramFiles 'Mednafen\mednafen.exe'),
        (Join-Path $programFilesX86 'Mednafen\mednafen.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Mednafen\mednafen.exe')
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    if (Test-Path $wingetPackagesRoot) {
        $wingetRoots = Get-ChildItem -Path $wingetPackagesRoot -Directory -Filter 'MednafenTeam.Mednafen_*' -ErrorAction SilentlyContinue
        foreach ($root in $wingetRoots) {
            $candidates += (Join-Path $root.FullName 'mednafen.exe')
        }
    }

    $cmd = Get-Command mednafen.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        $candidates += $cmd.Source
    }

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    return $null
}

foreach ($dir in @($EmulatorsRoot, $MednafenDir, $KronosDir)) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
}

$rootCandidates = @(
    $RequestedRoot,
    $env:LIBSATURN_MSYS2_ROOT,
    'C:\msys64',
    'C:\tools\msys64',
    (Join-Path $env:LOCALAPPDATA 'msys64')
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

$shellPath = Find-Msys2Shell -RootCandidates $rootCandidates

if (-not $shellPath) {
    throw @(
        'MSYS2 not found.'
        'Run .\scripts\bootstrap-msys2.ps1 host first or provide -Msys2Root.'
    ) -join ' '
}

$resolvedRoot = Split-Path -Parent $shellPath
$repoMsysPath = Convert-ToMsysPath -WindowsPath $RepoRoot

try {
    Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand "cd `"$repoMsysPath`" && pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-mednafen"
}
catch {
    Write-Warning "[download-emulators] Mednafen package not available in current MSYS2 UCRT64. Trying winget..."
}

$mednafenExe = Find-MednafenExe -ResolvedMsysRoot $resolvedRoot
if (-not $mednafenExe) {
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw @(
            'Mednafen not found via pacman and winget is not available.'
            'Install Mednafen manually and run this script again.'
        ) -join ' '
    }

    Write-Host '[download-emulators] Installing Mednafen via winget (MednafenTeam.Mednafen)...'
    & $winget.Source install --id MednafenTeam.Mednafen -e --accept-package-agreements --accept-source-agreements --disable-interactivity
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install Mednafen via winget (code $LASTEXITCODE)."
    }

    $mednafenExe = Find-MednafenExe -ResolvedMsysRoot $resolvedRoot
    if (-not $mednafenExe) {
        throw 'Mednafen was installed, but the executable could not be located automatically.'
    }
}
$mednafenExeLiteral = $mednafenExe -replace "'", "''"

$mednafenLauncher = Join-Path $MednafenDir 'run-mednafen.ps1'
$mednafenLauncherContent = @'
[CmdletBinding()]
param(
    [string]$GamePath,
    [ValidateSet('auto', 'jp', 'na', 'eu')]
    [string]$Region = 'auto',
    [string[]]$ExtraArgs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$defaultCue = Join-Path $repoRoot 'build\mvp.cue'
$defaultIso = Join-Path $repoRoot 'build\mvp.iso'
$exePath = '__MEDNAFEN_EXE__'

function Copy-BiosIfPresent {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$TargetPath
    )

    if (Test-Path $SourcePath) {
        Copy-Item $SourcePath $TargetPath -Force
        Write-Host "[run-mednafen] BIOS copied to: $TargetPath"
        return $true
    }

    return $false
}

if (-not (Test-Path $exePath)) {
    throw "Mednafen not found: $exePath"
}

$targetImage = if ($GamePath) {
    [System.IO.Path]::GetFullPath($GamePath)
} elseif (Test-Path $defaultCue) {
    $defaultCue
} else {
    $defaultIso
}
if (-not (Test-Path $targetImage)) {
    throw "Disc image not found: $targetImage"
}

$emuDir = Split-Path $exePath
$firmwareDir = Join-Path $emuDir 'firmware'
$jpFirmware = Join-Path $firmwareDir 'sega_101.bin'
$nonJpFirmware = Join-Path $firmwareDir 'mpr-17933.bin'

if (-not (Test-Path $jpFirmware) -or -not (Test-Path $nonJpFirmware)) {
    $biosDir = Join-Path $repoRoot 'bios'
    if (-not (Test-Path $firmwareDir)) {
        New-Item -ItemType Directory -Path $firmwareDir -Force | Out-Null
    }

    Copy-BiosIfPresent -SourcePath (Join-Path $biosDir 'saturn_bios_jp.bin') -TargetPath $jpFirmware | Out-Null
    $copiedNonJp = Copy-BiosIfPresent -SourcePath (Join-Path $biosDir 'saturn_bios_us.bin') -TargetPath $nonJpFirmware
    if (-not $copiedNonJp) {
        Copy-BiosIfPresent -SourcePath (Join-Path $biosDir 'saturn_bios_eu.bin') -TargetPath $nonJpFirmware | Out-Null
    }
}

$missingFirmware = @()
if (-not (Test-Path $jpFirmware)) {
    $missingFirmware += 'sega_101.bin'
}
if (-not (Test-Path $nonJpFirmware)) {
    $missingFirmware += 'mpr-17933.bin'
}
if ($missingFirmware.Count -gt 0) {
    throw "Mednafen BIOS missing: $($missingFirmware -join ', '). Run: .\scripts\download-bios.ps1"
}

$regionArgs = @()
if ($Region -eq 'auto') {
    $regionArgs = @('-ss.region_autodetect', '1', '-ss.region_default', 'na')
}
else {
    $regionArgs = @('-ss.region_autodetect', '0', '-ss.region_default', $Region)
}
 $videoArgs = @('-ss.h_overscan', '0', '-ss.videoip', '0')
Write-Host "[run-mednafen] Region: $Region"
Write-Host "[run-mednafen] Region args: $($regionArgs -join ' ')"
Write-Host "[run-mednafen] Video args: $($videoArgs -join ' ')"
Write-Host "[run-mednafen] Executing: $exePath -force_module ss $($regionArgs -join ' ') $($videoArgs -join ' ') $targetImage"
& $exePath '-force_module' 'ss' @regionArgs @videoArgs $targetImage @ExtraArgs
exit $LASTEXITCODE
'@
$mednafenLauncherContent = $mednafenLauncherContent.Replace('__MEDNAFEN_EXE__', $mednafenExeLiteral)
Set-Content -Path $mednafenLauncher -Value $mednafenLauncherContent -Encoding ASCII

$kronosLauncher = Join-Path $KronosDir 'run-kronos.ps1'
$kronosLauncherContent = @'
[CmdletBinding()]
param(
    [string]$GamePath,
    [string[]]$ExtraArgs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$defaultIso = Join-Path $repoRoot 'build\mvp.iso'
$exePath = Join-Path $PSScriptRoot 'kronos.exe'

if (-not (Test-Path $exePath)) {
    throw "Kronos not found at $exePath. Download the release and copy kronos.exe to this folder."
}

$targetIso = if ($GamePath) { [System.IO.Path]::GetFullPath($GamePath) } else { $defaultIso }
if (-not (Test-Path $targetIso)) {
    throw "ISO not found: $targetIso"
}

Write-Host "[run-kronos] Executing: $exePath $targetIso"
& $exePath $targetIso @ExtraArgs
exit $LASTEXITCODE
'@
Set-Content -Path $kronosLauncher -Value $kronosLauncherContent -Encoding ASCII

$kronosExe = Join-Path $KronosDir 'kronos.exe'
if (Test-Path $kronosExe) {
    Write-Host "[download-emulators] Kronos detected: $kronosExe"
}
else {
    Write-Warning "[download-emulators] Kronos not found at $KronosDir. Install manually as per emulators/README.md."
}

Write-Host "[download-emulators] Mednafen installed and launchers updated."
