# VDP1 — Comandos de Desenho (Exemplos em Assembly SH-2)

Cada exemplo assume VRAM base = 5C00000H.
Macro auxiliar para escrever word no VRAM:
```asm
; r4 = ponteiro VRAM atual (incrementado manualmente)
.macro wword val
    mov.w   #\val, r0
    mov.w   r0, @r4
    add     #2, r4
.endm
```

---

## Macro Geral: Inicializa Ponteiro

```asm
    mov.l   #5C00000H, r4       ; primeira command table em VRAM[0]
```

---

## 1. System Clipping Coordinate Set (obrigatório após reset)

Comm=1001B, END=0. Upper-left fixo em (0,0).

```asm
; System Clipping: 0,0 → 319,223  (Normal NTSC 320×224)
    ; CMDCTRL: JP=000(next), ZP=0, Dir=00, Comm=1001
    mov.w   #0x0009, r0
    mov.w   r0, @r4   ; +00H CMDCTRL
    add     #2, r4

    mov.w   #0x0000, r0
    mov.w   r0, @r4   ; +02H CMDLINK (ignorado, jump next)
    add     #2, r4

    ; +04H..+12H: ignorados (escrever zeros)
    .rept 8
    mov.w   #0x0000, r0
    mov.w   r0, @r4
    add     #2, r4
    .endr

    ; +14H CMDXC: lower-right X = 319
    mov.w   #319, r0
    mov.w   r0, @r4
    add     #2, r4

    ; +16H CMDYC: lower-right Y = 223
    mov.w   #223, r0
    mov.w   r0, @r4
    add     #2, r4

    ; +18H..+1EH: ignorados
    .rept 4
    mov.w   #0x0000, r0
    mov.w   r0, @r4
    add     #2, r4
    .endr
```

---

## 2. Local Coordinate Set

Comm=1010B. Offset adicionado a todos os draw commands subsequentes.

```asm
; Local Coordinates: (160, 112) = centro da tela 320×224
    mov.w   #0x000A, r0   ; CMDCTRL: Comm=1010
    mov.w   r0, @r4   add #2, r4

    mov.w   #0, r0
    mov.w   r0, @r4   add #2, r4    ; CMDLINK

    ; zeros para +04H..+0AH
    .rept 4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr

    ; +0CH CMDXA: X local = 160
    mov.w   #160, r0
    mov.w   r0, @r4   add #2, r4

    ; +0EH CMDYA: Y local = 112
    mov.w   #112, r0
    mov.w   r0, @r4   add #2, r4

    ; zeros para o restante
    .rept 8
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr
```

---

## 3. Polygon Draw (Quad Preenchido)

Comm=0100B, END=0.
CMDPMOD: ECD=1 (bit7), SPD=1 (bit6), ColMd=000, CC=000 → `0x00C0`
CMDCOLR: cor RGB direta. MSB=1.

```asm
; Polígono: quadrado 50×50 em (10,10), cor branca (RGB 1F,1F,1F = FFFFH)
    ; CMDCTRL: JP=000, ZP=0, Dir=00, Comm=0100
    mov.w   #0x0004, r0   mov.w r0, @r4   add #2, r4

    ; CMDLINK
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDPMOD: ECD=1, SPD=1, ColMd=000, CC=000 (replace)
    mov.w   #0x00C0, r0   mov.w r0, @r4   add #2, r4

    ; CMDCOLR: branco RGB = FFFFH
    mov.w   #0xFFFF, r0   mov.w r0, @r4   add #2, r4

    ; CMDSRCA: ignorado
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDSIZE: ignorado
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDXA=10, CMDYA=10
    mov.w   #10, r0   mov.w r0, @r4   add #2, r4
    mov.w   #10, r0   mov.w r0, @r4   add #2, r4

    ; CMDXB=59, CMDYB=10
    mov.w   #59, r0   mov.w r0, @r4   add #2, r4
    mov.w   #10, r0   mov.w r0, @r4   add #2, r4

    ; CMDXC=59, CMDYC=59
    mov.w   #59, r0   mov.w r0, @r4   add #2, r4
    mov.w   #59, r0   mov.w r0, @r4   add #2, r4

    ; CMDXD=10, CMDYD=59
    mov.w   #10, r0   mov.w r0, @r4   add #2, r4
    mov.w   #59, r0   mov.w r0, @r4   add #2, r4

    ; CMDGRDA: sem Gouraud
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; dummy +1EH
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4
```

---

## 4. Line Draw (Linha Simples)

Comm=0110B. Apenas vértices A e B.

```asm
; Linha de (0,0) a (319,223), cor verde = 83E0H
    mov.w   #0x0006, r0   mov.w r0, @r4   add #2, r4  ; CMDCTRL

    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDLINK

    ; CMDPMOD: ECD=1, SPD=1
    mov.w   #0x00C0, r0   mov.w r0, @r4   add #2, r4

    ; CMDCOLR: verde = 83E0H  (R=0,G=1FH,B=0, MSB=1 → 1_00000_11111_00000 = 83E0H)
    mov.w   #0x83E0, r0   mov.w r0, @r4   add #2, r4

    ; CMDSRCA, CMDSIZE: ignorados
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4

    ; CMDXA=0, CMDYA=0
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4

    ; CMDXB=319, CMDYB=223
    mov.w   #319, r0   mov.w r0, @r4   add #2, r4
    mov.w   #223, r0   mov.w r0, @r4   add #2, r4

    ; C, D, GRDA: ignorados → zeros (6 words)
    .rept 6
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr

    ; dummy
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
```

---

## 5. Normal Sprite Draw (Textured, Color Bank 16 cores)

Comm=0000B. CMDPMOD com color mode 0 (4bpp color bank).
- Character pattern: 4bpp, 8×8 pixels = 32 bytes, em VRAM[0x0020] (após a command table)
- Color bank: ex. 0x0000 (primeiros 16 colors da palette VDP2)

```asm
; Normal sprite 8×8 em (20,20), color bank 0
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDCTRL: Comm=0000

    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDLINK

    ; CMDPMOD: mode 0 (ColMd=000), replace, ECD=0, SPD=0
    ; → 0x0000 (deixa end code e transparência ativos)
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDCOLR: color bank = 0x0000 (lower 4 bits = 0 obrigatório)
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDSRCA: character pattern em VRAM[0x0020] → 0x0020/8 = 0x0004
    mov.w   #0x0004, r0   mov.w r0, @r4   add #2, r4

    ; CMDSIZE: 8×8 → sizeX/8=1 (bit[8]), sizeY=8
    ; word = (1 << 8) | 8 = 0x0108
    mov.w   #0x0108, r0   mov.w r0, @r4   add #2, r4

    ; CMDXA=20, CMDYA=20 (upper-left)
    mov.w   #20, r0   mov.w r0, @r4   add #2, r4
    mov.w   #20, r0   mov.w r0, @r4   add #2, r4

    ; B, C, D: ignorados para normal sprite
    .rept 6
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr

    ; CMDGRDA: sem Gouraud
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4

    ; dummy
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
```

---

## 6. Distorted Sprite (Rotação, Escalamento, Deformação)

Comm=0010B. Quatro vértices definem a forma final.

```asm
; Sprite 32×32 rotacionado ~45° (aproximação por 4 vértices)
; Character em VRAM[0x0040]: endereço/8 = 8
; Vértices (A=UL, B=UR, C=LR, D=LL) de um losango 40×40 centrado em (160,112):
;   A=(160,92), B=(180,112), C=(160,132), D=(140,112)

    mov.w   #0x0002, r0   mov.w r0, @r4   add #2, r4  ; CMDCTRL: Comm=0010

    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDLINK

    ; CMDPMOD: ColMd=101 (RGB 16bpp), replace, ECD=1, SPD=1
    ; bits: 0000 0000 1100 1101 = 00CDH  → ColMd=101→bits[5:3]=101, ECD=1, SPD=1
    ; = 0b_0000_0000_1100_1101 = 0x00CD... vamos calcular:
    ; MON=0, HSS=0, Pclp=0, Clip=0, Cmod=0, Mesh=0, ECD=1(bit7), SPD=1(bit6),
    ; ColMd=101(bits5:3), CC=000(bits2:0) → 0b00000000_11001000 = 0x00C8? 
    ; ColMd=101 → bits5:3 = 1,0,1 → bit5=1,bit4=0,bit3=1 → +0x28
    ; ECD=bit7=1 → +0x80, SPD=bit6=1 → +0x40
    ; Total: 0x40+0x80+0x28 = 0x00E8
    mov.w   #0x00E8, r0   mov.w r0, @r4   add #2, r4

    ; CMDCOLR: ignorado em RGB mode
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDSRCA: char em VRAM[0x0040] → 0x0040/8 = 8
    mov.w   #8, r0   mov.w r0, @r4   add #2, r4

    ; CMDSIZE: 32×32 → sizeX/8=4, sizeY=32 → (4<<8)|32 = 0x0420
    mov.w   #0x0420, r0   mov.w r0, @r4   add #2, r4

    ; CMDXA=160, CMDYA=92  (vértice A)
    mov.w   #160, r0   mov.w r0, @r4   add #2, r4
    mov.w   #92,  r0   mov.w r0, @r4   add #2, r4

    ; CMDXB=180, CMDYB=112 (vértice B)
    mov.w   #180, r0   mov.w r0, @r4   add #2, r4
    mov.w   #112, r0   mov.w r0, @r4   add #2, r4

    ; CMDXC=160, CMDYC=132 (vértice C)
    mov.w   #160, r0   mov.w r0, @r4   add #2, r4
    mov.w   #132, r0   mov.w r0, @r4   add #2, r4

    ; CMDXD=140, CMDYD=112 (vértice D)
    mov.w   #140, r0   mov.w r0, @r4   add #2, r4
    mov.w   #112, r0   mov.w r0, @r4   add #2, r4

    mov.w   #0, r0   mov.w r0, @r4   add #2, r4  ; CMDGRDA
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4  ; dummy
```

---

## 7. User Clipping Coordinate Set

Comm=1000B. Define área de clipping do usuário.

```asm
; User clip: área (50,50) → (269,173)
    mov.w   #0x0008, r0   mov.w r0, @r4   add #2, r4  ; CMDCTRL

    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDLINK

    ; +04H..+0AH: ignorados (6 bytes = 3 words)
    .rept 3
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr

    ; CMDXA=50 (upper-left X)
    mov.w   #50, r0   mov.w r0, @r4   add #2, r4
    ; CMDYA=50 (upper-left Y)
    mov.w   #50, r0   mov.w r0, @r4   add #2, r4

    ; CMDXB, CMDYB: ignorados
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4

    ; CMDXC=269 (lower-right X)
    mov.w   #269, r0   mov.w r0, @r4   add #2, r4
    ; CMDYC=173 (lower-right Y)
    mov.w   #173, r0   mov.w r0, @r4   add #2, r4

    ; CMDXD, CMDYD, CMDGRDA, dummy: ignorados
    .rept 4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr
```

---

## 8. Draw End Command

```asm
; Obrigatório ao final de cada frame de comandos
    mov.w   #0x8000, r0     ; END=1, resto ignorado
    mov.w   r0, @r4
    add     #2, r4
    ; preencher os +02H..+1EH (15 words) com 0 (boa prática)
    .rept 15
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr
```

---

## 9. Jump/Subroutine Pattern

```asm
; Sequência com subroutine call:
; Main table em 0x0000: call tabela em 0x0060
; Sub table em 0x0060: dois polígonos, depois return

; MAIN TABLE (0x0000): Comm=polygon, JP=010 (call), CMDLINK=0x0060/8=0x000C
    mov.w   #0x5004, r0   ; CMDCTRL: JP=010(call), Comm=0100(polygon)
    mov.w   r0, @r4   add #2, r4
    mov.w   #0x000C, r0   ; CMDLINK = 0x60/8 = 0xC
    mov.w   r0, @r4
    ; ... resto do polígono principal ...

; SUB TABLE (0x0060): primeiro polígono, JP=000(next)
; ... define polígono normalmente ...

; SUB TABLE (0x0080): segundo polígono, JP=011(return)
    mov.w   #0x6004, r0   ; CMDCTRL: JP=011(return), Comm=0100
    ; ... define segundo polígono ...
```

---

## RGB Color Reference

| Cor        | Hex    | Binário MSB-B-G-R           |
|------------|--------|-----------------------------|
| Preto      | 0x8000 | 1_00000_00000_00000         |
| Vermelho   | 0x801F | 1_00000_00000_11111         |
| Verde      | 0x83E0 | 1_00000_11111_00000         |
| Azul       | 0xFC00 | 1_11111_00000_00000         |
| Branco     | 0xFFFF | 1_11111_11111_11111         |
| Amarelo    | 0x83FF | 1_00000_11111_11111         |
| Ciano      | 0xFFE0 | 1_11111_11111_00000         |
| Magenta    | 0xFC1F | 1_11111_00000_11111         |
| Cinza 50%  | 0x8C63 | 1_01000_11000_10001 (≈)     |
| Transparente| 0x0000 | (VDP2 trata como transparente) |

> **ATENÇÃO:** 0x0000 é cor transparente! Preto RGB correto = 0x8000.

---

## Cálculo CMDPMOD rápido

```
CMDPMOD = (MON << 15) | (HSS << 12) | (Pclp << 11) | (Clip << 10) |
          (Cmod << 9) | (Mesh << 8) | (ECD << 7) | (SPD << 6) |
          (ColMd << 3) | CC
```

Valores comuns:
```
0x00C0 → Non-textured (ECD=1,SPD=1,ColMd=0,CC=replace)
0x00C4 → Non-textured + Gouraud (ECD=1,SPD=1,ColMd=0,CC=100)
0x00E8 → RGB sprite (ECD=1,SPD=1,ColMd=101=RGB,CC=replace)
0x00EC → RGB sprite + Gouraud (ECD=1,SPD=1,ColMd=101,CC=100)
0x00EA → RGB sprite + Half-Transparent (ECD=1,SPD=1,ColMd=101,CC=011)
0x00C2 → Non-textured + Half-Luminance (ECD=1,SPD=1,CC=010)
0x0000 → 16col color bank sprite, replace, ECD/SPD enabled
0x0008 → 16col color bank sprite, ColMd=001 (LUT mode)
```
