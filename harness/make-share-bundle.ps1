[CmdletBinding()]
param(
    [string]$PackageRoot = 'harness/build/optical-packages',
    [string]$Output = 'harness/build/share/optical_emulators_tester_native'
)
# Assembles the native-IP optical tester for testers: minimal + full, BIN/CUE and ISO
# in separate folders, with checksums, plain instructions and a results sheet.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$root = [IO.Path]::GetFullPath((Join-Path $repo $PackageRoot))
$out = [IO.Path]::GetFullPath((Join-Path $repo $Output))
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
$rows = @()
foreach ($profile in 'minimal', 'full') {
    $src = Join-Path $root "$profile-native"
    foreach ($f in 'optical_emulators_tester.iso', 'optical_emulators_tester.bin', 'optical_emulators_tester.cue') {
        if (-not (Test-Path (Join-Path $src $f))) { throw "Missing $src/$f; run build-optical-packages.ps1" }
    }
    $name = "optical_tester_$profile"
    $bc = Join-Path $out "$profile/bin_cue"; $iso = Join-Path $out "$profile/iso"
    New-Item -ItemType Directory -Force $bc, $iso | Out-Null
    Copy-Item (Join-Path $src 'optical_emulators_tester.bin') (Join-Path $bc "$name.bin")
    (Get-Content (Join-Path $src 'optical_emulators_tester.cue') -Raw) -replace 'optical_emulators_tester\.bin', "$name.bin" |
        Set-Content (Join-Path $bc "$name.cue") -NoNewline
    Copy-Item (Join-Path $src 'optical_emulators_tester.iso') (Join-Path $iso "$name.iso")
    if ((Get-Content (Join-Path $bc "$name.cue") -Raw) -notmatch [regex]::Escape("$name.bin")) { throw 'CUE rewrite failed' }
    $rows += "$profile/bin_cue/$name.cue", "$profile/iso/$name.iso"
}
Copy-Item (Join-Path $repo 'harness/share-readme.txt') (Join-Path $out 'README.txt')
@'
tester,console_and_bios_region,video_ntsc_or_pal,ode_and_firmware,menu_version,image_used,launch_mode,cold_or_reset,attempt,screen_reached,boot_stage_shown,cd_test_results,notes
'@ | Set-Content (Join-Path $out 'results.csv')
Get-ChildItem $out -Recurse -File | Where-Object { $_.Name -notin 'SHA256SUMS.txt' } | ForEach-Object {
    $rel = $_.FullName.Substring($out.Length + 1).Replace('\', '/')
    '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $rel
} | Set-Content (Join-Path $out 'SHA256SUMS.txt')
$zip = "$out.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path "$out/*" -DestinationPath $zip
Write-Host "[share-bundle] $zip ($([math]::Round((Get-Item $zip).Length/1KB)) KiB)"
