# VDP1 — Command Table (Estrutura Detalhada)

Cada command table tem **1EH (30) bytes úteis** + 2 bytes dummy = **20H (32) bytes totais**.
Devem estar alinhadas em boundary **20H** no VRAM.
A primeira command table **obrigatoriamente** começa em VRAM offset `000000H`.

---

## Layout Completo

```
Offset  Nome      Bits [15:0]                           Descrição
+00H    CMDCTRL   [15]END [14:12]JP [11:8]ZP [5:4]Dir [3:0]Comm
+02H    CMDLINK   [15:2] link_addr/8H, [1:0]=00
+04H    CMDPMOD   [15]MON [12]HSS [11]Pclp [10]Clip [9]Cmod [8]Mesh [7]ECD [6]SPD [5:3]ColMd [2:0]CC
+06H    CMDCOLR   color bank / LUT addr/8H / non-textured color
+08H    CMDSRCA   [15:2] char_addr/8H, [1:0]=00
+0AH    CMDSIZE   [12:8] sizeX/8 (1..63), [7:0] sizeY (1..255)
+0CH    CMDXA     [10:0]+sign  vertex A X
+0EH    CMDYA     [10:0]+sign  vertex A Y
+10H    CMDXB     [10:0]+sign  vertex B X (ou display width XB)
+12H    CMDYB     [10:0]+sign  vertex B Y (ou display width YB)
+14H    CMDXC     [10:0]+sign  vertex C X
+16H    CMDYC     [10:0]+sign  vertex C Y
+18H    CMDXD     [10:0]+sign  vertex D X
+1AH    CMDYD     [10:0]+sign  vertex D Y
+1CH    CMDGRDA   [15:0] gouraud_table/8H
+1EH    (dummy — 2 bytes ignorados)
```

---

## CMDCTRL (+00H)

### END — Bit 15
- `0` = comando normal (usar Comm para tipo)
- `1` = **Draw End Command** (Comm é ignorado; write `8000H` neste word)

### JP — Jump Mode (bits 14:12)

| JP  | Modo         | Comportamento                                                      |
|-----|--------------|---------------------------------------------------------------------|
| 000 | Jump Next    | Processa e vai para address+20H (CMDLINK ignorado)                 |
| 001 | Jump Assign  | Processa e pula para CMDLINK                                       |
| 010 | Jump Call    | Processa e chama CMDLINK como subroutine (1 nível de nesting)      |
| 011 | Jump Return  | Processa e retorna para main routine (CMDLINK ignorado)            |
| 100 | Skip Next    | Não processa, vai para address+20H (CMDLINK ignorado)              |
| 101 | Skip Assign  | Não processa, pula para CMDLINK                                    |
| 110 | Skip Call    | Não processa, chama CMDLINK como subroutine                        |
| 111 | Skip Return  | Não processa, retorna para main routine                            |

> Subroutines: apenas 1 nível de nesting. Não use jump call dentro de subroutine.

### ZP — Zoom Point (bits 11:8) — apenas Scaled Sprite

| ZP  | Ponto de referência        |
|-----|---------------------------|
| 0H  | Especifica dois vértices (A=upper-left, C=lower-right) |
| 5H  | Upper-left                |
| 6H  | Upper-center              |
| 7H  | Upper-right               |
| 9H  | Center-left               |
| AH  | Center-center             |
| BH  | Center-right              |
| DH  | Lower-left                |
| EH  | Lower-center              |
| FH  | Lower-right               |

Para todos os outros comandos: ZP = 0H.

### Dir — Character Read Direction (bits 5:4)

| Dir | Bit 5 (V) | Bit 4 (H) | Efeito                        |
|-----|-----------|-----------|-------------------------------|
| 00  | 0         | 0         | Normal                        |
| 01  | 0         | 1         | Inversão horizontal           |
| 10  | 1         | 0         | Inversão vertical             |
| 11  | 1         | 1         | Inversão vertical+horizontal  |

Para non-textured parts: Dir = 00B.

### Comm — Command Select (bits 3:0)

| Comm | Comando                                |
|------|----------------------------------------|
| 0000 | Normal Sprite Draw                     |
| 0001 | Scaled Sprite Draw                     |
| 0010 | Distorted Sprite Draw                  |
| 0100 | Polygon Draw (filled quadrangle)       |
| 0101 | Polyline Draw (outline quadrangle)     |
| 0110 | Line Draw                              |
| 1000 | User Clipping Coordinate Set           |
| 1001 | System Clipping Coordinate Set         |
| 1010 | Local Coordinate Set                   |

---

## CMDLINK (+02H)

```
bits [15:2] = endereço da próxima command table / 8H
bits [1:0]  = 00 (fixo, boundary 20H ÷ 8 → lower 2 bits sempre 0)
```

Exemplo: command table em VRAM offset 0x0040:
```asm
    mov.w   #(0x0040 >> 3), r0   ; = 0x0008
    mov.w   r0, @(CMDLINK_ADDR)
```

---

## CMDPMOD (+04H)

### MON — MSB ON (bit 15)
- `1` = Seta MSB de todos os pixels desenhados no frame buffer (para shadow/window no VDP2)
- Use com Replace (CC=000). Não combine com color calculation.
- Em mesh mode: MSB é setado nas posições "mesh on".

### HSS — High Speed Shrink (bit 12)
- Válido apenas para scaled/distorted sprite
- `0` = precisão (samplea todas as posições)
- `1` = velocidade (samplea apenas coord. even ou odd, conforme EOS no FBCR)
- Com HSS=1: end code é ignorado (mesmo quando ECD=0)
- Com HSS=1 em redução: use ECD=1 obrigatoriamente

### Pclp — Pre-Clipping Disable (bit 11)
- `0` = pre-clipping ativo (detecta linhas completamente fora da área → não desenha)
- `1` = sem pre-clipping (útil para sprites pequenos onde overhead > ganho)

### Clip — User Clipping Enable (bit 10)
### Cmod — Clipping Mode (bit 9)

| Clip | Cmod | Comportamento                              |
|------|------|--------------------------------------------|
|  0   |  0   | User clipping desabilitado                 |
|  0   |  1   | PROIBIDO                                   |
|  1   |  0   | Inside drawing mode (desenha dentro)       |
|  1   |  1   | Outside drawing mode (desenha fora)        |

> System clipping é sempre ativo. User clipping é adicional.

### Mesh (bit 8)
- `1` = mesh processing: apenas pixels onde (X+Y) é par são desenhados
- Cuidado: linhas a 45° começando em coord. ímpar podem não desenhar nada

### ECD — End Code Disable (bit 7)
- `0` = end code ativo: ao ler 2 end codes na horizontal, termina aquela linha
- `1` = desabilitado: end code tratado como cor normal
- **Obrigatório ECD=1** para polygons, polylines, lines
- Com HSS=1 e redução: use ECD=1

End codes por color mode:
| Mode | End Code |
|------|----------|
| 0,1  | FH (4 bits)  |
| 2,3,4| FFH (8 bits) |
| 5    | 7FFFH (16 bits) |

### SPD — Transparent Pixel Disable (bit 6)
- `0` = transparência ativa: pixels com transparent color code não são desenhados
- `1` = desabilitado
- **Obrigatório SPD=1** para polygons, polylines, lines
- Quando SPD=0 e ECD=0: máximo 14 cores usáveis (mode 0)

Transparent color codes:
| Mode | Transparent Code |
|------|------------------|
| 0,1  | 0H              |
| 2,3,4| 00H             |
| 5    | 0000H           |

### Color Mode Bits (bits 5:3)

| Bits | Mode | Cores  | Tipo       | bpp |
|------|------|--------|------------|-----|
| 000  |  0   | 16     | Color bank | 4   |
| 001  |  1   | 16     | LUT        | 4   |
| 010  |  2   | 64     | Color bank | 8   |
| 011  |  3   | 128    | Color bank | 8   |
| 100  |  4   | 256    | Color bank | 8   |
| 101  |  5   | 32768  | RGB        | 16  |

Para non-textured parts: ColMd = 000B.

### Color Calculation Bits (bits 2:0)

| Bits | Modo                     | Original  | Background | Restrição         |
|------|--------------------------|-----------|------------|-------------------|
| 000  | Replace                  | 1×        | —          | Nenhuma           |
| 001  | Shadow                   | —         | ½ (se MSB=1) | RGB background  |
| 010  | Half-Luminance           | ½         | —          | RGB original      |
| 011  | Half-Transparent         | ½         | ½ (se MSB=1) | RGB original    |
| 100  | Gouraud                  | Gouraud   | —          | RGB original      |
| 101  | PROIBIDO                 |           |            |                   |
| 110  | Gouraud + Half-Lum       | Gouraud×½ | —          | RGB original      |
| 111  | Gouraud + Half-Transp    | Gouraud×½ | ½ (se MSB=1) | RGB ambos       |

> Shadow e Half-Transparent são 6× mais lentos. Não é possível em 8bpp (usar replace).

---

## CMDCOLR (+06H)

| Tipo do part      | Color Mode | Conteúdo de CMDCOLR                    |
|-------------------|------------|----------------------------------------|
| Textured (sprite) | Bank (0,2,3,4) | Color bank (lower 4 bits = 0)     |
| Textured (sprite) | LUT (1)    | Endereço da LUT / 8H (lower 2 bits=00) |
| Textured (sprite) | RGB (5)    | Ignorado                               |
| Non-textured      | qualquer   | Cor direta (16-bit, escrita no FB)     |

**Color bank por modo:**
- Mode 0 (4bpp): bits[15:4]=color bank, bits[3:0]=0000
- Mode 2 (6bpp): bits[15:6]=color bank, bits[5:0]=000000  → na prática lower 4 bits = 0
- Mode 3 (7bpp): bits[15:7]=color bank, bits[6:0]=0
- Mode 4 (8bpp): bits[15:8]=color bank, bits[7:0]=0

**Non-textured color (RGB):** MSB=1, B[14:10], G[9:5], R[4:0]
- Vermelho puro: 0x801F | Verde puro: 0x83E0 | Azul puro: 0xFC00 | Branco: 0xFFFF | Preto: 0x8000

---

## CMDSRCA (+08H)

```
bits [15:2] = endereço do character pattern no VRAM / 8H
bits [1:0]  = 00 (boundary 20H ÷ 8 → lower 2 bits 0, mas boundary requer lower 3 bits = 0 para 20H/8)
```

Boundary: character patterns devem estar alinhados em **20H bytes**.
Endereço 0x000000 é reservado para a command table → character patterns a partir de 0x000020 (mínimo).

---

## CMDSIZE (+0AH)

```
bits [12:8] = sizeX / 8   → 1..63 → 8..504 pixels horizontais
bits  [7:0] = sizeY        → 1..255 pixels verticais
bits [15:13] = 00 (fixo)
```

Exemplo: sprite 32×32:
```asm
    mov.w   #((4 << 8) | 32), r0   ; sizeX/8=4→32px, sizeY=32
    mov.w   r0, @CMDSIZE_ADDR
```

---

## CMDXA..CMDYD (+0CH..+1AH)

Coordenadas em **two's complement, 11-bit com sign-extension**.
Range válido: −1024 ≤ coord ≤ 1023.
Bits [15:11] devem replicar o valor do bit 10 (sign extension).

```asm
; Escrita de coordenada negativa (-50):
; -50 em two's complement 11-bit = 0x7CE
; sign-extended para 16-bit: 0xFFCE
    mov.w   #-50, r0    ; SH-2: immediate sign-extended automaticamente
    mov.w   r0, @CMDXA_ADDR
```

**Uso por comando:**

| Comando              | XA,YA            | XB,YB              | XC,YC            | XD,YD     |
|----------------------|------------------|--------------------|------------------|-----------|
| Normal Sprite        | Vértice A (UL)   | —                  | —                | —         |
| Scaled (2 coords)    | Vértice A (UL)   | —                  | Vértice C (LR)   | —         |
| Scaled (zoom point)  | Zoom point coord | Display width X,Y  | —                | —         |
| Distorted Sprite     | Vértice A (UL)   | Vértice B (UR)     | Vértice C (LR)   | Vértice D (LL) |
| Polygon              | Vértice A        | Vértice B          | Vértice C        | Vértice D |
| Polyline             | Vértice A        | Vértice B          | Vértice C        | Vértice D |
| Line                 | Vértice A        | Vértice B          | —                | —         |
| User Clipping        | UL coord         | —                  | LR coord         | —         |
| System Clipping      | —                | —                  | LR coord         | —         |
| Local Coord          | Local offset     | —                  | —                | —         |

---

## CMDGRDA (+1CH)

```
bits [15:0] = endereço da Gouraud shading table / 8H
```

Válido somente quando CC bits indicam Gouraud (100, 110, 111).
Gouraud table: 8H bytes, alinhada em 8H-byte boundary.

```
Table addr + 0: RGB vertex A (UL para sprites)
Table addr + 2: RGB vertex B (UR)
Table addr + 4: RGB vertex C (LR)
Table addr + 6: RGB vertex D (LL)
```

Formato RGB na Gouraud table: MSB ignorado, B[14:10], G[9:5], R[4:0].
Valor 10H = sem modificação. 00H = −10H de luminosidade. 1FH = +0FH.

---

## Campos Ignorados por Tipo de Comando

| Comando            | Ignorados                                |
|--------------------|------------------------------------------|
| Normal Sprite      | CMDXB..CMDYD (ZP deve ser 0)             |
| Scaled (2 coords)  | CMDXB,CMDYB, CMDXD,CMDYD                 |
| Scaled (zoom)      | CMDXC..CMDYD                             |
| Polygon/Polyline   | CMDSRCA, CMDSIZE (non-textured)          |
| Line               | CMDXC..CMDYD, CMDSRCA, CMDSIZE           |
| System Clipping    | CMDPMOD, CMDCOLR, CMDSRCA, CMDSIZE, CMDXA..CMDXB, CMDYB, CMDXD..CMDYD |
| User Clipping      | CMDPMOD, CMDCOLR, CMDSRCA, CMDSIZE, CMDXB, CMDYB, CMDXD, CMDYD |
| Local Coord        | CMDPMOD, CMDCOLR, CMDSRCA, CMDSIZE, CMDXB..CMDYD |
| Draw End           | tudo exceto CMDCTRL (+00H = 8000H)       |
