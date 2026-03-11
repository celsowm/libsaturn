[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Example,

    [string]$Msys2Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = $PSScriptRoot
$RequestedRoot = if ($Msys2Root) { $Msys2Root } elseif ($env:LIBSATURN_MSYS2_ROOT) { $env:LIBSATURN_MSYS2_ROOT } else { 'C:\msys64' }

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
    & $ShellPath -defterm -no-start -ucrt64 -here -c $wrappedCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Comando no MSYS2 falhou (codigo $LASTEXITCODE): $ScriptCommand"
    }
}

$normalizedExample = $Example.Trim().Trim('\', '/')
if ($normalizedExample -match '^(examples[\\/])(.+)$') {
    $normalizedExample = $matches[2]
}

$exampleMain = Join-Path $RepoRoot ("examples\{0}\main.c" -f $normalizedExample)
if (-not (Test-Path $exampleMain)) {
    throw "Example nao encontrado: $exampleMain"
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
    throw 'MSYS2 nao encontrado. Informe -Msys2Root ou ajuste LIBSATURN_MSYS2_ROOT.'
}

$repoMsysPath = Convert-ToMsysPath -WindowsPath $RepoRoot
$exampleMainRelative = "examples/$normalizedExample/main.c"

Write-Host "[build-example] Buildando example: $normalizedExample"
Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand "export PATH=`"`$HOME/saturn-tools/bin:`$PATH`" && cd `"$repoMsysPath`" && make clean && make APP_C_SRCS=`"$exampleMainRelative`" all"

$safeName = ($normalizedExample -replace '[\\/]', '_')
$exampleBuildDir = Join-Path $RepoRoot 'build\examples'
if (-not (Test-Path $exampleBuildDir)) {
    New-Item -ItemType Directory -Path $exampleBuildDir -Force | Out-Null
}

$isoSource = Join-Path $RepoRoot 'build\mvp.iso'
$cueSource = Join-Path $RepoRoot 'build\mvp.cue'
$binSource = Join-Path $RepoRoot 'build\mvp.bin'
$elfSource = Join-Path $RepoRoot 'build\mvp.elf'

$isoTarget = Join-Path $exampleBuildDir "$safeName.iso"
$cueTarget = Join-Path $exampleBuildDir "$safeName.cue"
$binTarget = Join-Path $exampleBuildDir "$safeName.bin"
$elfTarget = Join-Path $exampleBuildDir "$safeName.elf"

Copy-Item $isoSource $isoTarget -Force
Copy-Item $cueSource $cueTarget -Force
Copy-Item $binSource $binTarget -Force
Copy-Item $elfSource $elfTarget -Force

# Update the CUE to reference the renamed ISO
(Get-Content $cueTarget) -replace 'mvp\.iso', "$safeName.iso" | Set-Content $cueTarget

Write-Host "[build-example] Artefatos:"
Write-Host "  $isoTarget"
Write-Host "  $cueTarget"
Write-Host "  $binTarget"
Write-Host "  $elfTarget"
