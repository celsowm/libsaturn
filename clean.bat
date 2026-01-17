@echo off
REM libsaturn Windows clean script

echo Cleaning libsaturn...

if exist lib\libsaturn.a del lib\libsaturn.a
if exist lib\*.o del lib\*.o

for /r %%f in (*.o) do del "%%f" 2>nul
for /r %%f in (*.elf) do del "%%f" 2>nul
for /r %%f in (*.BIN) do del "%%f" 2>nul
for /r %%f in (*.iso) do del "%%f" 2>nul

echo Clean complete!
