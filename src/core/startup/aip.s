! Independently authored LibSaturn AIP. SEGA ST-040, boot system section 4.
! BIOS/security code has completed the first-read transfer before this entry.
! No CD commands, vector changes or display reset. crt0 owns CPU setup.
    .section .text.aip
    .global _libsaturn_aip
    .balign 4
_libsaturn_aip:
    ! Optional application-owned trace address, resolved from the linked ELF.
    ! Zero is the normal application default. This never touches BIOS RAM.
    mov.l .Ltrace, r1
    tst r1, r1
    bt .Lhandoff
    mov #1, r0
    mov.l r0, @(12,r1)
    mov #2, r0
    mov.l r0, @(20,r1)
.Lhandoff:
    mov.l .Lapplication, r0
    jmp @r0
    nop
    .balign 4
.Ltrace:
    .ifndef AIP_TRACE_ADDRESS
    .set AIP_TRACE_ADDRESS, 0
    .endif
    .long AIP_TRACE_ADDRESS
.Lapplication:
    .long 0x06004000
