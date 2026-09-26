# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# Runs every Ymir acceptance wrapper in one video standard and prints a pass/fail table.
#
#   .\harness\run-acceptance-sweep.ps1 -Bios .\bios\saturn_bios_us.bin
#   .\harness\run-acceptance-sweep.ps1 -Pal -Bios .\bios\saturn_bios_eu.bin
#
# Each wrapper's full output goes to harness/build/sweep_<ntsc|pal>/<name>.log.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Bios,
    [switch]$Pal,
    [string[]]$Only,
    [string]$Msys2Root
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'
$mode = if ($Pal) { 'pal' } else { 'ntsc' }
$dir = Join-Path $PSScriptRoot "build/sweep_$mode"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$env:LIBSATURN_PAL = if ($Pal) { '1' } else { '0' }
$env:LIBSATURN_FRAME_HZ = if ($Pal) { '50' } else { '59.94' }
$env:LIBSATURN_BIOS = (Resolve-Path $Bios).Path

# name -> script block. The jukebox and the other harness-only checks have no wrapper of their own.
$harness = Join-Path $PSScriptRoot 'run-harness.ps1'
function Invoke-Unit([string]$file, [string]$probeJson) {
    if ($probeJson) { $env:LIBSATURN_PROBE_JSON = $probeJson }
    python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p $file -v
}
$common = @{}
if ($Msys2Root) { $common.Msys2Root = $Msys2Root }
$jukeboxJson = Join-Path $dir 'jukebox.json'
$items = [ordered]@{
    'abus'              = { & (Join-Path $PSScriptRoot 'run-abus.ps1') @common }
    'dma-demo'          = { & (Join-Path $PSScriptRoot 'run-dma-demo.ps1') @common }
    'expansion-probe'   = { & (Join-Path $PSScriptRoot 'run-expansion-probe.ps1') @common }
    'input-devices'     = { & (Join-Path $PSScriptRoot 'run-input-devices.ps1') @common }
    'irq-demo'          = { & (Join-Path $PSScriptRoot 'run-irq-demo.ps1') @common }
    'parallel-sched'    = { & (Join-Path $PSScriptRoot 'run-parallel-sched.ps1') @common }
    'ram-cart'          = { & (Join-Path $PSScriptRoot 'run-ram-cart-acceptance.ps1') @common }
    'save-cartridge'    = { & (Join-Path $PSScriptRoot 'run-save-cartridge.ps1') @common }
    'save-persistence'  = { & (Join-Path $PSScriptRoot 'run-save-persistence.ps1') @common }
    'scene-occlusion'   = { & (Join-Path $PSScriptRoot 'run-scene-cache-occlusion.ps1') @common }
    'scsp-dsp'          = { & (Join-Path $PSScriptRoot 'run-scsp-dsp.ps1') @common }
    'scu-dsp'           = { & (Join-Path $PSScriptRoot 'run-scu-dsp.ps1') @common }
    'sound-driver'      = { & (Join-Path $PSScriptRoot 'run-sound-driver.ps1') @common }
    'vdp2-layers'       = { & (Join-Path $PSScriptRoot 'run-vdp2-layers.ps1') @common }
    'vdp2-effects'      = { & (Join-Path $PSScriptRoot 'run-vdp2-effects.ps1') @common }
    'vdp2-compose'      = { & (Join-Path $PSScriptRoot 'run-vdp2-compose.ps1') @common }
    'jukebox'           = {
        & $harness -Example cd_streaming_jukebox -Frames 300 -Out $jukeboxJson @common
        if ($LASTEXITCODE -ne 0) { throw 'jukebox probe failed' }
        Invoke-Unit 'test_cd_streaming_jukebox.py' $jukeboxJson
    }
}

$results = @()
foreach ($name in $items.Keys) {
    if ($Only -and ($Only -notcontains $name)) { continue }
    $log = Join-Path $dir "$name.log"
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $status = 'PASS'
    try {
        $global:LASTEXITCODE = 0   # a plain assignment would shadow the global the wrappers read
        & $items[$name] *> $log
        if ($LASTEXITCODE -ne 0) { $status = 'FAIL' }
    } catch {
        $status = 'FAIL'
        $_ | Out-String | Add-Content $log
    }
    if ($status -eq 'PASS' -and (Select-String -Path $log -Pattern '^(FAILED|ERROR:|Traceback)' -Quiet)) { $status = 'FAIL' }
    # A wrapper that skipped every test would still exit 0.
    $ran = Select-String -Path $log -Pattern '^Ran (\d+) test' | Select-Object -Last 1
    if ($status -eq 'PASS' -and $ran) {
        $skipped = Select-String -Path $log -Pattern 'skipped=(\d+)' | Select-Object -Last 1
        if ($skipped -and [int]$skipped.Matches[0].Groups[1].Value -ge [int]$ran.Matches[0].Groups[1].Value) { $status = 'FAIL (all skipped)' }
    }
    $watch.Stop()
    $results += [pscustomobject]@{ Name = $name; Status = $status; Seconds = [int]$watch.Elapsed.TotalSeconds }
    Write-Host ("[{0}] {1,-18} {2}  ({3}s)" -f $mode, $name, $status, [int]$watch.Elapsed.TotalSeconds)
}
$results | Format-Table -AutoSize | Out-String | Set-Content (Join-Path $dir 'summary.txt')
$failed = @($results | Where-Object { $_.Status -ne 'PASS' })
Write-Host ("[{0}] {1} of {2} passed. Logs: {3}" -f $mode, ($results.Count - $failed.Count), $results.Count, $dir)
if ($failed.Count -ne 0) { exit 1 }
