! Minimal IP.BIN boot stub for Saturn (emulator-compatible)
! Embedded in the IP executable region and copied by BIOS to the
! IP load address configured in the IP header.
!
! The Saturn BIOS handles:
!   1. Reading the System ID (0x000-0x0FF)
!   2. Loading the 1st-read file (0.BIN) into the address at header +0xF0
!   3. Jumping to the IP boot code
!
! This stub sets up the master SH2, initializes minimal hardware,
! and jumps to the loaded program at 0x06010000.

    .section .text
    .global _ip_stub_start
    .align 2

_ip_stub_start:
    ! Disable interrupts
    mov.l   _sr_val, r0
    ldc     r0, sr

    ! Set up master stack
    mov.l   _stack_val, r15

    ! Clear VDP2 display (write 0x0000 to TVMD register to blank screen)
    mov.l   _vdp2_tvmd, r1
    mov     #0, r0
    mov.w   r0, @r1

    ! Small delay to let hardware settle
    mov.w   _delay_count, r2
.delay_loop:
    dt      r2
    bf      .delay_loop

    ! Jump to loaded 0.BIN at 1st read address
    mov.l   _app_entry, r0
    jmp     @r0
    nop

    .align 4
_sr_val:        .long 0x000000F0
_stack_val:     .long 0x060FFFFC
_vdp2_tvmd:     .long 0x05F80000
_app_entry:     .long 0x06010000
_delay_count:   .word 0x1000
