# Emulators

This folder contains emulator launchers and local emulator files.

## POSIX shell

With GNU Make 4.3+, an SH-2 toolchain, Mednafen, and a caller-owned Saturn BIOS:

```sh
./build-example.sh skybridge_3d
./run-example.sh skybridge_3d --bios /path/to/your/saturn-bios.bin
```

The Mednafen launcher expects a 512 KiB BIOS dump and stores it under the
ignored `bios/firmware/` directory. Set `LIBSATURN_MEDNAFEN_HOME` to use a
different writable Mednafen state directory. Additional Mednafen options can
be passed after `--` to `run-example.sh`.

Ymir captures use the native wrapper:

```sh
./harness/run-harness.sh skybridge_3d --bios /path/to/your/saturn-bios.bin \
    --frames 120 -- --screenshot 119:harness/build/skybridge_3d.png
```

## Windows wrappers

The Windows/MSYS2 workflow retains its PowerShell bootstrap and launchers:

```powershell
.\scripts\bootstrap-dev.ps1
.\build-example.ps1 -Example skybridge_3d
.\run-example.ps1 skybridge_3d -Emulator mednafen
```

Kronos and Yaba Sanshiro launchers are Windows-only.
