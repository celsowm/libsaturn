    .section .text.start
    .global _start
    .align 2

_start:
    mov.l   sr_val, r0
    ldc     r0, sr

    mov.l   stack_top, r15

    mov.l   data_lma, r0
    mov.l   data_vma, r1
    mov.l   data_end, r2
    cmp/eq  r1, r2
    bt      bss_zero
copy_data:
    mov.l   @r0+, r3
    mov.l   r3, @r1
    add     #4, r1
    cmp/hs  r2, r1
    bf      copy_data

bss_zero:
    mov.l   bss_start, r0
    mov.l   bss_end, r1
    xor     r2, r2
    cmp/eq  r0, r1
    bt      call_main
zero_loop:
    mov.l   r2, @r0
    add     #4, r0
    cmp/hs  r1, r0
    bf      zero_loop

call_main:
    mov.l   main_addr, r0
    jsr     @r0
    nop

hang:
    bra     hang
    nop

    .align 4
sr_val:     .long 0x000000F0
stack_top:  .long 0x060FFFFC
data_lma:   .long __data_load
data_vma:   .long __data_start
data_end:   .long __data_end
bss_start:  .long __bss_start
bss_end:    .long __bss_end
main_addr:  .long _main

