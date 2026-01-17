# UI helpers


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
