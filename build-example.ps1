[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Example,

    [ValidateSet('current', 'safe')]
    [string]$IpProfile = 'current',

    [ValidateSet('yaul', 'sbl', 'minimal', 'yaul_fixed', 'region_free', 'minimal_boot', 'correct', 'final')]
    [string]$IpTemplate = 'yaul',

    [string]$Msys2Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = $PSScriptRoot
$RequestedRoot = if ($Msys2Root) { $Msys2Root } elseif ($env:LIBSATURN_MSYS2_ROOT) { $env:LIBSATURN_MSYS2_ROOT } else { 'C:\msys64' }

function Convert-ToMsysPath {
    param([Parameter(Mandatory = $true)][string]$WindowsPath)
    if ($WindowsPath -match '^([A-Za-z]):\\(.*)$') {
        $drive = $matches[1].ToLowerInvariant()
        $rest = ($matches[2] -replace '\\', '/')
        return "/$drive/$rest"
    }
    return ($WindowsPath -replace '\\', '/')
}

function Find-Msys2Shell {
    param([Parameter(Mandatory = $true)][string[]]$RootCandidates)
    foreach ($candidate in $RootCandidates) {
        if (-not $candidate) { continue }
        $shellPath = Join-Path ([System.IO.Path]::GetFullPath($candidate)) 'msys2_shell.cmd'
        if (Test-Path $shellPath) { return $shellPath }
    }
    return $null
}

function Invoke-Msys2Command {
    param(
        [Parameter(Mandatory = $true)][string]$ShellPath,
        [Parameter(Mandatory = $true)][string]$ScriptCommand
    )
    $wrappedCommand = "rm -f /var/lib/pacman/db.lck >/dev/null 2>&1 || true; $ScriptCommand"
    & $ShellPath -defterm -no-start -ucrt64 -here -c $wrappedCommand
    if ($LASTEXITCODE -ne 0) {
        throw "MSYS2 command failed (code $LASTEXITCODE): $ScriptCommand"
    }
}

# -- Normalize example name --
$normalizedExample = $Example.Trim().Trim('\', '/')
if ($normalizedExample -match '^(examples[\\/])(.+)$') {
    $normalizedExample = $matches[2]
}

$exampleMain = Join-Path $RepoRoot ("examples\{0}\main.c" -f $normalizedExample)
if (-not (Test-Path $exampleMain)) {
    $availableExamples = Get-ChildItem (Join-Path $RepoRoot 'examples') -Directory |
        Where-Object { $_.Name -ne 'common' } |
        Select-Object -ExpandProperty Name
    $availableText = ($availableExamples | Sort-Object) -join ', '
    throw "Example not found: $exampleMain. Available: $availableText"
}

# -- Locate MSYS2 --
$rootCandidates = @(
    $RequestedRoot,
    $env:LIBSATURN_MSYS2_ROOT,
    'C:\msys64',
    'C:\tools\msys64',
    (Join-Path $env:LOCALAPPDATA 'msys64')
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

$shellPath = Find-Msys2Shell -RootCandidates $rootCandidates
if (-not $shellPath) {
    throw 'MSYS2 not found. Provide -Msys2Root or set LIBSATURN_MSYS2_ROOT.'
}

$repoMsysPath = Convert-ToMsysPath -WindowsPath $RepoRoot
Write-Host "[build-example] Building example: $normalizedExample"

# Auto-detect SH2 toolchain
$saturnBin = $null
$msys2Home = '/c/msys64/home'

# Search in /home/*/saturn-tools/bin
$homeDirs = Get-ChildItem 'C:\msys64\home' -Directory -ErrorAction SilentlyContinue
foreach ($dir in $homeDirs) {
    $binPath = "C:\msys64\home\$($dir.Name)\saturn-tools\bin"
    if (Test-Path "$binPath\sh2eb-elf-gcc.exe") {
        $saturnBin = "$msys2Home/$($dir.Name)/saturn-tools/bin"
        break
    }
}

if (-not $saturnBin) {
    Write-Warning "SH2 toolchain not auto-detected. Add to PATH or create symlinks."
}

# -- Verificar/gerar assets com hash check --
$prebuiltManifest = Join-Path $RepoRoot "examples\$normalizedExample\prebuilt\manifest.json"
$prebuiltDir = Join-Path $RepoRoot "examples\$normalizedExample\prebuilt"

$checkResult = & python (Join-Path $RepoRoot 'tools\check-prebuilt.py') $prebuiltManifest 2>$null

if ($checkResult -eq 'ok') {
    Write-Host "[asset] Prebuilt OK (hash unchanged)"
} else {
    # Precisa converter
    if (Test-Path $prebuiltManifest) {
        $manifest = Get-Content $prebuiltManifest -Raw | ConvertFrom-Json
        foreach ($name in $manifest.assets.PSObject.Properties.Name) {
            $info = $manifest.assets.$name
            $assetInput = Join-Path $prebuiltDir $info.input
            if (Test-Path $assetInput) {
                Write-Host "[asset] Converting $name ($($info.input))"
                $outPrefix = Join-Path $prebuiltDir $info.output
                $resizeArgs = if ($info.resize) { "--resize $($info.resize[0]) $($info.resize[1])" } else { "" }
                & python (Join-Path $RepoRoot 'tools\convert_indexed8.py') `
                    --input $assetInput `
                    $resizeArgs.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries) `
                    --out-prefix $outPrefix
                if ($LASTEXITCODE -ne 0) {
                    throw "Failed to generate asset $name for $normalizedExample"
                }
            }
        }
        # Atualizar hashes
        & python (Join-Path $RepoRoot 'tools\bake-assets.py') 2>$null
    }
}

# -- Build via MSYS2 --
$repoPath = $repoMsysPath.Replace('\', '/')
if ($saturnBin) {
    $makeCommand = 'export PATH="' + $saturnBin + ':$PATH" && cd "' + $repoPath + '" && make EXAMPLE=' + $normalizedExample + ' IP_PROFILE=' + $IpProfile + ' IP_TEMPLATE_KIND=' + $IpTemplate + ' all'
} else {
    $makeCommand = 'cd "' + $repoPath + '" && make EXAMPLE=' + $normalizedExample + ' IP_PROFILE=' + $IpProfile + ' IP_TEMPLATE_KIND=' + $IpTemplate + ' all'
}
Invoke-Msys2Command -ShellPath $shellPath -ScriptCommand $makeCommand

# -- Copy artifacts --
$exampleBuildDir = Join-Path $RepoRoot 'build\examples'
if (-not (Test-Path $exampleBuildDir)) {
    New-Item -ItemType Directory -Path $exampleBuildDir -Force | Out-Null
}

$safeName = $normalizedExample -replace '[\\/]', '_'
$isoSource = Join-Path $RepoRoot "build\$normalizedExample.iso"
$cueSource = Join-Path $RepoRoot "build\$normalizedExample.cue"
$binSource = Join-Path $RepoRoot "build\$normalizedExample.bin"
$elfSource = Join-Path $RepoRoot "build\$normalizedExample.elf"

$isoTarget = Join-Path $exampleBuildDir "$safeName.iso"
$cueTarget = Join-Path $exampleBuildDir "$safeName.cue"
$binTarget = Join-Path $exampleBuildDir "$safeName.bin"
$elfTarget = Join-Path $exampleBuildDir "$safeName.elf"

if (Test-Path $isoSource) { Copy-Item $isoSource $isoTarget -Force }
if (Test-Path $cueSource) { Copy-Item $cueSource $cueTarget -Force }
if (Test-Path $binSource) { Copy-Item $binSource $binTarget -Force }
if (Test-Path $elfSource) { Copy-Item $elfSource $elfTarget -Force }

Write-Host "[build-example] Artifacts:"
Write-Host "  $isoTarget"
Write-Host "  $cueTarget"
Write-Host "  $binTarget"
Write-Host "  $elfTarget"
