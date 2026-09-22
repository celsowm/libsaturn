    .section .text.slave_entry
    .global _saturn_slave_entry
    .align 2

/* Entry installed in the BIOS-managed Slave vector 0x94.  This is deliberately
 * not crt0: the Slave must not clear the application's BSS or call main(). */
_saturn_slave_entry:
    stc     sr, r0
    mov.l   .Lsr_mask, r1
    or      r1, r0
    ldc     r0, sr

    mov.l   .Lslave_stack, r15
    mov.l   .Lslave_vbr, r0
    ldc     r0, vbr

    mov.l   .Lslave_init, r0
    jsr     @r0
    nop

.Lhalt:
    bra     .Lhalt
    nop

    .align 4
.Lsr_mask:    .long 0x000000F0
.Lslave_stack: .long 0x06001000
.Lslave_vbr:  .long 0x06000400
.Lslave_init: .long _saturn_slave_init
