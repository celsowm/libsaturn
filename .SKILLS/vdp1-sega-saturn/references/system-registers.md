# VDP1 — System Registers (Referência Completa)

Todos os registradores estão no espaço de endereços absolutos **5D00000H–5D0001FH**.
Acesso exclusivamente em **word (16-bit)**. Nunca use DMA burst transfer.
Bits não utilizados devem ser escritos como 0.

---

## TVMR — TV Mode Selection Register
**Endereço:** 5D00000H | **Acesso:** Write-only

```
bit: 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
      0  0  0  0  0  0  0  0  0  0  0  0 VBE TVM[2:0]
```

### VBE — V-Blank Erase/Write Enable (bit 3)
- `0` = erase/write durante o display period (normal)
- `1` = erase/write durante o V-blank
  - Só pode ser set quando FCM=1 e FCT=1 (FBCR)
  - Deve ser set imediatamente após o interrupt V-blank IN
  - Proibido: do primeiro H-blank IN após V-blank IN até o próximo H-blank IN

### TVM — TV Mode Select (bits 2:0)

| TVM | Nome            | Resolução Display   | FB Size    | bpp | Interlace     |
|-----|-----------------|---------------------|------------|-----|---------------|
| 000 | Normal NTSC/PAL | 320×224, 320×240, 352×224, 352×240 | 512H×256V | 16 | Sim (incl. double) |
| 001 | High Resolution | 640×224 … 704×240   | 1024H×256V | 8  | Sim           |
| 010 | Rotation 16     | 320×224 … 352×240   | 512H×256V  | 16 | Single only   |
| 011 | Rotation 8      | 320×224 … 352×240   | 512H×512V  | 8  | Single only   |
| 100 | HDTV/31KC       | 320×240, 352×240    | 512H×256V  | 16 | Não           |

- Bit 2: `0`=NTSC/PAL, `1`=HDTV/31KC
- Bit 1: `0`=sem rotação, `1`=rotação
- Bit 0: `0`=16bpp, `1`=8bpp

**Timing:** TVM deve ser escrito entre o 2º H-blank IN após V-blank IN e o H-blank IN após V-blank OUT.

---

## FBCR — Frame Buffer Change Mode Register
**Endereço:** 5D00002H | **Acesso:** Write-only

```
bit: 15 14 13 12 11 10  9  8  7  6  5  4   3   2   1   0
      0  0  0  0  0  0  0  0  0  0  0 EOS DIE DIL FCM FCT
```

### FCM/FCT — Frame Buffer Change Mode/Trigger (bits 1, 0)

| VBE | FCM | FCT | Modo              | Quando troca               |
|-----|-----|-----|-------------------|----------------------------|
|  0  |  0  |  0  | 1-cycle (normal)  | Automático, 60fps          |
|  0  |  0  |  1  | PROIBIDO          | —                          |
|  0  |  1  |  0  | Manual (erase)    | Apaga no próximo field     |
|  0  |  1  |  1  | Manual (change)   | Troca no próximo field     |
|  1  |  1  |  1  | Manual (erase+change) | Apaga no V-blank + troca |

**Timing:** FCM/FCT devem ser escritos imediatamente após V-blank OUT interrupt.

### DIE — Double Interlace Enable (bit 3) / DIL — Draw Line (bit 2)

| DIE | DIL | Modo                     |
|-----|-----|--------------------------|
|  0  |  0  | Non/Single interlace     |
|  1  |  0  | Double interlace (even lines) |
|  1  |  1  | Double interlace (odd lines)  |

No double interlace: FCM=FCT=0 (1-cycle mode).

### EOS — Even/Odd Coordinate Select (bit 4)
Usado com High Speed Shrink (HSS=1 no CMDPMOD):
- `0` = samplea pixels em coordenadas even
- `1` = samplea pixels em coordenadas odd

---

## PTMR — Plot Trigger Register
**Endereço:** 5D00004H | **Acesso:** Write-only
**Reset value:** 00B (idle)

```
bit: 15..2 = 0 | 1:0 = PTM
```

| PTM | Comportamento                                               |
|-----|-------------------------------------------------------------|
| 00  | Idle — não inicia desenho                                   |
| 01  | Inicia imediatamente ao escrever (manual trigger)           |
| 10  | Auto-start a cada frame change (modo normal)                |
| 11  | PROIBIDO                                                    |

**Nota:** Ao escrever PTM=01B, reinicia da primeira command table mesmo se já estiver desenhando.
Para mudar apenas o modo sem redesenhar: escreva 00B, depois 01B no próximo frame.

---

## EWDR — Erase/Write Data Register
**Endereço:** 5D00006H | **Acesso:** Write-only

```
16bpp: bits[15:0] = fill data (uniform)
8bpp:  bits[15:8] = fill data for odd X | bits[7:0] = fill data for even X
```

- O frame buffer é preenchido com este valor antes de cada frame
- Para fundo preto em 16bpp: 0x0000
- Para fundo preto em RGB (evitar transparência no VDP2): 0x8000

---

## EWLR — Erase/Write Upper-Left Coordinate
**Endereço:** 5D00008H | **Acesso:** Write-only

```
bit: 15  14:9   8:0
      0   X1     Y1
```

- X1: bits [14:9] = valor do registro. X real = registro × 8 (16bpp) ou × 16 (8bpp)
- Y1: bits [8:0] = coordenada Y real (1 por linha)
- X1=0 (registro=0) é **proibido** — use registro=1 para X1=8

---

## EWRR — Erase/Write Lower-Right Coordinate
**Endereço:** 5D0000AH | **Acesso:** Write-only

```
bit: 15:9   8:0
      X3     Y3
```

- X3: bits [15:9]. X real = registro × 8 − 1 (16bpp) ou × 16 − 1 (8bpp)
- Y3: bits [8:0]

**Exemplos para Normal 16bpp 320×224:**
```
EWLR = (1 << 9) | 0    → X1=8, Y1=0    (ou use 0 para ambos, mas X=0 proibido)
EWRR = (40 << 9) | 223 → X3=319, Y3=223
```

Deve satisfazer X1 < X3 e Y1 ≤ Y3.

---

## ENDR — Draw Forced Termination Register
**Endereço:** 5D0000CH | **Acesso:** Write-only

```asm
    mov.w   #0x0000, r0
    mov.l   #5D0000CH, r1
    mov.w   r0, @r1         ; force-termina em ~30 clock cycles
```

- Termina o desenho atual em ≈30 ciclos após escrita
- Desenho interrompido **não pode ser retomado** normalmente
- Usado na técnica de "pseudo draw continuation" (ver COPR)

---

## EDSR — Transfer End Status Register
**Endereço:** 5D00010H | **Acesso:** Read-only

```
bit: 15:2 = 0 | 1 = CEF | 0 = BEF
```

### CEF — Current End Bit Fetch Status (bit 1)
- `0` = Draw End Command ainda não foi fetched no frame atual
- `1` = Draw End Command foi fetched → desenho terminado

### BEF — Before End Bit Fetch Status (bit 0)
- `0` = frame anterior terminou normalmente
- `1` = frame anterior terminou (end command alcançado)
- `0` após V-blank sem end = **transfer-over** (excesso de dados)

**Polling loop:**
```asm
    mov.l   #5D00010H, r1
.loop:
    mov.w   @r1, r0
    and     #2, r0
    tst     r0, r0
    bt      .loop           ; aguarda CEF=1
```

---

## LOPR — Last Operation Command Address Register
**Endereço:** 5D00012H | **Acesso:** Read-only

Valor = endereço da última command table processada no frame anterior, dividido por 8H.
Lower 2 bits sempre = 00B.

```asm
; Recuperar endereço real da última command table do frame anterior:
    mov.w   @(LOPR), r0
    shll2   r0          ; ×4
    shll    r0          ; ×8 → endereço real / 8H × 8 = endereço
    mov.l   #VRAM_BASE, r1
    add     r1, r0      ; + base absoluta do VRAM
```

---

## COPR — Current Operation Command Address Register
**Endereço:** 5D00014H | **Acesso:** Read-only

Igual ao LOPR mas para o frame atual em progresso. Atualizado continuamente.
Usado para "pseudo draw continuation":
1. Force-termina com ENDR
2. Lê COPR para saber onde parou
3. Escreve jump no topo do VRAM apontando para esse endereço
4. Dispara PTM=01B para continuar

---

## MODR — Mode Status Register
**Endereço:** 5D00016H | **Acesso:** Read-only

```
bit: 15:12=VER | 11:9=--- | 8=PTM1 | 7=EOS | 6=DIE | 5=DIL | 4=FCM | 3=VBE | 2:0=TVM
```

Mirror dos registradores write-only. VER=0001B (versão 1 do VDP1).
Principalmente para debug — valores podem diferir dos sinais internos.

---

## Timing Summary

| Quando fazer            | O quê                          |
|-------------------------|--------------------------------|
| Após reset              | TVMR, FBCR, PTMR, EWDR, EWLR, EWRR |
| Após V-blank IN         | VBE (se usar erase&change)     |
| Após V-blank OUT        | FCM, FCT                       |
| Entre 2º H-blank após VBI e H-blank após VBO | TVM |
| Qualquer momento        | Escrever command/char tables no VRAM |
| Antes de acessar VRAM   | Verificar se VDP1 não está desenhando (poll EDSR) |
