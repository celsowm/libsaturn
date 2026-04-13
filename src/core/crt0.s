/* ===========================================================================
 * crt0.s - Sega Saturn Boot Entry Point
 * ===========================================================================
 *
 * Saturn boot flow:
 *   1. BIOS reads IP.BIN from CD (sector 0)
 *   2. BIOS loads 0.BIN (our executable) at 0x06004000
 *   3. BIOS jumps to _start (entry point defined in linker)
 *   4. _start: disables IRQs, configures stack, clears BSS
 *   5. Calls _saturn_early_init() to initialize hardware
 *   6. Calls _main() (user's main function)
 *
 * Difference vs libyaul:
 *   - libyaul: _start -> ___sys_init() -> main()
 *   - libsaturn: _start -> _saturn_early_init() -> _main()
 *   - JoEngine: uses -nostartfiles, main() directly as entry point
 *
 * Memory addresses:
 *   - Work RAM High (WRAMH): 0x06000000 - 0x060FFFFF (1MB, code + data)
 *   - Work RAM Low (WRAML): 0x00200000 - 0x002FFFFF (512KB, data)
 *   - Stack: top at 0x060FFFFC (end of WRAMH)
 * =========================================================================== */

    .section .text.start
    .global _start
    .align 4

_start:
    /* -------------------------------------------------------------------
     * 1. Disable interrupts and configure CPU mode
     * -------------------------------------------------------------------
     * SR (Status Register) interrupt mask bits:
     *   I3-I0 (bits 4-7) = IRQ mask level
     *   0xF0 = mask all IRQs (most restrictive mode)
     *   BL (bit 0) = branch delay slot
     * ------------------------------------------------------------------- */
    stc     sr, r0
    mov.l   .Lsr_mask, r1
    or      r1, r0
    ldc     r0, sr

    /* -------------------------------------------------------------------
     * 2. Configure stack pointer (r15)
     * -------------------------------------------------------------------
     * Stack grows downward on SH2.
     * Top = 0x060FFFFC (last aligned address of WRAMH)
     * ------------------------------------------------------------------- */
    mov.l   .Lstack_top, r15

    /* -------------------------------------------------------------------
     * 3. Zero BSS section (uninitialized global variables)
     * -------------------------------------------------------------------
     * __bss_start and __bss_end defined by linker (saturn.ld)
     * ------------------------------------------------------------------- */
    xor     r2, r2              /* r2 = 0 (value to zero with) */
    mov.l   .Lbss_start, r0     /* r0 = BSS start address */
    mov.l   .Lbss_end, r1       /* r1 = BSS end address */
    cmp/eq  r0, r1              /* BSS empty? */
    bt      .Lmain_call         /* Yes -> skip zeroing */
.Lloop:
    mov.l   r2, @r0             /* Write 0 at current position */
    add     #4, r0              /* Advance 4 bytes */
    cmp/hs  r1, r0              /* Reached end? */
    bf      .Lloop              /* No -> continue */

    /* -------------------------------------------------------------------
     * 4. Call main()
     * -------------------------------------------------------------------
     * early_init is called inside sat_init(), not here.
     * This allows user's main() to execute first if desired.
     * ------------------------------------------------------------------- */
.Lmain_call:
    mov.l   .Lmain, r0
    jsr     @r0
    nop

    /* Infinite loop if main() returns (shouldn't happen) */
hang:
    bra     hang
    nop

    /* -------------------------------------------------------------------
     * Literal table (accessed via PC-relative mov.l)
     * ------------------------------------------------------------------- */
    .align 4
.Lsr_mask:   .long 0x000000F0   /* Mask: disable all IRQs */
.Lstack_top: .long 0x060FFFFC   /* Stack top (end of WRAMH) */
.Lbss_start: .long __bss_start   /* BSS section start (linker symbol) */
.Lbss_end:   .long __bss_end     /* BSS section end (linker symbol) */
.Lmain:      .long _main         /* Address of main() function */
