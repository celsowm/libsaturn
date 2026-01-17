#!/usr/bin/env pwsh
# Saturn Development Environment Setup - libsaturn Automation Script v2.0.0

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
$Script:Version = "2.0.0"

# ═══════════════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════════

$Script:Config = @{
    InstallPath = $InstallPath
    RepositoryUrl = "https://github.com/celsoastro/libsaturn.git"
    StateFile = "$env:TEMP\saturn-setup-state.json"
    LogFile = "$env:TEMP\saturn-setup.log"
    EmulatorChoice = $Emulator
    
    Toolchain = @{
        Name = "MCX-SDK"
        Version = "v2.0"
        Url = "https://github.com/jetsetilly/mcx-sdk/releases/download/v2.0/mcx-sdk-win64.zip"
        SizeMB = 100
        InstallDir = "mcx-sdk"
        BinPath = $null
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
        $null = Get-Command $Command -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

function Test-InternetConnection {
    try {
        $null = Invoke-WebRequest -Uri "https://github.com" -TimeoutSeconds 10 -UseBasicParsing
        return $true
    } catch {
        return $false
    }
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
        Write-Success "Internet available"
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
        Write-Error "Critical issues found: " + ($issues -join ", ")
        return $false
    }
    
    if ($warnings.Count -gt 0) {
        Write-Warning "Warnings: " + ($warnings -join ", ")
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
# DOWNLOAD FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Invoke-DownloadFile {
    param(
        [string]$Url,
        [string]$OutputPath,
        [string]$Description,
        [int]$MaxRetries = $Script:Config.DownloadRetry
    )
    
    $parentDir = Split-Path $OutputPath -Parent
    if (-not (Test-Path $parentDir)) {
        New-Item -ItemType Directory -Path $parentDir -Force | Out-Null
    }
    
    $attempt = 1
    $success = $false
    
    while ($attempt -le $MaxRetries -and -not $success) {
        try {
            Write-Info ("Downloading " + $Description + " (attempt " + $attempt + " of " + $MaxRetries + ")...")
            
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($Url, $OutputPath)
            $webClient.Dispose()
            
            $size = (Get-Item $OutputPath).Length / 1MB
            Write-Success ("Downloaded " + $Description + " (" + ([math]::Round($size, 2)) + " MB)")
            $success = $true
        } catch {
            Write-Warning ("Download attempt " + $attempt + " failed: " + $_.Exception.Message)
            if ($attempt -lt $MaxRetries) {
                $waitTime = $attempt * 5
                Write-Info ("Retrying in " + $waitTime + " seconds...")
                Start-Sleep -Seconds $waitTime
            }
            $attempt++
        }
    }
    
    if (-not $success) {
        Write-Error ("Failed to download " + $Description + " after " + $MaxRetries + " attempts")
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

function Install-Toolchain {
    param([string]$InstallPath)
    
    Write-Section "INSTALLING SH-ELF TOOLCHAIN (MCX-SDK)"
    
    if ($Script:State.ToolchainInstalled) {
        Write-Success "Toolchain already installed, skipping"
        return $true
    }
    
    $toolchainInstallDir = Join-Path $InstallPath $Script:Config.Toolchain.InstallDir
    $zipPath = "$env:TEMP\mcx-sdk.zip"
    $binPath = Join-Path $toolchainInstallDir "bin"
    
    Write-ProgressBar -Activity "Downloading Toolchain" -Status "Starting..." -PercentComplete 0
    $downloadSuccess = Invoke-DownloadFile -Url $Script:Config.Toolchain.Url -OutputPath $zipPath -Description "MCX-SDK Toolchain"
    
    if (-not $downloadSuccess) {
        Write-Error "Toolchain download failed"
        return $false
    }
    
    Write-ProgressBar -Activity "Downloading Toolchain" -Status "Complete" -PercentComplete 100
    
    Write-ProgressBar -Activity "Extracting Toolchain" -Status "Starting..." -PercentComplete 0
    $extractSuccess = Expand-ZipArchive -ArchivePath $zipPath -DestinationPath $InstallPath -Description "MCX-SDK Toolchain"
    
    if (-not $extractSuccess) {
        return $false
    }
    
    Write-ProgressBar -Activity "Extracting Toolchain" -Status "Complete" -PercentComplete 100
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    
    if (Test-Path $binPath) {
        Write-Info ("Toolchain binaries located at: " + $binPath)
        
        $shGcc = Get-ChildItem -Path $binPath -Filter "*-gcc.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($shGcc) {
            Write-Success ("Found compiler: " + $shGcc.Name)
            $env:Path = $binPath + ";" + $env:Path
        }
        
        $Script:State.ToolchainInstalled = $true
        Save-State
        
        Complete-Section
        return $true
    } else {
        Write-Error "Toolchain binaries not found after extraction"
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
        
        $gitProcess = Start-Process -FilePath "git" -ArgumentList "clone", $RepositoryUrl, $tempClonePath -Wait -PassThru -NoNewWindow
        
        if ($gitProcess.ExitCode -ne 0) {
            Write-Error ("Git clone failed (exit code: " + $gitProcess.ExitCode + ")")
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
            Write-Info "Check build output above for errors"
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
