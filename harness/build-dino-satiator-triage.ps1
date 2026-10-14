[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$previousLoading = $env:DINO_LOADING_SCREEN
Push-Location $root
try {
    $output = 'harness/build/dino_satiator'
    New-Item -ItemType Directory -Force $output | Out-Null
    $env:DINO_LOADING_SCREEN = '0'
    & .\build-example.ps1 dino_demo
    if ($LASTEXITCODE) { throw 'Dino build without loading screen failed.' }
    Copy-Item -LiteralPath build/examples/dino_demo.iso -Destination "$output/without_bar.iso"
    $env:DINO_LOADING_SCREEN = '1'
    & .\build-example.ps1 dino_demo
    if ($LASTEXITCODE) { throw 'Dino build with loading screen failed.' }
    Copy-Item -LiteralPath build/examples/dino_demo.iso -Destination "$output/with_bar.iso"
    & python harness/studies/satiator_boot/prepare.py `
        --with-bar "$output/with_bar.iso" --without-bar "$output/without_bar.iso" --output $output
    if ($LASTEXITCODE) { throw 'Dino boot experiment preparation failed.' }
    foreach ($name in @('with_bar', 'without_bar', 'cmok_wait', 'legacy_poll')) {
        & python tools/iso_to_raw.py --iso "$output/$name.iso" --output "$output/$name.bin"
        if ($LASTEXITCODE) { throw "Raw image conversion failed: $name" }
        @("FILE `"$name.bin`" BINARY", 'TRACK 01 MODE1/2352', 'INDEX 01 00:00:00') |
            Set-Content -LiteralPath "$output/$name.cue" -Encoding ascii
        & python tools/check_disc_image.py --iso "$output/$name.iso" `
            --bin "$output/$name.bin" --cue "$output/$name.cue"
        if ($LASTEXITCODE) { throw "Raw image validation failed: $name" }
    }
} finally {
    if ($null -eq $previousLoading) {
        Remove-Item Env:DINO_LOADING_SCREEN -ErrorAction SilentlyContinue
    } else {
        $env:DINO_LOADING_SCREEN = $previousLoading
    }
    Pop-Location
}
