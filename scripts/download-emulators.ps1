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
        throw "Comando no MSYS2 falhou (codigo $LASTEXITCODE): $ScriptCommand"
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
        'MSYS2 nao encontrado.'
        'Execute .\scripts\bootstrap-msys2.ps1 host primeiro ou informe -Msys2Root.'
    ) -join ' '
}

$resolvedRoot = Split-Path -Parent $shellPath
$repoMsysPath = Convert-ToMsysPath -WindowsPath $RepoRoot

try {
    Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand "cd `"$repoMsysPath`" && pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-mednafen"
}
catch {
    Write-Warning "[download-emulators] Pacote mednafen nao disponivel no MSYS2 UCRT64 atual. Tentando winget..."
}

$mednafenExe = Find-MednafenExe -ResolvedMsysRoot $resolvedRoot
if (-not $mednafenExe) {
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw @(
            'Mednafen nao encontrado via pacman e winget nao esta disponivel.'
            'Instale Mednafen manualmente e rode este script novamente.'
        ) -join ' '
    }

    Write-Host '[download-emulators] Instalando Mednafen via winget (MednafenTeam.Mednafen)...'
    & $winget.Source install --id MednafenTeam.Mednafen -e --accept-package-agreements --accept-source-agreements --disable-interactivity
    if ($LASTEXITCODE -ne 0) {
        throw "Falha ao instalar Mednafen via winget (codigo $LASTEXITCODE)."
    }

    $mednafenExe = Find-MednafenExe -ResolvedMsysRoot $resolvedRoot
    if (-not $mednafenExe) {
        throw 'Mednafen foi instalado, mas o executavel nao foi localizado automaticamente.'
    }
}
$mednafenExeLiteral = $mednafenExe -replace "'", "''"

$mednafenLauncher = Join-Path $MednafenDir 'run-mednafen.ps1'
$mednafenLauncherContent = @'
[CmdletBinding()]
param(
    [string]$GamePath,
    [ValidateSet('auto', 'jp', 'na', 'eu')]
    [string]$Region = 'na',
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
        Write-Host "[run-mednafen] BIOS copiada para: $TargetPath"
        return $true
    }

    return $false
}

if (-not (Test-Path $exePath)) {
    throw "Mednafen nao encontrado: $exePath"
}

$targetImage = if ($GamePath) {
    [System.IO.Path]::GetFullPath($GamePath)
} elseif (Test-Path $defaultCue) {
    $defaultCue
} else {
    $defaultIso
}
if (-not (Test-Path $targetImage)) {
    throw "Imagem do disco nao encontrada: $targetImage"
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
    throw "BIOS do Mednafen ausente: $($missingFirmware -join ', '). Rode: .\scripts\download-bios.ps1"
}

$regionArgs = @()
if ($Region -ne 'auto') {
    $regionArgs = @('-ss.region_autodetect', '0', '-ss.region_default', $Region)
}
Write-Host "[run-mednafen] Regiao: $Region"
Write-Host "[run-mednafen] Executando: $exePath -force_module ss $targetImage"
& $exePath '-force_module' 'ss' @regionArgs $targetImage @ExtraArgs
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
    throw "Kronos nao encontrado em $exePath. Baixe o release e copie kronos.exe para esta pasta."
}

$targetIso = if ($GamePath) { [System.IO.Path]::GetFullPath($GamePath) } else { $defaultIso }
if (-not (Test-Path $targetIso)) {
    throw "ISO nao encontrada: $targetIso"
}

Write-Host "[run-kronos] Executando: $exePath $targetIso"
& $exePath $targetIso @ExtraArgs
exit $LASTEXITCODE
'@
Set-Content -Path $kronosLauncher -Value $kronosLauncherContent -Encoding ASCII

$kronosExe = Join-Path $KronosDir 'kronos.exe'
if (Test-Path $kronosExe) {
    Write-Host "[download-emulators] Kronos detectado: $kronosExe"
}
else {
    Write-Warning "[download-emulators] Kronos nao encontrado em $KronosDir. Instale manualmente conforme emulators/README.md."
}

Write-Host "[download-emulators] Mednafen instalado e launchers atualizados."
