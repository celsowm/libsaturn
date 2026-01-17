@echo off
REM Build example 01_helloworld

cd examples\01_helloworld

echo Building example 01_helloworld...

sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I..\..\include -c ..\..\src\crt0.s -o crt0.o
sh-elf-gcc -m2 -mb -O2 -fomit-frame-pointer -nostartfiles -I..\..\include -c main.c -o main.o
sh-elf-ld -T ..\..\saturn.ld -o game.elf crt0.o main.o
sh-elf-objcopy -O binary game.elf 0.BIN

echo.
echo Build complete!
echo Output: examples\01_helloworld\0.BIN

cd ..\..
