# GPL-3.0; target differential capture, not a performance qualification.
[CmdletBinding()]
param([string]$Bios, [string]$Msys2Root, [int]$Frames = 600)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Bios) { $Bios = $env:LIBSATURN_BIOS }
if (-not $Bios) {
    foreach ($candidate in @('bios/saturn_bios_us.bin','bios/firmware/mpr-17933.bin','bios/mpr-17933.bin')) {
        $p = Join-Path $repo $candidate
        if (Test-Path $p -PathType Leaf) { $Bios = $p; break }
    }
}
if (-not $Bios -or -not (Test-Path $Bios -PathType Leaf)) {
    throw 'Set LIBSATURN_BIOS to your own Saturn BIOS or pass -Bios.'
}
$Bios = (Resolve-Path $Bios).Path
$dir = Join-Path $PSScriptRoot $(if ($env:LIBSATURN_PAL -eq '1') { 'build/sh2_kernel_verify_pal' } else { 'build/sh2_kernel_verify' })
New-Item -ItemType Directory -Force $dir | Out-Null
$zip = Join-Path $PSScriptRoot 'build/sh2_kernel_verify-return.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
foreach ($name in @('analysis.json','wram.bin','probe.json','misaligned.txt','run.log','metadata.json')) {
    $p = Join-Path $dir $name
    if (Test-Path $p) { Remove-Item $p -Force }
}
Push-Location $repo
$meta = [ordered]@{ frames = $Frames; bios_sha256 = (Get-FileHash $Bios -Algorithm SHA256).Hash; hardware_status = 'PENDING'; result = 'FAIL' }
$log = Join-Path $dir 'run.log'
$transcribing = $false
try {
    Start-Transcript -Path $log -Force | Out-Null
    $transcribing = $true
    $sha = git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'git rev-parse failed' }
    $meta['checkout'] = "$sha"
    . (Join-Path $PSScriptRoot 'scripts/run-example-tests.ps1')
    Invoke-ExampleTest -Name 'sh2_kernel_verify' -Example 'sh2_kernel_verify' -Bios:$Bios -Msys2Root:$Msys2Root -Frames $Frames -DumpName 'wram.bin' -MisalignedReport (Join-Path $dir 'misaligned.txt') -TestFile 'test_sh2_kernel_verify.py'
    $meta['elf_sha256'] = (Get-FileHash (Join-Path $repo 'build/sh2_kernel_verify.elf') -Algorithm SHA256).Hash
    if (-not (Test-Path (Join-Path $dir 'analysis.json'))) { throw 'Missing target analysis' }
    $meta['result'] = 'PASS'
} catch {
    $meta['error'] = $_.ToString()
    Write-Host $_
    throw
} finally {
    if ($transcribing) { Stop-Transcript | Out-Null }
    $meta | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $dir 'metadata.json')
    $reportFiles = @(foreach ($name in @('analysis.json','probe.json','misaligned.txt','run.log','metadata.json')) {
        $p = Join-Path $dir $name
        if (Test-Path $p) { $p }
    })
    Compress-Archive -LiteralPath $reportFiles -DestinationPath $zip -Force
    Write-Host "Return: $zip"
    Pop-Location
}
