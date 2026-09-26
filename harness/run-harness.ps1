# run-harness.ps1 — GPL-3.0 (see harness/LICENSE / harness/README.md).
#
# Builds (if needed) the libsaturn example ISO, builds (if needed) the
# harness's probe.exe against Ymir's emulator core, then runs the probe for
# N frames and writes harness/build/probe.json for harness/tests/*.py.
#
# The probe boots a real BIOS for -BootFrames (letting hardware init run),
# then injects the example's .bin directly into work RAM and jumps to it —
# it does NOT wait for the BIOS to load the disc itself. See
# harness/README.md for why: Ymir's CD block does not currently complete a
# real BIOS disc boot for these images.
#
# A Saturn BIOS/IPL image is required and is NOT provided by this repo or
# script — real Saturn BIOS firmware is Sega's copyrighted property. Point
# -Bios at your own dump, or set LIBSATURN_BIOS.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Example,

    [string]$Bios,
    [int]$Frames = 60,
    [int]$BootFrames = 90,
    [string]$Msys2Root,
    [switch]$ForceRebuildHarness,
    [string]$PadButton,
    [int]$PadPressAt = 0,
    [int]$PadReleaseAt = 0,
    [string]$PadScript,
    [switch]$PadScriptGameFrame,
    # "1:analog", "2:mouse" ...: what is plugged into each port, and a timeline of
    # its values (see the probe's --device-script).
    [string[]]$PortDevice,
    [string]$DeviceScript,
    [string]$BackupRam,
    [string]$BackupCart,
    [ValidateSet('none', '1m', '4m')]
    [string]$RamCart = 'none',
    [string[]]$Screenshot,
    [string]$ProfilePc,
    [string]$ProfileCycles,
    [string]$ProfileInstructions,
    [string]$ProfileTransfers,
    [switch]$ProfileSkybridgeTelemetry,
    [string]$SkybridgeTelemetryCsv,
    [switch]$ProfileParallelRuntimeTelemetry,
    [string]$ParallelRuntimeTelemetryCsv,
    [switch]$ScspTrace,
    [int]$FbSample = 256,
    # Work RAM High (0x06000000, 1 MiB) at the end of the run, for tests
    # that read a guest results struct through the map file.
    [string]$DumpWramHigh,
    [string]$Out
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$HarnessRoot = $PSScriptRoot

$biosPath = if ($Bios) { $Bios } elseif ($env:LIBSATURN_BIOS) { $env:LIBSATURN_BIOS } else { $null }
if (-not $biosPath) {
    throw "No BIOS provided. Pass -Bios <path> or set LIBSATURN_BIOS. " +
          "This must be your own Saturn IPL ROM dump (512 KiB) — see harness/README.md."
}
if (-not (Test-Path $biosPath)) {
    throw "BIOS file not found: $biosPath"
}

# -- Build the example ISO if missing (same convention as run-example.ps1) --
$normalizedExample = $Example.Trim().Trim('\', '/')
if ($normalizedExample -match '^(examples[\\/])(.+)$') {
    $normalizedExample = $matches[2]
}
$safeName = ($normalizedExample -replace '[\\/]', '_')
$isoPath = Join-Path $RepoRoot ("build\examples\{0}.iso" -f $safeName)
$binPath = Join-Path $RepoRoot ("build\examples\{0}.bin" -f $safeName)
$backupRamPath = if ($BackupRam) {
    $BackupRam
} elseif ($normalizedExample -eq 'save_backup_demo') {
    Join-Path $HarnessRoot 'build\save_backup_demo.bup'
} else {
    $null
}

if (-not (Test-Path $isoPath)) {
    Write-Host "[run-harness] Building example: $normalizedExample"
    $buildScript = Join-Path $RepoRoot 'build-example.ps1'
    $buildArgs = @{ Example = $normalizedExample }
    if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
    & $buildScript @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Failed to build example: $normalizedExample" }
}
if (-not (Test-Path $binPath)) {
    throw "Expected $binPath alongside $isoPath (the Makefile's ISO rule copies it there) but it's missing."
}

# Ymir's direct-injection harness does not run the Saturn BIOS's Slave handoff
# routine. For the dual-SH2 and high-level parallel examples, provide the
# entry symbol through Ymir's generic SH-2 reset vector so SSHON still starts
# the actual guest Slave code.
$slaveResetEntry = $null
if ($normalizedExample -eq 'dual_sh2' -or $normalizedExample -eq 'parallel_runtime' -or
    $normalizedExample -eq 'skybridge_3d' -or $normalizedExample -eq 'dino_demo') {
    $elfPath = Join-Path $RepoRoot ("build\examples\{0}.elf" -f $safeName)
    if (-not (Test-Path $elfPath)) {
        throw "Expected $elfPath to resolve _saturn_slave_entry for the Ymir dual-SH2 handoff."
    }
    $mapPath = Join-Path $RepoRoot ("build\{0}.map" -f $safeName)
    if (-not (Test-Path $mapPath)) {
        throw "Expected $mapPath to resolve _saturn_slave_entry for the Ymir dual-SH2 handoff."
    }
    $entryLine = Get-Content $mapPath | Where-Object { $_ -match '_saturn_slave_entry$' } | Select-Object -First 1
    if (-not $entryLine -or $entryLine -notmatch '^\s*0x([0-9A-Fa-f]+)\s+') {
        throw "Could not resolve _saturn_slave_entry from $mapPath (the ELF is stripped in the normal build)."
    }
    $slaveResetEntry = ('0x{0}' -f $matches[1])
}

# -- Build the harness probe if missing (needs the MSYS2 UCRT64 g++ toolchain
#    for CMake configure/compile; the resulting probe.exe is statically
#    linked and runs standalone afterward — see harness/CMakeLists.txt). --
$probeExe = Join-Path $HarnessRoot 'build\probe.exe'
if ($ForceRebuildHarness -or -not (Test-Path $probeExe)) {
    $RequestedRoot = if ($Msys2Root) { $Msys2Root } elseif ($env:LIBSATURN_MSYS2_ROOT) { $env:LIBSATURN_MSYS2_ROOT } else { 'C:\msys64' }
    $shellPath = Join-Path $RequestedRoot 'msys2_shell.cmd'
    if (-not (Test-Path $shellPath)) {
        throw "MSYS2 not found at $RequestedRoot. Provide -Msys2Root or set LIBSATURN_MSYS2_ROOT."
    }

    function Convert-ToMsysPath {
        param([Parameter(Mandatory = $true)][string]$WindowsPath)
        if ($WindowsPath -match '^([A-Za-z]):\\(.*)$') {
            $drive = $matches[1].ToLowerInvariant()
            $rest = ($matches[2] -replace '\\', '/')
            return "/$drive/$rest"
        }
        return ($WindowsPath -replace '\\', '/')
    }

    $harnessMsysPath = Convert-ToMsysPath -WindowsPath $HarnessRoot
    Write-Host "[run-harness] Building probe.exe (first run fetches Ymir — can take a while)"
    $cmakeCmd = "cd '$harnessMsysPath' && cmake -S . -B build -G Ninja && cmake --build build -j"
    & $shellPath -defterm -no-start -ucrt64 -here -c $cmakeCmd
    if ($LASTEXITCODE -ne 0) { throw "Harness build failed" }
}

# -- Run the probe --
$outJson = if ($Out) { $Out } else { Join-Path $HarnessRoot 'build\probe.json' }
Write-Host "[run-harness] Running probe: $normalizedExample, $BootFrames boot frames + $Frames frames"
$probeArgs = @(
    '--iso', $isoPath,
    '--bios', $biosPath,
    '--bin', $binPath,
    '--frames', $Frames,
    '--boot-frames', $BootFrames,
    '--dump-vram', '0x10000:48',
    '--dump-vram', '0x12000:448',
    '--fb-sample', $FbSample,
    '--out', $outJson
)
if ($ProfileSkybridgeTelemetry) {
    if ($normalizedExample -ne 'skybridge_3d') {
        throw '-ProfileSkybridgeTelemetry is available only for skybridge_3d.'
    }
    $mapPath = Join-Path $RepoRoot 'build\skybridge_3d.map'
    if (-not (Test-Path $mapPath -PathType Leaf)) {
        throw "Skybridge map file not found: $mapPath. Rebuild with -SkybridgeValidation."
    }
    $mapText = Get-Content -LiteralPath $mapPath -Raw
    $symbol = [regex]::Match($mapText, '(?m)^\s*(0x[0-9A-Fa-f]+)\s+_?g_sb_test_telemetry(?:\s|$)')
    if (-not $symbol.Success) {
        throw 'g_sb_test_telemetry is absent from the map; build with -SkybridgeValidation.'
    }
    $telemetryAddress = [Convert]::ToUInt32($symbol.Groups[1].Value.Substring(2), 16)
    $telemetryCsv = if ($SkybridgeTelemetryCsv) {
        $SkybridgeTelemetryCsv
    } else {
        Join-Path ([System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($outJson))) `
            (([System.IO.Path]::GetFileNameWithoutExtension($outJson)) + '_skybridge_telemetry.csv')
    }
    $telemetryDir = Split-Path -Parent $telemetryCsv
    if ($telemetryDir -and -not (Test-Path $telemetryDir)) {
        New-Item -ItemType Directory -Path $telemetryDir -Force | Out-Null
    }
    $probeArgs += @('--skybridge-telemetry-address', ('0x{0:X8}' -f $telemetryAddress),
                    '--skybridge-telemetry-csv', $telemetryCsv)
}
if ($ProfileParallelRuntimeTelemetry) {
    if ($normalizedExample -ne 'parallel_runtime') {
        throw '-ProfileParallelRuntimeTelemetry is available only for parallel_runtime.'
    }
    $mapPath = Join-Path $RepoRoot 'build\parallel_runtime.map'
    if (-not (Test-Path -LiteralPath $mapPath -PathType Leaf)) {
        throw "Parallel runtime map file not found: $mapPath. Rebuild with -ParallelRuntimeValidation."
    }
    $mapText = Get-Content -LiteralPath $mapPath -Raw
    $symbol = [regex]::Match($mapText, '(?m)^\s*(0x[0-9A-Fa-f]+)\s+_?g_parallel_runtime_telemetry(?:\s|$)')
    if (-not $symbol.Success) {
        throw 'g_parallel_runtime_telemetry is absent from the map; build with -ParallelRuntimeValidation.'
    }
    $telemetryAddress = [Convert]::ToUInt32($symbol.Groups[1].Value.Substring(2), 16)
    $telemetryCsv = if ($ParallelRuntimeTelemetryCsv) {
        $ParallelRuntimeTelemetryCsv
    } else {
        Join-Path ([System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($outJson))) `
            (([System.IO.Path]::GetFileNameWithoutExtension($outJson)) + '_parallel_runtime_telemetry.csv')
    }
    $telemetryDir = Split-Path -Parent $telemetryCsv
    if ($telemetryDir -and -not (Test-Path -LiteralPath $telemetryDir)) {
        New-Item -ItemType Directory -Path $telemetryDir -Force | Out-Null
    }
    $probeArgs += @('--parallel-runtime-telemetry-address', ('0x{0:X8}' -f $telemetryAddress),
                    '--parallel-runtime-telemetry-csv', $telemetryCsv)
}
if ($PadButton) {
    $probeArgs += @('--pad-button', $PadButton, '--pad-press-at', $PadPressAt, '--pad-release-at', $PadReleaseAt)
}
foreach ($d in $PortDevice) { $probeArgs += @('--port-device', $d) }
if ($DeviceScript) { $probeArgs += @('--device-script', $DeviceScript) }
if ($PadScript) {
    $probeArgs += @('--pad-script', $PadScript)
}
if ($PadScriptGameFrame) {
    if (-not $PadScript) { throw '-PadScriptGameFrame requires -PadScript.' }
    $probeArgs += '--pad-script-game-frame'
}
if ($BackupCart) {
    if (-not (Test-Path $BackupCart -PathType Leaf)) {
        throw "Backup cartridge image must already exist: $BackupCart"
    }
    $probeArgs += @('--backup-cart', (Resolve-Path $BackupCart).Path)
}
if ($RamCart -ne 'none') {
    if ($BackupCart) { throw "Backup cart and RAM expansion share one cartridge slot" }
    $probeArgs += @('--ram-cart', $RamCart)
}
if ($backupRamPath) {
    $backupDir = Split-Path -Parent $backupRamPath
    if ($backupDir -and -not (Test-Path $backupDir)) {
        New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    }
    $probeArgs += @('--backup-ram', $backupRamPath)
}
if ($Screenshot) {
    foreach ($shot in $Screenshot) {
        $probeArgs += @('--screenshot', $shot)
    }
}
if ($DumpWramHigh) {
    $probeArgs += @('--dump-wram-high', $DumpWramHigh)
}
if ($ProfilePc) {
    $probeArgs += @('--profile-pc', $ProfilePc)
}
if ($ProfileCycles) {
    $probeArgs += @('--profile-cycles', $ProfileCycles)
}
if ($ProfileInstructions) {
    $probeArgs += @('--profile-instructions', $ProfileInstructions)
}
if ($ProfileTransfers) {
    $probeArgs += @('--profile-transfers', $ProfileTransfers)
}
# The jukebox acceptance test asserts SCSP double-buffer behavior, so trace it
# automatically. Other examples can opt in with -ScspTrace.
if ($ScspTrace -or $normalizedExample -eq 'cd_streaming_jukebox') {
    $probeArgs += '--scsp-trace'
}
if ($slaveResetEntry) {
    $probeArgs += @('--slave-reset-entry', $slaveResetEntry)
}
& $probeExe @probeArgs
if ($LASTEXITCODE -ne 0) { throw "probe.exe failed (exit $LASTEXITCODE)" }

Write-Host "[run-harness] Wrote $outJson"
Write-Host "[run-harness] Next: `$env:LIBSATURN_PROBE_JSON = '$outJson'; python -m unittest discover harness/tests"
