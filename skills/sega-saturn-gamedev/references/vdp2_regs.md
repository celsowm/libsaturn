# VDP2 Registradores + VDP1 Command Table Completo

## VDP2 — Mapa de Registradores (Base 0x05F00000)

```
Offset  Nome     Descrição
──────────────────────────────────────────────────────────────────────────
0x000   TVMD     TV Mode
0x002   EXTEN    External Signal Enable
0x004   TVSTAT   TV Status (read-only)
0x006   VRSIZE   VRAM Size
0x008   HCNT     H Counter (read-only)
0x00A   VCNT     V Counter (read-only)
0x00C   (reserv)
0x00E   RAMCTL   VRAM Control
0x010   BGON     Screen Enable
0x012   MZCTL    Mosaic Control
0x014   SFSEL    Special Function Select
0x016   SFCODE   Special Function Code
0x018   CHCTLA   Character Control A (NBG0, NBG1)
0x01A   CHCTLB   Character Control B (NBG2, NBG3, RBG)
0x01C   BMPNA    Bitmap Palette NBG0, NBG1
0x01E   BMPNB    Bitmap Palette NBG2, NBG3
0x020   PNCN0    Pattern Name Control NBG0
0x022   PNCN1    Pattern Name Control NBG1
0x024   PNCN2    Pattern Name Control NBG2
0x026   PNCN3    Pattern Name Control NBG3
0x028   PNCR     Pattern Name Control RBG0
0x02A   PLSZ     Plane Size
0x02C   MPOFN    Map Offset (NBG0, 1, 2, 3)
0x02E   MPOFR    Map Offset (RBG)
0x030   MPABN0   Map NBG0 Plane A/B
0x032   MPCDN0   Map NBG0 Plane C/D
0x034   MPABN1   Map NBG1 Plane A/B
0x036   MPCDN1   Map NBG1 Plane C/D
0x038   MPABN2   Map NBG2 Plane A/B (1 plane only)
0x03A   MPABN3   Map NBG3 Plane A/B (1 plane only)
0x03C   MPABRA   Map RBG0 (rotation parameter A) Plane A/B
0x03E   MPCDRA   Map RBG0 Plane C/D
0x040   MPEFRA   Map RBG0 Plane E/F
0x042   MPGHRA   Map RBG0 Plane G/H
0x044   MPIJRA   Map RBG0 Plane I/J
0x046   MPKLRA   Map RBG0 Plane K/L
0x048   MPMNRA   Map RBG0 Plane M/N
0x04A   MPOPRA   Map RBG0 Plane O/P
0x04C   MPABRB   Map RBG0 (rotation parameter B)
0x04E–0x05A   (mais mapas RBG0 B)
0x060   SCXIN0   Screen Scroll X (Integer) NBG0
0x062   SCXDN0   Screen Scroll X (Decimal) NBG0
0x064   SCYIN0   Screen Scroll Y NBG0
0x066   SCYDN0   Screen Scroll Y Decimal NBG0
0x068   ZMXIN0   Zoom X NBG0
0x06A   ZMXDN0   Zoom X Decimal NBG0
0x06C   ZMYIN0   Zoom Y NBG0
0x06E   ZMYDN0   Zoom Y Decimal NBG0
0x070–0x07E  (scroll NBG1)
0x080–0x088  (scroll NBG2, NBG3)
0x090   SPCTL    Sprite Control
0x092   SDCTL    Shadow Control
0x094   CRAOFA   Color RAM Offset A
0x096   CRAOFB   Color RAM Offset B
0x098   LNCLEN   Line Color Screen Enable
0x09A   SFPRMD   Special Priority Mode
0x09C   CCCTL    Color Calculation Control
0x09E   SFCCMD   Special Color Calculation Mode
0x0A0   PRISA    Priority sprite types 0,1
0x0A2   PRISB    Priority sprite types 2,3
0x0A4   PRISC    Priority sprite types 4,5
0x0A6   PRISD    Priority sprite types 6,7
0x0A8   PRINA    Priority NBG0, NBG1
0x0AA   PRINB    Priority NBG2, NBG3
0x0AC   PRIR     Priority RBG0
0x0AE   (reservado)
0x0B0   CCRSA    Color Calc Ratio (sprites A)
0x0B2   CCRSB    Color Calc Ratio (sprites B)
0x0B4   CCRNB    Color Calc Ratio NBG0+1, NBG2+3
0x0B6   CCRR     Color Calc Ratio RBG0
0x0B8   CCRLB    Color Calc Ratio Line + Back
0x0BA   CLOFEN   Color Offset Enable
0x0BC   CLOFSL   Color Offset Select
0x0BE   COAR     Color Offset A Red
0x0C0   COAG     Color Offset A Green
0x0C2   COAB     Color Offset A Blue
0x0C4   COBR     Color Offset B Red
0x0C6   COBG     Color Offset B Green
0x0C8   COBB     Color Offset B Blue
```

## VDP2_TVMD (0x05F00000)

```
Bit 15:   DISP — Display on (1) / off (0). Manter 0 durante init.
Bits 9:8: LSMD — Interlace mode: 00=non-interlace, 11=double-density
Bits 7:6: HRESO — H resolution: 00=320, 01=352, 10=640, 11=704
Bits 5:4: VRESO — V resolution: 00=224, 01=240, 10=256*, 11=480*
                  (* exige interlace)
Bit  1:   BSTH — VBlank line (0=240/480, 1=224/448 lines)
Bit  0:   TV  — System clock: 0=NTSC, 1=PAL

Modos comuns:
  0x8110 = 320×224 NTSC (display on, non-interlace, 320px, 224l)
  0x8150 = 352×240 NTSC
  0x8190 = 640×224 NTSC hi-res
  0x81F0 = 320×240 PAL
```

## VDP2_RAMCTL (0x05F0000E)

```
Bits 15:14: VRBMD — VRAM B1 mode (0=normal, 1=rotation scroll)
Bits 13:12: VRAMD — VRAM B0 mode
Bits 11:10: VRCMD — VRAM A1 mode
Bits  9:8:  VRAX  — VRAM A0 mode
Bits  1:0:  CRMD  — Color RAM mode:
                    00 = 1024 × 16-bit (padrão)
                    01 = 2048 × 8-bit
                    10 = 1024 × 32-bit
Recomendado padrão: 0x1F00
```

## VDP2_CHCTLA (0x05F00018) — NBG0, NBG1

```
Bits 15:12: Controle NBG1
Bits  3:0:  Controle NBG0

Para NBG0:
  Bit 0: CHSZ  — Tamanho do character: 0=8×8, 1=16×16
  Bit 2: BMEN  — Bitmap mode: 0=cell/tile, 1=bitmap direto
  Bit 4: Reservado
  Bits 6:4: COLNO — Número de cores:
    000 = 16  cores (4bpp)
    001 = 64  cores (6bpp)
    010 = 128 cores (7bpp)
    011 = 256 cores (8bpp) ← mais comum para backgrounds
    100 = 2048 cores
    101 = 32768 cores (15bpp)
    110 = 16.7M cores (24bpp, somente bitmap)

Exemplos:
  0x0002 = NBG0: 256 cores, tiles 8×8, cell mode
  0x0012 = NBG0: 256 cores, bitmap mode, 8×8
```

## VDP2_SPCTL (0x05F00090) — Sprite Control

```
Bits 7:4: SPTYPE — Tipo de sprite:
  0x0 = Tipo 0: Prioridade nos bits 14:13 do color number
  0x1 = Tipo 1: Prioridade nos bits 14:13
  0x2 = Tipo 2: Prioridade nos bits 15:14
  ... (ver manual VDP2 seção sprite mapping para todos os tipos)
Bits 3:0: Selecão do shadow e color calculation

Tipo de sprite determina como VDP2 interpreta o "color number" vindo do VDP1
para definir prioridade, transparência e paleta.
Mais simples: tipo 0 (0x0000)
```

---

## VDP1 — Command Table Referência Completa

### Estrutura dos 32 Bytes

| Offset | Campo | Tipo     | Descrição                         |
|--------|-------|----------|-----------------------------------|
| 0x00   | ctrl  | uint16_t | Tipo do comando + opções          |
| 0x02   | link  | uint16_t | Endereço do próximo cmd (>> 3)    |
| 0x04   | pmod  | uint16_t | Draw mode (cor, transparência)    |
| 0x06   | colr  | uint16_t | Base da paleta ou cor             |
| 0x08   | srca  | uint16_t | Endereço da textura (>> 3)        |
| 0x0A   | size  | uint16_t | (largura/8 << 8) \| altura        |
| 0x0C   | xa    | int16_t  | Vértice A x                       |
| 0x0E   | ya    | int16_t  | Vértice A y                       |
| 0x10   | xb    | int16_t  | Vértice B x                       |
| 0x12   | yb    | int16_t  | Vértice B y                       |
| 0x14   | xc    | int16_t  | Vértice C x                       |
| 0x16   | yc    | int16_t  | Vértice C y                       |
| 0x18   | xd    | int16_t  | Vértice D x                       |
| 0x1A   | yd    | int16_t  | Vértice D y                       |
| 0x1C   | grda  | uint16_t | Endereço tabela Gouraud (>> 3)    |
| 0x1E   | _pad  | uint16_t | Não usado                         |

### CTRL — Bits de Controle

```
Bits 15:14: Tipo de comando
  00 = Normal Sprite (quad alinhado ao eixo, de textura)
  01 = Scaled Sprite (quad alinhado com escala definida)
  10 = Distorted Sprite ← USAR PARA 3D (quad arbitrário)
  11 = Polygon (quad sem textura, cor sólida)
  Especiais (bit 15=1 com padrões específicos):
  100 = Polyline
  101 = Line (segmento único)
  111 = User Clipping
 1000 = System Clipping
 1001 = Local Coordinate (deslocar origem)

Bit 13: 1 = Fim de lista (MUST set no último comando!)
Bit  7: Jump tipo: 0=next, 1=link field (call sublist)
Bit  4: Flip Y
Bit  3: Flip X
Bit  2: Gouraud shading (requer tabela em grda)
Bit  1: High Speed Shrink (evitar artifacts em textura miniaturizada)
Bit  0: Pre-clipping (descartar cmd fora da clip window)
```

### PMOD — Draw Mode

```
Bit 15: MSB do color mode (combinar com bits 6:4)
Bits 6:4: Color mode:
  000 = 16-color palette (4bpp)
  001 = 64-color palette (6bpp)
  010 = 128-color palette (7bpp)
  011 = 256-color palette (8bpp) ← mais usado
  100 = 32768-color RGB (15bpp, sem palette)

Bit 3: End code enable (1 = cor 0x0F/0xFF = transparente em palette mode)
Bit 2: Transparent pixels (cor 0 = skip pixel)
Bit 1: Shadow
Bit 0: Half luminance (dim para sombra)

Exemplos:
  0x00C0 = 256 cores, end codes on, transparência on
  0x0080 = 256 cores, end codes on, sem transparência
  0x0040 = 256 cores, end codes off, sem transparência
```

### COLR — Base da Paleta

```
Para mode 256 cores: COLR = índice da paleta × 256
  Paleta 0: COLR = 0x0000
  Paleta 1: COLR = 0x0100
  ...
  Paleta 7: COLR = 0x0700

Para mode 16 cores: COLR = índice × 16
Para RGB555 (mode 100): COLR = 0 (cor definida diretamente na textura)
```

### SRCA / SIZE — Textura

```
SRCA: endereço da textura na VDP1 VRAM, dividido por 8
  Textura em 0x05C81000 → SRCA = (0x1000) >> 3 = 0x0200

SIZE: (largura_em_pixels / 8) << 8 | altura_em_pixels
  8×8   pixels: SIZE = (1 << 8) | 8  = 0x0108
  16×16 pixels: SIZE = (2 << 8) | 16 = 0x0210
  32×32 pixels: SIZE = (4 << 8) | 32 = 0x0420
  64×64 pixels: SIZE = (8 << 8) | 64 = 0x0840

Restrições obrigatórias:
  - Largura: múltiplo de 8 (1 a 512 pixels)
  - Altura: 1 a 255 pixels
  - Endereço: múltiplo de 8 bytes
```

### Vértices para Distorted Sprite (3D quad)

```
(XA, YA) = Topo-esquerda
(XB, YB) = Topo-direita
(XC, YC) = Baixo-direita
(XD, YD) = Baixo-esquerda

Coordenadas em pixels de tela, origem top-left, Y cresce para baixo.
Faixa válida: -1024 a +1023 (signed 11-bit expandido para 16-bit)
Ordem: CLOCKWISE para rendering correto no VDP1.
```

### Tabela Gouraud (quando Bit 2 de ctrl = 1)

```
grda: endereço da tabela em VDP1 VRAM, dividido por 8
Tabela: 8 bytes = 4 × uint16_t = uma cor RGB555 por vértice (A, B, C, D)
Alinhamento: 8 bytes

Formato RGB555: 0RRRRRGGGGGBBBBB
  R: bits 14:10
  G: bits 9:5
  B: bits 4:0
```

### Comandos Especiais

**System Clipping (ctrl = 0x9000):**
```
xa, ya = (0, 0) sempre
xc, yc = (max_x, max_y) = canto inferior-direito da área de clip
Enviado uma vez na inicialização.
```

**User Clipping (ctrl = 0x8000 sem Bit 15 = end):**
```
Não confundir: ctrl = 0x8000 significa fim de lista.
User Clip: ctrl = 0x7 << 13... ver manual VDP1 seção clipping.
```

**Local Coordinate (ctrl = 0x9... ):**
```
Define offset para todos os comandos subsequentes.
xb, yb = novo ponto de origem local
```
