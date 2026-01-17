.section .text
.global _start
.global _slave_entry
.global _main
.global _slave_main

_start:
    mov.l _master_stack_ptr, r15
    
    mov.l _bss_start, r0
    mov.l _bss_end, r1
    mov #0, r2
clear_bss:
    mov.l r2, @r0
    add #4, r0
    cmp/ge r1, r0
    bf clear_bss
    
    mov.l _slave_start_ptr, r0
    mov.l r0, @r0
    
    mov.l _main_ptr, r0
    jsr @r0
    nop
    bra _start
    nop

_slave_entry:
    mov.l _slave_stack_ptr, r15
    mov.l _slave_main_ptr, r0
    jsr @r0
    nop
    bra _slave_entry
    nop

.align 4
_master_stack_ptr: .long _master_stack
_slave_stack_ptr:  .long _slave_stack
_main_ptr:         .long _main
_slave_main_ptr:   .long _slave_main
_bss_start:        .long _bss_start
_bss_end:          .long _bss_end
_slave_start_ptr:  .long _slave_start
