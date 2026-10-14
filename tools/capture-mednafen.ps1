[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][int]$ProcessId,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference='Stop'
$emulator=Get-Process -Id $ProcessId
if($emulator.ProcessName -ne 'mednafen' -or $emulator.MainWindowHandle -eq 0) {
    throw 'The selected process must be a running Mednafen window.'
}
$snaps=Join-Path (Split-Path $emulator.Path) 'snaps'
$previous=@{}
Get-ChildItem -LiteralPath $snaps -Filter '*.png' -ErrorAction SilentlyContinue |
    ForEach-Object {$previous[$_.FullName]=$_.LastWriteTimeUtc.Ticks}
if(-not ('LibSaturnCapture.Window' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
namespace LibSaturnCapture {
    public static class Window {
        [DllImport("user32.dll")]
        public static extern bool PostMessage(IntPtr window, uint message, IntPtr key, IntPtr data);
        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr window);
    }
}
'@
}
$keyDown=0x0100
$keyUp=0x0101
$screenshotKey=0x78 # F9, Mednafen's default screenshot binding.
[void][LibSaturnCapture.Window]::SetForegroundWindow($emulator.MainWindowHandle)
Start-Sleep -Milliseconds 200
[void][LibSaturnCapture.Window]::PostMessage($emulator.MainWindowHandle,$keyDown,[IntPtr]$screenshotKey,[IntPtr]0x00430001)
Start-Sleep -Milliseconds 500
[void][LibSaturnCapture.Window]::PostMessage($emulator.MainWindowHandle,$keyUp,[IntPtr]$screenshotKey,[IntPtr]3225616385)
$deadline=[DateTime]::UtcNow.AddSeconds(10)
do {
    $shot=Get-ChildItem -LiteralPath $snaps -Filter '*.png' -ErrorAction SilentlyContinue |
        Where-Object {-not $previous.ContainsKey($_.FullName) -or $previous[$_.FullName] -ne $_.LastWriteTimeUtc.Ticks} |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if($shot) {
        $target=[IO.Path]::GetFullPath($Output)
        [IO.Directory]::CreateDirectory((Split-Path $target)) | Out-Null
        Copy-Item -LiteralPath $shot.FullName -Destination $target
        Write-Output "[capture-mednafen] $target"
        return
    }
    Start-Sleep -Milliseconds 200
} while([DateTime]::UtcNow -lt $deadline)
throw 'Mednafen did not write a screenshot with the default F9 binding.'
