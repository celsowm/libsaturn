# Logging helpers

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
