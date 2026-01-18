#!/usr/bin/env pwsh
# Saturn Development Environment Setup - libsaturn Automation Script v2.0.1 (Fixed)

param(
    [switch]$Express,
    [switch]$Resume,
    [switch]$Rollback,
    [string]$InstallPath = "$env:USERPROFILE\saturn-sdk",
    [string]$Emulator = "Kronos",
    [switch]$SkipEmulator,
    [switch]$SkipVSCode,
    [int]$DownloadRetry = 3
)

$ErrorActionPreference = "Stop"
$Script:Version = "2.0.1"
$PSCommandPath = $MyInvocation.MyCommand.Path

# Ensure we use TLS 1.2 (Fixes GitHub download errors on some systems)
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor [System.Net.SecurityProtocolType]::Tls12

$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$PSDefaultParameterValues['Out-File:Encoding'] = 'utf8'

if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandArgumentPassing = "Windows"
}

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# CONFIGURATION
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

$Script:Config = @{
    InstallPath = $InstallPath
    DownloadRetry = $DownloadRetry # Fixed: Added this key so it is available globally
    RepositoryUrl = "https://github.com/celsoastro/libsaturn.git"
    UseLocalRepository = $true
    StateFile = "$env:TEMP\saturn-setup-state.json"
    LogFile = "$env:TEMP\saturn-setup.log"
    EmulatorChoice = $Emulator
    
    Toolchain = @{
        Name = "SH-ELF GCC (Jo Engine)"
        Version = "Based on GCC 6.2.0"
        Url = ""
        SizeMB = 50
        InstallDir = "sh-elf-gcc"
        BinPath = $null
        ManualUrl = "https://www.jo-engine.org"
        BundledToolchain = $true
        MSYS2InstallPath = "C:\msys64"
        MSYS2Pkg = "mingw-w64-i686-sh-elf-gcc"
        MSYS2PkgFallback = "mingw-w64-x86_64-sh-elf-gcc"
        MSYS2BinDirs = @(
            "mingw32\bin",
            "mingw64\bin",
            "ucrt64\bin",
            "clang32\bin",
            "clang64\bin",
            "clangarm64\bin"
        )
    }
    
    Python = @{
        Version = "3.11.8"
        Url = "https://www.python.org/ftp/python/3.11.8/python-3.11.8-amd64.exe"
        InstallArgs = "/quiet InstallAllUsers=0 PrependPath=1 Include_pip=1 Include_tcltk=0 Include_test=0"
    }
    
    Emulators = @{
        Kronos = @{
            Name = "Kronos"
            Url = "https://github.com/FCare/Kronos/releases/download/v2.7.2/Kronos-v2.7.2-Windows.zip"
            SizeMB = 25
            InstallDir = "Kronos"
            Description = "Advanced Sega Saturn emulator with high compatibility"
        }
        YabaSanshiro = @{
            Name = "YabaSanshiro"
            Url = "https://github.com/devmiyax/yabause/releases/download/v0.1.4/yabause_wiiu_v1.4.zip"
            SizeMB = 15
            InstallDir = "YabaSanshiro"
            Description = "Alternative emulator with unique features"
        }
    }
    
    VSCodeExtensions = @(
        "ms-vscode.cpptools-extension-pack",
        "ms-vscode.cpptools-themes"
    )
}

$Script:State = @{
    CompletedSteps = @()
    LastStep = $null
    StartedAt = $null
    ToolchainInstalled = $false
    PythonInstalled = $false
    RepositoryCloned = $false
    LibraryBuilt = $false
    EmulatorsInstalled = $false
    VSCodeConfigured = $false
}


# Module loading
$setupModuleRoot = Join-Path $PSScriptRoot "scripts\\setup"
$setupModules = @(
    "ui.ps1",
    "logging.ps1",
    "state.ps1",
    "prerequisites.ps1",
    "download.ps1",
    "install.ps1",
    "rollback.ps1",
    "main.ps1"
)
foreach ($module in $setupModules) {
    $modulePath = Join-Path $setupModuleRoot $module
    if (-not (Test-Path $modulePath)) {
        throw "Missing setup module: $modulePath"
    }
    . $modulePath
}

Main
