# run-save-persistence.ps1 — GPL-3.0 (see harness/LICENSE).
#
# End-to-end persistence acceptance for examples/save_backup_demo:
#   pass 1 creates LIBSAT_DEMO with boot_count=1
#   probe exits, preserving the same memory-mapped Backup RAM image
#   pass 2 reopens that image and the guest increments boot_count to 2
#   Python assertions inspect Ymir's logical BUP export from both processes.
[CmdletBinding()]
param(
    [string]$Bios,
    [int]$Frames = 90,
    [int]$BootFrames = 90,
    [string]$Msys2Root,
    [switch]$ForceRebuildHarness
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$HarnessRoot = $PSScriptRoot
$RunHarness = Join-Path $HarnessRoot 'run-harness.ps1'
$BuildDir = Join-Path $HarnessRoot 'build'
$BackupRam = Join-Path $BuildDir 'save_backup_persistence.bup'
$FirstJson = Join-Path $BuildDir 'save_backup_persistence_first.json'
$SecondJson = Join-Path $BuildDir 'save_backup_persistence_second.json'

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
Remove-Item $BackupRam, $FirstJson, $SecondJson -Force -ErrorAction SilentlyContinue

$common = @{
    Example = 'save_backup_demo'
    Frames = $Frames
    BootFrames = $BootFrames
    BackupRam = $BackupRam
}
if ($Bios) { $common.Bios = $Bios }
if ($Msys2Root) { $common.Msys2Root = $Msys2Root }
if ($ForceRebuildHarness) { $common.ForceRebuildHarness = $true }

Write-Host "[save-persistence] pass 1: create and verify save"
& $RunHarness @common -Out $FirstJson
if ($LASTEXITCODE -ne 0) { throw "save persistence pass 1 failed" }

if (-not (Test-Path $BackupRam)) {
    throw "Ymir did not create the expected Backup RAM image: $BackupRam"
}
$size = (Get-Item $BackupRam).Length
if ($size -ne 32768) {
    throw "Internal Backup RAM image has $size bytes; expected 32768"
}

Write-Host "[save-persistence] pass 2: reopen same image and increment boot counter"
& $RunHarness @common -Out $SecondJson
if ($LASTEXITCODE -ne 0) { throw "save persistence pass 2 failed" }

$env:LIBSATURN_SAVE_FIRST_JSON = $FirstJson
$env:LIBSATURN_SAVE_SECOND_JSON = $SecondJson
python -m unittest harness.tests.test_save_backup_persistence
if ($LASTEXITCODE -ne 0) { throw "save persistence assertions failed" }

Write-Host "[save-persistence] PASS: save survived a fresh emulator process"
