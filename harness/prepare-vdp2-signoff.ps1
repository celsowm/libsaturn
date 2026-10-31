# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Builds the reproducible VDP2 hardware-signoff bundle. The twelve E01-E12
# discs are the M6 core; E13 is included as a clearly marked supplemental disc
# unless -CoreOnly is passed. No hardware is touched here.
#
#   .\harness\prepare-vdp2-signoff.ps1
#   .\harness\prepare-vdp2-signoff.ps1 -CoreOnly
#   .\harness\prepare-vdp2-signoff.ps1 -Force -CompareTo dist\vdp2-signoff\2026-10-18\manifest.json
[CmdletBinding()]
param(
    [string]$OutputDirectory,
    [string]$DateTag = (Get-Date -Format 'yyyy-MM-dd'),
    [switch]$CoreOnly,
    [string]$Msys2Root,
    [switch]$Force,
    [string]$CompareTo
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Python = (Get-Command python -ErrorAction Stop).Source
$Makefile = Join-Path $RepoRoot 'Makefile'
$SessionDoc = Join-Path $RepoRoot 'docs/VDP2_HARDWARE_SESSION.md'
$BuildScript = Join-Path $RepoRoot 'build-example.ps1'

function Get-FullPathInRepo {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([IO.Path]::IsPathRooted($Path)) {
        return [IO.Path]::GetFullPath($Path)
    }
    return [IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

function Get-BundleRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$Base,
        [Parameter(Mandatory = $true)][string]$Path
    )
    $baseFull = [IO.Path]::GetFullPath($Base).TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
    $pathFull = [IO.Path]::GetFullPath($Path)
    if (-not $pathFull.StartsWith($baseFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the bundle: $pathFull"
    }
    return $pathFull.Substring($baseFull.Length).Replace('\', '/')
}

function Get-FileRecord {
    param(
        [Parameter(Mandatory = $true)][string]$Base,
        [Parameter(Mandatory = $true)][string]$Path
    )
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing signoff file: $Path"
    }
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        path = Get-BundleRelativePath -Base $Base -Path $Path
        bytes = [int64]$item.Length
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Invoke-Logged {
    param(
        [Parameter(Mandatory = $true)][string]$Log,
        [Parameter(Mandatory = $true)][scriptblock]$Command
    )
    & $Command *> $Log
    if ($LASTEXITCODE -ne 0) {
        $tail = Get-Content -LiteralPath $Log -Tail 50 -ErrorAction SilentlyContinue
        if ($tail) { $tail | Out-Host }
        throw "Command failed (exit $LASTEXITCODE); see $Log"
    }
}

if (-not (Test-Path -LiteralPath $SessionDoc -PathType Leaf)) {
    throw "Missing session protocol: $SessionDoc"
}
if (-not (Test-Path -LiteralPath $BuildScript -PathType Leaf)) {
    throw "Missing build script: $BuildScript"
}

$out = if ($OutputDirectory) {
    Get-FullPathInRepo -Path $OutputDirectory
} else {
    Get-FullPathInRepo -Path (Join-Path 'dist/vdp2-signoff' $DateTag)
}
if ($out.TrimEnd([char[]]@('\', '/')) -eq $RepoRoot.TrimEnd([char[]]@('\', '/'))) {
    throw 'Refusing to use the repository root as the signoff bundle directory.'
}
if (Test-Path -LiteralPath $out) {
    if (-not $Force) {
        throw "Signoff directory already exists: $out. Pass -Force to replace it."
    }
    Remove-Item -LiteralPath $out -Recurse -Force
}

$discDir = Join-Path $out 'discs'
$inspectDir = Join-Path $out 'inspect'
$bootDir = Join-Path $out 'boot-reports'
$logDir = Join-Path $out 'logs'
foreach ($dir in @($out, $discDir, $inspectDir, $bootDir, $logDir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$items = @(
    [ordered]@{ id = 'E01'; row = 29; example = 'vdp2_dual_rotation'; supplemental = $false },
    [ordered]@{ id = 'E02'; row = 28; example = 'vdp2_rotation_coefficients'; supplemental = $false },
    [ordered]@{ id = 'E03'; row = 30; example = 'vdp2_special_color'; supplemental = $false },
    [ordered]@{ id = 'E04'; row = 31; example = 'vdp2_hardware_blur'; supplemental = $false },
    [ordered]@{ id = 'E05'; row = 33; example = 'vdp2_sprite_shadows'; supplemental = $false },
    [ordered]@{ id = 'E06'; row = 32; example = 'vdp2_special_priority'; supplemental = $false },
    [ordered]@{ id = 'E07'; row = 34; example = 'vdp2_dynamic_tile_stream'; supplemental = $false },
    [ordered]@{ id = 'E08'; row = 27; example = 'vdp2_vram_stress'; supplemental = $false },
    [ordered]@{ id = 'E09'; row = 35; example = 'vdp2_video_modes'; supplemental = $false },
    [ordered]@{ id = 'E10'; row = 36; example = 'vdp2_parallax_world'; supplemental = $false },
    [ordered]@{ id = 'E11'; row = 37; example = 'vdp2_fighting_stage'; supplemental = $false },
    [ordered]@{ id = 'E12'; row = 38; example = 'vdp2_neon_city'; supplemental = $false },
    [ordered]@{ id = 'E13'; row = 39; example = 'vdp2_parallax_lab'; supplemental = $true }
)
if ($CoreOnly) {
    $items = @($items | Where-Object { -not $_.supplemental })
}

$commit = (& git -C $RepoRoot rev-parse HEAD 2>$null).Trim()
if ($LASTEXITCODE -ne 0 -or -not $commit) {
    throw "Could not read the git commit for $RepoRoot"
}
$dirty = @(& git -C $RepoRoot status --porcelain --untracked-files=no 2>$null).Count -ne 0
$epochMatch = Select-String -LiteralPath $Makefile -Pattern '^SOURCE_DATE_EPOCH\s*\?=\s*([0-9]+)' | Select-Object -First 1
if (-not $epochMatch) {
    throw 'SOURCE_DATE_EPOCH is missing from the Makefile'
}
$sourceDateEpoch = [int64]$epochMatch.Matches[0].Groups[1].Value

Write-Host "[signoff] Bundle: $out"
Write-Host "[signoff] Commit: $commit (dirty=$dirty); SOURCE_DATE_EPOCH=$sourceDateEpoch"
Write-Host "[signoff] Building $($items.Count) disc(s); E13 supplemental=$(-not $CoreOnly)"

$entries = @()
foreach ($item in $items) {
    $id = $item.id
    $example = $item.example
    Write-Host "[signoff] $id $example"
    $buildLog = Join-Path $logDir ("build-$id.log")
    $buildArgs = @{ Example = $example; ForceRebuild = $true }
    if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
    Invoke-Logged -Log $buildLog -Command { & $BuildScript @buildArgs }

    $artifactBase = Join-Path $RepoRoot ("build/examples/{0}" -f $example)
    $bin = "$artifactBase.bin"
    $cue = "$artifactBase.cue"
    $iso = "$artifactBase.iso"
    $app = "$artifactBase.app.bin"
    foreach ($file in @($bin, $cue, $iso, $app)) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "Build did not produce $file"
        }
    }

    $discBin = Join-Path $discDir ("{0}.bin" -f $example)
    $discCue = Join-Path $discDir ("{0}.cue" -f $example)
    $inspectIso = Join-Path $inspectDir ("{0}.iso" -f $example)
    $inspectApp = Join-Path $inspectDir ("{0}.app.bin" -f $example)
    Copy-Item -LiteralPath $bin -Destination $discBin -Force
    Copy-Item -LiteralPath $cue -Destination $discCue -Force
    Copy-Item -LiteralPath $iso -Destination $inspectIso -Force
    Copy-Item -LiteralPath $app -Destination $inspectApp -Force

    $checkLog = Join-Path $logDir ("check-$id.log")
    Invoke-Logged -Log $checkLog -Command {
        & $Python (Join-Path $RepoRoot 'tools/check_disc_image.py') `
            --iso $inspectIso --bin $discBin --cue $discCue
    }

    $bootReport = Join-Path $bootDir ("{0}.json" -f $example)
    $inspectLog = Join-Path $logDir ("inspect-$id.log")
    Invoke-Logged -Log $inspectLog -Command {
        & $Python (Join-Path $RepoRoot 'tools/inspect_boot_image.py') `
            $inspectIso --app $inspectApp --strict-ip --out $bootReport
    }

    $report = Get-Content -LiteralPath $bootReport -Raw | ConvertFrom-Json
    if (@($report.ip.issues).Count -ne 0) {
        throw "$id has IP issues: $($report.ip.issues -join ', ')"
    }
    if ([int64]$report.ip.first_read_address -ne 0x06004000) {
        throw "$id unexpected first-read address: $($report.ip.first_read_address)"
    }
    if ([int64]$report.root[0].size -ne (Get-Item -LiteralPath $inspectApp).Length) {
        throw "$id first-read size does not match the application binary"
    }
    $appSha = (Get-FileHash -LiteralPath $inspectApp -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($report.first_read_sha256 -ne $appSha) {
        throw "$id first-read hash does not match the application binary"
    }
    if ([string]$report.ip.regions -notmatch '^[JTUE]+$') {
        throw "$id has invalid region string: $($report.ip.regions)"
    }

    # Keep the report reproducible and free of a machine-specific absolute path.
    $report.source = "build/examples/$example.iso"
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $bootReport -Encoding utf8

    $entries += [ordered]@{
        id = $id
        checklist_row = [int]$item.row
        example = $example
        supplemental = [bool]$item.supplemental
        files = [ordered]@{
            bin = Get-FileRecord -Base $out -Path $discBin
            cue = Get-FileRecord -Base $out -Path $discCue
            iso = Get-FileRecord -Base $out -Path $inspectIso
            app = Get-FileRecord -Base $out -Path $inspectApp
            boot_report = Get-FileRecord -Base $out -Path $bootReport
        }
        boot = [ordered]@{
            first_read_address = '0x06004000'
            first_read_size = [int64]$report.ip.first_read_size
            regions = [string]$report.ip.regions
            title = [string]$report.ip.metadata.title
            version = [string]$report.ip.metadata.version
            iso_sha256 = [string]$report.iso_sha256
            first_read_sha256 = [string]$report.first_read_sha256
        }
    }
}

Copy-Item -LiteralPath $SessionDoc -Destination (Join-Path $out 'VDP2_HARDWARE_SESSION.md') -Force
$readme = @"
LibSaturn VDP2 hardware signoff bundle
=====================================

Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))
Git commit:       $commit
Git dirty:        $dirty
SOURCE_DATE_EPOCH: $sourceDateEpoch
Scope:            $($items.Count) disc(s); core E01-E12 plus E13 supplemental unless -CoreOnly was used.

Contents:
  discs/          burn/ODE-ready MODE1/2352 .bin and .cue pairs
  inspect/        logical .iso and .app.bin used to validate each pair
  boot-reports/   strict IP/ISO first-read reports
  logs/           build, disc-image and boot-image checks
  manifest.json   machine-readable inventory, hashes and boot summary
  SHA256SUMS.txt  integrity manifest for the whole bundle
  VDP2_HARDWARE_SESSION.md  physical-session protocol

Verify from the repository root:
  .\harness\verify-vdp2-signoff.ps1 -Bundle "<this directory>"

This bundle and every emulator observation are NOT physical-hardware
evidence. Phase C must still be run on real NTSC and PAL consoles.
"@
[IO.File]::WriteAllText((Join-Path $out 'README.txt'), $readme, (New-Object System.Text.UTF8Encoding($false)))

$supplementalIds = @()
if (-not $CoreOnly) { $supplementalIds = @('E13') }
$manifest = [ordered]@{
    schema_version = 1
    kind = 'libsaturn-vdp2-signoff'
    generated_utc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
    git = [ordered]@{
        commit = $commit
        dirty = $dirty
    }
    source_date_epoch = $sourceDateEpoch
    scope = [ordered]@{
        core_only = [bool]$CoreOnly
        core = @('E01', 'E02', 'E03', 'E04', 'E05', 'E06', 'E07', 'E08', 'E09', 'E10', 'E11', 'E12')
        supplemental = [object[]]$supplementalIds
    }
    discs = $entries
    gates = [ordered]@{
        disc_pairs_validated = $entries.Count
        boot_images_inspected = $entries.Count
        boot_header_issues = 0
    }
}

if ($CompareTo) {
    $comparePath = Get-FullPathInRepo -Path $CompareTo
    if (-not (Test-Path -LiteralPath $comparePath -PathType Leaf)) {
        throw "Compare manifest not found: $comparePath"
    }
    $prior = Get-Content -LiteralPath $comparePath -Raw | ConvertFrom-Json
    if ([string]$prior.kind -ne 'libsaturn-vdp2-signoff') {
        throw "Not a signoff manifest: $comparePath"
    }
    $priorByExample = @{}
    foreach ($disc in @($prior.discs)) {
        $priorByExample[[string]$disc.example] = $disc
    }
    $mismatches = @()
    foreach ($entry in $entries) {
        $example = [string]$entry.example
        if (-not $priorByExample.ContainsKey($example)) {
            $mismatches += "$example missing from the comparison manifest"
            continue
        }
        $old = $priorByExample[$example]
        foreach ($kind in @('bin', 'cue', 'iso', 'app')) {
            $newSha = [string]$entry.files[$kind].sha256
            $oldSha = [string]$old.files.PSObject.Properties[$kind].Value.sha256
            if ($newSha -ne $oldSha) {
                $mismatches += "$example $kind sha256 differs: $oldSha -> $newSha"
            }
        }
    }
    if ($mismatches.Count -ne 0) {
        $mismatches | ForEach-Object { Write-Host "[signoff] MISMATCH $_" }
        throw 'Reproducibility check failed.'
    }
    $manifest.reproducibility = [ordered]@{
        compared_to = $CompareTo
        compared_manifest_sha256 = (Get-FileHash -LiteralPath $comparePath -Algorithm SHA256).Hash.ToLowerInvariant()
        checked_discs = $entries.Count
        matched = $true
    }
    Write-Host "[signoff] Reproducibility OK: $($entries.Count)/$($entries.Count) disc hashes match"
}

$manifestPath = Join-Path $out 'manifest.json'
$manifest | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $manifestPath -Encoding utf8

# The sums file covers every distributed file except itself. Paths are relative
# and use forward slashes so an ODE host can verify the bundle independently.
$sumLines = @()
$allFiles = Get-ChildItem -LiteralPath $out -Recurse -File |
    Where-Object { $_.FullName -ne (Join-Path $out 'SHA256SUMS.txt') } |
    Sort-Object FullName
foreach ($file in $allFiles) {
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $sumLines += ("{0}  {1}" -f $hash, (Get-BundleRelativePath -Base $out -Path $file.FullName))
}
[IO.File]::WriteAllText((Join-Path $out 'SHA256SUMS.txt'), (($sumLines -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))

Write-Host "[signoff] Manifest: $manifestPath"
Write-Host "[signoff] SHA256SUMS: $(Join-Path $out 'SHA256SUMS.txt')"
Write-Host "[signoff] Built and inspected $($entries.Count) disc(s)."
