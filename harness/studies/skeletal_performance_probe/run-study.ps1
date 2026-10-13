[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('baseline','compact','shared','dual')]
    [string]$Variant,
    [Parameter(Mandatory=$true)]
    [string]$Bios,
    [ValidateRange(700,5000)]
    [int]$Frames=800
)
$ErrorActionPreference='Stop'
$repositoryRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$exampleName='_skeletal_study_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
$examplePath=[IO.Path]::GetFullPath((Join-Path $repositoryRoot ('examples\'+$exampleName)))
$examplesRoot=[IO.Path]::GetFullPath((Join-Path $repositoryRoot 'examples'))+[IO.Path]::DirectorySeparatorChar
if (-not $examplePath.StartsWith($examplesRoot,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Study directory escaped the examples workspace.'
}
if (Test-Path -LiteralPath $examplePath) { throw 'Temporary study directory already exists.' }
$oldCompact=$env:SKELETAL_STUDY_COMPACT
$oldDual=$env:SKELETAL_STUDY_DUAL
$oldMetrics=$env:SAT_PROFILE_METRICS
Push-Location $repositoryRoot
try {
    New-Item -ItemType Directory -Path $examplePath | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'main.c') -Destination (Join-Path $examplePath 'main.c')
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Makefile.inc') -Destination (Join-Path $examplePath 'Makefile.inc')
    $env:SKELETAL_STUDY_COMPACT=if ($Variant -eq 'compact') {'1'} else {'0'}
    $env:SKELETAL_STUDY_DUAL=if ($Variant -eq 'dual') {'1'} else {'0'}
    $env:SAT_PROFILE_METRICS='1'
    if ($Variant -eq 'compact') {
        & python (Join-Path $PSScriptRoot 'compact_asset.py')
        if ($LASTEXITCODE) { throw 'Candidate asset generation failed.' }
    }
    & .\build-example.ps1 $exampleName
    if ($LASTEXITCODE) { throw 'Study build failed.' }
    $prefix='harness/build/skel_study_'+$Variant+'_validation'
    $mapPath=$prefix+'.map'
    Copy-Item -LiteralPath ('build/'+$exampleName+'.map') -Destination $mapPath
    $mapText=Get-Content -LiteralPath $mapPath -Raw
    $symbol=[regex]::Match($mapText,'0x([0-9a-fA-F]+)\s+_g_skeletal_study\b')
    if (-not $symbol.Success) { throw 'Study telemetry is missing from the map.' }
    $counter=[Convert]::ToUInt32($symbol.Groups[1].Value,16)+8
    $shots=@(
        ('0x{0:X8}:20:{1}_count20.png' -f $counter,$prefix),
        ('0x{0:X8}:80:{1}_count80.png' -f $counter,$prefix)
    )
    & .\harness\run-harness.ps1 $exampleName -Bios $Bios -Frames $Frames `
        -ScreenshotWhen $shots -DumpWramHigh ($prefix+'_wram.bin') -Out ($prefix+'.json') `
        2>&1 | Tee-Object -FilePath ($prefix+'_output.txt')
    if ($LASTEXITCODE) { throw 'Study emulation failed.' }
    & python (Join-Path $PSScriptRoot 'read_profile.py') --map $mapPath `
        --dump ($prefix+'_wram.bin') --probe-log ($prefix+'_output.txt') --output ($prefix+'_analysis.json')
    if ($LASTEXITCODE) { throw 'Study analysis failed.' }
} finally {
    $env:SKELETAL_STUDY_COMPACT=$oldCompact
    $env:SKELETAL_STUDY_DUAL=$oldDual
    $env:SAT_PROFILE_METRICS=$oldMetrics
    # This exact directory was freshly created above, and its absolute path
    # was checked to remain below this repository's examples directory.
    if (Test-Path -LiteralPath $examplePath) {
        Remove-Item -LiteralPath $examplePath -Recurse -Force
    }
    Pop-Location
}
