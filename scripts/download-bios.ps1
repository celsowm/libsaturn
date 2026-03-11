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
        Write-Host "[download-bios] Ja existe: $($item.Name) ($($item.Desc)) - pulando"
        continue
    }
    Write-Host "[download-bios] Baixando $($item.Desc) -> $($item.Name) ..."
    Invoke-WebRequest -Uri $item.Url -OutFile $outPath -UseBasicParsing
    Write-Host "[download-bios] OK: $outPath"
}

# Copiar para o diretorio firmware do Mednafen (onde ele busca por padrao)
$mednafenExe = 'C:\Users\celso\AppData\Local\Microsoft\WinGet\Packages\MednafenTeam.Mednafen_Microsoft.Winget.Source_8wekyb3d8bbwe\mednafen.exe'
if (Test-Path $mednafenExe) {
    $firmwareDir = Join-Path (Split-Path $mednafenExe) 'firmware'
    if (-not (Test-Path $firmwareDir)) {
        New-Item -ItemType Directory -Path $firmwareDir -Force | Out-Null
    }
    $biosFile = Join-Path $BiosDir 'saturn_bios_us.bin'
    $target = Join-Path $firmwareDir 'sega_101.bin'
    if ((Test-Path $biosFile) -and -not (Test-Path $target)) {
        Copy-Item $biosFile $target -Force
        Write-Host "[download-bios] Copiado para Mednafen: $target"
    } elseif (Test-Path $target) {
        Write-Host "[download-bios] Ja existe no Mednafen: $target"
    }
}

Write-Host ""
Write-Host "[download-bios] BIOS files em: $BiosDir"
Get-ChildItem $BiosDir -Filter '*.bin' | ForEach-Object {
    Write-Host "  $($_.Name)  ($([math]::Round($_.Length / 1KB)) KB)"
}
