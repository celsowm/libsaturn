    .section .text.start
    .global _start
    .align 2

_start:
    mov.l   sr_val, r0
    ldc     r0, sr

    mov.l   stack_top, r15

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
bss_start:  .long __bss_start
bss_end:    .long __bss_end
main_addr:  .long _main
