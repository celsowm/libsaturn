@echo off
setlocal

set ROOT=%~dp0..\..
set TOOLCHAIN_BIN=%ROOT%\toolchains\sh-elf-gcc\bin
set TOOLCHAIN_LIB=%ROOT%\toolchains\sh-elf-gcc\lib\gcc\sh-elf\9.3.0

pushd %~dp0

set CC=%TOOLCHAIN_BIN%\sh-elf-gcc.exe
set OBJCOPY=%TOOLCHAIN_BIN%\sh-elf-objcopy.exe

set CFLAGS=-m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I%ROOT%\include -B%TOOLCHAIN_BIN%
set LDFLAGS=-T %ROOT%\saturn.ld -L%ROOT%\lib -L%TOOLCHAIN_LIB% -lsaturn

%CC% %CFLAGS% -c main.c -o main.o
if errorlevel 1 exit /b 1

%CC% %CFLAGS% main.o -o hello_world.elf %LDFLAGS%
if errorlevel 1 exit /b 1

%OBJCOPY% -O binary hello_world.elf hello_world.bin
if errorlevel 1 exit /b 1

echo Built hello_world.bin
popd
endlocal
