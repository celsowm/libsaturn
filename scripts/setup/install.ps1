# Installation functions

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
    
    $stdoutFile = [System.IO.Path]::GetTempFileName()
    $stderrFile = [System.IO.Path]::GetTempFileName()
    $stdout = ""
    $stderr = ""
    $process = $null
    $success = $false
    
    try {
        $msys2Args = "-lc", "export PATH='/usr/bin:$PATH'; $Command"
        $process = Start-Process -FilePath $bashPath -ArgumentList $msys2Args -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile
        
        $stdout = Get-Content -Path $stdoutFile -Raw -ErrorAction SilentlyContinue
        $stderr = Get-Content -Path $stderrFile -Raw -ErrorAction SilentlyContinue
        if ($stdout) { $stdout = $stdout.Trim() } else { $stdout = "" }
        if ($stderr) { $stderr = $stderr.Trim() } else { $stderr = "" }
        
        if ($process) {
            $success = $process.ExitCode -eq 0
            
            if (-not $success) {
                Write-Warning ("MSYS2 command failed (exit code {0}): {1}" -f $process.ExitCode, $Command)
                $outputLines = @()
                if ($stdout) {
                    $outputLines += ($stdout -split "[\r\n]+")
                }
                if ($stderr) {
                    $outputLines += ($stderr -split "[\r\n]+")
                }
                $outputLines = $outputLines | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }
                
                if ($outputLines.Count -eq 0) {
                    Write-Info "No output captured from MSYS2"
                } else {
                    $preview = @($outputLines | Select-Object -First 6)
                    foreach ($line in $preview) {
                        Write-Info $line
                    }
                    if ($outputLines.Count -gt $preview.Count) {
                        Write-Info "... (output truncated)"
                    }
                }
            }
        }
    } catch {
        Write-Error ("Failed to run MSYS2 command: " + $_.Exception.Message)
        $success = $false
    } finally {
        Remove-Item -Path $stdoutFile,$stderrFile -ErrorAction SilentlyContinue
    }
    
    return $success
}

function Find-MSYS2Executable {
    param(
        [string]$ExecutableName,
        [string]$Msys2Root,
        [string[]]$BinDirs
    )
    
    if ([string]::IsNullOrWhiteSpace($ExecutableName)) {
        return $null
    }
    
    if ([string]::IsNullOrWhiteSpace($Msys2Root) -or -not (Test-Path $Msys2Root)) {
        return $null
    }
    
    $nameBase = $ExecutableName
    if ($nameBase.EndsWith(".exe")) {
        $nameBase = $nameBase.Substring(0, $nameBase.Length - 4)
    }
    
    $candidateNames = @($nameBase, ($nameBase + ".exe"))
    
    $searchDirs = @()
    if ($BinDirs) {
        $searchDirs += $BinDirs
    }
    if (-not $searchDirs -or $searchDirs.Count -eq 0) {
        $searchDirs = @(
            "mingw32\bin",
            "mingw64\bin",
            "ucrt64\bin",
            "clang32\bin",
            "clang64\bin",
            "clangarm64\bin",
            "usr\bin"
        )
    } elseif ($searchDirs -notcontains "usr\bin") {
        $searchDirs += "usr\bin"
    }

    $searchDirs = $searchDirs | Where-Object { $_ } | Select-Object -Unique
    
    foreach ($dir in $searchDirs) {
        $binPath = Join-Path $Msys2Root $dir
        foreach ($name in $candidateNames) {
            $candidate = Join-Path $binPath $name
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }
    
    $bashPath = Join-Path $Msys2Root "usr\bin\bash.exe"
    if (Test-Path $bashPath) {
        try {
            $posixPath = & $bashPath -lc "command -v $nameBase 2>/dev/null"
            $posixPath = $posixPath | Select-Object -First 1
            if (-not [string]::IsNullOrWhiteSpace($posixPath)) {
                $posixPath = $posixPath.Trim()
                $winPath = & $bashPath -lc "cygpath -w `"$posixPath`""
                $winPath = $winPath | Select-Object -First 1
                if (-not [string]::IsNullOrWhiteSpace($winPath)) {
                    $winPath = $winPath.Trim()
                    if (Test-Path $winPath) {
                        return $winPath
                    }
                    if (Test-Path ($winPath + ".exe")) {
                        return ($winPath + ".exe")
                    }
                }
            }
        } catch {
        }
    }
    
    try {
        $found = Get-ChildItem -Path $Msys2Root -Filter ($nameBase + "*.exe") -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            return $found.FullName
        }
        
        $found = Get-ChildItem -Path $Msys2Root -Filter $nameBase -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            return $found.FullName
        }
    } catch {
        return $null
    }
    
    return $null
}

function Convert-MSYS2Path {
    param(
        [string]$Msys2Root,
        [string]$PosixPath
    )
    
    if ([string]::IsNullOrWhiteSpace($Msys2Root) -or [string]::IsNullOrWhiteSpace($PosixPath)) {
        return $null
    }
    
    $path = $PosixPath.Trim()
    
    if ($path -match "^/([a-zA-Z])/") {
        $drive = $Matches[1]
        $rest = $path.Substring(3)
        return ($drive + ":\\" + ($rest -replace "/", "\\"))
    }
    
    if ($path.StartsWith("/")) {
        $relative = $path.TrimStart("/")
        return Join-Path $Msys2Root ($relative -replace "/", "\\")
    }
    
    return $path
}

function Resolve-MSYS2ExecutableFromPacman {
    param(
        [string]$PackageName,
        [string]$ExecutableName,
        [string]$Msys2Root
    )
    
    if ([string]::IsNullOrWhiteSpace($PackageName) -or [string]::IsNullOrWhiteSpace($ExecutableName)) {
        return $null
    }
    
    if ([string]::IsNullOrWhiteSpace($Msys2Root) -or -not (Test-Path $Msys2Root)) {
        return $null
    }
    
    $bashPath = Join-Path $Msys2Root "usr\bin\bash.exe"
    if (-not (Test-Path $bashPath)) {
        return $null
    }
    
    $nameBase = $ExecutableName
    if ($nameBase.EndsWith(".exe")) {
        $nameBase = $nameBase.Substring(0, $nameBase.Length - 4)
    }
    
    try {
        $listing = @(& $bashPath -lc ("pacman -Ql " + $PackageName + " 2>/dev/null"))
        foreach ($line in $listing) {
            if ([string]::IsNullOrWhiteSpace($line)) {
                continue
            }
            
            $parts = $line -split "\s+", 2
            if ($parts.Count -lt 2) {
                continue
            }
            
            $posixPath = $parts[1].Trim()
            if ($posixPath -match ("/" + [regex]::Escape($nameBase) + "(\\.exe)?$")) {
                $winPath = Convert-MSYS2Path -Msys2Root $Msys2Root -PosixPath $posixPath
                if ($winPath -and (Test-Path $winPath)) {
                    return $winPath
                }
            }
        }
    } catch {
        return $null
    }
    
    return $null
}

function Get-MSYS2PackageFileList {
    param(
        [string]$PackageName,
        [string]$Msys2Root
    )

    if ([string]::IsNullOrWhiteSpace($PackageName) -or [string]::IsNullOrWhiteSpace($Msys2Root)) {
        return @()
    }

    $bashPath = Join-Path $Msys2Root "usr\bin\bash.exe"
    if (-not (Test-Path $bashPath)) {
        return @()
    }

    try {
        $listing = & $bashPath -lc ("pacman -Ql " + $PackageName + " 2>/dev/null")
        $files = @()

        foreach ($line in $listing) {
            if ([string]::IsNullOrWhiteSpace($line)) {
                continue
            }

            $parts = $line -split "\s+", 2
            if ($parts.Count -lt 2) {
                continue
            }

            $posixPath = $parts[1].Trim()
            if ($posixPath -eq "") {
                continue
            }

            $winPath = Convert-MSYS2Path -Msys2Root $Msys2Root -PosixPath $posixPath
            $files += [PSCustomObject]@{
                PosixPath = $posixPath
                WinPath = $winPath
            }
        }

        return $files
    } catch {
        return @()
    }
}

function Log-MissingShElfGccPaths {
    param(
        [string]$PackageName,
        [string]$Msys2Root,
        [string[]]$SearchDirs
    )

    $dirsToReport = $SearchDirs
    if (-not $dirsToReport -or $dirsToReport.Count -eq 0) {
        $dirsToReport = @(
            "mingw32\bin",
            "mingw64\bin",
            "ucrt64\bin",
            "clang32\bin",
            "clang64\bin",
            "clangarm64\bin",
            "usr\bin"
        )
    }

    Write-Info ("Searched MSYS2 directories: " + ($dirsToReport -join ", "))

    $files = Get-MSYS2PackageFileList -PackageName $PackageName -Msys2Root $Msys2Root
    $matches = $files | Where-Object { $_.PosixPath -match "sh-elf-gcc" }

    if ($matches.Count -gt 0) {
        Write-Info ("Package '" + $PackageName + "' reported the following matching files:")
        foreach ($entry in $matches) {
            $line = "  - " + $entry.PosixPath
            if ($entry.WinPath) {
                $line += " -> " + $entry.WinPath
            }
            Write-Info $line
        }
        Write-Info "Verify the reported bin directories are part of your PATH or rerun MSYS2 shell:"
        Write-Info ("  Run inside MSYS2: export PATH='/usr/bin:$PATH'; pacman -Ql " + $PackageName + " | grep sh-elf-gcc")
    } else {
        Write-Info ("Package '" + $PackageName + "' listing does not contain a 'sh-elf-gcc' binary.")
        Write-Info ("  Run inside MSYS2: export PATH='/usr/bin:\$PATH'; pacman -Ql " + $PackageName + " | head")
        $snippet = $files | Select-Object -First 8
        if ($snippet.Count -gt 0) {
            Write-Info "  Sample files from package:"
            foreach ($entry in $snippet) {
                $line = "    - " + $entry.PosixPath
                if ($entry.WinPath) {
                    $line += " -> " + $entry.WinPath
                }
                Write-Info $line
            }
            if ($files.Count -gt $snippet.Count) {
                Write-Info ("    ... (package contains " + $files.Count + " entries total)")
            }
        } else {
            Write-Info "  Package listing returned no files (empty or inaccessible package)."
        }
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
        
        $process = Start-Process -FilePath $installerPath -ArgumentList $installArgs -Wait -PassThru -WindowStyle Normal
        
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
    
    if (-not (Test-Path $msys2InstallPath) -or -not (Test-Path (Join-Path $msys2InstallPath "usr\bin\bash.exe"))) {
        $detectedMsys2 = @("C:\msys64","C:\tools\msys64") | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($detectedMsys2) {
            Write-Info "MSYS2 found at: $detectedMsys2 (not the configured path)"
            $msys2InstallPath = $detectedMsys2
        } else {
            Write-Error "MSYS2 installation not found (searched: $msys2InstallPath, C:\msys64, C:\tools\msys64)"
            Write-Info "Please install MSYS2 first"
            return $false
        }
    }
    
    $binDirs = $Script:Config.Toolchain.MSYS2BinDirs
    $gccPath = Find-MSYS2Executable -ExecutableName "sh-elf-gcc" -Msys2Root $msys2InstallPath -BinDirs $binDirs
    $gccBinPath = if ($gccPath) { Split-Path $gccPath } else { $null }
    
    if ($gccPath) {
        Write-Success "SH-ELF GCC already installed"
        Add-ToSessionPath $gccBinPath
        return $true
    }
    
    Write-Info "Updating MSYS2 package databases..."
    if (-not (Invoke-MSYS2Pacman "pacman --noconfirm -Sy")) {
        Write-Warning "Failed to update package database, continuing..."
    }
    
    Write-Info "Installing SH-ELF GCC (this may take several minutes)..."
    $pkgCandidates = @()
    if ($Script:Config.Toolchain.MSYS2Pkg) {
        $pkgCandidates += $Script:Config.Toolchain.MSYS2Pkg
    }
    if ($Script:Config.Toolchain.MSYS2PkgFallback) {
        $pkgCandidates += $Script:Config.Toolchain.MSYS2PkgFallback
    }
    $pkgCandidates = $pkgCandidates | Where-Object { $_ } | Select-Object -Unique
    
    $availablePkgs = @()
    $missingBinaryPackages = @()
    $unavailablePkgs = @()
    foreach ($pkg in $pkgCandidates) {
        if (Invoke-MSYS2Pacman "pacman -Si $pkg") {
            $availablePkgs += $pkg
        } else {
            Write-Warning ("MSYS2 package unavailable: " + $pkg)
            $unavailablePkgs += $pkg
        }
    }
    
    if ($availablePkgs.Count -eq 0) {
        Write-Warning "No SH-ELF GCC packages were found or could be queried from MSYS2"
        Write-Info "Check the MSYS2 output above for the specific package error"
        Write-Info "Use option 5 to point to an existing toolchain, or option 6 for manual instructions"
        return $false
    }
    
    $installSuccess = $false
    
    foreach ($pkg in $availablePkgs) {
        Write-Info ("Installing SH-ELF GCC package: " + $pkg + "...")
        if (Invoke-MSYS2Pacman "pacman --noconfirm -S $pkg") {
            if (-not (Invoke-MSYS2Pacman "pacman -Q $pkg")) {
                Write-Warning ("Package install did not register in MSYS2: " + $pkg)
                continue
            }
            
            $gccPath = Find-MSYS2Executable -ExecutableName "sh-elf-gcc" -Msys2Root $msys2InstallPath -BinDirs $binDirs
            if (-not $gccPath) {
                $gccPath = Resolve-MSYS2ExecutableFromPacman -PackageName $pkg -ExecutableName "sh-elf-gcc" -Msys2Root $msys2InstallPath
            }
            $gccBinPath = if ($gccPath) { Split-Path $gccPath } else { $null }
            
            if ($gccPath) {
                Write-Success "SH-ELF GCC installed successfully"
                Add-ToSessionPath $gccBinPath
                
                if (Test-Administrator) {
                    Add-ToPathEnvironment $gccBinPath
                } else {
                    Write-Warning "Administrator rights required to add to system PATH"
                    Write-Info "Run as administrator to persist PATH changes"
                }
                
                $installSuccess = $true
                break
            } else {
                Write-Warning "Package installed but sh-elf-gcc not found in expected MSYS2 paths"
                if ($missingBinaryPackages -notcontains $pkg) {
                    $missingBinaryPackages += $pkg
                }
                Log-MissingShElfGccPaths -PackageName $pkg -Msys2Root $msys2InstallPath -SearchDirs $binDirs
            }
        } else {
            Write-Warning ("Failed to install package: " + $pkg)
        }
    }

    if ($installSuccess) {
        return $true
    }

    if ($missingBinaryPackages.Count -gt 0) {
        $missingList = ($missingBinaryPackages | Select-Object -Unique) -join ", "
        Write-Info ("Packages missing sh-elf-gcc binary after install: " + $missingList)
    }
    if ($unavailablePkgs.Count -gt 0) {
        $unavailableList = ($unavailablePkgs | Select-Object -Unique) -join ", "
        Write-Info ("Packages that could not be queried: " + $unavailableList)
    }

    Write-Error "Failed to install SH-ELF GCC"
    Write-Info "Review the MSYS2 output above for clues (missing package, path, or install errors)."
    return $false
}

function Install-ShElfGcc-Prebuilt {
    param([string]$InstallPath)
    
    Write-Section "INSTALLING SH-ELF GCC TOOLCHAIN"
    
    $bundledToolchainPath = Join-Path $PSScriptRoot "..\..\toolchains\sh-elf-gcc"
    $toolchainDir = Join-Path $InstallPath $Script:Config.Toolchain.InstallDir
    $binDir = Join-Path $toolchainDir "bin"
    
    if (-not (Test-Path $bundledToolchainPath)) {
        Write-Warning "Bundled toolchain not found at: $bundledToolchainPath"
        Write-Info "Please download Jo Engine from https://www.jo-engine.org and extract the Compiler/WINDOWS folder to toolchains/sh-elf-gcc"
        return $false
    }
    
    if (Test-Path (Join-Path $binDir "sh-elf-gcc.exe")) {
        Write-Success "SH-ELF GCC already installed at: $binDir"
        Add-ToSessionPath $binDir
        return $true
    }
    
    Write-Info "Installing bundled SH-ELF GCC toolchain..."
    Write-Info "Source: Jo Engine Compiler (included with libsaturn)"
    
    try {
        Write-Info "Copying toolchain to: $toolchainDir"
        
        Copy-Item -Path $bundledToolchainPath -Destination $toolchainDir -Recurse -Force
        
        if (Test-Path (Join-Path $binDir "sh-elf-gcc.exe")) {
            Write-Success "SH-ELF GCC installed successfully"
            Add-ToSessionPath $binDir
            
            if (Test-Administrator) {
                Add-ToPathEnvironment $binDir
            } else {
                Write-Warning "Administrator rights required to add to system PATH"
                Write-Info "Run as administrator to persist PATH changes"
            }
            
            Write-Host ""
            Write-Host "Toolchain installed from: $bundledToolchainPath" -ForegroundColor Cyan
            Write-Host "Copied to: $toolchainDir" -ForegroundColor Cyan
            Write-Host ""
            
            return $true
        } else {
            Write-Error "Toolchain installation failed - sh-elf-gcc.exe not found"
            return $false
        }
    } catch {
        Write-Error ("Toolchain installation failed: " + $_.Exception.Message)
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
    
    Write-Info "Checking for existing SH-ELF toolchain..."
    $shGccPaths = @(
        (Join-Path $msys2InstallPath "mingw64\bin\sh-elf-gcc.exe"),
        (Join-Path $msys2InstallPath "mingw32\bin\sh-elf-gcc.exe"),
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
    
    Write-Section "INSTALLING SH-ELF TOOLCHAIN"
    Write-Host ""
    Write-Host "Installing bundled SH-ELF GCC toolchain from Jo Engine..." -ForegroundColor Cyan
    Write-Host ""
    
    if (Install-ShElfGcc-Prebuilt -InstallPath $InstallPath) {
        Write-Success "Toolchain installation completed"
        $Script:State.ToolchainInstalled = $true
        Save-State
        Complete-Section
        return $true
    } else {
        Write-Error "Failed to install bundled toolchain"
        Write-Host ""
        Write-Host "=== MANUAL INSTALLATION OPTIONS ===" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Option 1: SegaXtreme (EASIEST)" -ForegroundColor Cyan
        Write-Host "  1. Visit: https://segaxtreme.net/resources/gnu-sh-coff-toolchain-for-the-sega-saturn.31/" -ForegroundColor White
        Write-Host "  2. Download the GNU SH-COFF toolchain" -ForegroundColor White
        Write-Host "  3. Extract to: C:\sh-elf-gcc" -ForegroundColor White
        Write-Host "  4. Add to PATH: C:\sh-elf-gcc\bin" -ForegroundColor White
        Write-Host ""
        Write-Host "Option 2: WSL (RECOMMENDED FOR MODERN DEVELOPMENT)" -ForegroundColor Cyan
        Write-Host "  1. Install WSL: wsl --install -d Ubuntu" -ForegroundColor White
        Write-Host "  2. Run in WSL: sudo apt install gcc-sh-elf binutils-sh-elf" -ForegroundColor White
        Write-Host "  3. Use from Windows: wsl sh-elf-gcc [options]" -ForegroundColor White
        Write-Host ""
        Write-Host "Option 3: Build from source" -ForegroundColor Cyan
        Write-Host "  See: https://github.com/SaturnSDK/Saturn-SDK-GCC-SH2" -ForegroundColor White
        Write-Host ""
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
        
        $currentPath = (Get-Location).Path.TrimEnd("\/")
        $targetPath = $InstallPath.TrimEnd("\/")
        
        if ($currentPath -ne $targetPath) {
            Write-Info ("Copying files to installation directory: " + $InstallPath)
            
            try {
                if (-not (Test-Path $InstallPath)) {
                    New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
                }
                
                $sourceDir = Get-Location
                $items = Get-ChildItem -Path $sourceDir -Exclude ".git","nul"
                
                foreach ($item in $items) {
                    $dest = Join-Path $InstallPath $item.Name
                    if ($item.PSIsContainer) {
                        Copy-Item -Path $item.FullName -Destination $dest -Recurse -Force -ErrorAction Stop
                    } else {
                        Copy-Item -Path $item.FullName -Destination $dest -Force -ErrorAction Stop
                    }
                }
            } catch {
                Write-Error ("Failed to copy files: " + $_.Exception.Message)
                return $false
            }
            
            if (Test-Path $readmePath) {
                Write-Success ("Repository copied to: " + $InstallPath)
                $Script:State.RepositoryCloned = $true
                Save-State
                Complete-Section
                return $true
            }
        } else {
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

function Get-ExtendedToolchainSearchPaths {
    <#
    .SYNOPSIS
        Returns extended list of toolchain search paths
    #>
    return @(
        # Bundled with libsaturn
        (Join-Path $PSScriptRoot "..\..\toolchains\sh-elf-gcc\bin"),
        
        # User's Saturn SDK
        (Join-Path $env:USERPROFILE "saturn-sdk\sh-elf-gcc\bin"),
        (Join-Path $env:USERPROFILE "saturn-sdk\sh-elf-gcc"),
        
        # Common Saturn SDK locations
        "C:\saturn-sdk\sh-elf-gcc\bin",
        "C:\saturn-sdk\sh-elf-gcc",
        "C:\saturn\sh-elf-gcc\bin",
        "C:\saturn\sh-elf-gcc",
        
        # MSYS2 locations (all variants)
        "C:\msys64\mingw64\bin",
        "C:\msys64\mingw32\bin",
        "C:\msys64\ucrt64\bin",
        "C:\msys64\clang64\bin",
        "C:\tools\msys64\mingw64\bin",
        "C:\tools\msys64\mingw32\bin",
        "C:\tools\msys64\ucrt64\bin",
        
        # Program Files locations
        "$env:ProgramFiles\SaturnSDK\sh-elf-gcc\bin",
        "$env:ProgramFiles\SaturnSDK\sh-elf-gcc",
        "$env:ProgramFiles (x86)\SaturnSDK\sh-elf-gcc\bin",
        "$env:ProgramFiles (x86)\SaturnSDK\sh-elf-gcc",
        "$env:ProgramFiles\SEGA\SaturnSDK\bin",
        "$env:ProgramFiles\jo-engine\toolchain\bin",
        
        # Legacy/alternative locations
        "C:\sh-elf-gcc\bin",
        "C:\sh-elf-gcc",
        "D:\saturn-sdk\sh-elf-gcc\bin",
        "D:\saturn-sdk\sh-elf-gcc"
    )
}

function Find-ShElfGccExtended {
    <#
    .SYNOPSIS
        Enhanced toolchain finder with extended search paths
    #>
    
    $searchPaths = Get-ExtendedToolchainSearchPaths
    
    foreach ($path in $searchPaths) {
        if ([string]::IsNullOrEmpty($path)) { continue }
        
        $gccPath = Join-Path $path "sh-elf-gcc.exe"
        if (Test-Path $gccPath) {
            Write-Verbose "Found toolchain at: $path"
            return $path
        }
    }
    
    # Check if sh-elf-gcc is already in PATH
    if (Test-Command "sh-elf-gcc") {
        try {
            $shGcc = Get-Command "sh-elf-gcc" -ErrorAction Stop
            $binPath = Split-Path $shGcc.Source -Parent
            Write-Verbose "Found in PATH: $binPath"
            return $binPath
        } catch {
            Write-Verbose "sh-elf-gcc in PATH but command failed"
        }
    }
    
    return $null
}

function Select-ToolchainSource {
    <#
    .SYNOPSIS
        Presents toolchain source options to the user
    #>
    Write-Host ""
    Write-Host "TOOLCHAIN SOURCE SELECTION" -ForegroundColor Cyan
    Write-Host ""
    
    $options = @{
        "1" = @{
            Name = "Bundled Toolchain"
            Description = "Use the bundled GCC 9.3.0 toolchain (included with libsaturn)"
            Action = { return "bundled" }
        }
        "2" = @{
            Name = "Existing Installation"
            Description = "Use an existing sh-elf-gcc installation on your system"
            Action = { return "existing" }
        }
        "3" = @{
            Name = "Download from SegaXtreme"
            Description = "Download the GNU SH-COFF toolchain from SegaXtreme.net"
            Action = { return "download" }
        }
        "4" = @{
            Name = "MSYS2 Package"
            Description = "Install via MSYS2 package manager"
            Action = { return "msys2" }
        }
    }
    
    foreach ($key in $options.Keys | Sort-Object) {
        $opt = $options[$key]
        Write-Host "$key) $($opt.Name)" -ForegroundColor White
        Write-Host "   $($opt.Description)" -ForegroundColor Gray
    }
    
    Write-Host ""
    $choice = Read-Host "Select toolchain source"
    
    if ($options.ContainsKey($choice)) {
        return & $options[$choice].Action
    }
    
    Write-Warning "Invalid selection, defaulting to bundled"
    return "bundled"
}

function Invoke-ExistingToolchainSetup {
    <#
    .SYNOPSIS
        Guides user to select existing toolchain
    #>
    Write-Host ""
    Write-Host "EXISTING TOOLCHAIN SETUP" -ForegroundColor Cyan
    Write-Host ""
    
    $detectedPath = Find-ShElfGccExtended
    
    if ($detectedPath) {
        Write-Success "Found existing toolchain at: $detectedPath"
        
        $useDetected = Read-Host "Use this toolchain? (Y/n)"
        if ($useDetected -ne "n" -and $useDetected -ne "N") {
            return $detectedPath
        }
    }
    
    Write-Host ""
    Write-Host "Enter the path to your sh-elf-gcc installation:" -ForegroundColor White
    Write-Host "(Example: C:\saturn-sdk\sh-elf-gcc or C:\msys64\mingw64\bin)" -ForegroundColor Gray
    
    do {
        $customPath = Read-Host "Toolchain path"
        $customPath = $customPath.Trim().Trim('"').Trim("'")
        
        if ([string]::IsNullOrEmpty($customPath)) {
            Write-Warning "Please enter a valid path"
            continue
        }
        
        # Try to find sh-elf-gcc in the provided path
        $testPath = $customPath
        if (-not (Test-Path $testPath)) {
            # Maybe they gave the bin directory
            $testPath = Join-Path $customPath "bin"
        }
        
        $gccPath = Join-Path $testPath "sh-elf-gcc.exe"
        if (Test-Path $gccPath) {
            Write-Success "Toolchain found at: $testPath"
            return $testPath
        }
        
        Write-Error "sh-elf-gcc.exe not found at: $testPath"
        Write-Info "Please verify the path and try again"
        
    } while ($true)
}

function Get-ToolchainVersionInfo {
    <#
    .SYNOPSIS
        Gets detailed version information about the toolchain
    #>
    param([string]$BinPath)
    
    $gccPath = Join-Path $BinPath "sh-elf-gcc.exe"
    
    try {
        $versionOutput = & $gccPath --version 2>&1 | Select-Object -First 3
        $gccVersion = ""
        $targetInfo = ""
        
        foreach ($line in $versionOutput) {
            if ($line -match "GCC (\S+)") {
                $gccVersion = $Matches[1]
            }
            if ($line -match "Target: (\S+)") {
                $targetInfo = $Matches[1]
            }
        }
        
        return @{
            Version = $gccVersion
            Target = $targetInfo
            BinPath = $BinPath
        }
    } catch {
        return @{
            Version = "unknown"
            Target = "unknown"
            BinPath = $BinPath
        }
    }
}

function Test-BuildVerification {
    <#
    .SYNOPSIS
        Verifies the build was successful and displays detailed info
    #>
    param(
        [string]$InstallPath,
        [string]$ToolchainBinPath
    )
    
    $libPath = Join-Path $InstallPath "lib\libsaturn.a"
    $pkgConfigPath = Join-Path $InstallPath "libsaturn.pc"
    
    Write-Host ""
    Write-Host "BUILD VERIFICATION" -ForegroundColor Cyan
    Write-Host ""
    
    # Check library exists
    if (-not (Test-Path $libPath)) {
        Write-Error "Library not created: $libPath"
        return $false
    }
    
    $libSize = (Get-Item $libPath).Length
    Write-Success "Library created: $libPath"
    Write-Info "Size: $([Math]::Round($libSize / 1KB, 1)) KB"
    
    # Toolchain info
    $toolchainInfo = Get-ToolchainVersionInfo -BinPath $ToolchainBinPath
    Write-Info "Toolchain: GCC $($toolchainInfo.Version) ($($toolchainInfo.Target))"
    
    # List contents
    Write-Host ""
    Write-Info "Library contents:"
    $arPath = Join-Path $ToolchainBinPath "sh-elf-ar.exe"
    if (Test-Path $arPath) {
        $objects = & $arPath -t $libPath 2>$null
        if ($LASTEXITCODE -eq 0 -and $objects) {
            foreach ($obj in $objects) {
                Write-Host "  - $obj" -ForegroundColor Gray
            }
        }
    }
    
    # Check pkg-config
    if (Test-Path $pkgConfigPath) {
        Write-Host ""
        Write-Success "pkg-config file created: $pkgConfigPath"
    }
    
    # Usage instructions
    Write-Host ""
    Write-Host "USAGE INSTRUCTIONS" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "For Makefile:" -ForegroundColor White
    Write-Host "  INCLUDES += -I$InstallPath\include" -ForegroundColor Gray
    Write-Host "  LIBRARIES += -L$InstallPath\lib -lsaturn" -ForegroundColor Gray
    Write-Host ""
    Write-Host "For pkg-config:" -ForegroundColor White
    Write-Host "  pkg-config --cflags libsaturn" -ForegroundColor Gray
    Write-Host "  pkg-config --libs libsaturn" -ForegroundColor Gray
    
    return $true
}

# Override Install-Toolchain to use new functions
function Install-Toolchain-Enhanced {
    param([string]$InstallPath)
    
    Write-Section "INSTALLING SH-ELF TOOLCHAIN (Enhanced)"
    
    if ($Script:State.ToolchainInstalled) {
        Write-Success "Toolchain already installed, skipping"
        return $true
    }
    
    # Try auto-detection first
    Write-Info "Auto-detecting existing toolchain..."
    $detectedPath = Find-ShElfGccExtended
    
    if ($detectedPath) {
        Write-Success ("Found existing toolchain: " + $detectedPath)
        Add-ToSessionPath $detectedPath
        $Script:State.ToolchainInstalled = $true
        Save-State
        Complete-Section
        return $true
    }
    
    # No toolchain found, ask user
    Write-Host ""
    Write-Host "No SH-ELF toolchain found on your system." -ForegroundColor Yellow
    Write-Host ""
    
    $source = Select-ToolchainSource
    
    switch ($source) {
        "bundled" {
            return Install-ShElfGcc-Prebuilt -InstallPath $InstallPath
        }
        "existing" {
            $customPath = Invoke-ExistingToolchainSetup
            if ($customPath) {
                Add-ToSessionPath $customPath
                $Script:State.ToolchainInstalled = $true
                Save-State
                Complete-Section
                return $true
            }
            return $false
        }
        "download" {
            Write-Host ""
            Write-Host "Please download from:" -ForegroundColor Cyan
            Write-Host "  https://segaxtreme.net/resources/gnu-sh-coff-toolchain-for-the-sega-saturn.31/" -ForegroundColor White
            Write-Host ""
            Write-Host "Extract to C:\sh-elf-gcc and re-run this script." -ForegroundColor White
            return $false
        }
        "msys2" {
            return Install-ShElfGcc-MSYS2
        }
        default {
            Write-Warning "Unknown option, falling back to bundled"
            return Install-ShElfGcc-Prebuilt -InstallPath $InstallPath
        }
    }
}

function Build-Library-Enhanced {
    <#
    .SYNOPSIS
        Enhanced build function with verification and feedback
    #>
    param([string]$InstallPath)
    
    Write-Section "BUILDING LIBSATURN LIBRARY (Enhanced)"
    
    if ($Script:State.LibraryBuilt) {
        Write-Success "Library already built, skipping"
        return $true
    }
    
    $buildScript = Join-Path $InstallPath "build.bat"
    
    if (-not (Test-Path $buildScript)) {
        Write-Error ("Build script not found: " + $buildScript)
        return $false
    }
    
    # Find toolchain bin path
    $toolchainBinPath = Find-ShElfGccExtended
    if (-not $toolchainBinPath) {
        Write-Error "Could not find sh-elf-gcc toolchain"
        return $false
    }
    
    Write-Info ("Found toolchain at: " + $toolchainBinPath)
    
    try {
        $buildStartTime = Get-Date
        Write-ProgressBar -Activity "Building Library" -Status "Compiling..." -PercentComplete 0
        
        $buildProcess = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "cd /d", ("`"" + $InstallPath + "`""), "&&", "call", $buildScript -Wait -PassThru -NoNewWindow
        
        $buildDuration = (Get-Date) - $buildStartTime
        
        if ($buildProcess.ExitCode -eq 0) {
            Write-ProgressBar -Activity "Building Library" -Status "Complete" -PercentComplete 100
            Write-Success ("Library built successfully (took " + ($buildDuration.TotalSeconds.ToString("N1")) + " seconds)")
            
            # Run verification
            Test-BuildVerification -InstallPath $InstallPath -ToolchainBinPath $toolchainBinPath
            
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
