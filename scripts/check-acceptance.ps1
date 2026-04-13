[CmdletBinding()]
param(
    [ValidateSet('mednafen', 'kronos', 'both')]
    [string]$Emulator = 'both',

    [string]$IsoPath,
    [string]$ReportPath,
    [string]$Msys2Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RequestedRoot = if ($Msys2Root) { $Msys2Root } elseif ($env:LIBSATURN_MSYS2_ROOT) { $env:LIBSATURN_MSYS2_ROOT } else { 'C:\msys64' }

if (-not $IsoPath) {
    $IsoPath = Join-Path $RepoRoot 'build\mvp.iso'
}
if (-not $ReportPath) {
    $ReportPath = Join-Path $RepoRoot 'build\acceptance-report.txt'
}

$IsoPath = [System.IO.Path]::GetFullPath($IsoPath)
$CuePath = [System.IO.Path]::ChangeExtension($IsoPath, '.cue')
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)
$ReportDir = Split-Path -Parent $ReportPath
if ($ReportDir -and -not (Test-Path $ReportDir)) {
    New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null
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
    Write-Host "[check-acceptance] $ScriptCommand"
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

function Ask-Criterion {
    param(
        [Parameter(Mandatory = $true)][string]$EmulatorName,
        [Parameter(Mandatory = $true)][string]$Criterion
    )

    while ($true) {
        $answer = Read-Host "[$EmulatorName] $Criterion (y/n)"
        if ($answer -match '^(?i:y|yes|s|sim)$') {
            return 'PASS'
        }
        if ($answer -match '^(?i:n|no|nao)$') {
            return 'FAIL'
        }
        Write-Host 'Invalid answer. Enter y or n.'
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
        'Run .\scripts\bootstrap-msys2.ps1 host or provide -Msys2Root.'
    ) -join ' '
}

$resolvedRoot = Split-Path -Parent $shellPath
$repoMsysPath = Convert-ToMsysPath -WindowsPath $RepoRoot

Write-Host '[check-acceptance] Pre-checking toolchain and utilities...'
$preCheck = @(
    "cd `"$repoMsysPath`"",
    "command -v sh2eb-elf-gcc >/dev/null",
    "command -v make >/dev/null",
    "(command -v mkisofs >/dev/null || command -v genisoimage >/dev/null || command -v xorrisofs >/dev/null)"
) -join ' && '
Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand $preCheck

Write-Host '[check-acceptance] Clean build to generate ISO...'
Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand "cd `"$repoMsysPath`" && make clean && make all"

if (-not (Test-Path $IsoPath)) {
    throw "ISO not found after build: $IsoPath"
}

$targets = switch ($Emulator) {
    'mednafen' { @('mednafen') }
    'kronos' { @('kronos') }
    default { @('mednafen', 'kronos') }
}

$criteria = @(
    'ISO boots and enters main loop',
    '2D sprite rendering stable for 1800 frames',
    'No input ghost presses for 5 minutes'
)

$mednafenExe = Find-MednafenExe -ResolvedMsysRoot $resolvedRoot
$kronosExe = Join-Path $RepoRoot 'emulators\kronos\kronos.exe'

$results = New-Object System.Collections.Generic.List[object]

foreach ($target in $targets) {
    if ($target -eq 'mednafen') {
        if (-not $mednafenExe) {
            throw "Mednafen not found. Run .\scripts\download-emulators.ps1."
        }
        Write-Host ''
        Write-Host '--- Mednafen Execution ---'
        Write-Host "Suggested command:"
        $mednafenImage = if (Test-Path $CuePath) { $CuePath } else { $IsoPath }
        Write-Host "& `"$mednafenExe`" -force_module ss -ss.region_autodetect 0 -ss.region_default na `"$mednafenImage`""
    }

    if ($target -eq 'kronos') {
        if (-not (Test-Path $kronosExe)) {
            Write-Warning "Kronos not found at $kronosExe. Install manually and run again to validate Kronos."
            foreach ($criterion in $criteria) {
                $results.Add([pscustomobject]@{
                        Emulator  = 'kronos'
                        Criterion = $criterion
                        Status    = 'SKIPPED'
                        Notes     = 'Kronos missing'
                    })
            }
            continue
        }
        Write-Host ''
        Write-Host '--- Kronos Execution ---'
        Write-Host "Suggested command:"
        Write-Host "& `"$kronosExe`" `"$IsoPath`""
    }

    foreach ($criterion in $criteria) {
        $status = Ask-Criterion -EmulatorName $target -Criterion $criterion
        $note = if ($status -eq 'FAIL') { Read-Host "[$target] Note for '$criterion'" } else { '' }
        $results.Add([pscustomobject]@{
                Emulator  = $target
                Criterion = $criterion
                Status    = $status
                Notes     = $note
            })
    }
}

$timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz'
$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add('libsaturn acceptance report')
$reportLines.Add("generated_at=$timestamp")
$reportLines.Add("repo_root=$RepoRoot")
$reportLines.Add("msys2_root=$resolvedRoot")
$reportLines.Add("iso_path=$IsoPath")
$reportLines.Add('')
$reportLines.Add('results:')
foreach ($row in $results) {
    $notesText = if ($row.Notes) { $row.Notes } else { '-' }
    $reportLines.Add("[$($row.Emulator)] $($row.Criterion) => $($row.Status) | notes=$notesText")
}

$hasFailure = $results | Where-Object { $_.Status -eq 'FAIL' }
$hasSkipped = $results | Where-Object { $_.Status -eq 'SKIPPED' }
if ($hasFailure) {
    $overall = 'FAIL'
}
elseif ($hasSkipped) {
    $overall = 'PARTIAL'
}
else {
    $overall = 'PASS'
}
$reportLines.Add('')
$reportLines.Add("overall=$overall")

Set-Content -Path $ReportPath -Value $reportLines -Encoding ASCII
Write-Host ''
Write-Host "[check-acceptance] Report saved at: $ReportPath"
Write-Host "[check-acceptance] Overall result: $overall"

if ($overall -eq 'FAIL') {
    exit 2
}
if ($overall -eq 'PARTIAL') {
    exit 3
}
exit 0
