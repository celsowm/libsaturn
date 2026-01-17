# ╔════════════════════════════════════════════════════════════════════════════╗
# ║                          SATURN DEVELOPMENT SETUP                            ║
# ║                           libsaturn Automation Script                         ║
# ║                              Version 1.0.0                                    ║
# ╚════════════════════════════════════════════════════════════════════════════╝

#Requires -Version 5.1
#Requires -RunAsAdministrator

param(
    [switch]$Express,
    [switch]$Resume,
    [string]$InstallPath = "$env:USERPROFILE\saturn-sdk",
    [switch]$Offline
)

# ═════════════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═════════════════════════════════════════════════════════════════════════════

$Script:Config = @{
    Version = "1.0.0"
    DefaultInstallPath = $InstallPath
    Toolchain = @{
        Name = "MCX-SDK"
        Version = "v2.0"
        URL = "https://github.com/jetsetilly/mcx-sdk/releases/download/v2.0/mcx-sdk-win64.zip"
        Size = 100MB
    }
    Python = @{
        Version = "3.11"
        URL = "https://www.python.org/ftp/python/3.11.7/python-3.11.7-amd64.exe"
    }
    Emulator = @{
        Kronos = @{
            Name = "Kronos"
            URL = "https://github.com/FCare/Kronos/releases/download/v2.7.2/Kronos-v2.7.2-Windows.zip"
            Size = 20MB
        }
        YabaSanshiro = @{
            Name = "YabaSanshiro"
            URL = "https://github.com/devmiyax/yabause/releases/download/v0.1.4/yabause_wiiu_v1.4.zip"
            Size = 10MB
        }
    }
    Progress = @{}
}

$Script:State = @{
    Step = 0
    TotalSteps = 8
    ResumeFile = "$env:TEMP\saturn-setup-resume.json"
    RollbackList = @()
}

# ═════════════════════════════════════════════════════════════════════════════
# UI FUNCTIONS
# ═════════════════════════════════════════════════════════════════════════════

function Write-Banner {
    Clear-Host
    Write-Host "╔════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║" -NoNewline -ForegroundColor Cyan
    Write-Host "  🔮       SATURN DEVELOPMENT SETUP" -NoNewline -ForegroundColor Yellow
    Write-Host "       $((' ' * 40).Substring(0, 39))║" -ForegroundColor Cyan
    Write-Host "║" -NoNewline -ForegroundColor Cyan
    Write-Host "                    libsaturn v$($Script:Config.Version)" -NoNewline -ForegroundColor White
    Write-Host "     $((' ' * 31).Substring(0, 30))║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Section {
    param([string]$Message)
    Write-Host "`n$Message" -ForegroundColor Cyan
    Write-Host ("═" * $Message.Length) -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "[✓] $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "[✗] $Message" -ForegroundColor Red
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[!] $Message" -ForegroundColor Yellow
}

function Write-Info {
    param([string]$Message)
    Write-Host "[i] $Message" -ForegroundColor White
}

function Show-ProgressBar {
    param(
        [string]$Activity,
        [string]$Status,
        [int]$PercentComplete,
        [int]$SecondsRemaining = 0
    )
    
    Write-Progress -Activity $Activity -Status $Status -PercentComplete $PercentComplete -SecondsRemaining $SecondsRemaining
}

function Show-InteractiveMenu {
    param(
        [string]$Title,
        [hashtable]$Options
    )
    
    Write-Section $Title
    
    $i = 1
    foreach ($key in $Options.Keys) {
        Write-Host "    [$i] $Options[$key]" -ForegroundColor White
        $i++
    }
    
    Write-Host ""
    $selection = Read-Host "Select option"
    
    if ($selection -match '^\d+$' -and $selection -gt 0 -and $selection -le $Options.Count) {
        return $Options.Keys[$selection - 1]
    }
    
    return $null
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
    
    return $selection -eq 'y' -or $selection -eq 'Y'
}

# ═════════════════════════════════════════════════════════════════════════════
# DOWNLOAD FUNCTIONS
# ═════════════════════════════════════════════════════════════════════════════

function Invoke-DownloadFile {
    param(
        [string]$Url,
        [string]$OutputPath,
        [string]$Description
    )
    
    Write-Info "Downloading $Description..."
    
    try {
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile($Url, $OutputPath)
        $webClient.Dispose()
        
        $size = (Get-Item $OutputPath).Length / 1MB
        Write-Success "Downloaded $Description ($([math]::Round($size, 2)) MB)"
        return $true
    }
    catch {
        Write-Error "Failed to download $Description: $_"
        return $false
    }
}

function Expand-ArchiveCustom {
    param(
        [string]$ArchivePath,
        [string]$DestinationPath,
        [string]$Description
    )
    
    Write-Info "Extracting $Description..."
    
    try {
        Expand-Archive -Path $ArchivePath -DestinationPath $DestinationPath -Force
        Write-Success "Extracted $Description"
        return $true
    }
    catch {
        Write-Error "Failed to extract $Description: $_"
        return $false
    }
}

# ═════════════════════════════════════════════════════════════════════════════
# VERIFICATION FUNCTIONS
# ═════════════════════════════════════════════════════════════════════════════

function Test-Command {
    param([string]$Command)
    try {
        $null = Get-Command $Command -ErrorAction Stop
        return $true
    }
    catch {
        return $false
    }
}

function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Show-EnvironmentCheck {
    Write-Section "ENVIRONMENT CHECK"
    
    $issues = @()
    
    if (-not (Test-Administrator)) {
        Write-Warning "Not running as Administrator - some features may be limited"
        $issues += "Administrator access"
    }
    
    if (Test-Command "sh-elf-gcc") {
        $version = & sh-elf-gcc --version 2>&1 | Select-Object -First 1
        Write-Success "SH-ELF GCC found: $version"
    } else {
        Write-Warning "SH-ELF GCC not found - will install"
        $issues += "Toolchain"
    }
    
    if (Test-Command "python") {
        $version = & python --version 2>&1
        Write-Success "Python found: $version"
    } else {
        Write-Warning "Python not found - will install"
        $issues += "Python"
    }
    
    if ($issues.Count -eq 0) {
        Write-Success "All dependencies found!"
        return $true
    } else {
        Write-Info "Missing dependencies: $($issues -join ', ')"
        return $false
    }
}

# ═════════════════════════════════════════════════════════════════════════════
# SETUP STEPS
# ═════════════════════════════════════════════════════════════════════════════

function Install-Toolchain {
    param([string]$Path)
    
    Write-Section "INSTALLING SH-ELF TOOLCHAIN"
    
    $installPath = Join-Path $Path "mcx-sdk"
    $zipPath = "$env:TEMP\mcx-sdk.zip"
    
    Show-ProgressBar -Activity "Installing Toolchain" -Status "Downloading..." -PercentComplete 10
    if (-not (Invoke-DownloadFile $Script:Config.Toolchain.URL $zipPath "SH-ELF Toolchain")) {
        return $false
    }
    
    Show-ProgressBar -Activity "Installing Toolchain" -Status "Extracting..." -PercentComplete 50
    if (-not (Expand-ArchiveCustom $zipPath $installPath "Toolchain")) {
        return $false
    }
    
    Show-ProgressBar -Activity "Installing Toolchain" -Status "Configuring PATH..." -PercentComplete 80
    
    $binPath = Join-Path $installPath "bin"
    Add-ToPath $binPath
    
    $Script:State.RollbackList += @{
        Type = "Path"
        Value = $binPath
    }
    
    Show-ProgressBar -Activity "Installing Toolchain" -Status "Complete!" -PercentComplete 100
    Write-Progress -Activity "Installing Toolchain" -Completed
    
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    Write-Success "Toolchain installed to: $installPath"
    return $true
}

function Install-Python {
    param([string]$Path)
    
    Write-Section "INSTALLING PYTHON"
    
    if (Test-Command "python") {
        Write-Success "Python already installed"
        return $true
    }
    
    $installerPath = "$env:TEMP\python-installer.exe"
    
    Show-ProgressBar -Activity "Installing Python" -Status "Downloading..." -PercentComplete 20
    if (-not (Invoke-DownloadFile $Script:Config.Python.URL $installerPath "Python")) {
        return $false
    }
    
    Show-ProgressBar -Activity "Installing Python" -Status "Installing..." -PercentComplete 50
    $arguments = "/quiet InstallAllUsers=0 PrependPath=1 Include_test=0"
    Start-Process $installerPath -ArgumentList $arguments -Wait
    
    Show-ProgressBar -Activity "Installing Python" -Status "Complete!" -PercentComplete 100
    Write-Progress -Activity "Installing Python" -Completed
    
    Remove-Item $installerPath -Force -ErrorAction SilentlyContinue
    Write-Success "Python installed"
    return $true
}

function Install-Emulator {
    param(
        [string]$Path,
        [string]$EmulatorChoice
    )
    
    Write-Section "INSTALLING EMULATOR"
    
    $emulatorConfig = if ($EmulatorChoice -eq "Kronos") { $Script:Config.Emulator.Kronos } else { $Script:Config.Emulator.YabaSanshiro }
    
    $installPath = Join-Path $Path $emulatorConfig.Name
    $zipPath = "$env:TEMP\$($emulatorConfig.Name).zip"
    
    Show-ProgressBar -Activity "Installing $($emulatorConfig.Name)" -Status "Downloading..." -PercentComplete 20
    if (-not (Invoke-DownloadFile $emulatorConfig.URL $zipPath "Emulator")) {
        return $false
    }
    
    Show-ProgressBar -Activity "Installing $($emulatorConfig.Name)" -Status "Extracting..." -PercentComplete 60
    if (-not (Expand-ArchiveCustom $zipPath $installPath "Emulator")) {
        return $false
    }
    
    Show-ProgressBar -Activity "Installing $($emulatorConfig.Name)" -Status "Complete!" -PercentComplete 100
    Write-Progress -Activity "Installing $($emulatorConfig.Name)" -Completed
    
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    Write-Success "$($emulatorConfig.Name) installed to: $installPath"
    return $installPath
}

function Add-ToPath {
    param([string]$PathToAdd)
    
    if (-not (Test-Administrator)) {
        Write-Warning "Cannot modify PATH without administrator privileges"
        return
    }
    
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($currentPath -notlike "*$PathToAdd*") {
        [Environment]::SetEnvironmentVariable("Path", "$currentPath;$PathToAdd", "User")
        Write-Success "Added to PATH: $PathToAdd"
    } else {
        Write-Info "Already in PATH: $PathToAdd"
    }
}

function Build-Library {
    param([string]$Path)
    
    Write-Section "BUILDING LIBSATURN LIBRARY"
    
    $buildScript = Join-Path $Path "build.bat"
    
    if (-not (Test-Path $buildScript)) {
        Write-Error "build.bat not found at: $buildScript"
        return $false
    }
    
    Show-ProgressBar -Activity "Building Library" -Status "Compiling..." -PercentComplete 0
    
    $process = Start-Process "cmd.exe" -ArgumentList "/c $buildScript" -WorkingDirectory $Path -PassThru -NoNewWindow
    
    while (-not $process.HasExited) {
        Start-Sleep -Milliseconds 100
        $elapsed = ((Get-Date) - $process.StartTime).TotalSeconds
        $percent = [math]::Min(90, $elapsed / 5)
        Show-ProgressBar -Activity "Building Library" -Status "Compiling..." -PercentComplete $percent
    }
    
    if ($process.ExitCode -eq 0) {
        Show-ProgressBar -Activity "Building Library" -Status "Complete!" -PercentComplete 100
        Write-Progress -Activity "Building Library" -Completed
        Write-Success "Library built successfully"
        return $true
    } else {
        Write-Error "Library build failed with exit code: $($process.ExitCode)"
        return $false
    }
}

function Build-Examples {
    param([string]$Path)
    
    Write-Section "BUILDING EXAMPLES"
    
    $examplesPath = Join-Path $Path "examples"
    $examples = Get-ChildItem $examplesPath -Directory | Select-Object -First 5
    
    foreach ($i = 0; $i -lt $examples.Count; $i++) {
        $example = $examples[$i]
        $percent = [math]::Round(($i / $examples.Count) * 100)
        
        Show-ProgressBar -Activity "Building Examples" -Status "$($example.Name)..." -PercentComplete $percent
        
        $examplePath = $example.FullName
        $makefile = Join-Path $examplePath "Makefile"
        
        if (Test-Path $makefile) {
            $process = Start-Process "mingw32-make" -WorkingDirectory $examplePath -PassThru -NoNewWindow -Wait
        }
    }
    
    Show-ProgressBar -Activity "Building Examples" -Status "Complete!" -PercentComplete 100
    Write-Progress -Activity "Building Examples" -Completed
    Write-Success "Examples built"
    return $true
}

function Setup-VSCode {
    param([string]$Path)
    
    Write-Section "CONFIGURING VS CODE"
    
    $vscodePath = Join-Path $Path ".vscode"
    New-Item -ItemType Directory -Force -Path $vscodePath | Out-Null
    
    $cCppProperties = @"
{
    "configurations": [
        {
            "name": "Saturn",
            "includePath": [
                "${workspaceFolder}/include",
                "${workspaceFolder}/include/saturn"
            ],
            "compilerPath": "sh-elf-gcc",
            "cStandard": "c99",
            "intelliSenseMode": "gcc-x86"
        }
    ],
    "version": 4
}
"@
    
    $cCppProperties | Out-File -FilePath (Join-Path $vscodePath "c_cpp_properties.json") -Encoding UTF8
    
    $tasks = @"
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build Library",
            "type": "shell",
            "command": "build.bat",
            "problemMatcher": []
        }
    ]
}
"@
    
    $tasks | Out-File -FilePath (Join-Path $vscodePath "tasks.json") -Encoding UTF8
    
    Write-Success "VS Code configuration created"
    return $true
}

# ═════════════════════════════════════════════════════════════════════════════
# MAIN SETUP
# ═════════════════════════════════════════════════════════════════════════════

function Start-InteractiveSetup {
    Write-Banner
    
    $installMode = Show-InteractiveMenu "INSTALLATION MODE" @{
        "Express" = "Express (Recommended - All Defaults)"
        "Custom" = "Custom Configuration"
        "Resume" = "Resume Previous Setup"
    }
    
    if (-not $installMode) {
        $installMode = "Express"
    }
    
    if ($installMode -eq "Express") {
        return Start-ExpressSetup
    }
    
    # Custom setup would prompt for each option
    # For now, default to express
    return Start-ExpressSetup
}

function Start-ExpressSetup {
    Write-Banner
    Write-Section "EXPRESS SETUP"
    
    $steps = @(
        @{ Name = "Environment Check"; Script = { Show-EnvironmentCheck } },
        @{ Name = "Install Toolchain"; Script = { Install-Toolchain $Script:Config.DefaultInstallPath } },
        @{ Name = "Install Python"; Script = { Install-Python $Script:Config.DefaultInstallPath } },
        @{ Name = "Clone libsaturn"; Script = { Clone-Repository $Script:Config.DefaultInstallPath } },
        @{ Name = "Build Library"; Script = { Build-Library $Script:Config.DefaultInstallPath } },
        @{ Name = "Build Examples"; Script = { Build-Examples $Script:Config.DefaultInstallPath } },
        @{ Name = "Install Emulator"; Script = { Install-Emulator $Script:Config.DefaultInstallPath "Kronos" } },
        @{ Name = "Setup VS Code"; Script = { Setup-VSCode $Script:Config.DefaultInstallPath } }
    )
    
    for ($i = 0; $i -lt $steps.Count; $i++) {
        $step = $steps[$i]
        Write-Host ""
        Write-Host "[$($i + 1)/$($steps.Count)] $($step.Name)" -ForegroundColor Cyan
        
        $result = & $step.Script
        
        if (-not $result) {
            Write-Warning "Step '$($step.Name)' failed. Continue? (Y/n)"
            $continue = Read-Host
            if ($continue -ne 'y' -and $continue -ne 'Y' -and $continue -ne '') {
                Write-Error "Setup cancelled"
                return $false
            }
        }
    }
    
    return $true
}

function Clone-Repository {
    param([string]$Path)
    
    Write-Section "CLONING LIBSATURN"
    
    if (Test-Path (Join-Path $Path "README.md")) {
        Write-Success "libsaturn already exists"
        return $true
    }
    
    Write-Info "Cloning libsaturn repository..."
    
    if (-not (Test-Command "git")) {
        Write-Warning "Git not found - assuming repository already exists in: $Path"
        return $true
    }
    
    try {
        Show-ProgressBar -Activity "Cloning Repository" -Status "Downloading..." -PercentComplete 30
        & git clone "https://github.com/yourusername/libsaturn.git" $Path 2>&1 | Out-Null
        
        Show-ProgressBar -Activity "Cloning Repository" -Status "Complete!" -PercentComplete 100
        Write-Progress -Activity "Cloning Repository" -Completed
        
        Write-Success "Repository cloned to: $Path"
        return $true
    }
    catch {
        Write-Error "Failed to clone repository: $_"
        return $false
    }
}

function Show-Completion {
    param([bool]$Success)
    
    Write-Banner
    
    if ($Success) {
        Write-Success "Setup completed successfully!"
        Write-Host ""
        Write-Host "🚀 Saturn development environment is ready!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "  1. Open VS Code: code $Script:Config.DefaultInstallPath" -ForegroundColor White
        Write-Host "  2. Review README.md for documentation" -ForegroundColor White
        Write-Host "  3. Check examples folder for demos" -ForegroundColor White
        Write-Host ""
        
        $options = @{
            "Launch" = "Launch Emulator with First Example"
            "VSCode" = "Open VS Code"
            "Readme" = "Open README"
            "Exit" = "Exit"
        }
        
        $selection = Show-InteractiveMenu "WHAT'S NEXT?" $options
        
        switch ($selection) {
            "Launch" {
                $emulatorPath = Join-Path $Script:Config.DefaultInstallPath "Kronos"
                $examplePath = Join-Path $Script:Config.DefaultInstallPath "examples\01_helloworld\0.BIN"
                
                if (Test-Path $emulatorPath -and Test-Path $examplePath) {
                    Start-Process $emulatorPath -ArgumentList $examplePath
                }
            }
            "VSCode" {
                Start-Process "code" -ArgumentList $Script:Config.DefaultInstallPath
            }
            "Readme" {
                Start-Process (Join-Path $Script:Config.DefaultInstallPath "README.md")
            }
        }
    } else {
        Write-Error "Setup encountered errors. Please review the output above."
    }
    
    Write-Host ""
    Read-Host "Press Enter to exit"
}

# ═════════════════════════════════════════════════════════════════════════════
# ENTRY POINT
# ═════════════════════════════════════════════════════════════════════════════

function Main {
    if ($Express) {
        $result = Start-ExpressSetup
    } else {
        $result = Start-InteractiveSetup
    }
    
    Show-Completion $result
}

Main
