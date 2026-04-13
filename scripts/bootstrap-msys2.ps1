[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('host', 'full', 'smoke', 'acceptance')]
    [string]$Command = 'host',

    [string]$Msys2Root,
    [switch]$NoInstall,
    [string]$LogPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$CheckAcceptanceScript = Join-Path $PSScriptRoot 'check-acceptance.ps1'
$RequestedRoot = if ($Msys2Root) { $Msys2Root } elseif ($env:LIBSATURN_MSYS2_ROOT) { $env:LIBSATURN_MSYS2_ROOT } else { 'C:\msys64' }

if ($LogPath) {
    $LogPath = [System.IO.Path]::GetFullPath($LogPath)
    $LogDir = Split-Path -Parent $LogPath
    if ($LogDir -and -not (Test-Path $LogDir)) {
        New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
    }
    New-Item -ItemType File -Path $LogPath -Force | Out-Null
}

function Write-Log {
    param([string]$Message)
    $line = "[bootstrap-msys2] $Message"
    Write-Host $line
    if ($LogPath) {
        Add-Content -Path $LogPath -Value $line
    }
}

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

    $seen = New-Object System.Collections.Generic.HashSet[string]
    foreach ($candidate in $RootCandidates) {
        if (-not $candidate) {
            continue
        }

        $normalized = [System.IO.Path]::GetFullPath($candidate)
        if (-not $seen.Add($normalized)) {
            continue
        }

        $shellPath = Join-Path $normalized 'msys2_shell.cmd'
        if (Test-Path $shellPath) {
            return $shellPath
        }
    }

    return $null
}

function Resolve-Msys2ShellPath {
    param(
        [Parameter(Mandatory = $true)][string]$RootHint,
        [switch]$AllowInstall
    )

    $commonRoots = @(
        $RootHint,
        $env:LIBSATURN_MSYS2_ROOT,
        'C:\msys64',
        'C:\tools\msys64',
        (Join-Path $env:LOCALAPPDATA 'msys64')
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    $shellPath = Find-Msys2Shell -RootCandidates $commonRoots
    if ($shellPath) {
        return $shellPath
    }

    if (-not $AllowInstall) {
        throw @(
            "MSYS2 not found."
            "Install manually at C:\msys64 or use -Msys2Root."
        ) -join ' '
    }

    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw @(
            "MSYS2 not found and winget is not available."
            "Install manually: https://www.msys2.org/"
            "Then run this script again with -Msys2Root if needed."
        ) -join ' '
    }

    Write-Log 'MSYS2 not found. Attempting to install via winget (MSYS2.MSYS2)...'
    & $winget.Source install --id MSYS2.MSYS2 -e --accept-package-agreements --accept-source-agreements --disable-interactivity
    if ($LASTEXITCODE -ne 0) {
        throw @(
            "Automatic installation via winget failed (code $LASTEXITCODE)."
            "Install manually: https://www.msys2.org/"
        ) -join ' '
    }

    $shellPath = Find-Msys2Shell -RootCandidates $commonRoots
    if (-not $shellPath) {
        throw @(
            "MSYS2 was installed, but msys2_shell.cmd was not located."
            "Check installation and provide path with -Msys2Root."
        ) -join ' '
    }

    return $shellPath
}

function Invoke-Msys2Command {
    param(
        [Parameter(Mandatory = $true)][string]$ShellPath,
        [Parameter(Mandatory = $true)][string]$ScriptCommand
    )

    $wrappedCommand = "rm -f /var/lib/pacman/db.lck >/dev/null 2>&1 || true; $ScriptCommand"
    Write-Log "Executing in MSYS2: $ScriptCommand"
    & $ShellPath -defterm -no-start -ucrt64 -here -c $wrappedCommand
    if ($LASTEXITCODE -ne 0) {
        throw "MSYS2 command failed (code $LASTEXITCODE): $ScriptCommand"
    }
}

function Invoke-SetupMsys2 {
    param(
        [Parameter(Mandatory = $true)][string]$ShellPathValue,
        [Parameter(Mandatory = $true)][string]$RepoMsysPathValue
    )

    $setupCommand = "cd `"$RepoMsysPathValue`" && bash scripts/setup-msys2.sh"
    $maxAttempts = 5

    function Wait-ForPacmanExit {
        param([int]$TimeoutSeconds = 180)
        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        while ((Get-Date) -lt $deadline) {
            $activePacman = Get-Process -Name pacman -ErrorAction SilentlyContinue
            if (-not $activePacman) {
                return
            }
            Write-Log 'pacman running detected. Waiting for lock release...'
            Start-Sleep -Seconds 3
        }
        throw 'Timeout waiting for pacman.exe to finish.'
    }

    for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
        Wait-ForPacmanExit
        try {
            Invoke-Msys2Command -ShellPath $ShellPathValue -ScriptCommand $setupCommand
            if ($attempt -gt 1) {
                Write-Log "setup-msys2.sh completed on attempt $attempt."
            }
            return
        }
        catch {
            if ($attempt -ge $maxAttempts) {
                throw
            }
            Write-Log "setup-msys2.sh interrupted (attempt $attempt). Re-executing automatically..."
            Start-Sleep -Seconds 6
        }
    }
}

$allowInstall = -not $NoInstall.IsPresent
$shellPath = Resolve-Msys2ShellPath -RootHint $RequestedRoot -AllowInstall:$allowInstall
$resolvedRoot = Split-Path -Parent $shellPath
$repoMsysPath = Convert-ToMsysPath -WindowsPath $RepoRoot

Write-Log "MSYS2 detected at: $resolvedRoot"
Write-Log "Repository: $RepoRoot"

switch ($Command) {
    'host' {
        Invoke-SetupMsys2 -ShellPathValue $shellPath -RepoMsysPathValue $repoMsysPath
    }
    'full' {
        Invoke-SetupMsys2 -ShellPathValue $shellPath -RepoMsysPathValue $repoMsysPath
        Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand "cd `"$repoMsysPath`" && bash scripts/build-toolchain.sh"
    }
    'smoke' {
        Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand "cd `"$repoMsysPath`" && bash scripts/smoke-build.sh"
    }
    'acceptance' {
        if (-not (Test-Path $CheckAcceptanceScript)) {
            throw "Acceptance script not found: $CheckAcceptanceScript"
        }

        Write-Log 'Preparing host environment before acceptance checklist...'
        Invoke-SetupMsys2 -ShellPathValue $shellPath -RepoMsysPathValue $repoMsysPath
        & $CheckAcceptanceScript -Emulator both -Msys2Root $resolvedRoot
        if ($LASTEXITCODE -ne 0) {
            throw "Acceptance checklist failed (code $LASTEXITCODE)."
        }
    }
}

Write-Log "Flow completed successfully: $Command"
