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

# ═══════════════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════════

$Script:Config = @{
    InstallPath = $InstallPath
    DownloadRetry = $DownloadRetry # Fixed: Added this key so it is available globally
    RepositoryUrl = "https://github.com/celsoastro/libsaturn.git"
    UseLocalRepository = $true
    StateFile = "$env:TEMP\saturn-setup-state.json"
    LogFile = "$env:TEMP\saturn-setup.log"
    EmulatorChoice = $Emulator
    
    Toolchain = @{
        Name = "SH-ELF GCC"
        Version = "13.2.0"
        Url = ""
        SizeMB = 100
        InstallDir = "sh-elf-gcc"
        BinPath = $null
        ManualUrl = "https://github.com/SaturnSDK/Saturn-SDK-GCC-SH2"
        MSYS2InstallPath = "C:\msys64"
        MSYS2Pkg = "mingw-w64-x86_64-sh-elf-gcc"
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

# ═══════════════════════════════════════════════════════════════════════════════
# UI FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Write-Banner {
    param([string]$Subtitle = "")
    
    $host.UI.RawUI.WindowTitle = "Saturn Setup - libsaturn"
    
    Write-Host ""
    Write-Host "========================================================" -ForegroundColor Cyan
    Write-Host "  SATURN DEVELOPMENT ENVIRONMENT SETUP                 " -ForegroundColor Green
    Write-Host "                   libsaturn v$Script:Version" -ForegroundColor White
    if ($Subtitle) {
        Write-Host "                   $Subtitle" -ForegroundColor Yellow
    }
    Write-Host "========================================================" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Section {
    param([string]$Message)
    Write-Host ""
    Write-Host "--- $Message ---" -ForegroundColor Cyan
}

function Write-Subsection {
    param([string]$Message)
    Write-Host "  $Message" -ForegroundColor White
}

function Write-Success {
    param([string]$Message)
    Write-Host "  [OK] $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "  [X] $Message" -ForegroundColor Red
}

function Write-Warning {
    param([string]$Message)
    Write-Host "  [!] $Message" -ForegroundColor Yellow
}

function Write-Info {
    param([string]$Message)
    Write-Host "  [i] $Message" -ForegroundColor Cyan
}

function Write-ProgressBar {
    param(
        [string]$Activity,
        [string]$Status,
        [int]$PercentComplete,
        [int]$SecondsRemaining = 0
    )
    
    $width = 30
    $filled = [int]($width * $PercentComplete / 100)
    $empty = $width - $filled
    
    $bar = "  Progress: [" + ("=" * $filled) + (" " * $empty) + "] $PercentComplete%"
    
    if ($SecondsRemaining -gt 0) {
        $eta = [TimeSpan]::FromSeconds($SecondsRemaining)
        $etaStr = $eta.ToString("hh\:mm\:ss")
        $bar += " (ETA: $etaStr)"
    }
    
    Write-Host $bar -ForegroundColor Magenta
    Write-Progress -Activity $Activity -Status $Status -PercentComplete $PercentComplete -SecondsRemaining $SecondsRemaining
}

function Complete-Section {
    Write-Host "--------------------------------------------------------" -ForegroundColor Cyan
}

function Confirm-Prompt {
    param(
        [string]$Message,
        [bool]$Default = $true
    )
    
    $defaultText = if ($Default) { "[Y/n]" } else { "[y/N]" }
    $selection = Read-Host "$Message $defaultText"
    
    if ([string]::IsNullOrEmpty($selection)) {
        return $Default
    }
    
    return $selection -eq "y" -or $selection -eq "Y"
}

function Select-Option {
    param(
        [string]$Title,
        [hashtable]$Options
    )
    
    Write-Section $Title
    
    $optionList = @()
    $i = 1
    foreach ($key in $Options.Keys) {
        $displayValue = $Options[$key]
        Write-Host ("    " + $i + ") " + $displayValue) -ForegroundColor White
        $optionList += $key
        $i++
    }
    
    Write-Host ""
    
    do {
        $selection = Read-Host ("Select option (1-" + $optionList.Count + ")")
    } while ($selection -notmatch "^\d+$" -or [int]$selection -lt 1 -or [int]$selection -gt $optionList.Count)
    
    return $optionList[[int]$selection - 1]
}

# ═══════════════════════════════════════════════════════════════════════════════
# LOGGING FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Start-Logging {
    if (Test-Path $Script:Config.LogFile) {
        Remove-Item $Script:Config.LogFile -Force
    }
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Add-Content -Path $Script:Config.LogFile -Value "[$timestamp] Saturn Setup Started v$Script:Version"
}

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"
    Add-Content -Path $Script:Config.LogFile -Value $logEntry
    
    if ($Level -eq "ERROR") {
        Write-Error $Message
    } elseif ($Level -eq "WARNING") {
        Write-Warning $Message
    } elseif ($Level -eq "SUCCESS") {
        Write-Success $Message
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# STATE MANAGEMENT
# ═══════════════════════════════════════════════════════════════════════════════

function Save-State {
    $Script:State.LastStep = Get-Date
    $Script:State | ConvertTo-Json -Depth 10 | Set-Content -Path $Script:Config.StateFile -Encoding UTF8
    Write-Log "State saved" "INFO"
}

function Load-State {
    if (Test-Path $Script:Config.StateFile) {
        try {
            $loadedState = Get-Content -Path $Script:Config.StateFile -Raw | ConvertFrom-Json -ErrorAction Stop
            $Script:State = $loadedState
            Write-Log "Previous state loaded" "INFO"
            return $true
        } catch {
            Write-Warning "Could not load previous state, starting fresh"
            return $false
        }
    }
    return $false
}

function Clear-State {
    if (Test-Path $Script:Config.StateFile) {
        Remove-Item $Script:Config.StateFile -Force
        Write-Log "State cleared" "INFO"
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# PREREQUISITE FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Test-Command {
    param([string]$Command)
    try {
        $null = Get-Command $Command -ErrorAction Stop -CommandType Application | Where-Object { $_.Name -eq $Command }
        if ($null -ne $?) { return $true }
        return $false
    } catch {
        return $false
    }
}

function Test-InternetConnection {
    $testUrls = @(
        "https://github.com",
        "https://www.python.org",
        "https://raw.githubusercontent.com"
    )
    
    foreach ($url in $testUrls) {
        try {
            Write-Info ("Testing connection to: " + $url)
            $request = [System.Net.WebRequest]::Create($url)
            $request.Timeout = 5000
            $response = $request.GetResponse()
            $response.Close()
            Write-Success ("Connected to: " + $url)
            return $true
        } catch {
            Write-Warning ("Failed to reach " + $url + ": " + $_.Exception.Message)
        }
    }
    
    Write-Error "Cannot reach any test URL. Check your internet connection or proxy settings."
    return $false
}

function Test-GitInstalled {
    if (Test-Command "git") {
        try {
            $version = & git --version 2>&1
            Write-Info "Git found: $version"
            return $true
        } catch {
            return $false
        }
    }
    return $false
}

function Test-PythonInstalled {
    if (Test-Command "python") {
        try {
            $version = & python --version 2>&1
            Write-Info "Python found: $version"
            return $true
        } catch {
            return $false
        }
    }
    return $false
}

function Show-PrerequisitesCheck {
    param([bool]$RequireAdmin = $false)
    
    Write-Section "CHECKING PREREQUISITES"
    
    $issues = @()
    $warnings = @()
    
    Write-Info "Checking internet connection..."
    if (Test-InternetConnection) {
        Write-Success "Internet connection available"
    } else {
        Write-Error "No internet connection - downloads will fail"
        $issues += "Internet connection"
    }
    
    Write-Info "Checking Git installation..."
    if (Test-GitInstalled) {
        Write-Success "Git is installed"
    } else {
        Write-Warning "Git not found - will attempt to install"
        $warnings += "Git"
    }
    
    Write-Info "Checking Python installation..."
    if (Test-PythonInstalled) {
        Write-Success "Python is installed"
    } else {
        Write-Warning "Python not found - will be installed"
        $warnings += "Python"
    }
    
    if ($RequireAdmin) {
        Write-Info "Checking administrator privileges..."
        if (Test-Administrator) {
            Write-Success "Running as administrator"
        } else {
            Write-Warning "Not running as administrator - PATH modifications may fail"
            $warnings += "Administrator rights"
        }
    }
    
    Complete-Section
    
    if ($issues.Count -gt 0) {
        Write-Host ""
        Write-Error "CRITICAL: " + ($issues -join ", ")
        Write-Host "Some downloads may not work without internet access." -ForegroundColor Yellow
        Write-Host ""
    }
    
    if ($warnings.Count -gt 0) {
        Write-Host ""
        Write-Warning "Warnings: " + ($warnings -join ", ")
        Write-Host ""
    }
    
    if ($issues.Count -gt 0) {
        if (Confirm-Prompt "Continue anyway? (Some steps may fail)") {
            return $true
        }
        return $false
    }
    
    return $true
}

function Install-Git {
    Write-Section "INSTALLING GIT"
    
    $gitUrl = "https://github.com/git-for-windows/git/releases/download/v2.44.0.windows.2/Git-2.44.0.2-64-bit.exe"
    $installerPath = "$env:TEMP\git-installer.exe"
    
    Write-Info "Downloading Git installer..."
    try {
        Invoke-WebRequest -Uri $gitUrl -OutFile $installerPath -UseBasicParsing
        
        Write-Info "Installing Git (this may take a moment)..."
        $process = Start-Process -FilePath $installerPath -ArgumentList "/SILENT" -Wait -PassThru
        
        if ($process.ExitCode -eq 0) {
            Write-Success "Git installed successfully"
            
            $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
            
            Remove-Item $installerPath -Force -ErrorAction SilentlyContinue
            return $true
        } else {
            Write-Error ("Git installation failed (exit code: " + $process.ExitCode + ")")
            return $false
        }
    } catch {
        Write-Error ("Failed to install Git: " + $_.Exception.Message)
        return $false
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# DOWNLOAD FUNCTIONS (FIXED)
# ═══════════════════════════════════════════════════════════════════════════════

function Invoke-DownloadFile {
    param(
        [string]$Url,
        [string]$OutputPath,
        [string]$Description,
        [int]$MaxRetries = 3
    )
    
    $parentDir = Split-Path $OutputPath -Parent
    if (-not (Test-Path $parentDir)) {
        New-Item -ItemType Directory -Path $parentDir -Force | Out-Null
    }
    
    $attempt = 1
    $success = $false
    $lastError = ""
    
    # Logic changed to do-while to guarantee at least one execution
    do {
        try {
            Write-Info ("Downloading " + $Description + " (attempt " + $attempt + " of " + $MaxRetries + ")...")
            
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($Url, $OutputPath)
            $webClient.Dispose()
            
            $size = (Get-Item $OutputPath).Length / 1MB
            Write-Success ("Downloaded " + $Description + " (" + ([math]::Round($size, 2)) + " MB)")
            $success = $true
        } catch {
            $lastError = $_.Exception.Message
            Write-Warning ("Download attempt " + $attempt + " failed: " + $lastError)
            if ($attempt -lt $MaxRetries) {
                $waitTime = $attempt * 5
                Write-Info ("Retrying in " + $waitTime + " seconds...")
                Start-Sleep -Seconds $waitTime
            }
            $attempt++
        }
    } while ($attempt -le $MaxRetries -and -not $success)
    
    if (-not $success) {
        Write-Error ("Failed to download " + $Description + " after " + $MaxRetries + " attempts")
        Write-Error ("Last error: " + $lastError)
        Write-Info ("URL: " + $Url)
        Write-Info ("Output path: " + $OutputPath)
    }
    
    return $success
}

function Expand-ZipArchive {
    param(
        [string]$ArchivePath,
        [string]$DestinationPath,
        [string]$Description
    )
    
    Write-Info ("Extracting " + $Description + "...")
    
    try {
        if (-not (Test-Path $DestinationPath)) {
            New-Item -ItemType Directory -Path $DestinationPath -Force | Out-Null
        }
        
        Expand-Archive -Path $ArchivePath -DestinationPath $DestinationPath -Force -ErrorAction Stop
        Write-Success ("Extracted " + $Description)
        return $true
    } catch {
        Write-Error ("Failed to extract " + $Description + ": " + $_.Exception.Message)
        return $false
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# INSTALLATION FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Add-ToPathEnvironment {
    param([string]$PathToAdd)
    
    if (-not (Test-Administrator)) {
        Write-Warning "Cannot modify system PATH without administrator privileges"
        Write-Info "To add to PATH manually, run PowerShell as administrator"
        return $false
    }
    
    try {
        $currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
        if ($currentPath -notlike "*$PathToAdd*") {
            [Environment]::SetEnvironmentVariable("Path", $currentPath + ";" + $PathToAdd, "Machine")
            Write-Success ("Added to PATH: " + $PathToAdd)
            return $true
        } else {
            Write-Info ("Already in PATH: " + $PathToAdd)
            return $true
        }
    } catch {
        Write-Error ("Failed to modify PATH: " + $_.Exception.Message)
        return $false
    }
}

function Add-ToSessionPath {
    param([string]$PathToAdd)
    $currentPath = $env:Path
    if ($currentPath -notlike "*$PathToAdd*") {
        $env:Path = $PathToAdd + ";" + $currentPath
        Write-Info ("Added to session PATH: " + $PathToAdd)
    }
}

function Install-MSYS2-Winget {
    Write-Info "Attempting to install MSYS2 using winget..."
    
    if (-not (Test-Command "winget")) {
        Write-Warning "winget not found"
        return $false
    }
    
    try {
        $msys2InstallPath = $Script:Config.Toolchain.MSYS2InstallPath
        
        if (Test-Path $msys2InstallPath) {
            Write-Success "MSYS2 already installed at: $msys2InstallPath"
            return $true
        }
        
        Write-Info "Installing MSYS2 via winget..."
        $process = Start-Process -FilePath "winget" -ArgumentList "install", "-e", "--id", "MSYS2.MSYS2", "--accept-package-agreements", "--accept-source-agreements", "--silent" -Wait -PassThru -NoNewWindow
        
        if ($process.ExitCode -eq 0 -or $process.ExitCode -eq 1641) {
            Write-Success "MSYS2 installed successfully"
            Start-Sleep -Seconds 2
            return $true
        } else {
            Write-Warning ("winget installation failed with exit code: " + $process.ExitCode)
            return $false
        }
    } catch {
        Write-Error ("winget installation failed: " + $_.Exception.Message)
        return $false
    }
}

function Install-MSYS2-Chocolatey {
    Write-Info "Attempting to install MSYS2 using Chocolatey..."
    
    if (-not (Test-Command "choco")) {
        Write-Warning "Chocolatey not found"
        Write-Info "To install Chocolatey, run as administrator:"
        Write-Info 'Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString("https://community.chocolatey.org/install.ps1"))'
        return $false
    }
    
    try {
        $msys2InstallPath = $Script:Config.Toolchain.MSYS2InstallPath
        
        if (Test-Path $msys2InstallPath) {
            Write-Success "MSYS2 already installed at: $msys2InstallPath"
            return $true
        }
        
        Write-Warning "Chocolatey requires administrator privileges"
        if (-not (Test-Administrator)) {
            Write-Error "Not running as administrator. Chocolatey will likely fail."
            Write-Info "Please run PowerShell as Administrator and try again."
            Write-Info "Or try option 4 (Manual) to enter path to existing installation."
            return $false
        }
        
        Write-Info "Checking for Chocolatey lock files..."
        $chocolateyLib = "C:\ProgramData\chocolatey\lib"
        $lockFiles = Get-ChildItem -Path $chocolateyLib -Filter "*.lock" -ErrorAction SilentlyContinue
        
        if ($lockFiles.Count -gt 0) {
            Write-Warning "Found Chocolatey lock files from previous installation:"
            foreach ($lock in $lockFiles) {
                Write-Info "  - $($lock.FullName)"
            }
            Write-Info "Attempting to remove lock files..."
            try {
                $lockFiles | Remove-Item -Force -ErrorAction Stop
                Write-Success "Lock files removed"
            } catch {
                Write-Error "Could not remove lock files: $($_.Exception.Message)"
                Write-Info "Try running as Administrator or remove manually:"
                Write-Info "  Remove-Item 'C:\ProgramData\chocolatey\lib\*.lock' -Force"
                Write-Info ""
                Write-Host "Or try Option 1 (Direct Download) which doesn't require Chocolatey." -ForegroundColor Green
                return $false
            }
        }
        
        $libBadPath = "C:\ProgramData\chocolatey\lib-bad"
        if (Test-Path $libBadPath) {
            Write-Warning "Found Chocolatey lib-bad directory"
            Write-Info "Attempting to remove..."
            try {
                Remove-Item $libBadPath -Recurse -Force -ErrorAction Stop
                Write-Success "lib-bad directory removed"
            } catch {
                Write-Error "Could not remove lib-bad: $($_.Exception.Message)"
                Write-Info "Try running as Administrator to clean up"
                return $false
            }
        }
        
        Write-Info "Installing MSYS2 via Chocolatey..."
        Write-Info "This may take several minutes..."
        $process = Start-Process -FilePath "choco" -ArgumentList "install", "msys2", "-y", "--params", ('"/InstallDir:{0}"' -f $msys2InstallPath) -Wait -PassThru -NoNewWindow
        
        if ($process.ExitCode -eq 0) {
            Write-Success "MSYS2 installed successfully"
            Start-Sleep -Seconds 2
            return $true
        } else {
            Write-Warning ("Chocolatey installation failed with exit code: " + $process.ExitCode)
            Write-Info "Check Chocolatey log for details: C:\ProgramData\chocolatey\logs\chocolatey.log"
            return $false
        }
    } catch {
        Write-Error ("Chocolatey installation failed: " + $_.Exception.Message)
        return $false
    }
}

function Invoke-MSYS2Pacman {
    param(
        [string]$Command,
        [bool]$ShowOutput = $false
    )
    
    $msys2InstallPath = $Script:Config.Toolchain.MSYS2InstallPath
    $bashPath = Join-Path $msys2InstallPath "usr\bin\bash.exe"
    
    if (-not (Test-Path $bashPath)) {
        Write-Error "MSYS2 bash not found at: $bashPath"
        return $false
    }
    
    Write-Info "Running in MSYS2: $Command"
    
    try {
        $msys2Args = "-lc", "export PATH='/usr/bin:$PATH';", $Command
        $windowStyle = if ($ShowOutput) { "Normal" } else { "Hidden" }
        $process = Start-Process -FilePath $bashPath -ArgumentList $msys2Args -Wait -PassThru -NoNewWindow -WindowStyle $windowStyle
        
        return $process.ExitCode -eq 0
    } catch {
        Write-Error ("Failed to run MSYS2 command: " + $_.Exception.Message)
        return $false
    }
}

function Install-MSYS2-Direct {
    Write-Section "DOWNLOADING MSYS2 DIRECTLY"
    
    $msys2InstallPath = $Script:Config.Toolchain.MSYS2InstallPath
    
    if (Test-Path $msys2InstallPath) {
        Write-Success "MSYS2 already installed at: $msys2InstallPath"
        return $true
    }
    
    $msys2Url = "https://github.com/msys2/msys2-installer/releases/download/2023-10-26/msys2-x86_64-20231026.exe"
    $msys2Version = "20231026"
    $installerPath = "$env:TEMP\msys2-installer.exe"
    
    Write-Info "Downloading MSYS2 installer (approx. 100MB)..."
    Write-Info "Source: $msys2Url"
    
    $downloadSuccess = Invoke-DownloadFile -Url $msys2Url -OutputPath $installerPath -Description "MSYS2 Installer"
    
    if (-not $downloadSuccess) {
        Write-Error "Failed to download MSYS2 installer"
        return $false
    }
    
    Write-Info "Installing MSYS2 to: $msys2InstallPath"
    Write-Info "This will take 2-5 minutes..."
    
    try {
        $installArgs = @(
            "install",
            "--confirm-command",
            "--accept-messages",
            "--root", $msys2InstallPath
        )
        
        $process = Start-Process -FilePath $installerPath -ArgumentList $installArgs -Wait -PassThru -NoNewWindow -WindowStyle Normal
        
        Remove-Item $installerPath -Force -ErrorAction SilentlyContinue
        
        if ($process.ExitCode -eq 0) {
            Write-Success "MSYS2 installed successfully"
            Start-Sleep -Seconds 2
            return $true
        } else {
            Write-Error ("MSYS2 installation failed with exit code: " + $process.ExitCode)
            Write-Info "Installer exit code $process.ExitCode may indicate:"
            Write-Info "  - Installation was cancelled"
            Write-Info "  - Another instance is running"
            Write-Info "  - Insufficient permissions"
            return $false
        }
    } catch {
        Write-Error ("MSYS2 installation failed: " + $_.Exception.Message)
        return $false
    }
}

function Restart-AsAdministrator {
    Write-Warning "This operation requires administrator privileges"
    $currentScript = $PSCommandPath
    
    if ([string]::IsNullOrEmpty($currentScript)) {
        Write-Error "Could not determine script path"
        return $false
    }
    
    Write-Host ""
    Write-Host "Would you like to restart as Administrator?" -ForegroundColor Yellow
    $choice = Read-Host "Press Y to restart, N to continue without admin [Y/N]"
    
    if ($choice -eq "Y" -or $choice -eq "y") {
        $psiArgs = "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$currentScript`""
        
        foreach ($arg in $MyInvocation.BoundParameters.GetEnumerator()) {
            if ($arg.Value -is [switch] -and $arg.Value) {
                $psiArgs += "-$($arg.Key)"
            } elseif ($arg.Value -isnot [switch] -and $arg.Value -ne $null) {
                $psiArgs += "-$($arg.Key)", "`"$($arg.Value)`""
            }
        }
        
        $psi = New-Object Diagnostics.ProcessStartInfo
        $psi.FileName = "powershell.exe"
        $psi.Arguments = $psiArgs -join " "
        $psi.Verb = "runas"
        $psi.UseShellExecute = $true
        
        Write-Info "Restarting script as Administrator..."
        [Diagnostics.Process]::Start($psi) | Out-Null
        exit 0
    }
    
    return $false
}

function Install-ShElfGcc-MSYS2 {
    Write-Section "INSTALLING SH-ELF GCC VIA MSYS2"
    
    $msys2InstallPath = $Script:Config.Toolchain.MSYS2InstallPath
    
    if (-not (Test-Path $msys2InstallPath)) {
        Write-Error "MSYS2 not found at: $msys2InstallPath"
        Write-Info "Please install MSYS2 first"
        return $false
    }
    
    $mingw64BinPath = Join-Path $msys2InstallPath "mingw64\bin"
    $gccPath = Join-Path $mingw64BinPath "sh-elf-gcc.exe"
    
    if (Test-Path $gccPath) {
        Write-Success "SH-ELF GCC already installed"
        Add-ToSessionPath $mingw64BinPath
        return $true
    }
    
    Write-Info "Updating MSYS2 package databases..."
    if (-not (Invoke-MSYS2Pacman "pacman --noconfirm -Sy")) {
        Write-Warning "Failed to update package database, continuing..."
    }
    
    Write-Info "Installing SH-ELF GCC (this may take several minutes)..."
    $pkg = $Script:Config.Toolchain.MSYS2Pkg
    
    if (Invoke-MSYS2Pacman "pacman --noconfirm -S $pkg") {
        if (Test-Path $gccPath) {
            Write-Success "SH-ELF GCC installed successfully"
            Add-ToSessionPath $mingw64BinPath
            
            if (Test-Administrator) {
                Add-ToPathEnvironment $mingw64BinPath
            } else {
                Write-Warning "Administrator rights required to add to system PATH"
                Write-Info "Run as administrator to persist PATH changes"
            }
            
            return $true
        } else {
            Write-Error "Installation appeared successful but sh-elf-gcc not found"
            return $false
        }
    } else {
        Write-Error "Failed to install SH-ELF GCC"
        return $false
    }
}

function Install-Toolchain {
    param([string]$InstallPath)
    
    Write-Section "INSTALLING SH-ELF TOOLCHAIN"
    
    if ($Script:State.ToolchainInstalled) {
        Write-Success "Toolchain already installed, skipping"
        return $true
    }
    
    $toolchainInstallDir = Join-Path $InstallPath $Script:Config.Toolchain.InstallDir
    $binPath = Join-Path $toolchainInstallDir "bin"
    $msys2InstallPath = $Script:Config.Toolchain.MSYS2InstallPath
    
    # Check if toolchain is already installed
    Write-Info "Checking for existing SH-ELF toolchain..."
    $shGccPaths = @(
        (Join-Path $msys2InstallPath "mingw64\bin\sh-elf-gcc.exe"),
        "C:\sh-elf-gcc\bin\sh-elf-gcc.exe",
        "C:\sh-elf-gcc\bin\sh-elf-gcc",
        "$env:ProgramFiles\sh-elf-gcc\bin\sh-elf-gcc.exe",
        (Join-Path $InstallPath "mcx-sdk\bin\sh-elf-gcc.exe"),
        (Join-Path $InstallPath "sh-elf-gcc\bin\sh-elf-gcc.exe")
    )
    
    $existingToolchain = $null
    foreach ($path in $shGccPaths) {
        if (Test-Path $path) {
            $existingToolchain = $path
            break
        }
    }
    
    if ($existingToolchain) {
        Write-Success ("Found existing toolchain: " + $existingToolchain)
        $env:Path = (Split-Path $existingToolchain) + ";" + $env:Path
        $Script:State.ToolchainInstalled = $true
        Save-State
        Complete-Section
        return $true
    }
    
    # Check if sh-elf-gcc is in PATH
    if (Test-Command "sh-elf-gcc") {
        try {
            $version = & sh-elf-gcc --version 2>&1 | Select-Object -First 1
            Write-Success ("SH-ELF GCC found in PATH: " + $version)
            $Script:State.ToolchainInstalled = $true
            Save-State
            Complete-Section
            return $true
        } catch {
            Write-Warning "sh-elf-gcc command found but version check failed"
        }
    }
    
    Write-Host ""
    Write-Host "=== SH-ELF TOOLCHAIN INSTALLATION ===" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "The SH-ELF toolchain is required to compile Saturn homebrew programs." -ForegroundColor White
    Write-Host ""
    
    $isAdmin = Test-Administrator
    
    if (-not $isAdmin) {
        Write-Warning "Not running as Administrator"
        Write-Info "Some installation options require elevated privileges."
        Write-Host ""
    }
    
    Write-Host "I can attempt to install it automatically using MSYS2." -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Choose installation method:" -ForegroundColor Cyan
    Write-Host "  1) Direct Download - Download and install MSYS2 directly (No admin required)" -ForegroundColor Green
    Write-Host "  2) Automatic - Try winget, then Chocolatey (Admin required)" -ForegroundColor White
    Write-Host "  3) Automatic - Use winget only (Admin required)" -ForegroundColor White
    Write-Host "  4) Automatic - Use Chocolatey only (Admin required)" -ForegroundColor White
    Write-Host "  5) Manual - Enter path to existing installation" -ForegroundColor White
    Write-Host "  6) Manual - Show manual installation instructions" -ForegroundColor White
    Write-Host "  7) Skip - Continue without toolchain (Build will fail)" -ForegroundColor Yellow
    Write-Host ""
    
    $choice = Read-Host "Select option (1-7)"
    
    $autoInstall = $false
    $installSuccess = $false
    
    switch ($choice) {
        "1" {
            Write-Info "Using direct download (no admin required)..."
            if (Install-MSYS2-Direct) {
                if (Install-ShElfGcc-MSYS2) {
                    $installSuccess = $true
                }
            }
        }
        "2" {
            Write-Info "Trying winget first, then Chocolatey..."
            if (Install-MSYS2-Winget) {
                if (Install-ShElfGcc-MSYS2) {
                    $installSuccess = $true
                }
            }

            if (-not $installSuccess) {
                Write-Info "winget failed, trying Chocolatey..."
                if (-not (Test-Administrator)) {
                    if (Restart-AsAdministrator) {
                        return $true
                    }
                }
                if (Install-MSYS2-Chocolatey) {
                    if (Install-ShElfGcc-MSYS2) {
                        $installSuccess = $true
                    }
                }
            }
        }
        "3" {
            Write-Info "Using winget..."
            if (-not (Test-Administrator)) {
                if (Restart-AsAdministrator) {
                    return $true
                }
            }
            if (Install-MSYS2-Winget) {
                if (Install-ShElfGcc-MSYS2) {
                    $installSuccess = $true
                }
            }
        }
        "4" {
            Write-Info "Using Chocolatey..."
            if (-not (Test-Administrator)) {
                if (Restart-AsAdministrator) {
                    return $true
                }
            }
            if (Install-MSYS2-Chocolatey) {
                if (Install-ShElfGcc-MSYS2) {
                    $installSuccess = $true
                }
            }
        }
        "5" {
            $toolchainPath = Read-Host "Enter path to sh-elf-gcc installation directory (e.g., C:\sh-elf-gcc)"
            
            if ([string]::IsNullOrEmpty($toolchainPath)) {
                Write-Warning "No path provided, skipping"
                return $true
            }
            
            $gccPath = Join-Path $toolchainPath "bin\sh-elf-gcc.exe"
            
            if (Test-Path $gccPath) {
                Write-Success ("Toolchain found: " + $gccPath)
                Add-ToSessionPath (Join-Path $toolchainPath "bin")
                $installSuccess = $true
            } else {
                Write-Error ("Toolchain not found at: " + $gccPath)
                return $false
            }
        }
        "6" {
            Write-Host ""
            Write-Host "=== MANUAL INSTALLATION OPTIONS ===" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "Option 1: Use MSYS2 (Recommended)" -ForegroundColor Cyan
            Write-Host "  1. Download MSYS2: https://www.msys2.org/" -ForegroundColor White
            Write-Host "  2. Open 'MSYS2 MINGW64' terminal" -ForegroundColor White
            Write-Host "  3. Run: pacman -S mingw-w64-x86_64-sh-elf-gcc" -ForegroundColor White
            Write-Host "  4. Add to PATH: C:\msys64\mingw64\bin" -ForegroundColor White
            Write-Host ""
            Write-Host "Option 2: WSL (Windows Subsystem for Linux)" -ForegroundColor Cyan
            Write-Host "  1. Install WSL: wsl --install -d Ubuntu" -ForegroundColor White
            Write-Host "  2. Run: sudo apt install gcc-sh-elf binutils-sh-elf" -ForegroundColor White
            Write-Host "  3. Build using WSL: see WINDOWS_SETUP.md" -ForegroundColor White
            Write-Host ""
            Write-Host "Option 3: Build from source" -ForegroundColor Cyan
            Write-Host "  See: https://github.com/SaturnSDK/Saturn-SDK-GCC-SH2" -ForegroundColor White
            Write-Host ""
            Write-Host "After manual installation, run this script again and select option 5." -ForegroundColor Yellow
            Write-Host ""
            return $true
        }
        "7" {
            Write-Warning "Skipping toolchain installation"
            Write-Info "You can run this script again with: .\setup.ps1 -Resume"
            Write-Info "Build step will likely fail without toolchain"
            return $true
        }
        default {
            Write-Warning "Invalid choice, skipping toolchain installation"
            return $true
        }
    }
    
    if ($installSuccess) {
        Write-Success "Toolchain installation completed"
        $Script:State.ToolchainInstalled = $true
        Save-State
        Complete-Section
        return $true
    } else {
        Write-Error "Automatic installation failed"
        Write-Info "Try manual installation (option 4 or 5) or run script again"
        return $false
    }
}

function Install-Python {
    param([string]$InstallPath)
    
    Write-Section "INSTALLING PYTHON"
    
    if ($Script:State.PythonInstalled) {
        Write-Success "Python already installed, skipping"
        return $true
    }
    
    if (Test-PythonInstalled) {
        Write-Success "Python detected in system"
        $Script:State.PythonInstalled = $true
        Save-State
        Complete-Section
        return $true
    }
    
    $installerPath = "$env:TEMP\python-installer.exe"
    
    Write-ProgressBar -Activity "Downloading Python" -Status "Starting..." -PercentComplete 0
    $downloadSuccess = Invoke-DownloadFile -Url $Script:Config.Python.Url -OutputPath $installerPath -Description ("Python " + $Script:Config.Python.Version)
    
    if (-not $downloadSuccess) {
        Write-Error "Python download failed"
        return $false
    }
    
    Write-ProgressBar -Activity "Downloading Python" -Status "Complete" -PercentComplete 30
    
    Write-ProgressBar -Activity "Installing Python" -Status "Running installer..." -PercentComplete 40
    
    try {
        Write-Info "Running Python installer..."
        $process = Start-Process -FilePath $installerPath -ArgumentList $Script:Config.Python.InstallArgs -Wait -PassThru -NoNewWindow
        
        Write-ProgressBar -Activity "Installing Python" -Status "Complete" -PercentComplete 100
        
        if ($process.ExitCode -eq 0) {
            Remove-Item $installerPath -Force -ErrorAction SilentlyContinue
            
            $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
            
            Start-Sleep -Seconds 2
            if (Test-PythonInstalled) {
                Write-Success "Python installed successfully"
                $Script:State.PythonInstalled = $true
                Save-State
                Complete-Section
                return $true
            } else {
                Write-Warning "Python installed but not detected - may require restart"
                $Script:State.PythonInstalled = $true
                Save-State
                return $true
            }
        } else {
            Write-Error ("Python installation failed (exit code: " + $process.ExitCode + ")")
            return $false
        }
    } catch {
        Write-Error ("Python installation failed: " + $_.Exception.Message)
        return $false
    }
}

function Clone-Repository {
    param(
        [string]$InstallPath,
        [string]$RepositoryUrl = $Script:Config.RepositoryUrl
    )
    
    Write-Section "CLONING LIBSATURN REPOSITORY"
    
    if ($Script:State.RepositoryCloned) {
        Write-Success "Repository already cloned, skipping"
        return $true
    }
    
    $readmePath = Join-Path $InstallPath "README.md"
    
    # Check if we're already in the repository directory
    $currentReadme = Join-Path (Get-Location) "README.md"
    if (Test-Path $currentReadme) {
        Write-Success "Repository found in current directory: " + (Get-Location)
        
        # If InstallPath is different from current directory, copy files
        if ((Get-Location).Path.TrimEnd("\/") -ne $InstallPath.TrimEnd("\/")) {
            Write-Info ("Copying files to installation directory: " + $InstallPath)
            
            # Create directory if needed
            if (-not (Test-Path $InstallPath)) {
                New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
            }
            
            # Copy all items except .git
            Get-ChildItem -Path (Get-Location) -Exclude ".git" | Copy-Item -Destination $InstallPath -Recurse -Force
            
            if (Test-Path $readmePath) {
                Write-Success ("Repository copied to: " + $InstallPath)
                $Script:State.RepositoryCloned = $true
                Save-State
                Complete-Section
                return $true
            }
        } else {
            # We're already in the install path
            Write-Success ("Repository ready at: " + $InstallPath)
            $Script:State.RepositoryCloned = $true
            Save-State
            Complete-Section
            return $true
        }
    }
    
    if (Test-Path $readmePath) {
        Write-Success ("Repository already exists at: " + $InstallPath)
        $Script:State.RepositoryCloned = $true
        Save-State
        Complete-Section
        return $true
    }
    
    if (-not (Test-Path $InstallPath)) {
        New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
    }
    
    if (-not (Test-GitInstalled)) {
        Write-Info "Installing Git..."
        if (-not (Install-Git)) {
            Write-Error "Cannot proceed without Git"
            return $false
        }
    }
    
    Write-Info ("Cloning repository from: " + $RepositoryUrl)
    Write-Info "This may take a moment..."
    
    try {
        Write-ProgressBar -Activity "Cloning Repository" -Status "Starting..." -PercentComplete 0
        
        $tempClonePath = "$env:TEMP\libsaturn-clone"
        if (Test-Path $tempClonePath) {
            Remove-Item $tempClonePath -Recurse -Force
        }
        
        Write-Info "Running: git clone " + $RepositoryUrl
        
        $gitProcess = Start-Process -FilePath "git" -ArgumentList "clone", $RepositoryUrl, $tempClonePath -Wait -PassThru -NoNewWindow
        
        if ($gitProcess.ExitCode -ne 0) {
            Write-Error ("Git clone failed with exit code: " + $gitProcess.ExitCode)
            Write-Error "Possible causes:"
            Write-Error "  - Repository URL is incorrect or repository doesn't exist"
            Write-Error "  - No internet connection"
            Write-Error "  - Git authentication issues (if using private repo)"
            Write-Error ("URL: " + $RepositoryUrl)
            return $false
        }
        
        Write-ProgressBar -Activity "Cloning Repository" -Status "Complete" -PercentComplete 100
        
        Write-Info "Preparing installation directory..."
        Get-ChildItem -Path $tempClonePath | Move-Item -Destination $InstallPath -Force
        
        Remove-Item $tempClonePath -Recurse -Force
        
        if (Test-Path $readmePath) {
            Write-Success ("Repository cloned successfully to: " + $InstallPath)
            $Script:State.RepositoryCloned = $true
            Save-State
            Complete-Section
            return $true
        } else {
            Write-Error "Clone appeared successful but README not found"
            Write-Info "Expected README at: " + $readmePath
            return $false
        }
    } catch {
        Write-Error ("Failed to clone repository: " + $_.Exception.Message)
        return $false
    }
}

function Build-Library {
    param([string]$InstallPath)
    
    Write-Section "BUILDING LIBSATURN LIBRARY"
    
    if ($Script:State.LibraryBuilt) {
        Write-Success "Library already built, skipping"
        return $true
    }
    
    $buildScript = Join-Path $InstallPath "build.bat"
    $outputDir = Join-Path $InstallPath "output"
    
    if (-not (Test-Path $buildScript)) {
        Write-Error ("Build script not found: " + $buildScript)
        Write-Info "Checking for alternative build systems..."
        
        $makefile = Join-Path $InstallPath "Makefile"
        if (Test-Path $makefile) {
            Write-Info "Found Makefile - using alternative build process"
            return Invoke-BuildWithMake -InstallPath $InstallPath
        }
        
        return $false
    }
    
    Write-Info ("Found build script: " + $buildScript)
    Write-Info "Starting build process..."
    
    try {
        $buildStartTime = Get-Date
        
        Write-ProgressBar -Activity "Building Library" -Status "Compiling..." -PercentComplete 0
        
        $buildProcess = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "cd /d", ("`"" + $InstallPath + "`""), "&&", "call", $buildScript -Wait -PassThru -NoNewWindow
        
        $buildDuration = (Get-Date) - $buildStartTime
        
        if ($buildProcess.ExitCode -eq 0) {
            Write-ProgressBar -Activity "Building Library" -Status "Complete" -PercentComplete 100
            Write-Success ("Library built successfully (took " + ($buildDuration.TotalSeconds.ToString("N1")) + " seconds)")
            
            if (Test-Path $outputDir) {
                $outputFiles = Get-ChildItem $outputDir -Recurse -File | Measure-Object
                Write-Info ("Generated " + $outputFiles.Count + " output files")
            }
            
            $Script:State.LibraryBuilt = $true
            Save-State
            Complete-Section
            return $true
        } else {
            Write-Error ("Build failed (exit code: " + $buildProcess.ExitCode + ")")
            Write-Host ""
            Write-Host "=== BUILD TROUBLESHOOTING ===" -ForegroundColor Yellow
            Write-Host "Common causes:" -ForegroundColor Yellow
            Write-Host "  1. Missing toolchain - Run toolchain installation first" -ForegroundColor White
            Write-Host "  2. Toolchain not in PATH - Restart PowerShell or run as admin" -ForegroundColor White
            Write-Host "  3. Missing dependencies - Check README.md for requirements" -ForegroundColor White
            Write-Host "  4. Build script errors - Check " + $buildScript -ForegroundColor White
            Write-Host ""
            Write-Info "Check the build output above for specific error messages."
            return $false
        }
    } catch {
        Write-Error ("Build process failed: " + $_.Exception.Message)
        return $false
    }
}

function Invoke-BuildWithMake {
    param([string]$InstallPath)
    
    Write-Section "BUILDING WITH MAKEFILE"
    
    try {
        Write-Info "Running make..."
        $makeProcess = Start-Process -FilePath "mingw32-make" -ArgumentList "-C", $InstallPath -Wait -PassThru -NoNewWindow
        
        if ($makeProcess.ExitCode -eq 0) {
            Write-Success "Build completed successfully"
            $Script:State.LibraryBuilt = $true
            Save-State
            return $true
        } else {
            Write-Error ("Make failed (exit code: " + $makeProcess.ExitCode + ")")
            return $false
        }
    } catch {
        Write-Error ("Make process not found or failed: " + $_.Exception.Message)
        return $false
    }
}

function Install-Emulator {
    param(
        [string]$InstallPath,
        [string]$EmulatorChoice
    )
    
    if ($SkipEmulator) {
        Write-Section "SKIPPING EMULATOR INSTALLATION"
        Write-Info "Emulator installation skipped by user"
        Complete-Section
        return $true
    }
    
    Write-Section "INSTALLING EMULATOR(S)"
    
    if ($Script:State.EmulatorsInstalled) {
        Write-Success "Emulators already installed, skipping"
        return $true
    }
    
    $emulatorsToInstall = @()
    
    switch ($EmulatorChoice.ToLower()) {
        "kronos" { $emulatorsToInstall = @("Kronos") }
        "yabasanhiro" { $emulatorsToInstall = @("YabaSanshiro") }
        "both" { $emulatorsToInstall = @("Kronos", "YabaSanshiro") }
        default { $emulatorsToInstall = @("Kronos") }
    }
    
    $installed = 0
    $total = $emulatorsToInstall.Count
    
    foreach ($emuName in $emulatorsToInstall) {
        $emuConfig = $Script:Config.Emulators[$emuName]
        $installDir = Join-Path $InstallPath $emuConfig.InstallDir
        $zipPath = "$env:TEMP\" + $emuName + ".zip"
        
        Write-Subsection ("Installing " + $emuName)
        
        $downloadSuccess = Invoke-DownloadFile -Url $emuConfig.Url -OutputPath $zipPath -Description ($emuName + " emulator")
        
        if (-not $downloadSuccess) {
            Write-Warning ("Failed to download " + $emuName)
            continue
        }
        
        $extractSuccess = Expand-ZipArchive -ArchivePath $zipPath -DestinationPath $installDir -Description ($emuName + " emulator")
        
        if ($extractSuccess) {
            Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
            
            $exePattern = if ($emuName -eq "Kronos") { "Kronos.exe" } else { "yabause*.exe" }
            $exePath = Get-ChildItem -Path $installDir -Filter $exePattern -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            
            if ($exePath) {
                Write-Success ($emuName + " installed: " + $exePath.FullName)
            } else {
                Write-Success ($emuName + " installed to: " + $installDir)
            }
            
            $installed++
        }
    }
    
    if ($installed -gt 0) {
        $Script:State.EmulatorsInstalled = $true
        Save-State
        Complete-Section
        return $true
    } else {
        Write-Warning "No emulators were successfully installed"
        return $false
    }
}

function Install-VSCodeExtensions {
    param([string]$InstallPath)
    
    if ($SkipVSCode) {
        Write-Section "SKIPPING VS CODE CONFIGURATION"
        Write-Info "VS Code configuration skipped by user"
        Complete-Section
        return $true
    }
    
    Write-Section "CONFIGURING VS CODE"
    
    if ($Script:State.VSCodeConfigured) {
        Write-Success "VS Code already configured, skipping"
        return $true
    }
    
    if (-not (Test-Command "code")) {
        Write-Warning "VS Code not found - skipping extension installation"
        Write-Info "Install VS Code from https://code.visualstudio.com/"
    } else {
        Write-Info "Installing VS Code extensions..."
        
        foreach ($extension in $Script:Config.VSCodeExtensions) {
            Write-Info ("Installing " + $extension + "...")
            try {
                $proc = Start-Process -FilePath "code" -ArgumentList "--install-extension", $extension -Wait -PassThru -NoNewWindow
                if ($proc.ExitCode -eq 0) {
                    Write-Success ("Installed: " + $extension)
                } else {
                    Write-Warning ("Failed to install: " + $extension)
                }
            } catch {
                Write-Warning ("Could not install " + $extension + ": " + $_.Exception.Message)
            }
        }
    }
    
    Write-Info "Creating VS Code configuration..."
    
    $vscodePath = Join-Path $InstallPath ".vscode"
    New-Item -ItemType Directory -Path $vscodePath -Force | Out-Null
    
    $cCppProperties = @{
        configurations = @(
            @{
                name = "Saturn"
                includePath = @(
                    "`${workspaceFolder}/include",
                    "`${workspaceFolder}/include/saturn"
                )
                compilerPath = "sh-elf-gcc"
                cStandard = "c99"
                intelliSenseMode = "gcc-x86"
            }
        )
        version = 4
    } | ConvertTo-Json -Depth 10
    
    $cCppProperties | Set-Content -Path (Join-Path $vscodePath "c_cpp_properties.json") -Encoding UTF8
    
    $tasks = @{
        version = "2.0.0"
        tasks = @(
            @{
                label = "Build Library"
                type = "shell"
                command = "build.bat"
                problemMatcher = @("$gcc")
                group = @{
                    kind = "build"
                    isDefault = $true
                }
            }
        )
    } | ConvertTo-Json -Depth 10
    
    $tasks | Set-Content -Path (Join-Path $vscodePath "tasks.json") -Encoding UTF8
    
    $launch = @{
        version = "0.2.0"
        configurations = @(
            @{
                name = "Launch Saturn Emulator"
                type = "cppvsdbg"
                request = "launch"
                program = "`${workspaceFolder}/output/*.bin"
                args = @()
                stopAtEntry = $false
                cwd = "`${workspaceFolder}"
                environment = @()
                externalConsole = $true
            }
        )
    } | ConvertTo-Json -Depth 10
    
    $launch | Set-Content -Path (Join-Path $vscodePath "launch.json") -Encoding UTF8
    
    Write-Success "VS Code configuration created"
    $Script:State.VSCodeConfigured = $true
    Save-State
    
    Complete-Section
    return $true
}

# ═══════════════════════════════════════════════════════════════════════════════
# ROLLBACK FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Invoke-Rollback {
    param([string]$InstallPath)
    
    Write-Section "ROLLBACK - CLEANING UP INSTALLATION"
    
    Write-Warning ("This will remove all installed components from: " + $InstallPath)
    
    if (-not (Confirm-Prompt "Are you sure you want to proceed?" $false)) {
        Write-Info "Rollback cancelled"
        return
    }
    
    $removedItems = @()
    
    if (Test-Path $InstallPath) {
        try {
            Remove-Item $InstallPath -Recurse -Force -ErrorAction Stop
            $removedItems += "Installation directory"
            Write-Success ("Removed: " + $InstallPath)
        } catch {
            Write-Error ("Could not remove " + $InstallPath + " - may require administrator privileges")
        }
    }
    
    Clear-State
    
    Write-Info "Note: You may want to manually remove the toolchain PATH entries"
    Write-Info ("  Path: " + (Join-Path $InstallPath "mcx-sdk\bin"))
    
    Write-Success "Rollback completed"
    Write-Host ""
    Write-Host "Removed:" -ForegroundColor Cyan
    foreach ($item in $removedItems) {
        Write-Host "  - $item" -ForegroundColor White
    }
    
    Complete-Section
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Start-ExpressSetup {
    param([string]$InstallPath)
    
    Write-Banner "EXPRESS SETUP"
    Write-Info ("Using installation path: " + $InstallPath)
    Write-Info ("Emulator choice: " + $Script:Config.EmulatorChoice)
    Write-Host ""
    
    $Script:State.StartedAt = Get-Date
    Save-State
    
    $steps = @(
        @{ Name = "Check Prerequisites"; Script = { Test-Prerequisites } },
        @{ Name = "Install Toolchain"; Script = { Install-Toolchain $InstallPath } },
        @{ Name = "Install Python"; Script = { Install-Python $InstallPath } },
        @{ Name = "Clone Repository"; Script = { Clone-Repository $InstallPath } },
        @{ Name = "Build Library"; Script = { Build-Library $InstallPath } },
        @{ Name = "Install Emulator"; Script = { Install-Emulator $InstallPath $Script:Config.EmulatorChoice } },
        @{ Name = "Configure VS Code"; Script = { Install-VSCodeExtensions $InstallPath } }
    )
    
    $stepNum = 0
    $failedSteps = @()
    
    foreach ($step in $steps) {
        $stepNum++
        $percent = [int](($stepNum / $steps.Count) * 100)
        
        Write-Host ""
        Write-Host "========================================================" -ForegroundColor DarkGray
        Write-Host ("Step " + $stepNum + " of " + $steps.Count + ": " + $step.Name) -ForegroundColor Cyan
        Write-Host "========================================================" -ForegroundColor DarkGray
        
        $stepStartTime = Get-Date
        
        try {
            $result = & $step.Script
            
            $stepDuration = (Get-Date) - $stepStartTime
            Write-Info ("Completed in " + ($stepDuration.TotalSeconds.ToString("N1")) + " seconds")
            
            if (-not $result) {
                Write-Warning ("Step '" + $step.Name + "' reported failure")
                $failedSteps += $step.Name
                
                Write-Host ""
                if (-not (Confirm-Prompt "Continue despite failure?" $false)) {
                    Write-Error "Setup cancelled by user"
                    Show-ErrorSummary -Steps $steps -FailedSteps $failedSteps -InstallPath $InstallPath
                    return $false
                }
            }
        } catch {
            $errorMsg = $_.Exception.Message
            Write-Error ("Step '" + $step.Name + "' threw exception: " + $errorMsg)
            $failedSteps += ($step.Name + " (error)")
            
            if (-not (Confirm-Prompt "Continue despite error?" $false)) {
                Write-Error "Setup cancelled by user"
                Show-ErrorSummary -Steps $steps -FailedSteps $failedSteps -InstallPath $InstallPath
                return $false
            }
        }
    }
    
    $Script:State.StartedAt = $null
    Save-State
    
    Show-Completion -Success $true -InstallPath $InstallPath -FailedSteps $failedSteps
    
    return $true
}

function Test-Prerequisites {
    Write-Section "CHECKING PREREQUISITES"
    
    $allGood = $true
    
    Write-Info "Checking internet connection..."
    if (Test-InternetConnection) {
        Write-Success "Internet available"
    } else {
        Write-Error "No internet connection"
        $allGood = $false
    }
    
    Write-Info "Checking Git..."
    if (Test-GitInstalled) {
        Write-Success "Git installed"
    } else {
        Write-Warning "Git not found - will install"
        if (-not (Install-Git)) {
            Write-Error "Could not install Git"
            $allGood = $false
        }
    }
    
    Write-Info "Checking Python..."
    if (Test-PythonInstalled) {
        Write-Success "Python installed"
    } else {
        Write-Warning "Python not found - will install"
    }
    
    Write-Info "Checking administrator rights..."
    if (Test-Administrator) {
        Write-Success "Running as administrator"
    } else {
        Write-Warning "Not running as administrator"
        Write-Info "Some PATH modifications may require running as administrator later"
    }
    
    Complete-Section
    
    return $allGood
}

function Show-ErrorSummary {
    param(
        [array]$Steps,
        [array]$FailedSteps,
        [string]$InstallPath
    )
    
    Write-Banner "SETUP INCOMPLETE"
    Write-Host ""
    Write-Host "The following steps failed:" -ForegroundColor Yellow
    foreach ($step in $FailedSteps) {
        Write-Host "  - $step" -ForegroundColor Red
    }
    Write-Host ""
    Write-Host "You can:" -ForegroundColor Cyan
    Write-Host "  1. Review the error messages above" -ForegroundColor White
    Write-Host "  2. Fix the issues manually" -ForegroundColor White
    Write-Host "  3. Run '.\setup.ps1 -Resume' to continue from where it left off" -ForegroundColor White
    Write-Host ""
    Write-Host "State has been saved. Run with -Resume to continue later." -ForegroundColor Cyan
}

function Show-Completion {
    param(
        [bool]$Success,
        [string]$InstallPath,
        [array]$FailedSteps = @()
    )
    
    Write-Banner "SETUP COMPLETE"
    
    if ($Success -and $FailedSteps.Count -eq 0) {
        Write-Success "Saturn development environment is ready!"
        Write-Host ""
        Write-Host "Installed components:" -ForegroundColor Cyan
        Write-Host "  - Toolchain (MCX-SDK)" -ForegroundColor White
        Write-Host "  - Python" -ForegroundColor White
        Write-Host "  - libsaturn library" -ForegroundColor White
        Write-Host "  - Emulator: " + $Script:Config.EmulatorChoice -ForegroundColor White
        Write-Host "  - VS Code configuration" -ForegroundColor White
        Write-Host ""
        Write-Host ("Location: " + $InstallPath) -ForegroundColor White
        Write-Host ""
        
        $emuPath = Join-Path $InstallPath ($Script:Config.EmulatorChoice + "\*.exe")
        $emuExe = Get-ChildItem -Path $emuPath -ErrorAction SilentlyContinue | Select-Object -First 1
        
        if ($emuExe) {
            Write-Host "To launch emulator with an example:" -ForegroundColor Cyan
            Write-Host ("  & """ + $emuExe.FullName + """") -ForegroundColor White
        }
        
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host ("  1. Open VS Code: code """ + $InstallPath + """") -ForegroundColor White
        Write-Host ("  2. Review examples in: " + $InstallPath + "\examples") -ForegroundColor White
        Write-Host "  3. Check README.md for documentation" -ForegroundColor White
        Write-Host ""
        
        if (Test-Command "code" -and (Confirm-Prompt "Open VS Code now?")) {
            Start-Process -FilePath "code" -ArgumentList $InstallPath
        }
    } else {
        Show-ErrorSummary -Steps @() -FailedSteps $FailedSteps -InstallPath $InstallPath
    }
}

function Start-InteractiveSetup {
    Write-Banner "SATURN DEVELOPMENT SETUP"
    
    Write-Host "Welcome to the Saturn development environment setup!" -ForegroundColor White
    Write-Host ""
    Write-Host "This script will:" -ForegroundColor Cyan
    Write-Host "  - Install the SH-ELF toolchain (MCX-SDK)" -ForegroundColor White
    Write-Host "  - Install Python 3.11" -ForegroundColor White
    Write-Host "  - Clone the libsaturn repository" -ForegroundColor White
    Write-Host "  - Build the library" -ForegroundColor White
    Write-Host "  - Install an emulator" -ForegroundColor White
    Write-Host "  - Configure VS Code" -ForegroundColor White
    Write-Host ""
    
    Write-Host ("Installation path: " + $InstallPath) -ForegroundColor Cyan
    if (Confirm-Prompt "Use this path?") {
        $finalPath = $InstallPath
    } else {
        do {
            $finalPath = Read-Host "Enter installation path"
        } while ([string]::IsNullOrEmpty($finalPath))
    }
    
    Write-Host ""
    
    $emuChoice = Select-Option "SELECT EMULATOR" @{
        "Kronos" = "Kronos - Advanced Saturn emulator (Recommended)"
        "YabaSanshiro" = "YabaSanshiro - Alternative emulator"
        "Both" = "Both emulators"
        "None" = "Skip emulator installation"
    }
    
    $Script:Config.EmulatorChoice = $emuChoice
    $Script:Config.InstallPath = $finalPath
    
    Write-Host ""
    Write-Host ("Ready to install to: " + $finalPath) -ForegroundColor Green
    Write-Host ("Emulator: " + $emuChoice) -ForegroundColor Green
    Write-Host ""
    
    if (-not (Confirm-Prompt "Proceed with installation?")) {
        Write-Info "Setup cancelled"
        exit 0
    }
    
    return Start-ExpressSetup -InstallPath $finalPath
}

# ═══════════════════════════════════════════════════════════════════════════════
# ENTRY POINT
# ═══════════════════════════════════════════════════════════════════════════════

function Main {
    Start-Logging
    
    if ($Rollback) {
        Invoke-Rollback -InstallPath $InstallPath
        return
    }
    
    if ($Resume) {
        if (Load-State) {
            Write-Banner "RESUMING SETUP"
            Write-Host "Resuming from saved state..." -ForegroundColor Yellow
            Write-Host ""
            
            if ($Script:State.StartedAt) {
                Write-Info ("Setup started at: " + $Script:State.StartedAt)
            }
            
            $result = Start-ExpressSetup -InstallPath $InstallPath
            return
        } else {
            Write-Warning "No previous state found, starting fresh"
        }
    }
    
    if ($Express) {
        $result = Start-ExpressSetup -InstallPath $InstallPath
    } else {
        $result = Start-InteractiveSetup
    }
}

Main