OUTPUT_FORMAT("elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)

MEMORY {
    hram (rwx) : ORIGIN = 0x06000000, LENGTH = 0x00040000
    lram (rwx) : ORIGIN = 0x06004000, LENGTH = 0x000FC000
}

SECTIONS {
    .text : {
        *(.text)
        *(.rodata)
        _etext = .;
    } > lram

    .slave_text : {
        . = ALIGN(16);
        _slave_start = .;
        *(.slave_code)
        _slave_end = .;
    } > hram

    .data : {
        *(.data)
        _edata = .;
    } > lram

    .bss : {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        _bss_end = .;
    } > lram

    _master_stack = ORIGIN(lram) + LENGTH(lram);
    _slave_stack  = ORIGIN(hram) + LENGTH(hram);
}
