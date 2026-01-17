# State management

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
