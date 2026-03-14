[CmdletBinding()]
param(
    [string]$Msys2Root,
    [string]$ReportPath,
    [string]$MatrixCsvPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RequestedRoot = if ($Msys2Root) { $Msys2Root } elseif ($env:LIBSATURN_MSYS2_ROOT) { $env:LIBSATURN_MSYS2_ROOT } else { 'C:\msys64' }

if (-not $ReportPath) {
    $ReportPath = Join-Path $RepoRoot 'build\boot-matrix-build-report.txt'
}
if (-not $MatrixCsvPath) {
    $MatrixCsvPath = Join-Path $RepoRoot 'build\boot-matrix-manual-results.csv'
}

$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)
$MatrixCsvPath = [System.IO.Path]::GetFullPath($MatrixCsvPath)

$reportDir = Split-Path -Parent $ReportPath
if ($reportDir -and -not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
}

$matrixDir = Split-Path -Parent $MatrixCsvPath
if ($matrixDir -and -not (Test-Path $matrixDir)) {
    New-Item -ItemType Directory -Path $matrixDir -Force | Out-Null
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
    Write-Host "[build-boot-matrix] $ScriptCommand"
    & $ShellPath -defterm -no-start -ucrt64 -here -c $wrappedCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Comando no MSYS2 falhou (codigo $LASTEXITCODE): $ScriptCommand"
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
    throw 'MSYS2 nao encontrado. Informe -Msys2Root ou ajuste LIBSATURN_MSYS2_ROOT.'
}

$repoMsysPath = Convert-ToMsysPath -WindowsPath $RepoRoot

$combinations = @(
    @{ Ip = 'current' },
    @{ Ip = 'safe' }
)

$timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz'
$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add('libsaturn boot matrix build report')
$reportLines.Add("generated_at=$timestamp")
$reportLines.Add("repo_root=$RepoRoot")
$reportLines.Add("msys2_shell=$shellPath")
$reportLines.Add('')

foreach ($combo in $combinations) {
    $ip = $combo.Ip
    $variantName = "mvp-$ip"
    $variantBuildRel = "build/matrix/$variantName"
    $variantIsoRootRel = "iso_root_matrix/$variantName"
    $variantIpRel = "$variantBuildRel/ip.bin"
    $variantIso = Join-Path $RepoRoot ("build\matrix\{0}\{0}.iso" -f $variantName)
    $variantCue = Join-Path $RepoRoot ("build\matrix\{0}\{0}.cue" -f $variantName)

    Write-Host ''
    Write-Host "[build-boot-matrix] Buildando variante $variantName"

    $buildCommand = "export PATH=`"`$HOME/saturn-tools/bin:`$PATH`" && cd `"$repoMsysPath`" && make clean BUILD_DIR=$variantBuildRel ISO_ROOT=$variantIsoRootRel IP_BIN=$variantIpRel IP_PROFILE=$ip && make all BUILD_DIR=$variantBuildRel ISO_ROOT=$variantIsoRootRel IP_BIN=$variantIpRel IP_PROFILE=$ip"
    Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand $buildCommand

    if (-not (Test-Path $variantIso)) {
        throw "ISO da variante nao encontrada: $variantIso"
    }
    if (-not (Test-Path $variantCue)) {
        throw "CUE da variante nao encontrada: $variantCue"
    }

    $reportLines.Add("[$variantName]")
    $reportLines.Add("iso=$variantIso")
    $reportLines.Add("cue=$variantCue")
    $reportLines.Add("mednafen_cmd=powershell -ExecutionPolicy Bypass -File .\\emulators\\mednafen\\run-mednafen.ps1 -GamePath `"$variantCue`" -Region na")
    $reportLines.Add('')
}

$csvLines = @(
    'variant,vdp_profile,ip_profile,stage_max,scene_ok,input_ok,notes',
    'mvp-current,main,current,unknown,false,false,',
    'mvp-safe,main,safe,unknown,false,false,'
)

Set-Content -Path $ReportPath -Value $reportLines -Encoding ASCII
Set-Content -Path $MatrixCsvPath -Value $csvLines -Encoding ASCII

Write-Host ''
Write-Host "[build-boot-matrix] Relatorio: $ReportPath"
Write-Host "[build-boot-matrix] Planilha de resultados manuais: $MatrixCsvPath"
Write-Host '[build-boot-matrix] Preencha scene_ok/input_ok/stage_max e rode evaluate-boot-matrix.ps1.'
