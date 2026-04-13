[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BiosDir = Join-Path $RepoRoot 'bios'

if (-not (Test-Path $BiosDir)) {
    New-Item -ItemType Directory -Path $BiosDir -Force | Out-Null
}

$baseUrl = 'https://archive.org/download/segasaturnbios/Sega%20Saturn%20BIOS.zip'

$downloads = @(
    @{
        Name   = 'saturn_bios_jp.bin'
        Url    = "$baseUrl/Sega%20Saturn%20BIOS%2FBios%20Saturn%201.01%20%28J%29%20%5B%21%5D.bin"
        Desc   = 'Japan v1.01'
    },
    @{
        Name   = 'saturn_bios_us.bin'
        Url    = "$baseUrl/Sega%20Saturn%20BIOS%2FBios%20Saturn%201.01a%20%28U%29%20%5B%21%5D.bin"
        Desc   = 'US v1.01a'
    },
    @{
        Name   = 'saturn_bios_eu.bin'
        Url    = "$baseUrl/Sega%20Saturn%20BIOS%2FSega%20Saturn%20BIOS%20%28EUR%29.bin"
        Desc   = 'Europe'
    }
)

foreach ($item in $downloads) {
    $outPath = Join-Path $BiosDir $item.Name
    if (Test-Path $outPath) {
        Write-Host "[download-bios] Already exists: $($item.Name) ($($item.Desc)) - skipping"
        continue
    }
    Write-Host "[download-bios] Downloading $($item.Desc) -> $($item.Name) ..."
    Invoke-WebRequest -Uri $item.Url -OutFile $outPath -UseBasicParsing
    Write-Host "[download-bios] OK: $outPath"
}

# Copy to Mednafen firmware directory (where it looks by default)
# Auto-detect Mednafen installed via Winget
$mednafenBase = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
$mednafenDir = Get-ChildItem $mednafenBase -Directory -Filter 'Mednafen*' -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName
$mednafenExe = if ($mednafenDir) { Join-Path $mednafenDir 'mednafen.exe' } else { $null }
if (Test-Path $mednafenExe) {
    $firmwareDir = Join-Path (Split-Path $mednafenExe) 'firmware'
    if (-not (Test-Path $firmwareDir)) {
        New-Item -ItemType Directory -Path $firmwareDir -Force | Out-Null
    }

    $jpSource = Join-Path $BiosDir 'saturn_bios_jp.bin'
    $nonJpSource = @(
        (Join-Path $BiosDir 'saturn_bios_us.bin'),
        (Join-Path $BiosDir 'saturn_bios_eu.bin')
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (Test-Path $jpSource) {
        $jpTarget = Join-Path $firmwareDir 'sega_101.bin'
        Copy-Item $jpSource $jpTarget -Force
        Write-Host "[download-bios] Copied JP BIOS to Mednafen: $jpTarget"
    }

    if ($nonJpSource) {
        $nonJpTarget = Join-Path $firmwareDir 'mpr-17933.bin'
        Copy-Item $nonJpSource $nonJpTarget -Force
        Write-Host "[download-bios] Copied non-JP BIOS to Mednafen: $nonJpTarget"
    }
}

Write-Host ""
Write-Host "[download-bios] BIOS files at: $BiosDir"
Get-ChildItem $BiosDir -Filter '*.bin' | ForEach-Object {
    Write-Host "  $($_.Name)  ($([math]::Round($_.Length / 1KB)) KB)"
}
