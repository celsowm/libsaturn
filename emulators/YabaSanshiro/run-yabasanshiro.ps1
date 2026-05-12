[CmdletBinding()]
param(
    [string]$GamePath,
    [string[]]$ExtraArgs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$defaultIso = Join-Path $repoRoot 'build\mvp.iso'
$exePath = Join-Path $PSScriptRoot 'yabasanshiro.exe'
$transPath = Join-Path $PSScriptRoot 'trans'
$fallbackTransPath = Join-Path $repoRoot '.external\JoEngine\Emulators\YabaSanshiro\trans'
$saveDir = Join-Path $PSScriptRoot 'save'
$bkramPath = Join-Path $PSScriptRoot 'bkram.bin'
$biosCandidates = @(
    (Join-Path $repoRoot 'bios\saturn_bios_us.bin'),
    (Join-Path $repoRoot 'bios\saturn_bios_jp.bin'),
    (Join-Path $repoRoot 'bios\saturn_bios_eu.bin'),
    (Join-Path $repoRoot 'bios\mpr-17933.bin'),
    (Join-Path $repoRoot 'bios\sega_101.bin')
)

if (-not (Test-Path $exePath)) {
    throw "YabaSanshiro not found at $exePath. Copy yabasanshiro.exe to this folder."
}

$targetIso = if ($GamePath) { [System.IO.Path]::GetFullPath($GamePath) } else { $defaultIso }
if (-not (Test-Path $targetIso)) {
    throw "ISO not found: $targetIso"
}

$targetExt = [System.IO.Path]::GetExtension($targetIso).ToLowerInvariant()
$bootAddress = '0x06004000'

if ((-not (Test-Path $transPath)) -and (Test-Path $fallbackTransPath)) {
    Copy-Item $fallbackTransPath -Destination $transPath -Recurse
}

New-Item -ItemType Directory -Path $saveDir -Force | Out-Null
if (-not (Test-Path $bkramPath)) {
    $stream = [System.IO.File]::Open($bkramPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $stream.SetLength(8MB)
    }
    finally {
        $stream.Dispose()
    }
}

$selectedBios = $biosCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$cliArgs = @('--autostart')
if ($targetExt -eq '.bin') {
    $cliArgs += "--binary=$($targetIso):$bootAddress"
}
else {
    $cliArgs += "--iso=$targetIso"
}
if ($selectedBios) {
    $cliArgs += "--bios=$selectedBios"
}
else {
    $cliArgs += '--no-bios'
}
$cliArgs += $ExtraArgs | Where-Object { $_ }

Write-Host "[run-yabasanshiro] Executing: $exePath $($cliArgs -join ' ')"
$process = Start-Process -FilePath $exePath -ArgumentList $cliArgs -WorkingDirectory $PSScriptRoot -PassThru -Wait
exit $process.ExitCode
