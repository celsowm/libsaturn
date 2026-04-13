# libsaturn-1

Biblioteca bare-metal para desenvolvimento de jogos de Sega Saturn sem SGL/libyaul.

## Status do MVP

Este repositório entrega o MVP `2D Core`:

- Runtime bare-metal (startup, linker, loop de frame em VBlank).
- HAL mínima para VDP1, VDP2, SCU e SMPC.
- API pública C (`include/saturn/saturn.h`) com núcleo interno C++.
- Pipeline de build para `ELF -> BIN -> ISO`.
- Demo jogável 2D em `examples/mvp_2d_scene`.
- Demo simples de movimento em `examples/red_square`.
- Demo separada de textura em `examples/text_sprite`.
- Conversor de assets indexados 8-bit em `tools/convert_indexed8.py`, com saida em `C/H` para embed no build.

## Estrutura principal

- `include/saturn/saturn.h`: API C pública.
- `src/core`: implementação core, startup e linker script.
- `src/hal`: acesso direto a registradores de hardware.
- `examples/mvp_2d_scene`: demo de validação do MVP.
- `examples/red_square`: quadrado vermelho movido pelo direcional.
- `scripts`: setup MSYS2, build de toolchain, smoke build e automacao PowerShell.
- `tools`: utilitários (geração de `ip.bin`, conversor de assets).

## Fluxo Windows (PowerShell)

Para Windows 10/11, use o fluxo PowerShell (nao precisa abrir bash manualmente):

```powershell
.\scripts\bootstrap-msys2.ps1 full
```

Para preparar tudo de uma vez (host + toolchain + emuladores), use:

```powershell
.\scripts\bootstrap-dev.ps1
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

Para Mednafen, mantenha BIOS JP em `firmware/sega_101.bin` e BIOS US/EU em `firmware/mpr-17933.bin`.
O launcher tenta copiar automaticamente a partir de `bios/saturn_bios_jp.bin` e `bios/saturn_bios_us.bin` (ou `bios/saturn_bios_eu.bin`).
Por padrao, o launcher do Mednafen usa `region_autodetect=1` com fallback `region_default=na` e forca `ss.h_overscan=0` / `ss.videoip=0` para evitar corte/artefatos na tela de licenca.

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
- `build/mvp.cue`
- `build/libsaturn.a`

Perfis de diagnóstico de boot (IP.BIN):

```bash
make IP_PROFILE=current
make IP_PROFILE=safe
make IP_TEMPLATE_KIND=yaul
make IP_TEMPLATE_KIND=sbl
```

Cada build também gera artefatos nomeados por variante:

- `build/mvp-<ip_profile>.iso`
- `build/mvp-<ip_profile>.cue`

Matriz 1x2 automatizada (build + decisão):

```powershell
.\scripts\build-boot-matrix.ps1
# preencher build\boot-matrix-manual-results.csv
.\scripts\evaluate-boot-matrix.ps1
```

## Smoke tests

```bash
bash scripts/smoke-build.sh
python -m unittest tests/test_asset_converter.py
```

## Assets 2D para VDP1

O conversor `tools/convert_indexed8.py` gera:

- `.tex8` e `.pal` para inspeção/binário legado.
- `.h` e `.c` com pixels, palette e metadados para compilar no exemplo.

Exemplo usado pelo repositório:

```bash
python tools/convert_indexed8.py \
  --input assets/sonic_head.png \
  --resize 128 96 \
  --out-prefix build/generated/text_sprite/sonic_head
```

O exemplo `text_sprite` reduz o `sonic_head.png` para caber no caminho simples de sprite do VDP1.
Para o exemplo `text_sprite`, o `make` chama essa geração automaticamente antes de compilar o binário.

## Emuladores alvo

- Kronos (debug).
- Mednafen (timing/compatibilidade).

Para alternar BIOS/regiao no launcher de exemplo sem editar config global do Mednafen:

```powershell
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -BiosProfile na
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -BiosProfile jp
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -BiosProfile eu
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -BiosProfile auto
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -IpTemplate yaul
.\run-example.ps1 mvp_2d_scene -Emulator mednafen -IpTemplate sbl
.\run-example.ps1 red_square -Emulator mednafen -BiosProfile auto
```

## Nota sobre IP.BIN

O projeto gera `ip.bin` em `make` a partir de um template de boot selecionavel por `IP_TEMPLATE_KIND`:
- `yaul` (default): `assets/boot/ip_yaul_template.bin`
- `sbl`: `assets/boot/ip_sbl_template.bin`

Em ambos os casos, o build mantem o template textual/boot original e sobrescreve apenas `1ST_READ` (`0x0F0/0x0F4`).
Os blocos sensiveis de boot (`0x0100..0x05FF`) e de area code object (`0x0E00..0x7FFF`) permanecem identicos ao template selecionado.
Para compatibilidade de boot, a ISO grava o payload como `0.BIN` (primario) e `1ST_READ.BIN` (alias).
No Mednafen, prefira abrir `build/mvp.cue` em vez de `build/mvp.iso`.
Antes de distribuição pública, faça revisão legal/licenciamento de boot assets conforme sua política de release.
