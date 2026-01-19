param(
  [string]$Agree = ""
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$emuDir = Join-Path $root "tools\emulator\mednafen"
$fwDir = Join-Path $emuDir "firmware"
$outFile = Join-Path $fwDir "mpr-17933.bin"
$legacyFile = Join-Path $emuDir "mpr-17933.bin"
$url = "https://raw.githubusercontent.com/Abdess/retroarch_system/libretro/Sega%20-%20Saturn/mpr-17933.bin"

if (Test-Path $outFile) {
  Write-Host "BIOS already present: $outFile"
  exit 0
}
if (Test-Path $legacyFile) {
  Write-Host "BIOS already present: $legacyFile"
  exit 0
}

Write-Host "This BIOS is copyrighted. Download only if you own the original hardware or ROM."
if ($Agree.ToUpperInvariant() -ne "Y") {
  Write-Host "Download mpr-17933.bin from:"
  Write-Host "  $url"
  $answer = Read-Host "Download now? [Y/n]"
  if ($answer.ToUpperInvariant() -ne "Y" -and $answer -ne "") {
    Write-Host "Aborted."
    exit 1
  }
}

New-Item -ItemType Directory -Force -Path $fwDir | Out-Null
Invoke-WebRequest -UseBasicParsing $url -OutFile $outFile

if (-not (Test-Path $outFile)) {
  throw "Download failed"
}

Write-Host "Downloaded BIOS to $outFile"
