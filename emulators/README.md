# Emulators

Esta pasta centraliza binarios e launchers para validacao do MVP.

## Estrutura

- `mednafen/`: launchers e arquivos locais do Mednafen.
- `kronos/`: instalacao manual do Kronos e launcher.

## Mednafen (automatico)

Instale via MSYS2 UCRT64 e gere launcher:

```powershell
.\scripts\download-emulators.ps1
```

Launcher gerado:

```powershell
.\emulators\mednafen\run-mednafen.ps1
```

## Kronos (manual)

1. Baixe o release do Kronos para Windows.
2. Copie `kronos.exe` e DLLs para `emulators/kronos/`.
3. Execute:

```powershell
.\emulators\kronos\run-kronos.ps1
```

Se o binario nao estiver no local esperado, o launcher retorna erro explicito.
