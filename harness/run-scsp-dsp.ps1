# GPL-3.0; harness/ is separate from the MIT-licensed libsaturn.
# SCSP DSP echo and reverb on Ymir: the recorded output and the echo delay line.
# Requires the user's own Saturn BIOS.
[CmdletBinding()]
param(
    [string]$Bios,
    [string]$Msys2Root,
    [int]$Frames = 150,
    # Also record Mednafen's output (-soundrecord) for this many seconds and check it the same way
    [switch]$Mednafen,
    [int]$MednafenSeconds = 45
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $PSScriptRoot 'build/scsp_dsp'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

$buildArgs = @{ Example = 'scsp_dsp_demo' }
if ($Msys2Root) { $buildArgs.Msys2Root = $Msys2Root }
& (Join-Path $repo 'build-example.ps1') @buildArgs | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Failed to build scsp_dsp_demo' }

$args2 = @{
    Example = 'scsp_dsp_demo'
    Frames = $Frames
    DumpWramHigh = (Join-Path $dir 'wram.bin')
    DumpAudio = (Join-Path $dir 'audio.raw')
    Out = (Join-Path $dir 'probe.json')
}
if ($Bios) { $args2.Bios = $Bios }
if ($Msys2Root) { $args2.Msys2Root = $Msys2Root }
& (Join-Path $PSScriptRoot 'run-harness.ps1') @args2 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'The Ymir scsp_dsp_demo probe failed' }

$env:LIBSATURN_DSP_DIR = [string]$dir
$env:LIBSATURN_DSP_MAP = Join-Path $repo 'build/scsp_dsp_demo.map'
if ($Mednafen) {
    $mednafenExe = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\MednafenTeam.Mednafen_Microsoft.Winget.Source_8wekyb3d8bbwe\mednafen.exe'
    if (-not (Test-Path $mednafenExe)) { throw "Mednafen not found: $mednafenExe" }
    $cue = Join-Path $repo 'build\examples\scsp_dsp_demo.cue'
    $wav = Join-Path $dir 'mednafen.wav'
    if (Test-Path $wav) { Remove-Item $wav -Force }
    $emu = Start-Process -FilePath $mednafenExe -PassThru -ArgumentList @(
        '-force_module', 'ss', '-ss.region_autodetect', '1', '-ss.region_default', 'na', '-video.fs', '0',
        '-video.driver', 'sdl', '-sound.rate', '44100', '-soundrecord', "`"$wav`"", "`"$cue`"")
    Start-Sleep -Seconds $MednafenSeconds
    [void]$emu.CloseMainWindow()
    Start-Sleep -Seconds 3
    if (-not $emu.HasExited) { Stop-Process -Id $emu.Id -Force }
    $env:LIBSATURN_DSP_WAV = $wav
} else {
    Remove-Item Env:\LIBSATURN_DSP_WAV -ErrorAction SilentlyContinue
}

python -m unittest discover (Join-Path $PSScriptRoot 'tests') -p test_scsp_dsp.py -v
if ($LASTEXITCODE -ne 0) { throw "SCSP DSP acceptance failed; inspect $dir" }
Write-Host "[dsp] Passed. Capture: $dir"
