# VDP1 — Color Tables (Color Bank, LUT, Gouraud, RGB)

---

## 1. Character Pattern Table

Pixels do sprite armazenados no VRAM. Boundary: **20H bytes**.
Endereço 0x000000 é reservado para command tables. Mínimo: VRAM[0x000020].

### Tamanho por modo

| Color Mode | bpp | 8px×1 linha | 8px×8 linhas |
|------------|-----|-------------|--------------|
| 0,1 (16 cores) | 4 | 4 bytes | 32 bytes |
| 2,3,4 (64/128/256) | 8 | 8 bytes | 64 bytes |
| 5 (32768 RGB)  | 16 | 16 bytes | 128 bytes |

Fórmula: `bytes = (sizeX × sizeY × bpp) / 8`

### Layout na memória (4bpp, 8×3 pixels)

```
VRAM offset:  +0H    +1H    +2H    +3H
pixel row 0:  [0][1] [2][3] [4][5] [6][7]

              +4H    +5H    +6H    +7H
pixel row 1:  [8][9] [A][B] [C][D] [E][F]

              +8H    +9H    +AH    +BH
pixel row 2: [10][11][12][13][14][15][16][17]
```

Cada nibble = 1 pixel (upper nibble = pixel esquerdo).

### Layout 8bpp (8×3 pixels)

```
+0H: pixel 0   +1H: pixel 1   +2H: pixel 2   ...  +7H: pixel 7
+8H: pixel 8   ...
+10H: pixel 16 ...
```

### Layout 16bpp (8×1 pixels)

```
+0H,+1H: pixel 0 (MSB no byte low)
+2H,+3H: pixel 1
...
+EH,+FH: pixel 7
```

---

## 2. Color Lookup Table (LUT)

Usado quando Color Mode = 1 (lookup table mode). Tamanho: **20H bytes** (boundary 20H).
Define 16 cores como valores 16-bit. Os 4 bits do character pattern indexam a tabela.

```
VRAM offset   Conteúdo
+00H          16-bit color code para índice 0H (transparente se SPD=0)
+02H          16-bit color code para índice 1H
+04H          16-bit color code para índice 2H
...
+1CH          16-bit color code para índice EH
+1EH          16-bit color code para índice FH (end code se ECD=0)
```

Os 16-bit values podem ser:
- **Color bank code** (MSB=0): processado pelo VDP2 com color RAM
- **RGB code** (MSB=1): RGB direto, bypassando color RAM do VDP2

O endereço da LUT é especificado em CMDCOLR como `addr/8H`.
Como boundary é 20H: `CMDCOLR = LUT_VRAM_ADDR >> 3` (lower 2 bits = 00)

```asm
; LUT em VRAM[0x0060]
; CMDCOLR = 0x0060 >> 3 = 0x000C
    mov.w   #0x000C, r0
    mov.w   r0, @CMDCOLR_ADDR
```

---

## 3. Gouraud Shading Table

Tamanho: **8H bytes** (4 words). Boundary: **8H bytes**.
Define variação de luminosidade RGB para 4 vértices da part.

```
Table offset  Conteúdo
+0H           RGB data para Vértice A (upper-left em sprites)
+2H           RGB data para Vértice B (upper-right)
+4H           RGB data para Vértice C (lower-right)
+6H           RGB data para Vértice D (lower-left)
```

Para linhas: apenas +0H (start) e +2H (end) são usados; +4H e +6H ignorados.

### Formato RGB na tabela Gouraud

```
bit: [15] ignorado | [14:10] B | [9:5] G | [4:0] R
```

### Mapeamento de valor → correção

| Valor hex | Correção aplicada |
|-----------|-------------------|
| 00H       | −10H (escurece máx) |
| 08H       | −08H              |
| 0FH       | −01H              |
| **10H**   | **0 (sem mudança)** |
| 11H       | +01H              |
| 18H       | +08H              |
| 1FH       | +0FH (clareia máx) |

- Resultado < 00H → clamp a 00H
- Resultado > 1FH → clamp a 1FH
- Para "white Gouraud" (só luminosidade, sem mudança de hue): use mesmo valor para R=G=B

### Exemplo: gradiente de escuro para claro (esquerda para direita)

```asm
; Gouraud table em VRAM[0x0080]
; A(UL): R=G=B=08H → escuro   → 0x1108 (B=02,G=02,R=08? vamos calcular corretamente)
; Formato: bit14:10=B, bit9:5=G, bit4:0=R
; Valor 08H para R,G,B: B=08<<10=0x2000, G=08<<5=0x0100, R=08=0x0008
; → 0x2108  (mas MSB é ignorado, então ok)
; Valor 18H para R,G=B=18H → B=18<<10=0x6000, G=18<<5=0x0300, R=18=0x0018 → 0x6318
; A: escuro (08,08,08)
; B: claro  (18,18,18)

    mov.l   #(VRAM_BASE + 0x0080), r1
    mov.w   #0x2108, r0   ; Vertex A: R=8,G=8,B=8 → 000 01000 01000 01000
    ; Recalculando: bit14:10=B=08→01000, bit9:5=G=08→01000, bit4:0=R=08→01000
    ; = 0b0_01000_01000_01000 = 0x2108
    mov.w   r0, @r1
    add     #2, r1

    mov.w   #0x6318, r0   ; Vertex B: R=18,G=18,B=18
    ; bit14:10=18→11000, bit9:5=18→11000, bit4:0=18→11000
    ; = 0b0_11000_11000_11000 = 0x6318
    mov.w   r0, @r1
    add     #2, r1

    mov.w   #0x6318, r0   ; Vertex C: mesmo que B
    mov.w   r0, @r1
    add     #2, r1

    mov.w   #0x2108, r0   ; Vertex D: mesmo que A
    mov.w   r0, @r1
```

---

## 4. Color Bank Mode

Usado com Color Mode 0, 2, 3, 4. O pixel data do character pattern tem N bits.
Os bits superiores do color bank são concatenados para formar o endereço de 16 bits
na color RAM do VDP2.

### Frame buffer data = color bank + pixel data

| Mode | bpp | Pixel data bits | Color bank bits | Total |
|------|-----|-----------------|-----------------|-------|
|  0   |  4  | [3:0]           | [15:4] (12 bits)| 16    |
|  2   |  8  | [5:0]           | [15:6] (10 bits)| 16    |
|  3   |  8  | [6:0]           | [15:7] (9 bits) | 16    |
|  4   |  8  | [7:0]           | [15:8] (8 bits) | 16    |

**CMDCOLR lower 4 bits devem ser 0** (são OR'd com pixel data).

```asm
; Color bank para mode 0 (16 cores), usando palette 3 do VDP2
; Palette 3 começa em color RAM offset 0x30 (16 cores × 2 bytes × 3 = 0x60 se bank direto)
; Depende de como o VDP2 está configurado. Exemplo genérico:
    mov.w   #0x0030, r0   ; bank code (lower 4 bits = 0 obrigatório)
    mov.w   r0, @CMDCOLR_ADDR
```

### Em 8bpp (high-res ou rotation):
Apenas o byte inferior dos 16 bits é escrito no frame buffer.
Para mode 0: upper 8 bits ignorados → apenas 4 bits de palette code escritos.

---

## 5. Resumo de Endereçamento de Tabelas

| Tabela             | Boundary | Endereço mínimo | Como especificar em command table |
|--------------------|----------|-----------------|-----------------------------------|
| Command Table      | 20H      | 000000H         | Automático (VDP1 começa de 0)    |
| Character Pattern  | 20H      | 000020H         | CMDSRCA = addr / 8H              |
| Color Lookup Table | 20H      | 000020H         | CMDCOLR = addr / 8H              |
| Gouraud Table      | 8H       | 000008H         | CMDGRDA = addr / 8H              |
| (qualquer tabela)  | —        | max = 07FFE0H   | Não ultrapassar 080000H          |

**Nunca defina tabelas além de VRAM[07FFFFH].**

---

## 6. Color Calculation — Detalhes de Implementação

### Half-Transparent (CC=011)
- Pixel do background deve ter MSB=1 para ocorrer transparência
- Se MSB=0: replace normal é aplicado
- Fórmula: `resultado = (original + background) / 2`
- **6× mais lento** que replace
- Cuidado com polylines (pixels desenhados 2× nos vértices → double half-transp)

### Shadow (CC=001)
- Background com MSB=1: `background_RGB = background_RGB / 2`
- Background com MSB=0: nenhuma modificação
- **6× mais lento**
- Para shadow 1/4: escreva a mesma command table 2× no VRAM

### Gouraud Shading (CC=100)
- Apenas em RGB mode (color mode 5 ou LUT com RGB codes)
- Interpola a correção de luminosidade entre os 4 vértices
- Cada R, G, B é ajustado independentemente (pode mudar hue)
- Para manter hue: use R=G=B na Gouraud table

### MSB ON (CMDPMOD bit 15)
- Seta MSB=1 em todos pixels desenhados
- Transparente (0000H) → 8000H (preto com MSB setado)
- Use com Replace (CC=000)
- Habilita shadow/window no VDP2 para aquela área
