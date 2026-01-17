# Download helpers

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
