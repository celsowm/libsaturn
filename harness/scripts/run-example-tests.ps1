# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
#
# Shared scaffolding for the run-*.ps1 wrappers in this directory.
# Each wrapper's job is mostly the same: rebuild the example, run it
# in Ymir, dump Work RAM, set the LIBSATURN_<NAME>_DUMP and
# LIBSATURN_<NAME>_MAP environment variables the harness pytest expects,
# and invoke the matching test_*.py file. Extracting the common block
# keeps the wrappers short enough to read top-to-bottom and lands every
# bug fix in one place.
#
# Usage from a per-example wrapper:
#
#   [CmdletBinding()]
#   param([string]$Bios, [string]$Msys2Root, [int]$Frames = 130)
#   . (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
#   Invoke-ExampleTest `
#       -Name 'dim' -Example 'vdp2_window_dim' `
#       -Bios:$Bios -Msys2Root:$Msys2Root `
#       -Frames $Frames `
#       -Screenshot @('75:' + (Join-Path $PSScriptRoot 'build/vdp2_window_dim/a.png'),
#                     '{0}:{1}' -f ($Frames - 5), (Join-Path $PSScriptRoot 'build/vdp2_window_dim/b.png'))
#
# Wrapper-specific output (which screenshots to capture, which env vars
# to set, which python test to discover) is driven by parameters.

function Invoke-ExampleTest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)] [string]$Name,
        [Parameter(Mandatory = $true)] [string]$Example,
        [string]$Bios,
        [string]$Msys2Root,
        [int]$Frames = 200,
        [string[]]$Screenshot = @(),
        [string]$MisalignedReport = '',
        [string[]]$TestFile = @(),
        [string[]]$ExtraEnv = @(),
        [string]$DumpName = 'wram_high.bin',
        # build-example.ps1 always writes build/<Example>.map.
        [string]$MapSuffix = '.map',
        # Forwarded to run-harness.ps1. -Out defaults to
        # build/<example>/probe.json. The audio / sound-RAM dumps are
        # only used by the SCSP and 68K sound-driver examples.
        [string]$Out = '',
        [string]$DumpAudio = '',
        [string]$DumpSoundRam = '',
        # PAL timing: when set, -Pal becomes the per-run frame rate
        # (LIBSATURN_FRAME_HZ=50). The harness reads the env var.
        [switch]$Pal
    )

    Set-StrictMode -Version Latest
    $ErrorActionPreference = 'Stop'

    # $PSScriptRoot is harness\scripts (this file): the harness root is its
    # parent and the repository root - where build-example.ps1 lives - the
    # grandparent.
    $scripts = $PSScriptRoot
    $harness = Split-Path -Parent $scripts
    $repo = Split-Path -Parent $harness
    $dir = Join-Path $harness "build/$Example"
    New-Item -ItemType Directory -Force -Path $dir | Out-Null

    # Always rebuild: run-harness only builds a missing ISO, and the
    # test reads the struct through $Example$MapSuffix, which must match.
    $buildArgs = @{ Example = $Example }
    if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
    & (Join-Path $repo 'build-example.ps1') @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Failed to build $Example" }

    $dump = Join-Path $dir $DumpName
    $outPath = if ($Out) { $Out } else { Join-Path $dir 'probe.json' }
    $argsForHarness = @{
        Example = $Example
        Frames = $Frames
        Screenshot = $Screenshot
        DumpWramHigh = $dump
        Out = $outPath
    }
    if ($MisalignedReport) { $argsForHarness.MisalignedReport = $MisalignedReport }
    if ($DumpAudio) { $argsForHarness.DumpAudio = $DumpAudio }
    if ($DumpSoundRam) { $argsForHarness.DumpSoundRam = $DumpSoundRam }
    if ($Pal -or $env:LIBSATURN_PAL -eq '1') {
        $argsForHarness.Pal = $true
        $env:LIBSATURN_FRAME_HZ = '50'
    } elseif (-not $env:LIBSATURN_FRAME_HZ) {
        $env:LIBSATURN_FRAME_HZ = '59.94'
    }
    if ($Bios) { $argsForHarness.Bios = $Bios }
    if ($Msys2Root) { $argsForHarness.Msys2Root = $Msys2Root }
    & (Join-Path $harness 'run-harness.ps1') @argsForHarness
    if ($LASTEXITCODE -ne 0) { throw "The Ymir $Example probe failed" }
    if (-not (Test-Path $dump)) { throw "No Work RAM dump for $Example" }

    # Env var convention: LIBSATURN_<NAME>_DUMP and
    # LIBSATURN_<NAME>_MAP, both uppercase.
    $nameUpper = $Name.ToUpper()
    Set-Item -Path "Env:LIBSATURN_${nameUpper}_DUMP" -Value $dump
    Set-Item -Path "Env:LIBSATURN_${nameUpper}_MAP" -Value (Join-Path $repo "build/$Example$MapSuffix")

    foreach ($kv in $ExtraEnv) {
        $parts = $kv -split '=', 2
        if ($parts.Length -ne 2) { throw "ExtraEnv entry '$kv' must be KEY=VALUE" }
        Set-Item -Path "Env:$($parts[0])" -Value $parts[1]
    }

    # The test file pattern defaults to the wrapper name; pass -p and each
    # pattern as separate arguments - a single "-p foo.py" string would
    # reach Python as one argument and discover nothing.
    $patterns = if ($TestFile) { $TestFile } else { @("test_$($Name.Replace('-','_')).py") }
    $testFileArgs = @()
    foreach ($pattern in $patterns) { $testFileArgs += @('-p', $pattern) }
    python -m unittest discover (Join-Path $harness 'tests') @testFileArgs -v
    if ($LASTEXITCODE -ne 0) { throw "Acceptance failed for $Example; inspect $dir" }
    Write-Host "[$Name] Passed. Capture: $dir"
}
