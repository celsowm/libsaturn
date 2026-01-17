# Main orchestration

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

# ENTRY POINT

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
