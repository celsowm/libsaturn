@echo off
setlocal EnableDelayedExpansion
pushd %~dp0

set ROOT=%~dp0..\..
set TOOLCHAIN_UTIL=%ROOT%\toolchains\sh-elf-gcc\Other Utilities
set MKISOFS=%TOOLCHAIN_UTIL%\mkisofs.exe
set CUE_MAKER=%TOOLCHAIN_UTIL%\JoEngineCueMaker.exe

set BIN=hello_world.bin
set BIN_COPY=0.BIN
set ISO=game.iso
set CUE=game.cue
set ABS=ABS.TXT
set BIB=BIB.TXT
set CPY=CPY.TXT

call "%~dp0build.bat"
if errorlevel 1 (
  popd
  exit /b 1
)

set EMU=%SATURN_EMU%
if "%EMU%"=="" set EMU=%SATURN_EMULATOR%
if "%EMU%"=="" set EMU=%SATURN_EMU_EXE%
if "%EMU%"=="" if exist "%ROOT%\tools\emulator\mednafen\mednafen.exe" set EMU=%ROOT%\tools\emulator\mednafen\mednafen.exe

set EMU_EXE=
for %%I in ("%EMU%") do set EMU_EXE=%%~nxI
set IS_MEDNAFEN=0
if /I "%EMU_EXE%"=="mednafen.exe" set IS_MEDNAFEN=1

if "%EMU%"=="" (
  echo Set SATURN_EMU to your emulator executable path.
  echo Or run tools\emulator\install_mednafen.bat to install Mednafen.
  popd
  exit /b 1
)

if "%IS_MEDNAFEN%"=="1" (
  if not exist "%ROOT%\tools\emulator\mednafen\firmware\sega_101.bin" if not exist "%ROOT%\tools\emulator\mednafen\firmware\mpr-17933.bin" if not exist "%ROOT%\tools\emulator\mednafen\sega_101.bin" if not exist "%ROOT%\tools\emulator\mednafen\mpr-17933.bin" (
    echo Mednafen BIOS missing. Place sega_101.bin or mpr-17933.bin in "%ROOT%\tools\emulator\mednafen\firmware".
    echo Download mpr-17933.bin from:
    echo   https://raw.githubusercontent.com/Abdess/retroarch_system/libretro/Sega%%20-%%20Saturn/mpr-17933.bin
    set BIOS_AGREE=
    set /p BIOS_AGREE=Download now? [Y/n]:
    if /I "!BIOS_AGREE!"=="Y" (
      call "%ROOT%\tools\emulator\install_bios_mpr17933.bat" Y
    ) else if "!BIOS_AGREE!"=="" (
      call "%ROOT%\tools\emulator\install_bios_mpr17933.bat" Y
    ) else (
      echo BIOS download skipped.
      popd
      exit /b 1
    )

    if not exist "%ROOT%\tools\emulator\mednafen\firmware\sega_101.bin" if not exist "%ROOT%\tools\emulator\mednafen\firmware\mpr-17933.bin" if not exist "%ROOT%\tools\emulator\mednafen\sega_101.bin" if not exist "%ROOT%\tools\emulator\mednafen\mpr-17933.bin" (
      echo BIOS still missing. Aborting.
      popd
      exit /b 1
    )
  )
)

set IPBIN=%SATURN_IPBIN%
if "%IPBIN%"=="" if exist "%~dp0IP.BIN" set IPBIN=%~dp0IP.BIN
if "%IPBIN%"=="" if exist "%ROOT%\IP.BIN" set IPBIN=%ROOT%\IP.BIN

if "%SATURN_IPBIN%"=="" if "%IS_MEDNAFEN%"=="1" (
  if exist "%ROOT%\tools\ip\make_ip.bat" (
    call "%ROOT%\tools\ip\make_ip.bat" -BinaryPath "%~dp0%BIN%"
    if errorlevel 1 (
      popd
      exit /b 1
    )
    if exist "%~dp0IP.BIN" set IPBIN=%~dp0IP.BIN
  )
)

if not "%IPBIN%"=="" (
  if exist "%IPBIN%" (
    copy /b "%BIN%" "%BIN_COPY%" >nul
    powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='%BIN_COPY%';$min=0x20000;$fi=Get-Item $f;if ($fi.Length -lt $min) { $fs=[IO.File]::Open($f,'Open','ReadWrite'); $fs.SetLength($min); $fs.Close() }" >nul 2>&1
    if not exist "%MKISOFS%" (
      echo mkisofs not found: "%MKISOFS%"
      popd
      exit /b 1
    )
    if not exist "%ABS%" echo Libsaturn > "%ABS%"
    if not exist "%BIB%" echo Libsaturn > "%BIB%"
    if not exist "%CPY%" echo Libsaturn > "%CPY%"

    "%MKISOFS%" -quiet -sysid "SEGA SEGASATURN" -volid "HELLO_WORLD" -publisher "SEGA ENTERPRISES, LTD." -preparer "SEGA ENTERPRISES, LTD." -appid "SEGA ENTERPRISES, LTD." -iso-level 1 -input-charset iso8859-1 -no-bak -m ".*" -abstract "%ABS%" -biblio "%BIB%" -copyright "%CPY%" -G "%IPBIN%" -o "%ISO%" -graft-points "0.BIN=%BIN_COPY%" "%ABS%=%ABS%" "%BIB%=%BIB%" "%CPY%=%CPY%"
    if errorlevel 1 (
      popd
      exit /b 1
    )

    if exist "%CUE_MAKER%" (
      "%CUE_MAKER%" "%~dp0" >nul 2>&1
    )

    if not exist "%CUE%" (
      > "%CUE%" echo FILE "%ISO%" BINARY
      >> "%CUE%" echo   TRACK 01 MODE1/2048
      >> "%CUE%" echo     INDEX 01 00:00:00
    )

    if exist "%CUE%" (
      "%EMU%" "%CUE%" %SATURN_EMU_ARGS%
    ) else (
      "%EMU%" "%ISO%" %SATURN_EMU_ARGS%
    )
    popd
    exit /b 0
  )
)

if "%IS_MEDNAFEN%"=="1" (
  echo Mednafen requires a bootable ISO with IP.BIN.
  echo Run tools\ip\make_ip.bat -BinaryPath "%~dp0%BIN%" or set SATURN_IPBIN.
  popd
  exit /b 1
)

echo No IP.BIN found. Set SATURN_IPBIN if your emulator requires a bootable ISO.
echo Falling back to running raw binary.
"%EMU%" "%BIN%" %SATURN_EMU_ARGS%
popd
