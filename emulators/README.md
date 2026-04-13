# Emulators

This folder centralizes binaries and launchers for MVP validation.

## Structure

- `mednafen/`: Mednafen launchers and local files.
- `kronos/`: Manual Kronos installation and launcher.

## Mednafen (automatic)

Install via MSYS2 UCRT64 and generate launcher:

```powershell
.\scripts\download-emulators.ps1
```

Generated launcher:

```powershell
.\emulators\mednafen\run-mednafen.ps1
```

## Kronos (manual)

1. Download Kronos release for Windows.
2. Copy `kronos.exe` and DLLs to `emulators/kronos/`.
3. Execute:

```powershell
.\emulators\kronos\run-kronos.ps1
```

If the binary is not in the expected location, the launcher returns an explicit error.