# libsaturn-1

Biblioteca bare-metal para desenvolvimento de jogos de Sega Saturn sem SGL/libyaul.

## Status do MVP

Este repositório entrega o MVP `2D Core`:

- Runtime bare-metal (startup, linker, loop de frame em VBlank).
- HAL mínima para VDP1, VDP2, SCU e SMPC.
- API pública C (`include/saturn/saturn.h`) com núcleo interno C++.
- Pipeline de build para `ELF -> BIN -> ISO`.
- Demo jogável 2D em `examples/mvp_2d_scene`.
- Conversor de assets indexados 8-bit em `tools/convert_indexed8.py`.

## Estrutura principal

- `include/saturn/saturn.h`: API C pública.
- `src/core`: implementação core, startup e linker script.
- `src/hal`: acesso direto a registradores de hardware.
- `examples/mvp_2d_scene`: demo de validação do MVP.
- `scripts`: setup MSYS2, build de toolchain, smoke build e automacao PowerShell.
- `tools`: utilitários (geração de `ip.bin`, conversor de assets).

## Fluxo Windows (PowerShell)

Para Windows 10/11, use o fluxo PowerShell (nao precisa abrir bash manualmente):

```powershell
.\scripts\bootstrap-msys2.ps1 full
```

O script tenta localizar MSYS2 nesta ordem:

1. `-Msys2Root`
2. `LIBSATURN_MSYS2_ROOT`
3. `C:\msys64`

Se nao encontrar MSYS2, tenta instalar via `winget install MSYS2.MSYS2`. Se falhar, exibe instrucoes de instalacao manual.

Comandos disponiveis:

```powershell
.\scripts\bootstrap-msys2.ps1 host
.\scripts\bootstrap-msys2.ps1 full
.\scripts\bootstrap-msys2.ps1 smoke
.\scripts\bootstrap-msys2.ps1 acceptance
```

Flags uteis:

```powershell
.\scripts\bootstrap-msys2.ps1 full -Msys2Root C:\msys64 -LogPath .\build\bootstrap.log
.\scripts\bootstrap-msys2.ps1 host -NoInstall
```

## Emuladores (Windows)

Prepare a pasta `emulators/` e instale Mednafen via MSYS2:

```powershell
.\scripts\download-emulators.ps1
```

Se o pacote nao estiver disponivel no repo MSYS2 atual, o script tenta instalar Mednafen via winget (`MednafenTeam.Mednafen`).

Isso cria:

- `emulators/mednafen/run-mednafen.ps1`
- `emulators/kronos/run-kronos.ps1`

Kronos segue instalacao manual em `emulators/kronos/kronos.exe`.

## Checklist de aceite

Execute o checklist guiado:

```powershell
.\scripts\check-acceptance.ps1 -Emulator both
```

Saida:

- `build/acceptance-report.txt`

Protocolo completo: `docs/acceptance.md`.

## Requisitos de host (MSYS2 shell)

Execute no shell UCRT64 ou MINGW64:

```bash
bash scripts/bootstrap.sh host
```

## Build da toolchain SH2

```bash
bash scripts/build-toolchain.sh
```

Atalho para host + toolchain em um comando:

```bash
bash scripts/bootstrap.sh full
```

Por padrão instala em `$HOME/saturn-tools` e produz `sh2eb-elf-gcc`.

## Build do MVP

```bash
make
```

Saídas:

- `build/mvp.elf`
- `build/mvp.bin`
- `build/mvp.iso`
- `build/libsaturn.a`

## Smoke tests

```bash
bash scripts/smoke-build.sh
python -m unittest tests/test_asset_converter.py
```

## Emuladores alvo

- Kronos (debug).
- Mednafen (timing/compatibilidade).

## Nota sobre IP.BIN

O projeto gera um `ip.bin` mínimo em `make` usando `tools/gen_ip_bin.py`. Antes de distribuição pública, faça revisão legal/licenciamento de boot assets conforme sua política de release.
