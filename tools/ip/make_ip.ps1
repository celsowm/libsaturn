param(
  [Parameter(Mandatory = $true)]
  [string]$BinaryPath,

  [string]$Title = "LIBSATURN HELLO WORLD",
  [string]$Version = "V1.000",
  [string]$Areas = "JTUE",
  [string]$Peripherals = "J",
  [string]$ReleaseDate = (Get-Date -Format "yyyyMMdd"),

  [uint32]$MasterStackAddr = 0x06010000,
  [uint32]$SlaveStackAddr = 0x06040000,
  [uint32]$FirstReadAddr = 0x06004000,
  [int]$FirstReadSize = -1
)

$ErrorActionPreference = "Stop"

function Format-FixedWidth {
  param(
    [string]$Text,
    [int]$Width
  )

  if ($null -eq $Text) {
    $Text = ""
  }
  if ($Text.Length -gt $Width) {
    $Text = $Text.Substring(0, $Width)
  }
  return $Text.PadRight($Width)
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ipDir = Join-Path $root "tools\ip"
$templatePath = Join-Path $ipDir "ip.sx"
$toolchainBin = Join-Path $root "toolchains\sh-elf-gcc\bin"
$asExe = Join-Path $toolchainBin "sh-elf-as.exe"
$gccExe = Join-Path $toolchainBin "sh-elf-gcc.exe"

if (-not (Test-Path $BinaryPath)) {
  throw "Binary not found: $BinaryPath"
}
if (-not (Test-Path $templatePath)) {
  throw "IP template not found: $templatePath"
}
if (-not (Test-Path $asExe)) {
  throw "Assembler not found: $asExe"
}
if (-not (Test-Path $gccExe)) {
  throw "Compiler not found: $gccExe"
}

$binaryFull = (Resolve-Path $BinaryPath).Path
$binarySize = (Get-Item $binaryFull).Length

if ($FirstReadSize -lt 0) {
  $FirstReadSize = [Math]::Max(0x20000, [int]$binarySize)
}

$versionFixed = Format-FixedWidth $Version 6
$releaseFixed = Format-FixedWidth $ReleaseDate 8
$areasFixed = Format-FixedWidth $Areas 10
$periphFixed = Format-FixedWidth $Peripherals 16

$titleClean = ($Title -replace "[`t`r`n`f`v]", "")
if ($titleClean.Length -gt 112) {
  $titleClean = $titleClean.Substring(0, 112)
}

$masterHex = ("0x{0:X8}" -f $MasterStackAddr)
$slaveHex = ("0x{0:X8}" -f $SlaveStackAddr)
$readAddrHex = ("0x{0:X8}" -f $FirstReadAddr)
$readSizeHex = ("0x{0:X8}" -f $FirstReadSize)

$workDir = Join-Path $ipDir ".tmp"
$tempAsm = Join-Path $workDir "ip.sx"
$tempObj = Join-Path $workDir "ip.o"

if (Test-Path $workDir) {
  Remove-Item -Recurse -Force $workDir
}
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

$pushed = $false
try {
  $outLines = New-Object System.Collections.Generic.List[string]
  foreach ($line in Get-Content $templatePath) {
    if ($line -match '\$VERSION') {
      $outLines.Add(($line -replace '\$VERSION', $versionFixed))
      continue
    }
    if ($line -match '\$RELEASE_DATE') {
      $outLines.Add(($line -replace '\$RELEASE_DATE', $releaseFixed))
      continue
    }
    if ($line -match '\$AREAS') {
      $outLines.Add(($line -replace '"\$AREAS"', ('"' + $areasFixed + '"')))
      continue
    }
    if ($line -match '\$PERIPHERALS') {
      $outLines.Add(($line -replace '"\$PERIPHERALS"', ('"' + $periphFixed + '"')))
      continue
    }
    if ($line -match '\$TITLE') {
      $indent = ""
      if ($line -match '^(\\s*)') {
        $indent = $Matches[1]
      }
      for ($i = 0; $i -lt 7; $i++) {
        $chunkStart = $i * 16
        $chunkLen = [Math]::Min(16, [Math]::Max(0, $titleClean.Length - $chunkStart))
        $chunk = ""
        if ($chunkLen -gt 0) {
          $chunk = $titleClean.Substring($chunkStart, $chunkLen)
        }
        $chunk = $chunk.PadRight(16)
        $outLines.Add($indent + '.ascii "' + $chunk + '"')
      }
      continue
    }
    if ($line -match '\$MASTER_STACK_ADDR') {
      $outLines.Add(($line -replace '\$MASTER_STACK_ADDR', $masterHex))
      continue
    }
    if ($line -match '\$SLAVE_STACK_ADDR') {
      $outLines.Add(($line -replace '\$SLAVE_STACK_ADDR', $slaveHex))
      continue
    }
    if ($line -match '\$1ST_READ_ADDR') {
      $outLines.Add(($line -replace '\$1ST_READ_ADDR', $readAddrHex))
      continue
    }
    if ($line -match '\$1ST_READ_SIZE') {
      $outLines.Add(($line -replace '\$1ST_READ_SIZE', $readSizeHex))
      continue
    }

    $outLines.Add($line)
  }

  Set-Content -Path $tempAsm -Value $outLines -Encoding ASCII

  Push-Location $ipDir
  $pushed = $true
  & $asExe --isa=sh2 --big --reduce-memory-overheads -I $ipDir -o $tempObj $tempAsm
  if ($LASTEXITCODE -ne 0) {
    throw "Assembler failed"
  }

  $outputDir = Split-Path -Parent $binaryFull
  $ipBin = Join-Path $outputDir "IP.BIN"
  $ipMap = Join-Path $outputDir "IP.BIN.map"
  $linkerScript = Join-Path $ipDir "ldscripts\\ip.x"

  & $gccExe -nostdlib -m2 -mb -nostartfiles ("-Wl,-T,$linkerScript") ("-Wl,-Map,$ipMap") $tempObj -o $ipBin
  if ($LASTEXITCODE -ne 0) {
    throw "Linker failed"
  }

  Write-Host "Generated IP.BIN at $ipBin"
} finally {
  if ($pushed) {
    Pop-Location
  }
  if (Test-Path $workDir) {
    Remove-Item -Recurse -Force $workDir
  }
}
