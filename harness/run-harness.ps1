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
    [int]$FbSample = 256,
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
if ($PadButton) {
    $probeArgs += @('--pad-button', $PadButton, '--pad-press-at', $PadPressAt, '--pad-release-at', $PadReleaseAt)
}
& $probeExe @probeArgs
if ($LASTEXITCODE -ne 0) { throw "probe.exe failed (exit $LASTEXITCODE)" }

Write-Host "[run-harness] Wrote $outJson"
Write-Host "[run-harness] Next: `$env:LIBSATURN_PROBE_JSON = '$outJson'; python -m unittest discover harness/tests"
