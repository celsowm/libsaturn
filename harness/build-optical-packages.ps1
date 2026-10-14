[CmdletBinding()]
param(
    [string]$OutputDirectory = 'harness/build/optical-packages',
    [ValidateSet('yaul','sbl','native')][string[]]$BootProfiles = @('yaul','sbl','native'),
    [ValidateSet('minimal','full')][string[]]$TestProfiles = @('minimal','full')
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = [IO.Path]::GetFullPath((Join-Path $repo $OutputDirectory))
New-Item -ItemType Directory -Force $output | Out-Null
$previous = $env:OPTICAL_TEST_PROFILE
try {
    foreach ($testProfile in $TestProfiles) {
        $env:OPTICAL_TEST_PROFILE = $testProfile
        foreach ($bootProfile in $BootProfiles) {
            $name = "$testProfile-$bootProfile"
            $folder = Join-Path $output $name
            New-Item -ItemType Directory -Force $folder | Out-Null
            & (Join-Path $repo 'build-example.ps1') optical_emulators_tester -IpTemplate $bootProfile *> (Join-Path $folder 'build.log')
            if ($LASTEXITCODE -ne 0) { throw "Build failed: $name; see $folder/build.log" }
            Get-ChildItem (Join-Path $repo 'build/examples/optical_emulators_tester.*') -File | Copy-Item -Destination $folder -Force
            Copy-Item (Join-Path $repo 'build/optical_emulators_tester.map') $folder -Force
            Copy-Item (Join-Path $repo 'build/optical_emulators_tester.IP.BIN') $folder -Force
            $python = if (Test-Path 'C:/Python/python.exe') { 'C:/Python/python.exe' } else { 'python' }
            & $python (Join-Path $repo 'tools/inspect_boot_image.py') (Join-Path $folder 'optical_emulators_tester.iso') --app (Join-Path $folder 'optical_emulators_tester.app.bin') --out (Join-Path $folder 'boot-report.json')
            if ($LASTEXITCODE -ne 0) { throw "Image inspection failed: $name" }
            Write-Host "[optical-package] $name built and inspected"
        }
    }
    $variantBase = Join-Path $output 'minimal-yaul/optical_emulators_tester.iso'
    if (Test-Path $variantBase) {
        & $python (Join-Path $repo 'tools/build_boot_header_variants.py') $variantBase --out (Join-Path $output 'header-variants-minimal-yaul')
        if ($LASTEXITCODE -ne 0) { throw 'Header variant build failed' }
    }
    & $python (Join-Path $repo 'tools/package_optical_results.py') --root $output
    if ($LASTEXITCODE -ne 0) { throw 'Package manifest failed' }
} finally {
    if ($null -eq $previous) { Remove-Item Env:OPTICAL_TEST_PROFILE -ErrorAction SilentlyContinue }
    else { $env:OPTICAL_TEST_PROFILE = $previous }
}
