# Mapa de Memória Completo — Sega Saturn

## Espaço de Endereçamento SH2 (32-bit, Big-Endian)

```
Endereço          Tamanho   Região                     Bus / Velocidade
─────────────────────────────────────────────────────────────────────────
0x00000000        512 KB    Boot ROM (BIOS)            A-Bus CS0, ~70ns
0x00100000        4 KB      Área de I/O SMPC
0x00180000        32 KB     Backup RAM (interna)       B-Bus
0x00200000        1 MB      WRAM-L (DRAM)              B-Bus, ~130ns
0x00300000        ...       (mirror de WRAM-L)
0x01000000        8 MB      A-Bus CS0 (cartucho)       A-Bus
0x01800000        8 MB      A-Bus CS1 (cartucho)
0x02000000        512 MB    A-Bus CS2 (CD-ROM block)
0x04000000        32 MB     A-Bus CS3 (SDRAM externo)
0x05900000        4 KB      SSH2 On-Chip Registers
0x05A00000        4 KB      SCU Registers
0x05B00000        4 KB      SMPC Registers
0x05C00000        512 KB    VDP1 VRAM                  VDP1 Bus
0x05D00000        512 KB    VDP1 Registers (write only)
0x05E00000        512 KB    VDP2 VRAM                  VDP2 Bus
0x05F00000        4 KB      VDP2 Color RAM (4096 × 16b)
0x05F80000        4 KB      VDP2 Registers
0x05FE0000        ...       (área de sistema)
0x05FF8000        1 KB      SCU DSP Program RAM
0x05FF8400        1 KB      SCU DSP Data RAM (CT0–CT3)
0x06000000        1 MB      WRAM-H (SDRAM)             SH2 Bus, ~70ns, cached
─────────────────────────────────────────────────────────────────────────
0x20000000+       Mirror     Cache-Through (addr | 0x20000000)
                             Mesmo espaço acima mas sem cache do SH2
```

## Velocidades de Acesso (Ciclos SH2)

| Memória         | Acesso Direto | Via Cache | Notas                          |
|-----------------|:------------:|:---------:|--------------------------------|
| WRAM-H (SDRAM)  | 2–3 ciclos    | 1 ciclo   | Código + dados hot aqui        |
| WRAM-L (DRAM)   | 5–8 ciclos    | N/A       | Fonte para SCU DMA             |
| Boot ROM        | 5 ciclos      | 1 ciclo   | Somente leitura, BIOS          |
| VDP1 VRAM       | 4–6 ciclos    | N/A       | Escritas diretas durante VBLANK|
| VDP2 VRAM       | 4–6 ciclos    | N/A       | Tiles, planos                  |
| VDP1/VDP2 Regs  | 4–8 ciclos    | N/A       | Polling pesado = stall         |
| SCU Registers   | 8+ ciclos     | N/A       | DMA setup apenas               |
| SMPC Registers  | 32+ ciclos    | N/A       | Somente em VBlank              |

## Regras de Acesso por Subsistema

### WRAM-H (0x06000000)
- **SH2 Master/Slave:** acesso pleno (com e sem cache)
- **SCU DMA:** ❌ NÃO ACESSÍVEL como fonte/destino de DMA!
- **Uso:** code (.text), dados quentes, tabelas math, buffers de frame

### WRAM-L (0x00200000)
- **SH2 Master/Slave:** acesso pleno (mais lento, sem cache eficiente)
- **SCU DMA:** ✅ Fonte e destino de DMA
- **SCU DSP:** ✅ Leitura/escrita via DMA nível 2
- **Uso:** meshes estáticos, buffers DMA, dados de áudio, staging de textura

### VDP1 VRAM (0x05C80000)
- **SH2:** escrita direta ✅ (mas lento; preferir SCU DMA)
- **SCU DMA:** ✅ Destino de DMA (canal 0 ou 1)
- **VDP1:** leitura exclusiva durante rasterização
- **Layout recomendado:**
  ```
  0x05C80000 – 0x05C80FFF  Lista de comandos (4KB = ~128 commands)
  0x05C81000 – 0x05CFFFFF  Texturas (restante dos 512KB menos a cmd area)
  ```
- **Alinhamento obrigatório:** textura deve estar em endereço múltiplo de 8 bytes

### VDP2 VRAM (0x05E00000)
- Bancos A (0x05E00000) e B (0x05E80000)
- **Layout recomendado:**
  ```
  Banco A: Pattern name tables (tilemap)
  Banco B: Cell data (tiles gráficos)
  ```

## Cache do SH2

Cada SH2 tem 4 KB de cache 4-way set-associative para instrução + dados.

```
Linha de cache: 16 bytes (4 longs)
Cache hit:  1 ciclo
Cache miss: 2-8 ciclos (WRAM-H), 5-13 ciclos (WRAM-L)
```

**Cache-Through (addr | 0x20000000):**
- Bypassa o cache do SH2
- Escritas vão direto para memória
- Leituras buscam sempre da memória
- **Obrigatório para dados compartilhados entre Master e Slave SH2**
- Sem cache-through → um SH2 lê dado stale de cache enquanto o outro escreveu

## SCU Registradores (0x05A00000)

```
0x05A00000  D0R   DMA Ch0 Read Address
0x05A00004  D0W   DMA Ch0 Write Address
0x05A00008  D0C   DMA Ch0 Byte Count
0x05A0000C  D0AD  DMA Ch0 Address Add
0x05A00010  D0EN  DMA Ch0 Enable / Status
0x05A00014  D0MD  DMA Ch0 Mode
0x05A00020  D1R   DMA Ch1 Read Address
0x05A00024  D1W   DMA Ch1 Write Address
0x05A00028  D1C   DMA Ch1 Byte Count
0x05A0002C  D1AD  DMA Ch1 Address Add
0x05A00030  D1EN  DMA Ch1 Enable / Status
0x05A00034  D1MD  DMA Ch1 Mode
0x05A00040  D2R   DMA Ch2 Read Address (menor prioridade, para DSP)
0x05A00044  D2W   DMA Ch2 Write Address
0x05A00048  D2C   DMA Ch2 Byte Count
0x05A0004C  D2AD  DMA Ch2 Address Add
0x05A00050  D2EN  DMA Ch2 Enable / Status
0x05A00054  D2MD  DMA Ch2 Mode
0x05A00060  DSTP  DMA Stop
0x05A00070  DSTA  DMA Status
0x05A0001C  IST   Interrupt Status
0x05A00020  AIACK Interrupt Acknowledge
0x05A00024  ASR0  Interrupt Mask (SCU)
0x05A00028  RSEL  Interrupt Route Select
0x05A0007C  VER   SCU Version
```

## D0AD — Controle de Incremento DMA

```
Bits 9:8 = Incremento de destino: 00=+0, 01=+2, 10=+4, 11=+8? (ver manual)
Bits 1:0 = Incremento de fonte:   00=+0, 01=+2, 10=+4, 11=invalid

Valor típico para copiar buffer: 0x00000101 (src+4, dst+4)
Valor para broadcast:            0x00000100 (src+4, dst fixo)
```

## Vetor de Interrupção SCU (IST bits)

```
Bit  0: VBlank-IN   (mais importante — sincronização de frame)
Bit  1: VBlank-OUT  (VDP1 terminou de rasterizar)
Bit  2: HBlank-IN
Bit  3: Timer 0
Bit  4: Timer 1
Bit  5: DSP End     (SCU DSP terminou programa)
Bit  6: Sound Request (M68k pediu atenção)
Bit  7: System Manager (SMPC)
Bit  8: Pad Interrupt
Bit  9–15: A-Bus (CD-ROM, cartucho)
```

## SH2 On-Chip Registers (Internos ao SH2, acessíveis em 0xFFFFF000)

```
0xFFFFFF00  DVSR    Hardware Divider — Divisor
0xFFFFFF04  DVDNT   Dividend Low / Result
0xFFFFFF08  DVCR    Divider Control
0xFFFFFF0C  VCRDIV  Interrupt vector for divider
0xFFFFFF10  DVDNTH  Dividend High (iniciar divisão 32/32: escrever DVDNTH antes de DVDNT)
0xFFFFFF14  DVDNTL  (mirror)
0xFFFFFF40  DMAC0   DMAC interno Ch0 (não confundir com SCU DMA!)
0xFFFFFF80  ITU     Timer Unit (FRT - Free-Running Timer)
0xFFFFFE00  Cache Control Register
```
