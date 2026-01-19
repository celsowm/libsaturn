$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$downloadDir = Join-Path $root "tools\emulator\downloads"
$emuDir = Join-Path $root "tools\emulator\mednafen"
$exePath = Join-Path $emuDir "mednafen.exe"

if (Test-Path $exePath) {
  Write-Host "Mednafen already installed: $exePath"
  exit 0
}

New-Item -ItemType Directory -Force -Path $downloadDir, $emuDir | Out-Null

$releases = Invoke-WebRequest -UseBasicParsing "https://mednafen.github.io/releases/"
$pattern = "href=""(?<path>/releases/files/mednafen-[^""]*-win64\.zip)"""
$match = [regex]::Match($releases.Content, $pattern)
if (-not $match.Success) {
  throw "Could not find win64 release on mednafen.github.io"
}

$path = $match.Groups["path"].Value
$url = "https://mednafen.github.io$path"
$fileName = [IO.Path]::GetFileName($path)
$zipPath = Join-Path $downloadDir $fileName

Write-Host "Downloading $url"
Invoke-WebRequest -UseBasicParsing $url -OutFile $zipPath

$tempDir = Join-Path $downloadDir "tmp"
if (Test-Path $tempDir) {
  Remove-Item -Recurse -Force $tempDir
}
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null

Expand-Archive -Path $zipPath -DestinationPath $tempDir -Force

$exe = Get-ChildItem -Path $tempDir -Recurse -Filter "mednafen.exe" | Select-Object -First 1
if (-not $exe) {
  throw "mednafen.exe not found after extract"
}

$srcDir = $exe.Directory.FullName
Copy-Item -Path (Join-Path $srcDir "*") -Destination $emuDir -Recurse -Force

Remove-Item -Recurse -Force $tempDir

Write-Host "Installed Mednafen to $emuDir"
