[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][int]$ProcessId,
    [Parameter(Mandatory=$true)][ValidateSet('Enter','Escape','Left','Right','Up','Down','J','K','L')][string]$Key,
    [ValidateRange(50,5000)][int]$HoldMilliseconds=250
)
$ErrorActionPreference='Stop'
$emulator=Get-Process -Id $ProcessId -ErrorAction Stop
if($emulator.ProcessName -ne 'mednafen' -or $emulator.MainWindowHandle -eq 0){throw 'Select a visible Mednafen process.'}
if(-not ('LibSaturnInput.Window' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
namespace LibSaturnInput {
    public static class Window {
        [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint message,IntPtr key,IntPtr data);
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
        [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint key,uint mapType);
    }
}
'@
}
$keys=@{Enter=0x0D;Escape=0x1B;Left=0x25;Right=0x27;Up=0x26;Down=0x28;J=0x4A;K=0x4B;L=0x4C}
$virtualKey=$keys[$Key]
$scan=[LibSaturnInput.Window]::MapVirtualKey($virtualKey,0)
$keyData=[int64](($scan -shl 16) -bor 1)
if($Key -in @('Left','Right','Up','Down')){$keyData=$keyData -bor 0x01000000}
[void][LibSaturnInput.Window]::SetForegroundWindow($emulator.MainWindowHandle)
Start-Sleep -Milliseconds 200
[void][LibSaturnInput.Window]::PostMessage($emulator.MainWindowHandle,0x0100,[IntPtr]$virtualKey,[IntPtr]$keyData)
Start-Sleep -Milliseconds $HoldMilliseconds
[void][LibSaturnInput.Window]::PostMessage($emulator.MainWindowHandle,0x0101,[IntPtr]$virtualKey,[IntPtr]($keyData -bor 0xC0000000L))
Write-Output "[send-mednafen-key] $Key -> process $ProcessId"
