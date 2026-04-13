/* ===========================================================================
 * saturn.ld - Linker Script for Sega Saturn
 * ===========================================================================
 *
 * Saturn memory layout:
 *
 * +------------------+----------------+--------+---------------------------+
 * | Region           | Address        | Size   | Use                       |
 * +------------------+----------------+--------+---------------------------+
 * | Work RAM High    | 0x06000000     | 1 MB   | Code + data (primary)     |
 * | (WRAMH)          |                |        |                           |
 * +------------------+----------------+--------+---------------------------+
 * | Work RAM Low     | 0x00200000     | 512 KB | Data (secondary)          |
 * | (WRAML)          |                |        |                           |
 * +------------------+----------------+--------+---------------------------+
 * | VDP1 VRAM        | 0x25C00000     | 512 KB | Sprites/polygons         |
 * | VDP2 VRAM        | 0x25E00000     | 512 KB | Backgrounds              |
 * | VDP2 CRAM        | 0x25F00000     | 64 KB  | Color palette            |
 * | VDP2 regs        | 0x25F80000     | -      | Control registers        |
 * | VDP1 regs        | 0x25D00000     | -      | Control registers        |
 * +------------------+----------------+--------+---------------------------+
 *
 * Executable load address:
 *   - IP.BIN header offset 0xF0: 0x06004000 (first address to load)
 *   - BIOS loads 0.BIN from this address
 *   - _start (entry point) must be at 0x06004000
 *
 * Why 0x06004000 instead of 0x06000000?
 *   - First 16KB (0x06000000-0x06003FFF) reserved for:
 *     - Interrupt vectors
 *     - Slave CPU communication area
 *     - Slave CPU stack
 *   - Code starts at 0x06004000 to avoid overwriting these areas
 * =========================================================================== */

OUTPUT_FORMAT("elf32-sh", "elf32-sh", "elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)  /* Entry point: _start symbol in crt0.s */

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

MEMORY {
    /* Work RAM High: code + main data
     * Origin: 0x06004000 (after 16KB reserved area)
     * Size: 0xFC000 = 1MB - 16KB = 1,032,192 bytes */
    WRAMH (rwx) : ORIGIN = 0x06004000, LENGTH = 0x000FC000

    /* Work RAM Low: secondary data
     * Used for large buffers that don't fit in WRAMH
     * Origin: 0x00200000
     * Size: 0x100000 = 512 KB */
    WRAML (rw)  : ORIGIN = 0x00200000, LENGTH = 0x00100000
}

SECTIONS {
    /* =========================================================================
     * Section .text: executable code and read-only data
     * =========================================================================
     * Placed at 0x06004000 (start of usable WRAMH)
     * Link order (critical for boot):
     *   1. .text.start: contains _start (crt0.s) - MUST be first!
     *   2. .text: rest of code
     *   3. .rodata: strings and constants
     * ========================================================================= */
    .text 0x06004000 : {
        *(.text.start)       /* crt0.s: entry point _start */
        *(.text .text.*)     /* All other code */
        *(.rodata .rodata.*) /* Strings, constants, C++ vtables */
        . = ALIGN(4);        /* Align to 4 bytes */
    } > WRAMH :text

    /* =========================================================================
     * Section .data: initialized global variables
     * =========================================================================
     * In ELF, initialized data is in the binary file.
     * At runtime, crt0 should copy from ROM to RAM, but since
     * we're running directly from RAM (0.BIN loaded by BIOS),
     * the data is already in the right place.
     * ========================================================================= */
    .data : {
        __data_start = .;    /* Symbol for size calculation */
        *(.data .data.*)     /* Initialized variables */
        . = ALIGN(4);
        __data_end = .;
    } > WRAMH :data

    /* =========================================================================
     * Section .bss: uninitialized global variables (zeroed at boot)
     * =========================================================================
     * NOLOAD = does not occupy space in final binary.
     * crt0.s iterates from __bss_start to __bss_end writing zeros.
     * ========================================================================= */
    .bss (NOLOAD) : {
        __bss_start = .;     /* Used by crt0.s to zero BSS */
        *(.bss .bss.*)       /* Uninitialized variables */
        *(COMMON)            /* COMMON variables (old C style) */
        . = ALIGN(4);
        __bss_end = .;       /* Used by crt0.s to zero BSS */
    } > WRAMH :data

    /* =========================================================================
     * Section .wram_l: data in Work RAM Low
     * =========================================================================
     * For large buffers (audio, textures, etc) that don't fit in WRAMH.
     * Used with __attribute__((section(".wram_l"))) attribute in code.
     * ========================================================================= */
    .wram_l (NOLOAD) : {
        *(.wram_l .wram_l.*) /* Data explicitly placed in WRAML */
        . = ALIGN(4);
    } > WRAML :data

    /* =========================================================================
     * Discarded sections (don't go to final binary)
     * ========================================================================= */
    /DISCARD/ : {
        *(.comment)          /* Compiler comments (GCC version) */
        *(.note*)            /* ELF notes (build-id, etc) */
        *(.eh_frame*)        /* C++ exception handling (not used) */
    }
}
