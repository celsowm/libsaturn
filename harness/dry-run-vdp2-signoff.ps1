# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Emulator-only smoke of a prepared VDP2 signoff bundle. It launches Mednafen
# with the real BIOS disc-boot path, waits for the app to appear, captures one
# frame per disc and closes the emulator. This is NOT hardware evidence.
#
#   .\harness\dry-run-vdp2-signoff.ps1 -Bundle dist\vdp2-signoff\2026-10-18
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Bundle,
    [string]$OutDirectory = 'harness/build/vdp2-signoff-dryrun',
    [ValidateSet('na', 'eu', 'jp')][string]$Region = 'na',
    [int]$BootSeconds = 20,
    [string[]]$Only
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$bundlePath = if ([IO.Path]::IsPathRooted($Bundle)) {
    [IO.Path]::GetFullPath($Bundle)
} else {
    [IO.Path]::GetFullPath((Join-Path $RepoRoot $Bundle))
}
$out = if ([IO.Path]::IsPathRooted($OutDirectory)) {
    [IO.Path]::GetFullPath($OutDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $RepoRoot $OutDirectory))
}
$launcher = Join-Path $RepoRoot 'emulators/mednafen/run-mednafen.ps1'
$capturer = Join-Path $RepoRoot 'tools/capture-mednafen.ps1'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "Missing Mednafen launcher: $launcher"
}
if (-not (Test-Path -LiteralPath $capturer -PathType Leaf)) {
    throw "Missing Mednafen capture helper: $capturer"
}

function Wait-NoMednafen {
    for ($i = 0; $i -lt 40; $i++) {
        if (-not (Get-Process mednafen -ErrorAction SilentlyContinue)) { return }
        Start-Sleep -Milliseconds 250
    }
    throw 'Mednafen did not exit within 10 seconds.'
}

$existing = @(Get-Process mednafen -ErrorAction SilentlyContinue)
if ($existing.Count -ne 0) {
    $existing | Stop-Process -Force
    Wait-NoMednafen
}


& (Join-Path $PSScriptRoot 'verify-vdp2-signoff.ps1') -Bundle $bundlePath | Out-Host
$manifestPath = Join-Path $bundlePath 'manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$discs = @($manifest.discs)
if ($Only) {
    $discs = @($discs | Where-Object { $Only -contains $_.id -or $Only -contains $_.example })
    if ($discs.Count -eq 0) { throw "No discs matched -Only: $($Only -join ', ')" }
}

New-Item -ItemType Directory -Force -Path $out | Out-Null
$records = @()
foreach ($disc in $discs) {
    $id = [string]$disc.id
    $example = [string]$disc.example
    $cueRelative = [string]$disc.files.cue.path
    $cue = Join-Path $bundlePath $cueRelative.Replace('/', '\')
    $shot = Join-Path $out ("{0}.png" -f $id)
    $log = Join-Path $out ("{0}.mednafen.log" -f $id)
    $errLog = "$log.err"
    foreach ($stale in @($shot, $log, $errLog)) {
        Remove-Item -LiteralPath $stale -Force -ErrorAction SilentlyContinue
    }
    Write-Host "[dry-run] $id $example (Mednafen, BIOS disc boot)"
    Wait-NoMednafen
    $arguments = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $launcher,
        '-GamePath', $cue,
        '-Region', $Region,
        '-Cart', 'extram4'
    )
    $wrapper = $null
    $emulator = $null
    for ($attempt = 1; $attempt -le 2; $attempt++) {
        $wrapper = Start-Process -FilePath 'powershell.exe' -ArgumentList $arguments -PassThru `
            -WindowStyle Hidden -RedirectStandardOutput $log -RedirectStandardError $errLog
        for ($i = 0; $i -lt 60; $i++) {
            Start-Sleep -Milliseconds 500
            $emulator = Get-Process mednafen -ErrorAction SilentlyContinue |
                Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
            if ($emulator) { break }
        }
        if ($emulator) { break }
        if (Test-Path -LiteralPath $errLog) { Get-Content -LiteralPath $errLog -Tail 30 | Out-Host }
        if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Tail 30 | Out-Host }
        Get-Process mednafen -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
        Stop-Process -Id $wrapper.Id -Force -ErrorAction SilentlyContinue
        Wait-NoMednafen
        Start-Sleep -Seconds 2
    }
    if (-not $emulator) {
        throw "$id did not open a Mednafen window after two attempts"
    }

    try {
        Start-Sleep -Seconds $BootSeconds
        & $capturer -ProcessId $emulator.Id -Output $shot | Out-Host
        if (-not (Test-Path -LiteralPath $shot -PathType Leaf)) {
            throw "$id did not produce a screenshot"
        }
        Add-Type -AssemblyName System.Drawing
        $image = [System.Drawing.Image]::FromFile($shot)
        try {
            $width = $image.Width
            $height = $image.Height
        } finally {
            $image.Dispose()
        }
        $records += [ordered]@{
            id = $id
            example = $example
            checklist_row = [int]$disc.checklist_row
            supplemental = [bool]$disc.supplemental
            cue = $cueRelative
            screenshot = if ($shot.StartsWith($RepoRoot, [StringComparison]::OrdinalIgnoreCase)) {
                $shot.Substring($RepoRoot.Length).TrimStart([char[]]@('\', '/')).Replace('\', '/')
            } else {
                $shot
            }
            width = $width
            height = $height
            bytes = (Get-Item -LiteralPath $shot).Length
            sha256 = (Get-FileHash -LiteralPath $shot -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    } finally {
        Stop-Process -Id $emulator.Id -Force -ErrorAction SilentlyContinue
        Wait-NoMednafen
        Stop-Process -Id $wrapper.Id -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 1
    }
}

$result = [ordered]@{
    schema_version = 1
    kind = 'libsaturn-vdp2-signoff-mednafen-dryrun'
    generated_utc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
    emulator = 'Mednafen'
    region = $Region
    boot_seconds = $BootSeconds
    notes = 'Emulator-only BIOS disc boot; not physical-hardware evidence. Ymir uses direct injection because its CD block does not complete this disc boot.'
    bundle_manifest_sha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    discs = $records
}
$resultPath = Join-Path $out 'dry-run.json'
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $resultPath -Encoding utf8
Write-Host "[dry-run] $($records.Count) disc(s) captured; report: $resultPath"
