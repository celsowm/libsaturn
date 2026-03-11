---
name: sega-saturn-gamedev-baremetal
description: >
  Use this skill para qualquer tarefa de desenvolvimento de biblioteca, engine, framework ou
  código de baixo nível para o Sega Saturn SEM SGL e SEM libyaul ou qualquer SDK externo.
  100% bare-metal: compilador cross GCC próprio, crt0.s próprio, linker script próprio.
  Triggers: "Saturn lib", "Saturn engine", "homebrew Saturn", "VDP1", "VDP2", "SH2",
  "SCU DSP", "SCSP", "Saturn sem SGL", "Saturn bare-metal", "Saturn 3D engine",
  "Saturn game library", qualquer pedido de acesso direto ao hardware do Saturn.
  Sempre usar esta skill quando o usuário mencionar Sega Saturn no contexto de desenvolvimento.
---

# Sega Saturn — Bare-Metal Game Library (Sem SGL, Sem libyaul)

Abordagem totalmente independente de qualquer SDK externo. Apenas:
- Cross-compiler **sh2eb-elf-gcc** compilado do zero
- **crt0.s** e **linker script** escritos à mão
- Acesso direto a registradores de hardware
- C++ moderno + assembly SH2 inline/puro

Detalhes de subsistemas estão nos arquivos de referência em `references/`.

---

## 1. Por que não SGL e não libyaul?

**SGL:**
- Limite de ~500 quads/frame é imposição de software, não hardware
- C puro sem otimização de pipeline, sem uso do SCU DSP nem Slave SH2
- Força uso de estruturas internas incompatíveis com C++ moderno

**libyaul:**
- Sem atualizações relevantes desde ~2022; GCC travado em versão antiga
- Abstrai demais: impede acesso fino à command list do VDP1
- Dependência de ambiente complexo que frequentemente quebra em novas distros

**Solução:** compilador próprio + zero dependências em runtime externo.

---

## 2. Hardware em 30 Segundos

| Chip      | Função                          | Clock       |
|-----------|---------------------------------|-------------|
| SH2 Master| Lógica principal, render        | 28.63 MHz   |
| SH2 Slave | Geometria paralela              | 28.63 MHz   |
| SCU DSP   | Multiplicação matricial em batch| 14.31 MHz   |
| VDP1      | Rasterizador (command list)     | 28.63 MHz   |
| VDP2      | Planos de fundo, compositing    | 28.63 MHz   |
| M68k+SCSP | Áudio PCM/FM, 32 canais         | 11.3 MHz    |
| SMPC      | Controles, reset, relógio       | 4 MHz       |

Mapa de memória completo → `references/memory_map.md`

---

## 3. Toolchain: Compilando o Cross-Compiler do Zero

**Única dependência:** GCC + binutils host. Sem pacotes Saturn específicos.

### 3.1 Construir sh2eb-elf-gcc

```bash
#!/usr/bin/env bash
# build-toolchain.sh — cria sh2eb-elf-gcc em ~/saturn-tools/

set -e

PREFIX="$HOME/saturn-tools"
TARGET="sh2eb-elf"
GCC_VER="13.2.0"
BINUTILS_VER="2.41"
NEWLIB_VER="4.3.0.20230120"

JOBS=$(nproc)
mkdir -p "$PREFIX" build-binutils build-gcc

# ---- Binutils ----
[ ! -f "binutils-$BINUTILS_VER.tar.xz" ] && \
  wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz"
tar xf "binutils-$BINUTILS_VER.tar.xz"

cd build-binutils
../binutils-$BINUTILS_VER/configure \
  --target=$TARGET \
  --prefix=$PREFIX \
  --with-sysroot \
  --disable-nls \
  --disable-werror
make -j$JOBS && make install
cd ..

# ---- GCC (sem newlib primeiro — só compilador C/C++) ----
[ ! -f "gcc-$GCC_VER.tar.xz" ] && \
  wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz"
tar xf "gcc-$GCC_VER.tar.xz"
cd gcc-$GCC_VER && ./contrib/download_prerequisites && cd ..

mkdir -p build-gcc && cd build-gcc
../gcc-$GCC_VER/configure \
  --target=$TARGET \
  --prefix=$PREFIX \
  --without-headers \
  --disable-nls \
  --disable-shared \
  --disable-multilib \
  --disable-decimal-float \
  --disable-threads \
  --disable-libatomic \
  --disable-libgomp \
  --disable-libquadmath \
  --disable-libssp \
  --disable-libvtv \
  --disable-libstdcxx \
  --enable-languages=c,c++ \
  --with-endian=big \
  --with-cpu=sh2
make -j$JOBS all-gcc all-target-libgcc
make install-gcc install-target-libgcc
cd ..

echo "Toolchain pronta em $PREFIX/bin/${TARGET}-gcc"
```

Adicionar ao PATH:
```bash
export PATH="$HOME/saturn-tools/bin:$PATH"
# Ou permanentemente em ~/.bashrc
echo 'export PATH="$HOME/saturn-tools/bin:$PATH"' >> ~/.bashrc
```

### 3.2 Flags de Compilação Obrigatórias

```makefile
# Makefile — flags críticas
CC  = sh2eb-elf-gcc
CXX = sh2eb-elf-g++
AS  = sh2eb-elf-as
LD  = sh2eb-elf-ld
OBJCOPY = sh2eb-elf-objcopy

# -m2       → ISA SH2
# -mb       → Big-endian (Saturn é big-endian!)
# -O2       → Habilita instrução MAC, scheduling do pipeline
# -fno-exceptions -fno-rtti  → Remove overhead C++
# -ffreestanding → Sem libc host
# -fomit-frame-pointer → R14 livre para uso geral
# -fipa-ra  → Inter-procedure register allocation
CFLAGS  = -m2 -mb -O2 -ffreestanding -fno-exceptions \
           -fomit-frame-pointer -fipa-ra -Wall
CXXFLAGS = $(CFLAGS) -fno-rtti -std=c++20
ASFLAGS = --isa=sh2 -big
```

---

## 4. Startup Completo do Zero

O Saturn não tem OS. Precisamos de:
1. **crt0.s** — entrada do programa, configura stack, zera BSS, chama `main`
2. **saturn.ld** — linker script com mapa de memória correto
3. **IP.BIN** — cabeçalho de boot (16 setores fixos; usar o do SGL sample/sys ou gerar com isomaker)

### 4.1 crt0.s — Startup Assembly SH2

```asm
! crt0.s — Saturn bare-metal startup
! Executa após IP.BIN transferir controle para 0x06004000

    .section .text.start
    .global _start
    .align 2

_start:
    ! ── Desabilitar interrupções ─────────────────────────────────
    mov.l   sr_val, r0
    ldc     r0, sr              ! SR = 0xF0 (máscara IPM = 15)

    ! ── Stack pointer Master SH2 ─────────────────────────────────
    mov.l   stack_top, r15      ! SP = topo de WRAM-H (0x060FFFFC)

    ! ── Limpar registradores ─────────────────────────────────────
    xor     r0, r0
    xor     r1, r1
    xor     r2, r2
    xor     r3, r3
    xor     r4, r4
    xor     r5, r5
    xor     r6, r6
    xor     r7, r7

    ! ── Copiar seção .data de ROM para WRAM-H ────────────────────
    mov.l   data_lma, r0        ! Fonte: LMA no binário
    mov.l   data_vma, r1        ! Destino: WRAM-H
    mov.l   data_end, r2
    cmp/eq  r1, r2
    bt      bss_zero            ! Nada a copiar se igual
copy_data:
    mov.l   @r0+, r3
    mov.l   r3, @r1
    add     #4, r1
    cmp/hs  r2, r1
    bf      copy_data

    ! ── Zerar seção .bss ─────────────────────────────────────────
bss_zero:
    mov.l   bss_start, r0
    mov.l   bss_end,   r1
    xor     r2, r2
    cmp/eq  r0, r1
    bt      call_ctors
zero_loop:
    mov.l   r2, @r0
    add     #4, r0
    cmp/hs  r1, r0
    bf      zero_loop

    ! ── Construtores C++ (se houver) ─────────────────────────────
call_ctors:
    mov.l   ctors_start, r0
    mov.l   ctors_end,   r1
    cmp/eq  r0, r1
    bt      jump_main
ctor_loop:
    mov.l   @r0+, r2
    jsr     @r2
    nop
    cmp/hs  r1, r0
    bf      ctor_loop

    ! ── Chamar main ──────────────────────────────────────────────
jump_main:
    mov.l   main_addr, r0
    jsr     @r0
    nop

    ! ── Jamais retornar — loop infinito ──────────────────────────
hang:
    bra     hang
    nop

    .align 4
sr_val:     .long 0x000000F0    ! IPM=15 (todos ints mascarados)
stack_top:  .long 0x060FFFFC    ! Topo de WRAM-H (Master SH2)
data_lma:   .long __data_load
data_vma:   .long __data_start
data_end:   .long __data_end
bss_start:  .long __bss_start
bss_end:    .long __bss_end
ctors_start:.long __ctors_start
ctors_end:  .long __ctors_end
main_addr:  .long main
```

### 4.2 saturn.ld — Linker Script

```ld
/* saturn.ld — Linker script bare-metal para Sega Saturn
   Programa carregado para WRAM-H a partir de 0x06004000
   (IP.BIN ocupa 0x06000000..0x06003FFF) */

OUTPUT_FORMAT("elf32-sh", "elf32-sh", "elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)

MEMORY {
    /* WRAM-H: 1MB SDRAM rápido. Reserva 16KB para IP.BIN */
    WRAMH (rwx) : ORIGIN = 0x06004000, LENGTH = 0x000FC000
    /* WRAM-L: 1MB DRAM lento. Para dados DMA e malhas estáticas */
    WRAML (rw)  : ORIGIN = 0x00200000, LENGTH = 0x00100000
}

SECTIONS {
    /* ── Código + constantes somente-leitura ── */
    .text 0x06004000 : {
        *(.text.start)      /* crt0 sempre primeiro */
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > WRAMH

    /* ── Dados inicializados ── */
    __data_load = LOADADDR(.data);
    .data : AT(__data_load) {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > WRAMH

    /* ── Construtores / destrutores C++ ── */
    .ctors : {
        __ctors_start = .;
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(.ctors))
        __ctors_end = .;
    } > WRAMH

    .dtors : {
        __dtors_start = .;
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))
        __dtors_end = .;
    } > WRAMH

    /* ── BSS (não ocupa espaço no binário) ── */
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > WRAMH

    /* ── WRAM-L: meshes estáticos, buffers DMA ── */
    .wram_l (NOLOAD) : {
        *(.wram_l)
        . = ALIGN(4);
    } > WRAML

    /* Stacks são alocadas no final de WRAM-H via SP inicial no crt0 */
    /* Master SH2: 0x060FFFFC (decresce) */
    /* Slave SH2:  0x060BFFFC (decresce) — definir via SMPC antes de ativar */

    /* Descartar seções desnecessárias */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.ARM.*)
    }
}
```

### 4.3 Makefile Completo

```makefile
# Makefile — projeto bare-metal Saturn

CC      := sh2eb-elf-gcc
CXX     := sh2eb-elf-g++
AS      := sh2eb-elf-as
LD      := sh2eb-elf-ld
OBJCOPY := sh2eb-elf-objcopy
MKISOFS := mkisofs

CFLAGS   := -m2 -mb -O2 -ffreestanding -fno-exceptions \
            -fomit-frame-pointer -fipa-ra -Wall -Wextra
CXXFLAGS := $(CFLAGS) -fno-rtti -std=c++20
ASFLAGS  := --isa=sh2 -big
LDFLAGS  := -T saturn.ld --no-gc-sections

SRC_C   := $(wildcard src/**/*.c src/*.c)
SRC_CXX := $(wildcard src/**/*.cpp src/*.cpp)
SRC_AS  := $(wildcard src/**/*.s src/*.s) crt0.s

OBJ := $(SRC_C:.c=.o) $(SRC_CXX:.cpp=.o) $(SRC_AS:.s=.o)

TARGET := game

all: $(TARGET).iso

$(TARGET).elf: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).iso: $(TARGET).bin
	mkdir -p iso_root
	cp $(TARGET).bin iso_root/0.BIN   # "primeiro arquivo" = entry point
	$(MKISOFS) \
	  -sysid "SEGA SATURN" \
	  -volid "GAME" \
	  -publisher "STUDIO" \
	  -l -iso-level 1 \
	  -joliet \
	  -o $(TARGET).iso \
	  ip.bin                           # IP.BIN como track especial
	  # (ver seção 5 sobre IP.BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).bin $(TARGET).iso

.PHONY: all clean
```

---

## 5. IP.BIN — Cabeçalho de Boot

O IP.BIN ocupa os primeiros 16 setores (32 KB) do CD. Ele é lido pelo BIOS do Saturn antes do seu código. Você tem duas opções:

**Opção A — Usar o IP.BIN dos samples do SGL (recomendado para começar):**
O arquivo `sample/sys/ip.bin` que acompanha o SBL/SGL é amplamente redistribuído e funciona sem modificações. Ele configura o hardware básico e salta para `0x06004000`.

**Opção B — IP.BIN mínimo escrito do zero:**
```
Estrutura IP.BIN — System ID (32KB = 0x8000 bytes):
  Offset 0x000–0x00F : Hardware ID        "SEGA SEGASATURN " (16 bytes)
  Offset 0x010–0x01F : Maker ID           (16 bytes)
  Offset 0x020–0x029 : Product Number     (10 bytes)
  Offset 0x02A–0x02F : Version            (6 bytes, ex: "V1.000")
  Offset 0x030–0x037 : Release Date       YYYYMMDD (8 bytes)
  Offset 0x038–0x03F : Device Info        (8 bytes, ex: "CD-1/1  ")
  Offset 0x040–0x049 : Area Symbols       (10 bytes, ex: "JTUE      ")
  Offset 0x04A–0x04F : (padding)          (6 bytes)
  Offset 0x050–0x05F : Peripherals        (16 bytes)
  Offset 0x060–0x0CF : Game Title         (112 bytes ASCII)
  Offset 0x0D0–0x0DF : (reserved)
  Offset 0x0E0–0x0E3 : IP Size            (4 bytes, binary big-endian)
  Offset 0x0E8–0x0EB : Master Stack       (4 bytes, binary BE, ex: 0x060FFFFC)
  Offset 0x0EC–0x0EF : Slave Stack        (4 bytes, binary BE)
  Offset 0x0F0–0x0F3 : 1st Read Address   (4 bytes, binary BE, ex: 0x06004000)
  Offset 0x0F4–0x0F7 : 1st Read Size      (4 bytes, binary BE)
  Offset 0x100–0x7FFF: Security/boot code area
```

Para fins práticos: **use o ip.bin do SBL sample/sys**. Ele é o mesmo em todos os jogos de 1ª party e não contém código proprietário novo.

---

## 6. VDP1 — Acesso Direto (Sem SGL)

VDP1 é orientado a **command list**: você escreve uma lista de comandos de 32 bytes na VRAM do VDP1 e dispara a execução.

### 6.1 Registradores VDP1

```cpp
// hal/vdp1.hpp — acesso direto via ponteiros voláteis
namespace VDP1 {
    inline auto& TVMR = *reinterpret_cast<volatile uint16_t*>(0x05D00000);
    inline auto& FBCR = *reinterpret_cast<volatile uint16_t*>(0x05D00002);
    inline auto& PTMR = *reinterpret_cast<volatile uint16_t*>(0x05D00004);
    inline auto& EWDR = *reinterpret_cast<volatile uint16_t*>(0x05D00006);
    inline auto& EWLR = *reinterpret_cast<volatile uint16_t*>(0x05D00008);
    inline auto& EWRR = *reinterpret_cast<volatile uint16_t*>(0x05D0000A);
    inline auto& EDSR = *reinterpret_cast<volatile uint16_t*>(0x05D0000E); // read

    constexpr uint32_t VRAM_BASE = 0x05C80000;

    void init() {
        TVMR = 0x0000; // 16-bit color, 512px wide, non-rotate
        FBCR = 0x0000; // Manual erase
        PTMR = 0x0002; // Auto-draw on VBLANK
        EWDR = 0x0000; // Erase color = preto
        EWLR = 0x0000;
        EWRR = static_cast<uint16_t>(((320 / 8) << 9) | 224); // Área de erase 320×224
    }
}
```

### 6.2 Estrutura de Comando (32 bytes)

```cpp
struct alignas(4) Vdp1Cmd {
    uint16_t ctrl;   // Tipo do comando + opções
    uint16_t link;   // Próximo comando: endereço >> 3 (0 = sequencial)
    uint16_t pmod;   // Draw mode: color mode, transparência
    uint16_t colr;   // Base da paleta ou cor direta
    uint16_t srca;   // Endereço da textura >> 3 (relativo à VRAM)
    uint16_t size;   // (largura/8 << 8) | altura
    int16_t  xa, ya; // Vértice A (topo-esquerda)
    int16_t  xb, yb; // Vértice B (topo-direita)
    int16_t  xc, yc; // Vértice C (baixo-direita)
    int16_t  xd, yd; // Vértice D (baixo-esquerda)
    uint16_t grda;   // Tabela Gouraud >> 3
    uint16_t _pad;
};
static_assert(sizeof(Vdp1Cmd) == 32);

// ctrl bits:
//   15:14 = tipo: 00=sprite normal, 10=quad distorcido (3D!), 11=polígono cor-sólida
//   13    = fim de lista
//    2    = usar Gouraud shading
// pmod bits:
//    6:4  = modo de cor: 011=256 cores, 100=RGB555
//    2    = transparência (cor 0 = transparente)
```

### 6.3 Textura na VDP1 VRAM

```cpp
// Gerenciador simples de textura (cursor linear)
namespace TexCache {
    static uint32_t cursor = 0x1000; // Primeiros 4KB reservados para cmd list

    // Retorna valor para o campo srca (offset >> 3)
    uint16_t upload(const void* data, uint32_t bytes) {
        cursor = (cursor + 7) & ~7u; // Alinha em 8 bytes
        auto* dst = reinterpret_cast<volatile uint16_t*>(VDP1::VRAM_BASE + cursor);
        auto* src = static_cast<const uint16_t*>(data);
        for (uint32_t i = 0; i < bytes / 2; ++i) dst[i] = src[i];
        uint16_t srca = static_cast<uint16_t>(cursor >> 3);
        cursor += bytes;
        return srca;
    }
}
```

### 6.4 Construindo Quads 3D

```cpp
// Escreve um quad texturizado na lista de comandos
inline void write_quad(Vdp1Cmd* cmd,
                       int16_t xa, int16_t ya,
                       int16_t xb, int16_t yb,
                       int16_t xc, int16_t yc,
                       int16_t xd, int16_t yd,
                       uint16_t srca, uint16_t w8, uint16_t h,
                       uint16_t palette) {
    cmd->ctrl = 0x0004;  // Distorted sprite (quad arbitrário)
    cmd->link = 0;
    cmd->pmod = 0x00C0;  // 256 cores, end-codes on, transparência on
    cmd->colr = palette;
    cmd->srca = srca;
    cmd->size = static_cast<uint16_t>((w8 << 8) | h);
    cmd->xa = xa; cmd->ya = ya;
    cmd->xb = xb; cmd->yb = yb;
    cmd->xc = xc; cmd->yc = yc;
    cmd->xd = xd; cmd->yd = yd;
    cmd->grda = 0;
    cmd->_pad = 0;
}

// Terminar lista de comandos (obrigatório!)
inline void end_list(Vdp1Cmd* cmd) {
    cmd->ctrl = 0x8000; // Bit 15 = End of List
}
```

### 6.5 Upload da Lista e Disparo

```cpp
void frame_submit(const Vdp1Cmd* cmds, uint32_t count) {
    // Copiar lista de WRAM-H para VDP1 VRAM via escrita direta
    // (para N grande, usar SCU DMA — ver seção SCU DMA)
    auto* dst = reinterpret_cast<volatile uint32_t*>(VDP1::VRAM_BASE);
    auto* src = reinterpret_cast<const uint32_t*>(cmds);
    uint32_t dwords = (count * sizeof(Vdp1Cmd)) / 4;
    for (uint32_t i = 0; i < dwords; ++i) dst[i] = src[i];

    // Disparar: trocar framebuffers + apagar + desenhar
    VDP1::FBCR = 0x0003;
    VDP1::PTMR = 0x0001;
}
```

---

## 7. VDP2 — Backgrounds

```cpp
namespace VDP2 {
    // Macro de acesso por índice de registrador (base 0x05F00000)
    template<uint32_t Offset>
    inline auto& REG = *reinterpret_cast<volatile uint16_t*>(0x05F00000 + Offset);

    inline auto& TVMD   = REG<0x00>;
    inline auto& RAMCTL = REG<0x0E>;
    inline auto& BGON   = REG<0x10>;
    inline auto& CHCTLA = REG<0x18>;
    inline auto& PNCN0  = REG<0x20>;
    inline auto& PRISA  = REG<0x98>; // Sprite priority
    inline auto& PRINA  = REG<0xA0>; // NBG0/NBG1 priority

    // 320×224 NTSC, sprites VDP1 em prioridade 6, NBG0 em 1
    void init_320x224_ntsc() {
        TVMD   = 0x8110; // Display on, 320×224, NTSC
        RAMCTL = 0x1F00;
        BGON   = 0x0003; // Sprites (VDP1) + NBG0
        CHCTLA = 0x0002; // NBG0: 256 cores, tiles 8×8
        PRISA  = 0x0006; // Sprites tipo 0 = prioridade 6
        PRINA  = 0x0001; // NBG0 = prioridade 1
    }
}
```

Registradores completos do VDP2 → `references/vdp2_regs.md`

---

## 8. Interrupções via SCU

```cpp
// Vetores de interrupção (tabela BIOS em WRAM-H)
// O BIOS do Saturn lê esses endereços na inicialização
constexpr uint32_t* IVT = reinterpret_cast<uint32_t*>(0x06000300);

#define SCU_IST  (*reinterpret_cast<volatile uint32_t*>(0x05A0001C))
#define SCU_IMS  (*reinterpret_cast<volatile uint32_t*>(0x05A00024))
#define INT_VBLANK_IN  (1u << 0)

static volatile uint32_t g_frame = 0;

// Handler de VBlank — deve ser linkado na seção .text
extern "C" void vblank_in_handler() {
    SCU_IST &= ~INT_VBLANK_IN; // Limpar flag
    ++g_frame;
}

void interrupts_init() {
    IVT[0x40] = reinterpret_cast<uint32_t>(vblank_in_handler); // VBlank-IN
    SCU_IMS &= ~INT_VBLANK_IN; // Desmascarar

    // Habilitar interrupções no SR do SH2
    asm volatile(
        "stc  sr, r0    \n"
        "and  #0x0F, r0 \n" // IPM = 0 (aceitar tudo)
        "ldc  r0, sr    \n"
        ::: "r0"
    );
}

// Esperar próximo VBlank (sincronização de frame)
inline void wait_vblank() {
    uint32_t prev = g_frame;
    while (g_frame == prev);
}
```

---

## 9. Dual SH2 — Slave Paralelo

```cpp
// Ativar Slave SH2 via SMPC
namespace SMPC {
    inline auto& COMREG = *reinterpret_cast<volatile uint8_t*>(0x20100001);
    inline auto& SF     = *reinterpret_cast<volatile uint8_t*>(0x20100063);

    void wait() { while (SF & 0x01); }
    void slave_on()  { wait(); COMREG = 0x02; wait(); }
    void slave_off() { wait(); COMREG = 0x03; wait(); }
}

// Estrutura de job (em WRAM-L para que ambos os SH2 enxerguem via cache-through)
struct alignas(16) SlaveJob {
    void (*func)(void*);
    void* arg;
    volatile int32_t done;
    uint32_t _pad;
};

// Colocar na WRAM-L e acessar via endereço cache-through (|0x20000000)
static SlaveJob _sjob __attribute__((section(".wram_l")));
#define SJOB (*(reinterpret_cast<volatile SlaveJob*>( \
               reinterpret_cast<uint32_t>(&_sjob) | 0x20000000u)))

// Slave SH2 main loop (deve estar no binário — chamado pelo startup do slave)
extern "C" [[noreturn]] void slave_main() {
    // Mascarar interrupções no slave
    asm volatile("ldc %0, sr" :: "r"(0xF0));
    for (;;) {
        if (SJOB.func) {
            auto f = SJOB.func;
            auto a = SJOB.arg;
            SJOB.func = nullptr;
            f(a);
            SJOB.done = 1;
        }
    }
}

// Disparar job no slave e aguardar
void slave_run_sync(void (*f)(void*), void* arg) {
    SJOB.done = 0;
    SJOB.arg  = arg;
    SJOB.func = f; // Slave vê isso e executa
    while (!SJOB.done);
}
```

---

## 10. Matemática Fixed-Point (Nunca Float!)

O SH2 não tem FPU. Qualquer `float` vira chamada de biblioteca de software (~50× mais lento).

```cpp
// math/fixed.hpp
using fx16 = int32_t; // 16.16 fixed-point: 1.0 = 0x00010000
using fx32 = int64_t; // Intermediário para multiplicações

constexpr fx16 FX_ONE = 0x00010000;
constexpr fx16 fx_from_float(float f) { return static_cast<fx16>(f * 65536.0f); }
constexpr fx16 fx_int(int n)          { return n << 16; }
constexpr int  fx_toint(fx16 f)       { return f >> 16; }

// Multiplicação: (a * b) >> 16
inline fx16 fx_mul(fx16 a, fx16 b) {
    return static_cast<fx16>((static_cast<fx32>(a) * b) >> 16);
}

// Divisão usando hardware divider do SH2 (36 ciclos, pipeline!)
namespace SH2Div {
    inline auto& DVSR   = *reinterpret_cast<volatile int32_t*>(0xFFFFFF00);
    inline auto& DVDNT  = *reinterpret_cast<volatile int32_t*>(0xFFFFFF04);
    inline auto& DVDNTH = *reinterpret_cast<volatile int32_t*>(0xFFFFFF10);
}

// Iniciar divisão — leia o resultado 36+ ciclos depois
inline void fx_div_start(fx16 a, fx16 b) {
    SH2Div::DVSR   = b;
    SH2Div::DVDNTH = a >> 16;
    SH2Div::DVDNT  = a << 16;
    // Fazer outros cálculos aqui enquanto HW divide...
}
inline fx16 fx_div_read() { return static_cast<fx16>(SH2Div::DVDNT); }

// Vec3 e Mat4 → references/math_3d.md
```

---

## 11. Pipeline 3D Completo

```
[Model verts, fixed16] ──► [Slave SH2: MVP matrix × vertex]
                                    │
                           [Master: frustum cull]
                                    │
                           [Projeção perspectiva]
                                    │
                           [Back-face culling (CCW 2D cross)]
                                    │
                           [Z-sort: insertion sort por avg_z]
                                    │
                           [Construir Vdp1Cmd[] em WRAM-H]
                                    │
                           [SCU DMA → VDP1 VRAM]
                                    │
                           [Disparar VDP1]
```

```cpp
// Projeção perspectiva (sem float!)
constexpr fx16 FOCAL = fx_from_float(200.f);
constexpr int  CX = 160, CY = 112;

struct Screen { int16_t x, y; };

Screen project(fx16 vx, fx16 vy, fx16 vz) {
    if (vz < fx_int(1)) vz = fx_int(1); // Clip near

    // Iniciar duas divisões em sequência, lendo entre elas para pipeline
    fx_div_start(fx_mul(vx, FOCAL), vz);
    // 36 ciclos de latência — aproveitar para calcular vy*focal antes de ler:
    fx16 ynum = fx_mul(vy, FOCAL);
    fx16 sx = fx_div_read();
    fx_div_start(ynum, vz);
    fx16 sy = fx_div_read();

    return {
        static_cast<int16_t>(fx_toint(sx) + CX),
        static_cast<int16_t>(fx_toint(sy) + CY)
    };
}

// Back-face cull (produto vetorial 2D, descarte horário)
inline bool is_backface(Screen a, Screen b, Screen c) {
    return (int32_t(b.x - a.x) * (c.y - a.y)
          - int32_t(b.y - a.y) * (c.x - a.x)) <= 0;
}
```

---

## 12. SCU DMA — Transferências Sem CPU

```cpp
namespace SCUDMA {
    // Channel 0 (maior prioridade — usar para cmd list → VDP1 VRAM)
    inline auto& D0R  = *reinterpret_cast<volatile uint32_t*>(0x05A00000);
    inline auto& D0W  = *reinterpret_cast<volatile uint32_t*>(0x05A00004);
    inline auto& D0C  = *reinterpret_cast<volatile uint32_t*>(0x05A00008);
    inline auto& D0AD = *reinterpret_cast<volatile uint32_t*>(0x05A0000C);
    inline auto& D0EN = *reinterpret_cast<volatile uint32_t*>(0x05A00010);
    inline auto& D0MD = *reinterpret_cast<volatile uint32_t*>(0x05A00014);

    // Copiar src (WRAM-L!) → destino, sem CPU
    // ATENÇÃO: src DEVE estar em WRAM-L (0x002xxxxx). WRAM-H não é acessível pelo SCU DMA!
    void transfer(uint32_t src_wram_l, uint32_t dst, uint32_t bytes) {
        D0R  = src_wram_l;
        D0W  = dst;
        D0C  = bytes;
        D0AD = 0x00000101; // src +4, dst +4
        D0MD = 0x00000000; // Modo direto
        D0EN = 0x01000001; // Iniciar
        while (D0EN & 0x01000000); // Poll até fim
    }

    // Atalho: cmd list de WRAM-L para VDP1 VRAM
    void cmd_to_vdp1(uint32_t src_wram_l, uint32_t cmd_count) {
        transfer(src_wram_l, 0x05C80000, cmd_count * 32);
    }
}
```

> **Importante:** SCU DMA só acessa WRAM-L (0x002xxxxx). Para DMA de dados em WRAM-H, copie primeiro para WRAM-L, depois dispare o DMA.

---

## 13. SCU DSP — Transforms em Batch

O SCU DSP é assembly-only (VLIW, 6 ops/ciclo). Ideal para multiplicação matricial em batch.

Detalhes completos, instruction set e exemplos → `references/scu_dsp.md`

Uso básico:
```cpp
namespace SCUDSP {
    inline auto& PPAF  = *reinterpret_cast<volatile uint32_t*>(0x05A00000); // DSP ctrl
    // Programa DSP em 0x05FF8000 (1KB)
    // Data RAM CT0-CT3 em 0x05FF8400 (1KB por CT, ring buffer de 64 words)

    void execute_from_pc0() {
        PPAF = 0x00000101; // Executar do PC=0
    }
    void wait_done() {
        while (PPAF & 0x00010000); // Aguarda flag EXEC
    }
}
```

---

## 14. SMPC — Leitura de Controles

```cpp
// Endereços SMPC (acessar como cache-through: | 0x20000000)
#define SMPC_IREG(n)  (*reinterpret_cast<volatile uint8_t*>(0x20100001 + (n)*4))
#define SMPC_OREG(n)  (*reinterpret_cast<volatile uint8_t*>(0x20100021 + (n)*4))
#define SMPC_COMREG   (*reinterpret_cast<volatile uint8_t*>(0x20100001))
#define SMPC_SF       (*reinterpret_cast<volatile uint8_t*>(0x20100063))

enum PadBtn : uint16_t {
    BTN_A = 1<<2, BTN_B = 1<<4, BTN_C = 1<<5,
    BTN_X = 1<<6, BTN_Y = 1<<7, BTN_Z = 1<<8,
    BTN_UP = 1<<12, BTN_DOWN = 1<<13, BTN_LEFT = 1<<14, BTN_RIGHT = 1<<15,
    BTN_START = 1<<3, BTN_L = 1<<9, BTN_R = 1<<10
};

struct PadState { uint16_t held, pressed, released; };

// SMPC retorna dados em OREG após comando GETPERIPHERAL
// Execução deve ocorrer durante VBlank (16ms de janela)
uint16_t pad_read_raw() {
    while (SMPC_SF & 0x01);        // Aguardar SMPC livre
    SMPC_COMREG = 0x08;            // Comando: INTBACK (poll periféricos)
    while (SMPC_SF & 0x01);        // Aguardar conclusão
    // OREG[0] e OREG[1] contêm os dados do pad 1 em formato digital
    uint8_t hi = SMPC_OREG(0);
    uint8_t lo = SMPC_OREG(1);
    return static_cast<uint16_t>((hi << 8) | lo) ^ 0xFFFF; // Inverter: 1=pressionado
}
```

---

## 15. Áudio — Comunicação SH2 ↔ M68k

```cpp
// Área de comunicação em Sound RAM (acessível por ambos)
#define SCOMM_BASE 0x05A01000

namespace Audio {
    inline auto& CMD  = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x00);
    inline auto& CH   = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x02);
    inline auto& ADRL = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x04);
    inline auto& ADRH = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x06);
    inline auto& LENL = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x08);
    inline auto& LENH = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x0A);
    inline auto& FREQ = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x0C);

    // Aguardar M68k processar comando anterior
    void wait() { while (CMD != 0); }

    void play_pcm(uint8_t ch, uint32_t addr, uint32_t len, uint16_t freq) {
        wait();
        CH   = ch;
        ADRL = static_cast<uint16_t>(addr & 0xFFFF);
        ADRH = static_cast<uint16_t>(addr >> 16);
        LENL = static_cast<uint16_t>(len & 0xFFFF);
        LENH = static_cast<uint16_t>(len >> 16);
        FREQ = freq;
        CMD  = 1; // M68k executa quando vê CMD != 0
    }

    void stop(uint8_t ch) {
        wait();
        CH  = ch;
        CMD = 3;
    }
}
```

Driver M68k e mapa de registradores SCSP → `references/scsp_audio.md`

---

## 16. Estrutura de Projeto Recomendada

```
meu-jogo/
├── build-toolchain.sh       ← Compilar sh2eb-elf-gcc do zero
├── Makefile
├── crt0.s                   ← Startup assembly
├── saturn.ld                ← Linker script
├── ip.bin                   ← IP.BIN (do SBL sample/sys)
├── hal/
│   ├── vdp1.hpp             ← Acesso direto ao VDP1
│   ├── vdp2.hpp             ← Acesso direto ao VDP2
│   ├── scu.hpp              ← SCU DMA + interrupções
│   ├── scu_dsp.hpp/.s       ← SCU DSP programs
│   ├── sh2_slave.hpp        ← Slave SH2 job dispatcher
│   ├── smpc.hpp             ← Controles + system control
│   └── audio.hpp            ← Interface áudio SH2-side
├── math/
│   ├── fixed.hpp            ← fixed16: mul, div, sincos
│   ├── vec3.hpp             ← Vec3 operations
│   └── mat4.hpp             ← Mat4 × Vec3 (usa MAC.L do SH2)
├── gfx/
│   ├── render3d.hpp/.cpp    ← Pipeline 3D completo
│   ├── zsort.hpp            ← Painter's algorithm
│   ├── tex_cache.hpp        ← Upload textura → VDP1 VRAM
│   └── sprite2d.hpp         ← Sprites 2D simples
├── audio/
│   ├── m68k_driver.s        ← Driver M68k assembly (68000)
│   └── audio_api.hpp        ← API SH2-side
├── src/
│   └── main.cpp             ← Ponto de entrada do jogo
└── tools/
    ├── make_iso.sh          ← mkisofs wrapper
    └── palette_conv.py      ← Converter imagens → formato Saturn
```

---

## 17. Pitfalls Críticos

| Erro | Causa | Solução |
|------|-------|---------|
| Dados stale entre SH2s | Cache coherency | Usar endereços `|0x20000000` para shared data |
| Textura corrompida | Largura não múltipla de 8 | VDP1 exige: width % 8 == 0 |
| SCU DMA não funciona | Fonte em WRAM-H | SCU DMA só lê WRAM-L (0x002xxxxx) |
| Performance péssima | Uso de float | Nunca usar `float`; usar `fx16` (fixed16) |
| Crash no slave SH2 | Bus contention | Usar cache-through e evitar WRAM-L simultâneo |
| VDP1 não desenha | Lista de cmds sem End | Sempre terminar com ctrl = 0x8000 |
| Big-endian invertido | Cross-compilando de x86 | Todos os shorts/longs em big-endian no binário |
| IP.BIN não salta | Entry point errado | Confirmar que 0x06004000 está no linker script |

---

## 18. Emuladores para Teste

```bash
# Mednafen — mais preciso para timing exato
mednafen -ss.bios_path bios.bin game.cue

# Kronos — melhor para debug (baseado em Yabause, mais ativo)
# https://github.com/FCare/Kronos

# Yabause — alternativa mais simples
yabause -b bios.bin -i game.iso
```

Build de imagem:
```bash
# Gerar ISO bootável (requer mkisofs / genisoimage)
mkisofs \
  -sysid "SEGA SATURN" \
  -volid "MYJOGO" \
  -publisher "STUDIO" \
  -l -iso-level 1 \
  -o game.iso \
  -x game.iso \
  ip.bin \
  iso_root/

# ip.bin deve ser o PRIMEIRO arquivo (IP.BIN = Track1 setores 0-15)
# Binário do jogo deve estar como primeiro arquivo na iso_root/
```

---

## 19. Arquivos de Referência

Ler quando precisar de detalhes de um subsistema:

- `references/memory_map.md` — Mapa completo de memória com velocidades de bus
- `references/scu_dsp.md` — Instruction set SCU DSP, regras VLIW, exemplos
- `references/vdp1_cmds.md` — Todos os campos da command table VDP1
- `references/vdp2_regs.md` — Mapa de registradores VDP2, planos de scroll, RBG0
- `references/math_3d.md` — Vetores, matrizes 4×4, seno/cosseno, MAC.L inline asm
- `references/scsp_audio.md` — SCSP registers, driver M68k assembly, API SH2
