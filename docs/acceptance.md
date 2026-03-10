# Acceptance Checklist

Este documento e a referencia do fluxo de aceite manual para o MVP.
Use o script `scripts/check-acceptance.ps1` para executar o protocolo e gerar relatorio.

## Pre-requisitos

- Windows PowerShell.
- MSYS2 com perfil UCRT64.
- Toolchain SH2 disponivel no PATH do ambiente MSYS2 (`sh2eb-elf-gcc`).
- `mkisofs`, `genisoimage` ou `xorrisofs` disponivel no ambiente MSYS2.
- Mednafen instalado (`scripts/download-emulators.ps1`) e/ou Kronos instalado manualmente.

## Execucao do checklist

```powershell
.\scripts\check-acceptance.ps1 -Emulator both
```

Opcoes:

- `-Emulator mednafen|kronos|both`
- `-IsoPath <caminho>`
- `-ReportPath <caminho>`
- `-Msys2Root <caminho>`

## O que o script faz

1. Valida ferramenta SH2, `make` e `mkisofs`/`genisoimage`/`xorrisofs` no MSYS2.
2. Executa build limpa (`make clean && make all`) para gerar ISO.
3. Mostra comandos para abrir a ISO no emulador selecionado.
4. Coleta confirmacao manual para os criterios de aceite.
5. Salva relatorio em `build/acceptance-report.txt` (ou caminho customizado).

## Criterios de aceite

1. ISO inicia e entra no loop principal.
2. Render de sprites 2D estavel por 1800 frames.
3. Input sem ghost presses por 5 minutos.

## Resultado e codigos de saida

- `0`: PASS (todos os criterios aprovados).
- `2`: FAIL (ao menos um criterio reprovado).
- `3`: PARTIAL (criterios pulados, por exemplo Kronos ausente).
