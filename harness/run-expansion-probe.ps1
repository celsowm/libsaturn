# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# The MPEG card probe and the UART open on a Saturn with neither (Ymir): both must
# report "not there". Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 120
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/expansion_probe'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

$buildArgs = @{ Example = 'expansion_probe_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Failed to build expansion_probe_demo' }

$args2 = @{
    Example = 'expansion_probe_demo'
    Frames = $Frames
    DumpWramHigh = (Join-Path $dir 'wram.bin')
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $args2.Bios = $Bios }
if ($Msys2Root) { $args2.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @args2 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir expansion_probe_demo probe failed' }

$env:LIBSATURN_EXPANSION_DIR = [string]$dir
$env:LIBSATURN_EXPANSION_MAP = Join-Path $repo 'build/expansion_probe_demo.map'
python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_expansion_probe.py -v
if ($LASTEXITCODE -ne 0) { throw "Expansion probe acceptance failed; inspect $dir" }
Write-Host "[expansion] Passed. Capture: $dir"
