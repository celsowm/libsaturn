---
name: vdp1-sega-saturn
description: >
  Referência completa do VDP1 (Video Display Processor 1) do Sega Saturn para
  desenvolvimento em assembly SH-2. Use este skill sempre que o usuário precisar
  programar gráficos no Saturn, escrever command tables no VRAM, configurar
  system registers, desenhar sprites/polígonos/linhas, implementar color
  calculation (Gouraud shading, half-transparency, shadow), controlar o frame
  buffer, gerenciar clipping, ou qualquer tarefa envolvendo o VDP1 em baixo nível.
  Também use quando o usuário mencionar: draw commands, CMDCTRL, CMDPMOD,
  frame buffer flip, erase/write, plot trigger, textured/non-textured parts.
---

# VDP1 — Sega Saturn Drawing Processor

O VDP1 é o IC de desenho de sprites do Sega Saturn. Ele lê command tables do VRAM
e escreve pixels no frame buffer (DRAM), que é então exibido via VDP2.

> **Para detalhes de um tópico específico, carregue o arquivo de referência correspondente:**
> - `references/system-registers.md` — Registradores, endereços, bits
> - `references/command-table.md`    — Estrutura e campos das command tables
> - `references/commands.md`         — Todos os comandos de desenho com exemplos ASM
> - `references/color-tables.md`     — Color bank, LUT, Gouraud shading, modos RGB

---

## 1. Mapa de Endereços (absoluto, base 5C00000H)

```
Endereço Relativo   Absoluto         Conteúdo
000000–07FFFF       5C00000–5C7FFFF  VRAM (4 Mbit) — command/char/LUT/Gouraud tables
080000–0BFFFF       5C80000–5CBFFFF  Frame Buffer 0 (2 Mbit)
0C0000–0FFFFF       5CC0000–5CFFFFF  Frame Buffer 1 (2 Mbit)
100000–1FFFFF       5D00000–5DFFFFF  System Registers (word access only)
```

**Regra:** endereço absoluto = relativo + 5C00000H

---

## 2. System Registers — Resumo Rápido

| Abrev | Endereço Abs | R/W    | Função                              |
|-------|--------------|--------|-------------------------------------|
| TVMR  | 5D00000H     | W-only | TV mode (TVM) + V-blank erase (VBE) |
| FBCR  | 5D00002H     | W-only | Frame buffer change mode            |
| PTMR  | 5D00004H     | W-only | Plot trigger (start drawing)        |
| EWDR  | 5D00006H     | W-only | Erase/write fill data               |
| EWLR  | 5D00008H     | W-only | Erase area upper-left (X1,Y1)       |
| EWRR  | 5D0000AH     | W-only | Erase area lower-right (X3,Y3)      |
| ENDR  | 5D0000CH     | W-only | Force-terminate drawing (escreva 0) |
| EDSR  | 5D00010H     | R-only | Draw end status (CEF/BEF bits)      |
| LOPR  | 5D00012H     | R-only | Last processed command address /8H  |
| COPR  | 5D00014H     | R-only | Current command address /8H         |
| MODR  | 5D00016H     | R-only | Mirror dos registradores write-only |

> Acesso sempre em **word (16-bit)**. Nunca use DMA burst nos system registers.
> Ver `references/system-registers.md` para cada bit detalhado.

---

## 3. Inicialização Mínima (template ASM)

```asm
; Constantes de base
VDP1_BASE   equ 5D00000H      ; Base absoluta dos system registers
VRAM_BASE   equ 5C00000H      ; Base do VRAM

; ─── Passo 1: TV Mode (Normal NTSC 320x224, 16bpp, sem rotação) ───────────────
    mov.w   #0000H, r0
    mov.l   #(VDP1_BASE + 0), r1   ; TVMR
    mov.w   r0, @r1

; ─── Passo 2: Frame Buffer Change Mode (1-cycle, automático 60fps) ────────────
    mov.w   #0000H, r0
    mov.l   #(VDP1_BASE + 2), r1   ; FBCR
    mov.w   r0, @r1

; ─── Passo 3: Erase/Write Data (preencher com preto = 0) ──────────────────────
    mov.w   #0000H, r0
    mov.l   #(VDP1_BASE + 6), r1   ; EWDR
    mov.w   r0, @r1

; ─── Passo 4: Erase area (320x224 em 16bpp: X1=0→reg=0 proibido, usar 1=8px) ─
; EWLR: bit14-9 = X1/8, bit8-0 = Y1
;   X1=0, Y1=0 → word = 0x0000  (X1=0 é proibido! usar 0 mesmo, VDP1 força 8)
    mov.w   #0x0000, r0
    mov.l   #(VDP1_BASE + 8), r1   ; EWLR
    mov.w   r0, @r1

; EWRR: bit15-9 = X3/8, bit8-0 = Y3
;   X3=319 → reg=40 (40*8-1=319), Y3=223
    mov.w   #((40 << 9) | 223), r0
    mov.l   #(VDP1_BASE + 0AH), r1 ; EWRR
    mov.w   r0, @r1

; ─── Passo 5: Escrever command tables no VRAM ─────────────────────────────────
;   (ver seção 4 e arquivo references/commands.md)

; ─── Passo 6: Plot Trigger (iniciar desenho automático a cada frame) ──────────
    mov.w   #0002H, r0             ; PTM = 10B = auto-start
    mov.l   #(VDP1_BASE + 4), r1   ; PTMR
    mov.w   r0, @r1
```

---

## 4. Command Table — Estrutura (32 bytes, boundary 20H)

Cada command table ocupa **1EH bytes úteis + 2 bytes dummy = 20H bytes**.
A primeira command table **deve** estar em VRAM offset 000000H.

```
Offset  Campo     Bits         Descrição
+00H    CMDCTRL   [15]END      1=Draw End Command
                  [14:12]JP    Jump mode (000=next, 001=assign, 010=call, 011=return,
                               100=skip-next, 101=skip-assign, 110=skip-call, 111=skip-return)
                  [11:8]ZP     Zoom point (scaled sprite only; 0=two-coords mode)
                  [5:4]Dir     Character read direction (bit5=V-invert, bit4=H-invert)
                  [3:0]Comm    Command select (ver tabela abaixo)

+02H    CMDLINK   [15:2]       Link address / 8H (lower 2 bits = 00)

+04H    CMDPMOD   [15]MON      MSB ON (VDP2 shadow/window)
                  [12]HSS      High Speed Shrink (scaled/distorted only)
                  [11]Pclp     Pre-clipping disable
                  [10]Clip     User clipping enable
                  [9]Cmod      Clipping mode (0=inside, 1=outside)
                  [8]Mesh      Mesh/tiling enable
                  [7]ECD       End Code Disable
                  [6]SPD       Transparent Pixel Disable
                  [5:3]ColMd   Color mode (000=16col bank, 001=16col LUT,
                               010=64col bank, 011=128col bank, 100=256col bank,
                               101=32768col RGB)
                  [2:0]CC      Color Calculation (000=replace, 001=shadow,
                               010=half-lum, 011=half-transp, 100=Gouraud,
                               110=Gouraud+half-lum, 111=Gouraud+half-transp)

+06H    CMDCOLR              Color bank / LUT address/8H / non-textured color

+08H    CMDSRCA   [15:2]       Character address / 8H (lower 2 bits = 00)

+0AH    CMDSIZE   [12:8]       Char size X / 8  (1–63 → 8–504 pixels)
                  [7:0]        Char size Y       (1–255 pixels)

+0CH    CMDXA     [10:0]+sign  Vertex A X (sign-extended 11-bit, -1024..1023)
+0EH    CMDYA                  Vertex A Y
+10H    CMDXB                  Vertex B X  (ou display width XB para scaled)
+12H    CMDYB                  Vertex B Y  (ou display width YB para scaled)
+14H    CMDXC                  Vertex C X
+16H    CMDYC                  Vertex C Y
+18H    CMDXD                  Vertex D X
+1AH    CMDYD                  Vertex D Y

+1CH    CMDGRDA   [15:0]       Gouraud shading table address / 8H

+1EH    (dummy — 2 bytes, ignorados pelo VDP1)
```

**Tabela de Comandos (CMDCTRL[3:0] = Comm, com END=0):**

| Comm | Comando                            |
|------|------------------------------------|
| 0000 | Normal sprite draw                 |
| 0001 | Scaled sprite draw                 |
| 0010 | Distorted sprite draw              |
| 0100 | Polygon draw (filled quad)         |
| 0101 | Polyline draw (outline quad)       |
| 0110 | Line draw                          |
| 1000 | User clipping coordinate set       |
| 1001 | System clipping coordinate set     |
| 1010 | Local coordinate set               |
| END=1| Draw end command (CMDCTRL=8000H)   |

---

## 5. Fluxo de Desenho por Frame

```
1. CPU escreve character pattern tables → VRAM
2. CPU escreve color lookup tables → VRAM
3. CPU escreve Gouraud shading tables → VRAM
4. CPU escreve command tables → VRAM (a partir de 000000H)
5. VDP1 inicia automaticamente ao trocar frame (PTM=10B)
   ou manualmente (escreve PTM=01B no PTMR)
6. VDP1 lê command tables em sequência, desenha no frame buffer traseiro
7. Ao ler Draw End Command → seta CEF=1 no EDSR e gera interrupt
8. Na próxima troca de frame buffer → buffer desenhado vira display
```

**Verificar fim do desenho (polling):**
```asm
    mov.l   #5D00010H, r1       ; EDSR
.wait:
    mov.w   @r1, r0
    tst     #2, r0              ; CEF = bit 1
    bt      .wait               ; loop enquanto CEF=0
```

---

## 6. Exemplo: Polígono Simples (RGB, replace)

```asm
; Desenha um quadrilátero preenchido em vermelho puro (RGB 1F,00,00 = FC00H + MSB = BC00H)
; Endereço VRAM: 5C00000H (offset 000000H)

    mov.l   #5C00000H, r4   ; base VRAM

    ; CMDCTRL: END=0, JP=000(next), ZP=0, Dir=00, Comm=0100(polygon)
    mov.w   #0x0004, r0 ;  0000 0000 0000 0100
    mov.w   r0, @r4

    ; CMDLINK: não usado (jump next ignora CMDLINK)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; CMDPMOD: MON=0, HSS=0, Pclp=0, Clip=0, Cmod=0, Mesh=0, ECD=1, SPD=1,
    ;          ColorMode=000, CC=000 (replace)
    ; bits: 0000 0000 1100 0000 = 00C0H
    add     #2, r4
    mov.w   #0x00C0, r0
    mov.w   r0, @r4

    ; CMDCOLR: non-textured color = vermelho RGB (1,31,0,0) = 8400H | 8000H = 8400H
    ; RGB format: MSB=1, B[4:0], G[4:0], R[4:0]
    ; Vermelho puro: R=1FH, G=00H, B=00H → 1_00000_00000_11111 = 801FH
    add     #2, r4
    mov.w   #0x801F, r0
    mov.w   r0, @r4

    ; CMDSRCA: não usado para polygon (ignorado)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; CMDSIZE: não usado para polygon (ignorado)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; CMDXA: Vertex A (10, 10)
    add     #2, r4
    mov.w   #10, r0 ; X=10
    mov.w   r0, @r4
    add     #2, r4
    mov.w   #10, r0 ; Y=10
    mov.w   r0, @r4

    ; CMDXB: Vertex B (100, 10)
    add     #2, r4
    mov.w   #100, r0
    mov.w   r0, @r4
    add     #2, r4
    mov.w   #10, r0
    mov.w   r0, @r4

    ; CMDXC: Vertex C (100, 80)
    add     #2, r4
    mov.w   #100, r0
    mov.w   r0, @r4
    add     #2, r4
    mov.w   #80, r0
    mov.w   r0, @r4

    ; CMDXD: Vertex D (10, 80)
    add     #2, r4
    mov.w   #10, r0
    mov.w   r0, @r4
    add     #2, r4
    mov.w   #80, r0
    mov.w   r0, @r4

    ; CMDGRDA: não usado (Gouraud desabilitado)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; Dummy +1EH (2 bytes)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; ─── Draw End Command em +20H ─────────────────────────────────────────────
    add     #2, r4
    mov.w   #0x8000, r0     ; END=1
    mov.w   r0, @r4
```

---

## 7. Coordenadas e Clipping

- **Frame buffer plane:** −1024 ≤ X ≤ 1023, −1024 ≤ Y ≤ 1023
- **System clipping** deve ser configurado antes do primeiro draw (undefined após reset):
  - Upper-left: fixo em (0,0)
  - Lower-right: especificado via CMDXC/CMDYC no System Clipping Set Command
- **Local coordinates** são adicionadas às coordenadas dos draw commands
- Partes fora do frame buffer plane simplesmente não são desenhadas

```asm
; System Clipping: área 0,0 → 319,223
; Command table para system clipping (Comm=1001B)
    mov.w   #0x0009, r0     ; CMDCTRL: END=0, JP=next, Comm=1001
    mov.w   r0, @r4
    ; ... pular campos ignorados (write zeros) ...
    ; CMDXC offset +14H: lower-right X = 319
    ; CMDYC offset +16H: lower-right Y = 223
```

> Para detalhes completos de cada registrador e bit, ver `references/system-registers.md`
> Para todos os comandos com exemplos completos, ver `references/commands.md`

---

## 8. Dicas Críticas para Assembly

1. **VRAM address / 8H:** CMDSRCA, CMDGRDA, CMDLINK e LOPR/COPR guardam `endereço/8H`. Sempre divida por 8 antes de escrever.
2. **Coordenadas sign-extended:** bits [15:11] devem replicar bit 10 (sinal). Use `exts.w` no SH-2.
3. **Boundary 20H:** toda command table deve estar alinhada em 32 bytes. Use `.align 5` ou equivalente.
4. **Character pattern boundary 20H:** char patterns também em boundary 20H. Endereço 0 é reservado para a primeira command table.
5. **Acesso word-only nos system registers:** nunca byte ou longword.
6. **Não acesse frame buffer sendo exibido:** apenas o buffer traseiro (draw) é acessível.
7. **Color bank:** lower 4 bits devem ser 0 (ex: CMDCOLR para 16-color bank mode).
8. **RGB format:** MSB=1, bits [14:10]=B, [9:5]=G, [4:0]=R. Preto RGB = 8000H (não 0000H, que é transparente!).
9. **Após reset:** system/user clipping coordinates são undefined — sempre configure antes de desenhar.
10. **ECD=1, SPD=1** obrigatório para polygons, polylines e lines (não usam character data).
