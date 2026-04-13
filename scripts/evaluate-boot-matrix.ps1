[CmdletBinding()]
param(
    [string]$MatrixCsvPath,
    [string]$ReportPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

if (-not $MatrixCsvPath) {
    $MatrixCsvPath = Join-Path $RepoRoot 'build\boot-matrix-manual-results.csv'
}
if (-not $ReportPath) {
    $ReportPath = Join-Path $RepoRoot 'build\boot-matrix-decision.txt'
}

$MatrixCsvPath = [System.IO.Path]::GetFullPath($MatrixCsvPath)
$ReportPath = [System.IO.Path]::GetFullPath($ReportPath)

if (-not (Test-Path $MatrixCsvPath)) {
    throw "Matrix file not found: $MatrixCsvPath"
}

$reportDir = Split-Path -Parent $ReportPath
if ($reportDir -and -not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
}

function Parse-Bool {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $false
    }
    $normalized = $Value.Trim().ToLowerInvariant()
    return @('1', 'true', 'yes', 'y', 'sim', 's', 'pass') -contains $normalized
}

$rows = Import-Csv -Path $MatrixCsvPath
if (-not $rows -or $rows.Count -eq 0) {
    throw "Empty matrix: $MatrixCsvPath"
}

$requiredKeys = @('main|current', 'main|safe')
$statusByKey = @{}

foreach ($row in $rows) {
    $vdp = $row.vdp_profile
    $ip = $row.ip_profile
    if ([string]::IsNullOrWhiteSpace($vdp) -or [string]::IsNullOrWhiteSpace($ip)) {
        continue
    }

    $key = "$($vdp.Trim().ToLowerInvariant())|$($ip.Trim().ToLowerInvariant())"
    $sceneOk = Parse-Bool -Value $row.scene_ok
    $inputOk = Parse-Bool -Value $row.input_ok
    $statusByKey[$key] = [pscustomobject]@{
        Variant  = $row.variant
        Key      = $key
        StageMax = $row.stage_max
        SceneOk  = $sceneOk
        InputOk  = $inputOk
        Pass     = ($sceneOk -and $inputOk)
        Notes    = $row.notes
    }
}

foreach ($requiredKey in $requiredKeys) {
    if (-not $statusByKey.ContainsKey($requiredKey)) {
        throw "Incomplete matrix: missing row for '$requiredKey'"
    }
}

$mainCurrent = $statusByKey['main|current'].Pass
$mainSafe = $statusByKey['main|safe'].Pass

$decision = ''
$nextAction = ''

if ($mainCurrent) {
    $decision = 'keep_current'
    $nextAction = 'Keep IP current as default.'
}
elseif ($mainSafe) {
    $decision = 'fix_ip_safe'
    $nextAction = 'Apply IP safe (first_read_size=0) as default.'
}
elseif (-not ($mainCurrent -or $mainSafe)) {
    $decision = 'round2_startup_ip_stub'
    $nextAction = 'Start second round focused on startup/IP stub without touching gameplay.'
}
else {
    $decision = 'inconclusive'
    $nextAction = 'Mixed results; review notes/stage_max and repeat conflicting cases.'
}

$timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz'
$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add('libsaturn boot matrix decision report')
$reportLines.Add("generated_at=$timestamp")
$reportLines.Add("matrix_csv=$MatrixCsvPath")
$reportLines.Add('')
$reportLines.Add('results:')

foreach ($key in $requiredKeys) {
    $row = $statusByKey[$key]
    $notes = if ($row.Notes) { $row.Notes } else { '-' }
    $reportLines.Add("$key pass=$($row.Pass) stage_max=$($row.StageMax) scene_ok=$($row.SceneOk) input_ok=$($row.InputOk) notes=$notes")
}

$reportLines.Add('')
$reportLines.Add("decision=$decision")
$reportLines.Add("next_action=$nextAction")

Set-Content -Path $ReportPath -Value $reportLines -Encoding ASCII

Write-Host "[evaluate-boot-matrix] Report: $ReportPath"
Write-Host "[evaluate-boot-matrix] decision=$decision"
Write-Host "[evaluate-boot-matrix] $nextAction"

if ($decision -eq 'inconclusive') {
    exit 2
}
exit 0
