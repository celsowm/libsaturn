# Rollback helpers

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
