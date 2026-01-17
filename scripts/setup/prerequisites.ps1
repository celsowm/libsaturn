# Prerequisite checks

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
