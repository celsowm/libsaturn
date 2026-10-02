<#
.SYNOPSIS
    Runs the LibSaturn package-boundary gates through MSYS2.

.DESCRIPTION
    Wraps scripts/test-package.sh: host header smoke tests, the installed
    package consumer test and (with -Conan) the Conan test_package. Uses the
    same MSYS2 discovery as build-example.ps1 and puts the sh2eb-elf toolchain
    on PATH.
#>
[CmdletBinding()]
param(
    [switch]$Conan,
    [string]$Msys2Root,
    [string]$ToolchainBin
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot

function Convert-ToMsysPath {
    param([Parameter(Mandatory = $true)][string]$WindowsPath)
    if ($WindowsPath -match '^([A-Za-z]):\\(.*)$') {
        return "/$($matches[1].ToLowerInvariant())/$($matches[2] -replace '\\', '/')"
    }
    return ($WindowsPath -replace '\\', '/')
}

$candidates = @($Msys2Root, $env:LIBSATURN_MSYS2_ROOT, 'C:\msys64', 'C:\tools\msys64',
    (Join-Path $env:LOCALAPPDATA 'msys64')) | Where-Object { $_ }
$shellPath = $null
foreach ($candidate in $candidates) {
    $probe = Join-Path ([System.IO.Path]::GetFullPath($candidate)) 'msys2_shell.cmd'
    if (Test-Path $probe) { $shellPath = $probe; break }
}
if (-not $shellPath) { throw 'MSYS2 not found. Provide -Msys2Root or set LIBSATURN_MSYS2_ROOT.' }

if (-not $ToolchainBin) {
    $root = Split-Path -Parent $shellPath
    $ToolchainBin = Get-ChildItem (Join-Path $root 'home') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'saturn-tools\bin' } |
        Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $ToolchainBin) { throw 'sh2eb-elf toolchain not found. Pass -ToolchainBin.' }

$repo = Convert-ToMsysPath $RepoRoot
$toolchain = Convert-ToMsysPath $ToolchainBin
$python = Convert-ToMsysPath (Get-Command python).Source
$conanFlag = if ($Conan) { '--conan' } else { '' }

# A script file avoids the nested-quoting limits of msys2_shell.cmd -c.
$script = Join-Path ([System.IO.Path]::GetTempPath()) 'libsaturn-test-package.sh'
@"
export PATH='$toolchain':`$PATH
export PATH="`$(dirname '$python'):`$PATH"
cd '$repo'
bash scripts/test-package.sh $conanFlag
"@ | Set-Content -Path $script -Encoding ascii -NoNewline

Write-Host "[test-package] running through $shellPath"
& $shellPath -defterm -no-start -ucrt64 -here -c "sh '$(Convert-ToMsysPath $script)'" | Out-Host
$global:LASTEXITCODE = $LASTEXITCODE
if ($global:LASTEXITCODE -ne 0) { throw "package gates failed (exit $global:LASTEXITCODE)" }
Write-Host '[test-package] passed'
