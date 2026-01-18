#!/usr/bin/env pwsh
<#
.SYNOPSIS
    libsaturn Library Setup - Automated toolchain and library build
.DESCRIPTION
    This script automatically sets up the SH-ELF toolchain and builds libsaturn.
    It detects existing toolchains, uses the bundled toolchain as fallback,
    and provides a clean build with verification.
.PARAMETER ToolchainPath
    Path to existing SH-ELF toolchain (auto-detected if not provided)
.PARAMETER InstallPath
    Installation path for libsaturn (default: ~/saturn-sdk)
.PARAMETER SkipBuild
    Skip building the library (toolchain setup only)
.PARAMETER Verbose
    Show detailed output
.EXAMPLE
    .\setup_lib.ps1
    Run with auto-detected toolchain
.EXAMPLE
    .\setup_lib.ps1 -ToolchainPath "C:\saturn-sdk\sh-elf-gcc"
    Use specific toolchain location
#>

param(
    [string]$ToolchainPath = "",
    [string]$InstallPath = "$env:USERPROFILE\saturn-sdk",
    [switch]$SkipBuild,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$Script:Version = "1.0.0"

if ($Verbose) {
    $VerbosePreference = "Continue"
}

function Write-Banner {
    param([string]$Text)
    $width = 60
    $padding = " " * [Math]::Max(0, ($width - $Text.Length) / 2)
    Write-Host ""
    Write-Host ("=" * $width) -ForegroundColor Cyan
    Write-Host ($padding + $Text) -ForegroundColor Cyan
    Write-Host ("=" * $width) -ForegroundColor Cyan
    Write-Host ""
}

function Write-Success {
    param([string]$Message)
    Write-Host "[OK] " -ForegroundColor Green -NoNewline
    Write-Host $Message
}

function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] " -ForegroundColor Blue -NoNewline
    Write-Host $Message
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARN] " -ForegroundColor Yellow -NoNewline
    Write-Host $Message
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] " -ForegroundColor Red -NoNewline
    Write-Host $Message
}

function Test-Command {
    param([string]$Name)
    try {
        Get-Command $Name -ErrorAction Stop | Out-Null
        return $true
    } catch {
        return $false
    }
}

function Find-ShElfGcc {
    <#
    .SYNOPSIS
        Locates sh-elf-gcc executable in various locations
    #>
    
    $searchPaths = @(
        # Bundled with libsaturn
        (Join-Path $PSScriptRoot "toolchains\sh-elf-gcc\bin"),
        
        # Common Saturn SDK locations
        "$env:USERPROFILE\saturn-sdk\sh-elf-gcc\bin",
        "$env:USERPROFILE\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc.exe",
        "C:\saturn-sdk\sh-elf-gcc\bin",
        "C:\saturn-sdk\sh-elf-gcc\bin\sh-elf-gcc.exe",
        
        # MSYS2 locations
        "C:\msys64\mingw64\bin",
        "C:\msys64\mingw32\bin",
        "C:\tools\msys64\mingw64\bin",
        
        # Other known locations
        "C:\sh-elf-gcc\bin",
        "$env:ProgramFiles\SaturnSDK\sh-elf-gcc\bin",
        "$env:ProgramFiles (x86)\SaturnSDK\sh-elf-gcc\bin"
    )
    
    $providedPath = $ToolchainPath.Trim()
    if (-not [string]::IsNullOrEmpty($providedPath)) {
        if (Test-Path (Join-Path $providedPath "sh-elf-gcc.exe")) {
            return (Join-Path $providedPath "bin")
        }
        if (Test-Path (Join-Path $providedPath "bin\sh-elf-gcc.exe")) {
            return (Join-Path $providedPath "bin")
        }
        if (Test-Path $providedPath) {
            $gccPath = Get-ChildItem -Path $providedPath -Filter "sh-elf-gcc.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($gccPath) {
                return $gccPath.DirectoryName
            }
        }
        Write-Warning "Provided toolchain path not found: $providedPath"
    }
    
    foreach ($path in $searchPaths) {
        $gccPath = Join-Path $path "sh-elf-gcc.exe"
        if (Test-Path $gccPath) {
            Write-Verbose "Found at: $path"
            return $path
        }
    }
    
    if (Test-Command "sh-elf-gcc") {
        $shGcc = Get-Command "sh-elf-gcc"
        $shGccPath = Split-Path $shGcc.Source -Parent
        Write-Verbose "Found in PATH: $shGccPath"
        return $shGccPath
    }
    
    return $null
}

function Install-BundledToolchain {
    <#
    .SYNOPSIS
        Installs the bundled toolchain to the user's Saturn SDK directory
    #>
    
    $bundledPath = Join-Path $PSScriptRoot "toolchains\sh-elf-gcc"
    $installDir = Join-Path $InstallPath "sh-elf-gcc"
    $installBin = Join-Path $installDir "bin"
    
    if (-not (Test-Path $bundledPath)) {
        Write-Error "Bundled toolchain not found at: $bundledPath"
        return $null
    }
    
    Write-Info "Bundled toolchain found at: $bundledPath"
    
    if (Test-Path (Join-Path $installBin "sh-elf-gcc.exe")) {
        Write-Success "Toolchain already installed at: $installDir"
        return $installBin
    }
    
    Write-Info "Installing bundled toolchain to: $installDir"
    
    try {
        if (-not (Test-Path $installDir)) {
            New-Item -ItemType Directory -Path $installDir -Force | Out-Null
        }
        
        Copy-Item -Path $bundledPath -Destination $installDir -Recurse -Force
        
        if (Test-Path (Join-Path $installBin "sh-elf-gcc.exe")) {
            Write-Success "Toolchain installed successfully"
            return $installBin
        } else {
            Write-Error "Toolchain installation failed"
            return $null
        }
    } catch {
        Write-Error "Failed to install toolchain: $($_.Exception.Message)"
        return $null
    }
}

function Set-ToolchainPath {
    <#
    .SYNOPSIS
        Adds toolchain to PATH for current session
    #>
    param([string]$BinPath)
    
    if ($env:PATH -notlike "*$BinPath*") {
        $env:PATH = $BinPath + ";" + $env:PATH
        Write-Verbose "Added to PATH: $BinPath"
    }
}

function Get-ToolchainVersion {
    <#
    .SYNOPSIS
        Gets the toolchain version string
    #>
    param([string]$BinPath)
    
    $gccPath = Join-Path $BinPath "sh-elf-gcc.exe"
    try {
        $output = & $gccPath --version 2>&1 | Select-Object -First 1
        $version = $output -replace ".*\(GCC\) (\S+).*", '$1'
        return $version
    } catch {
        return "unknown"
    }
}

function Test-BuildSuccess {
    <#
    .SYNOPSIS
        Verifies the build was successful and displays info
    #>
    param(
        [string]$InstallPath,
        [string]$ToolchainVersion
    )
    
    $libPath = Join-Path $InstallPath "lib\libsaturn.a"
    
    if (-not (Test-Path $libPath)) {
        Write-Error "Library not created at: $libPath"
        return $false
    }
    
    $libSize = (Get-Item $libPath).Length
    Write-Host ""
    Write-Success "Library built successfully!"
    Write-Host ""
    Write-Host "  Library: $libPath" -ForegroundColor White
    Write-Host "  Size:    $([Math]::Round($libSize / 1KB, 1)) KB" -ForegroundColor White
    Write-Host "  Toolchain: GCC $ToolchainVersion" -ForegroundColor White
    Write-Host ""
    
    Write-Info "Contents:"
    $objects = & (Join-Path (Split-Path $libPath) "..\..\toolchains\sh-elf-gcc\bin\sh-elf-ar.exe") -t $libPath 2>$null
    if ($LASTEXITCODE -eq 0 -and $objects) {
        foreach ($obj in $objects) {
            Write-Host "    - $obj" -ForegroundColor Gray
        }
    }
    
    Write-Host ""
    Write-Info "Quick start for your project:"
    Write-Host ""
    Write-Host "  # Add to your Makefile:" -ForegroundColor Cyan
    Write-Host "  INCLUDES += -I$InstallPath/include" -ForegroundColor White
    Write-Host "  LIBRARIES += -L$InstallPath/lib -lsaturn" -ForegroundColor White
    Write-Host ""
    
    return $true
}

function New-PkgConfigFile {
    <#
    .SYNOPSIS
        Creates a pkg-config file for consumer projects
    #>
    param(
        [string]$InstallPath,
        [string]$ToolchainPath
    )
    
    $pkgConfigPath = Join-Path $InstallPath "libsaturn.pc"
    
    $libDir = Join-Path $InstallPath "lib"
    $includeDir = Join-Path $InstallPath "include"
    
    $pkgConfig = @"
prefix=$InstallPath
exec_prefix=`${prefix}
bindir=`${exec_prefix}/toolchains/sh-elf-gcc/bin
libdir=`${exec_prefix}/lib
incdir=`${exec_prefix}/include

Name: libsaturn
Description: Sega Saturn development library
Version: 1.0.0
Cflags: -I`${incdir}
Libs: -L`${libdir} -lsaturn
"@
    
    try {
        $pkgConfig | Set-Content -Path $pkgConfigPath -Encoding UTF8
        Write-Verbose "Created pkg-config file: $pkgConfigPath"
        return $true
    } catch {
        Write-Warning "Failed to create pkg-config file: $($_.Exception.Message)"
        return $false
    }
}

function Invoke-Build {
    <#
    .SYNOPSIS
        Runs the build process
    #>
    param([string]$InstallPath)
    
    $buildScript = Join-Path $InstallPath "build.bat"
    
    if (-not (Test-Path $buildScript)) {
        Write-Error "Build script not found: $buildScript"
        return $false
    }
    
    Write-Info "Running build script..."
    Write-Host ""
    
    try {
        $buildProcess = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "cd /d", "`"$InstallPath`"", "&&", "call", $buildScript -Wait -PassThru -NoNewWindow
        
        if ($buildProcess.ExitCode -eq 0) {
            return $true
        } else {
            Write-Error "Build failed with exit code: $($buildProcess.ExitCode)"
            return $false
        }
    } catch {
        Write-Error "Build failed: $($_.Exception.Message)"
        return $false
    }
}

function New-EnvironmentShim {
    <#
    .SYNOPSIS
        Creates a setup script for easy environment configuration
    #>
    param(
        [string]$InstallPath,
        [string]$ToolchainPath
    )
    
    $shimPath = Join-Path $InstallPath "saturn-env.ps1"
    
    $shimContent = @"
# libsaturn Environment Configuration
# Generated by setup_lib.ps1
# Source this file or dot-source it in your PowerShell session

`$env:SATURN_SDK = "$InstallPath"
`$env:SATURN_TOOLCHAIN = "$ToolchainPath"
`$env:PATH = "$ToolchainPath;`$env:PATH"

Write-Host "Saturn SDK configured:"
Write-Host "  SATURN_SDK:      `$env:SATURN_SDK"
Write-Host "  SATURN_TOOLCHAIN: `$env:SATURN_TOOLCHAIN"
"@
    
    try {
        $shimContent | Set-Content -Path $shimPath -Encoding UTF8
        Write-Verbose "Created environment shim: $shimPath"
    } catch {
        Write-Warning "Failed to create environment shim: $($_.Exception.Message)"
    }
}

function New-MakefileTemplate {
    <#
    .SYNOPSIS
        Creates a Makefile template for consumer projects
    #>
    param([string]$InstallPath)
    
    $templatePath = Join-Path $InstallPath "Makefile.libsaturn"
    
    $template = @"
# libsaturn Project Template Makefile
# Copy this to your project's Makefile and customize

# Toolchain configuration
SATURN_SDK ?= $InstallPath
SATURN_TOOLCHAIN ?= $(SATURN_SDK)/toolchains/sh-elf-gcc

CC := $(SATURN_TOOLCHAIN)/bin/sh-elf-gcc
AR := $(SATURN_TOOLCHAIN)/bin/sh-elf-ar
OBJCOPY := $(SATURN_TOOLCHAIN)/bin/sh-elf-objcopy

# Compiler flags
CFLAGS := -m2 -mb -O2 -fomit-frame-pointer -nostartfiles
CFLAGS += -I$(SATURN_SDK)/include
CFLAGS += -Wall -Wextra

# Linker flags
LDFLAGS := -L$(SATURN_SDK)/lib -lsaturn

# Output
TARGET := program.bin
OBJS := main.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
"@
    
    try {
        $template | Set-Content -Path $templatePath -Encoding UTF8
        Write-Verbose "Created Makefile template: $templatePath"
    } catch {
        Write-Warning "Failed to create Makefile template: $($_.Exception.Message)"
    }
}

function New-ExampleMain {
    <#
    .SYNOPSIS
        Creates an example main.c file
    #>
    param([string]$InstallPath)
    
    $examplePath = Join-Path $InstallPath "example_main.c"
    
    $example = @"
/*
 * libsaturn Example Program
 * Basic Saturn initialization and main loop
 */

#include <saturn/saturn.h>
#include <saturn/vdp.h>
#include <saturn/peripheral.h>

int main(void) {
    /* Initialize Saturn hardware */
    saturn_init();
    
    /* Clear the screen */
    vdp_clear();
    
    /* Initialize peripherals */
    controller_init();
    
    /* Main loop */
    while (1) {
        /* Update controllers */
        controller_update();
        
        /* Check for exit */
        if (controller_get_button(0, CONTROLLER_BUTTON_START) &&
            controller_get_button(0, CONTROLLER_BUTTON_A)) {
            break;
        }
        
        /* Wait for VBLANK */
        vdp_wait_vblank();
    }
    
    return 0;
}
"@
    
    try {
        $example | Set-Content -Path $examplePath -Encoding UTF8
        Write-Verbose "Created example main: $examplePath"
    } catch {
        Write-Warning "Failed to create example main: $($_.Exception.Message)"
    }
}

function Start-LibrarySetup {
    <#
    .SYNOPSIS
        Main function to run the library setup
    #>
    
    Write-Banner "LIBSATURN SETUP v$($Script:Version)"
    
    Write-Host "Initializing Saturn development environment..." -ForegroundColor White
    Write-Host ""
    
    $toolchainBinPath = $null
    
    # Step 1: Find existing toolchain
    Write-Info "Step 1/5: Detecting SH-ELF toolchain..."
    $toolchainBinPath = Find-ShElfGcc
    
    if ($toolchainBinPath) {
        Write-Success "Found existing toolchain at: $toolchainBinPath"
    } else {
        Write-Warning "No existing toolchain found"
        
        # Step 2: Install bundled toolchain
        Write-Host ""
        Write-Info "Step 2/5: Installing bundled toolchain..."
        $toolchainBinPath = Install-BundledToolchain
        
        if (-not $toolchainBinPath) {
            Write-Error "Failed to install toolchain. Please install manually."
            return $false
        }
    }
    
    # Step 3: Configure environment
    Write-Host ""
    Write-Info "Step 3/5: Configuring environment..."
    Set-ToolchainPath -BinPath $toolchainBinPath
    $toolchainVersion = Get-ToolchainVersion -BinPath $toolchainBinPath
    Write-Success "Environment configured (GCC $toolchainVersion)"
    
    # Step 4: Build library (if not skipped)
    Write-Host ""
    if ($SkipBuild) {
        Write-Info "Step 4/5: Skipping build (--SkipBuild specified)"
    } else {
        Write-Info "Step 4/5: Building libsaturn library..."
        $buildSuccess = Invoke-Build -InstallPath $InstallPath
        
        if (-not $buildSuccess) {
            Write-Error "Build failed. Check output above for errors."
            return $false
        }
    }
    
    # Step 5: Create supporting files
    Write-Host ""
    Write-Info "Step 5/5: Creating supporting files..."
    
    New-PkgConfigFile -InstallPath $InstallPath -ToolchainPath $toolchainBinPath
    New-EnvironmentShim -InstallPath $InstallPath -ToolchainPath $toolchainBinPath
    New-MakefileTemplate -InstallPath $InstallPath
    New-ExampleMain -InstallPath $InstallPath
    
    # Verify and report
    Write-Host ""
    if (-not $SkipBuild) {
        Test-BuildSuccess -InstallPath $InstallPath -ToolchainVersion $toolchainVersion
    }
    
    Write-Host ""
    Write-Host "Setup complete!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Files created:" -ForegroundColor Cyan
    Write-Host "  - lib\libsaturn.a       (library)" -ForegroundColor White
    Write-Host "  - libsaturn.pc          (pkg-config)" -ForegroundColor White
    Write-Host "  - saturn-env.ps1        (environment)" -ForegroundColor White
    Write-Host "  - Makefile.libsaturn    (template)" -ForegroundColor White
    Write-Host "  - example_main.c        (example)" -ForegroundColor White
    Write-Host ""
    
    return $true
}

# Run the setup
$result = Start-LibrarySetup

if (-not $result) {
    exit 1
}

exit 0
