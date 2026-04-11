/* ===========================================================================
 * saturn.ld - Linker Script para Sega Saturn
 * ===========================================================================
 *
 * Layout de memória do Saturn:
 *
 * +------------------+----------------+--------+---------------------------+
 * | Região           | Endereço       | Tamanho| Uso                       |
 * +------------------+----------------+--------+---------------------------+
 * | Work RAM High    | 0x06000000     | 1 MB   | Código + dados (primário) |
 * | (WRAMH)          |                |        |                           |
 * +------------------+----------------+--------+---------------------------+
 * | Work RAM Low     | 0x00200000     | 512 KB | Dados (secundário)        |
 * | (WRAML)          |                |        |                           |
 * +------------------+----------------+--------+---------------------------+
 * | VDP1 VRAM        | 0x25C00000     | 512 KB | Sprites/polígonos         |
 * | VDP2 VRAM        | 0x25E00000     | 512 KB | Backgrounds               |
 * | VDP2 CRAM        | 0x25F00000     | 64 KB  | Paleta de cores           |
 * | VDP2 regs        | 0x25F80000     | -      | Registros de controle     |
 * | VDP1 regs        | 0x25D00000     | -      | Registros de controle     |
 * +------------------+----------------+--------+---------------------------+
 *
 * Endereço de carga do executável:
 *   - IP.BIN header offset 0xF0: 0x06004000 (primeiro endereço a carregar)
 *   - O BIOS carrega 0.BIN a partir deste endereço
 *   - _start (entry point) deve estar em 0x06004000
 *
 * Por que 0x06004000 e não 0x06000000?
 *   - Primeiros 16KB (0x06000000-0x06003FFF) reservados para:
 *     - Vetores de interrupção
 *     - Área de comunicação com slave CPU
 *     - Stack do slave CPU
 *   - Código começa em 0x06004000 para não sobrescrever estas áreas
 * =========================================================================== */

OUTPUT_FORMAT("elf32-sh", "elf32-sh", "elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)  /* Entry point: símbolo _start em crt0.s */

MEMORY {
    /* Work RAM High: código + dados principais
     * Origem: 0x06004000 (após área reservada de 16KB)
     * Tamanho: 0xFC000 = 1MB - 16KB = 1.032.192 bytes */
    WRAMH (rwx) : ORIGIN = 0x06004000, LENGTH = 0x000FC000

    /* Work RAM Low: dados secundários
     * Usada para buffers grandes que não cabem em WRAMH
     * Origem: 0x00200000
     * Tamanho: 0x100000 = 512 KB */
    WRAML (rw)  : ORIGIN = 0x00200000, LENGTH = 0x00100000
}

SECTIONS {
    /* =========================================================================
     * Seção .text: código executável e dados read-only
     * =========================================================================
     * Colocada em 0x06004000 (início da WRAMH utilizável)
     * Ordem de linkagem (crítica para boot):
     *   1. .text.start: contém _start (crt0.s) - DEVE ser primeiro!
     *   2. .text: resto do código
     *   3. .rodata: strings e constantes
     * ========================================================================= */
    .text 0x06004000 : {
        *(.text.start)       /* crt0.s: entry point _start */
        *(.text .text.*)     /* Todo o resto do código */
        *(.rodata .rodata.*) /* Strings, constantes, vtables C++ */
        . = ALIGN(4);        /* Alinhar a 4 bytes */
    } > WRAMH

    /* =========================================================================
     * Seção .data: variáveis globais inicializadas
     * =========================================================================
     * Na ELF, dados inicializados ficam no arquivo binário.
     * No runtime, o crt0 deveria copiar de ROM para RAM, mas como
     * estamos rodando direto da RAM (0.BIN carregado pelo BIOS),
     * os dados já estão no lugar correto.
     * ========================================================================= */
    .data : {
        __data_start = .;    /* Símbolo para cálculo de tamanho */
        *(.data .data.*)     /* Variáveis inicializadas */
        . = ALIGN(4);
        __data_end = .;
    } > WRAMH

    /* =========================================================================
     * Seção .bss: variáveis globais NÃO inicializadas (zeradas no boot)
     * =========================================================================
     * NOLOAD = não ocupa espaço no binário final.
     * O crt0.s itera de __bss_start a __bss_end escrevendo zeros.
     * ========================================================================= */
    .bss (NOLOAD) : {
        __bss_start = .;     /* Usado pelo crt0.s para zerar BSS */
        *(.bss .bss.*)       /* Variáveis não inicializadas */
        *(COMMON)            /* Variáveis COMMON (estilo C antigo) */
        . = ALIGN(4);
        __bss_end = .;       /* Usado pelo crt0.s para zerar BSS */
    } > WRAMH

    /* =========================================================================
     * Seção .wram_l: dados na Work RAM Low
     * =========================================================================
     * Para buffers grandes (áudio, texturas, etc) que não cabem em WRAMH.
     * Usada com atributo __attribute__((section(".wram_l"))) no código.
     * ========================================================================= */
    .wram_l (NOLOAD) : {
        *(.wram_l .wram_l.*) /* Dados explicitamente colocados em WRAML */
        . = ALIGN(4);
    } > WRAML

    /* =========================================================================
     * Seções descartadas (não vão para o binário final)
     * ========================================================================= */
    /DISCARD/ : {
        *(.comment)          /* Comentários do compilador (GCC version) */
        *(.note*)            /* Notas ELF (build-id, etc) */
        *(.eh_frame*)        /* C++ exception handling (não usamos) */
    }
}
