# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Verifies a prepared VDP2 hardware-signoff bundle without building or touching
# an emulator. Checks SHA256SUMS.txt, the manifest inventory and every boot
# report. Run this before burning/loading the discs on real hardware.
#
#   .\harness\verify-vdp2-signoff.ps1 -Bundle dist\vdp2-signoff\2026-10-18
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Bundle
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$bundlePath = if ([IO.Path]::IsPathRooted($Bundle)) {
    [IO.Path]::GetFullPath($Bundle)
} else {
    [IO.Path]::GetFullPath((Join-Path $RepoRoot $Bundle))
}
$sumPath = Join-Path $bundlePath 'SHA256SUMS.txt'
$manifestPath = Join-Path $bundlePath 'manifest.json'
if (-not (Test-Path -LiteralPath $sumPath -PathType Leaf)) {
    throw "Missing SHA256SUMS.txt: $sumPath"
}
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Missing manifest.json: $manifestPath"
}

function Get-BundleRelativePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    $baseFull = [IO.Path]::GetFullPath($bundlePath).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    $pathFull = [IO.Path]::GetFullPath($Path)
    if (-not $pathFull.StartsWith($baseFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the bundle: $pathFull"
    }
    return $pathFull.Substring($baseFull.Length).Replace('\', '/')
}

$listed = @{}
$errors = @()
foreach ($line in Get-Content -LiteralPath $sumPath) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line -notmatch '^([0-9A-Fa-f]{64})  (.+)$') {
        $errors += "Malformed SHA256SUMS line: $line"
        continue
    }
    $expected = $matches[1].ToLowerInvariant()
    $relative = $matches[2].Replace('/', '\')
    if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|\\)\.\.(\\|$)') {
        $errors += "Unsafe path in SHA256SUMS: $($matches[2])"
        continue
    }
    $path = Join-Path $bundlePath $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $errors += "Missing file: $($matches[2])"
        continue
    }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) {
        $errors += "Hash mismatch: $($matches[2])"
    }
    $listed[(Get-BundleRelativePath -Path $path)] = $true
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ([string]$manifest.kind -ne 'libsaturn-vdp2-signoff') {
    $errors += 'manifest.json kind is not libsaturn-vdp2-signoff'
}
if ([int]$manifest.schema_version -ne 1) {
    $errors += "Unsupported manifest schema: $($manifest.schema_version)"
}
$discs = @($manifest.discs)
if ($discs.Count -eq 0) {
    $errors += 'manifest.json has no discs'
}
foreach ($disc in $discs) {
    $example = [string]$disc.example
    foreach ($kind in @('bin', 'cue', 'iso', 'app', 'boot_report')) {
        $record = $disc.files.PSObject.Properties[$kind].Value
        if ($null -eq $record) {
            $errors += "$example missing manifest file record: $kind"
            continue
        }
        $relative = [string]$record.path
        if (-not $listed.ContainsKey($relative)) {
            $errors += "$example $kind is not covered by SHA256SUMS.txt"
        }
        $file = Join-Path $bundlePath $relative.Replace('/', '\')
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            $errors += "$example $kind file missing: $relative"
            continue
        }
        $actual = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne [string]$record.sha256) {
            $errors += "$example $kind manifest hash mismatch"
        }
    }
    $bootRelative = [string]$disc.files.boot_report.path
    $bootPath = Join-Path $bundlePath $bootRelative.Replace('/', '\')
    if (Test-Path -LiteralPath $bootPath -PathType Leaf) {
        $report = Get-Content -LiteralPath $bootPath -Raw | ConvertFrom-Json
        if (@($report.ip.issues).Count -ne 0) {
            $errors += "$example boot report has IP issues"
        }
        if ([string]$report.first_read_sha256 -ne [string]$disc.boot.first_read_sha256) {
            $errors += "$example boot report first-read hash differs from manifest"
        }
        if ([string]$report.iso_sha256 -ne [string]$disc.boot.iso_sha256) {
            $errors += "$example boot report ISO hash differs from manifest"
        }
    }
}

# Extra files do not invalidate the signed inventory, but report them so a
# modified bundle is visible.
$extras = @()
Get-ChildItem -LiteralPath $bundlePath -Recurse -File | ForEach-Object {
    if ($_.Name -eq 'SHA256SUMS.txt') { return }
    $relative = Get-BundleRelativePath -Path $_.FullName
    if (-not $listed.ContainsKey($relative)) { $extras += $relative }
}
if ($extras.Count -ne 0) {
    Write-Host "[verify] Note: $($extras.Count) file(s) are not in SHA256SUMS.txt:"
    $extras | ForEach-Object { Write-Host "  $_" }
}

if ($errors.Count -ne 0) {
    $errors | ForEach-Object { Write-Host "[verify] FAIL $_" }
    throw "Bundle verification failed with $($errors.Count) error(s)."
}
$supplemental = @($discs | Where-Object { $_.supplemental }).Count
Write-Host "[verify] OK: $($discs.Count) disc(s), $($listed.Count) listed file(s), $supplemental supplemental."
Write-Host "[verify] Bundle: $bundlePath"
