OUTPUT_FORMAT("elf32-sh", "elf32-sh", "elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)

MEMORY {
    WRAMH (rwx) : ORIGIN = 0x06004000, LENGTH = 0x000FC000
    WRAML (rw)  : ORIGIN = 0x00200000, LENGTH = 0x00100000
}

SECTIONS {
    .text 0x06004000 : {
        *(.text.start)
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > WRAMH

    .data : {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > WRAMH

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > WRAMH

    .wram_l (NOLOAD) : {
        *(.wram_l .wram_l.*)
        . = ALIGN(4);
    } > WRAML

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
