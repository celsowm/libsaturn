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
        [string]$MapSuffix = '_demo.map'
    )

    Set-StrictMode -Version Latest
    $ErrorActionPreference = 'Stop'

    $harness = $PSScriptRoot
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
    $argsForHarness = @{
        Example = $Example
        Frames = $Frames
        Screenshot = $Screenshot
        DumpWramHigh = $dump
    }
    if ($MisalignedReport) { $argsForHarness.MisalignedReport = $MisalignedReport }
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

    $testFileParam = if ($TestFile) { "-p $($TestFile -join ' -p ')" } else { "-p test_$($Name.Replace('-','_')).py" }
    python -m unittest discover (Join-Path $harness 'tests') $testFileParam -v
    if ($LASTEXITCODE -ne 0) { throw "Acceptance failed for $Example; inspect $dir" }
    Write-Host "[$Name] Passed. Capture: $dir"
}
