# Volatile RAM expansion acceptance. Requires a legally obtained Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 5
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$runner = Join-Path $PSScriptRoot 'run-harness.ps1'
$cases = @(
    @{ Mode = 'none'; Bytes = 0; Verified = $false },
    @{ Mode = '1m'; Bytes = 1048576; Verified = $true },
    @{ Mode = '4m'; Bytes = 4194304; Verified = $true }
)
foreach ($case in $cases) {
    $out = Join-Path $PSScriptRoot ("build\ram_cart_{0}.json" -f $case.Mode)
    $args = @{
        Example = 'ram_cart_demo'
        RamCart = $case.Mode
        Frames = $Frames
        Out = $out
    }
    if ($Bios) { $args.Bios = $Bios }
    if ($Msys2Root) { $args.Msys2Root = $Msys2Root }
    & $runner @args
    if ($LASTEXITCODE -ne 0) { throw "Probe exited unsuccessfully in $($case.Mode) mode" }
    if (-not (Test-Path $out -PathType Leaf)) { throw "Missing probe output: $out" }
    $json = Get-Content -Raw -Path $out | ConvertFrom-Json
    $cart = $json.ram_cartridge
    if ($null -eq $cart -or $cart.type -ne $case.Mode -or
        [uint64]$cart.capacity_bytes -ne [uint64]$case.Bytes -or
        [bool]$cart.bank_edge_verified -ne [bool]$case.Verified) {
        throw "RAM expansion validation failed for $($case.Mode). Inspect $out"
    }
    Write-Host "[ram-cart] $($case.Mode): PASS"
}
Write-Host '[ram-cart] all three cartridge configurations passed'
