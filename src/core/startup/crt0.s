/* ===========================================================================
 * crt0.s - Sega Saturn Boot Entry Point
 * ===========================================================================
 *
 * Saturn boot flow:
 *   1. BIOS reads IP.BIN from CD (sector 0)
 *   2. BIOS loads 0.BIN (our executable) at 0x06004000
 *   3. BIOS jumps to _start (entry point defined in linker)
 *   4. _start: disables IRQs, configures stack, clears BSS
 *   5. Calls _main() (user's main function)
 *   6. Application calls sat_init(), which owns hardware initialization
 *
 * Difference vs libyaul:
 *   - libyaul: _start -> ___sys_init() -> main()
 *   - libsaturn: _start -> _main() -> sat_init()
 *   - JoEngine: uses -nostartfiles, main() directly as entry point
 *
 * Memory addresses:
 *   - Work RAM High (WRAMH): 0x06000000 - 0x060FFFFF (1MB, code + data)
 *   - Work RAM Low (WRAML): 0x00200000 - 0x002FFFFF (1MB, data)
 *   - Stack: top at 0x060FFFFC (end of WRAMH)
 * =========================================================================== */

    .section .text.start
    .global _start
    .align 4

_start:
    /* Undefined weak symbol means tracing is disabled. The record lives in
     * initialized application data, survives BSS clear and owns no BIOS RAM. */
    mov.l   .Lboot_trace, r0
    tst     r0, r0
    bt      .Lno_entry_trace
    mov.l   .Lcache_through, r1
    or      r1, r0
    mov     #2, r1
    mov.l   r1, @(12,r0)
    mov.l   @(20,r0), r1
    mov     #4, r2
    or      r2, r1
    mov.l   r1, @(20,r0)
    stc     sr, r1
    mov.l   r1, @(28,r0)
    stc     vbr, r1
    mov.l   r1, @(32,r0)
    mov.l   r15, @(36,r0)
.Lno_entry_trace:
    /* -------------------------------------------------------------------
     * 1. Disable interrupts and configure CPU mode
     * -------------------------------------------------------------------
     * SR (Status Register) interrupt mask bits:
     *   I3-I0 (bits 4-7) = IRQ mask level
     *   0xF0 = mask all IRQs (most restrictive mode)
     *   Other SR bits are preserved.
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

    /* Make the Master CPU context explicit.  The Slave later receives its
     * own VBR in slave_entry.s; neither CPU relies on the other's state. */
    mov.l   .Lmaster_vbr, r0
    ldc     r0, vbr

    /* -------------------------------------------------------------------
     * 3. Zero BSS section (uninitialized global variables)
     * -------------------------------------------------------------------
     * __bss_start and __bss_end defined by linker (saturn.ld)
     * ------------------------------------------------------------------- */
    xor     r2, r2              /* r2 = 0 (value to zero with) */
    mov.l   .Lbss_start, r0     /* r0 = BSS start address */
    mov.l   .Lbss_end, r1       /* r1 = BSS end address */
    cmp/eq  r0, r1              /* BSS empty? */
    bt      .Lwram_l_clear      /* Yes -> still clear .wram_l */
.Lloop:
    mov.l   r2, @r0             /* Write 0 at current position */
    add     #4, r0              /* Advance 4 bytes */
    cmp/hs  r1, r0              /* Reached end? */
    bf      .Lloop              /* No -> continue */

    /* -------------------------------------------------------------------
     * 3b. Zero .wram_l the same way
     * -------------------------------------------------------------------
     * .wram_l holds the big buffers that do not fit in WRAMH. It is a
     * NOLOAD section, so without this loop anything placed there starts
     * on whatever the previous program left in Work RAM Low -- a global
     * that is zero-initialised in C would silently not be. Clearing it
     * makes the section a drop-in for .bss, which is the only reason a
     * library runtime may be moved there.
     * ------------------------------------------------------------------- */
.Lwram_l_clear:
    mov.l   .Lwram_l_start, r0  /* r0 = .wram_l start address */
    mov.l   .Lwram_l_end, r1    /* r1 = .wram_l end address */
    cmp/eq  r0, r1              /* Section empty? */
    bt      .Lmain_call         /* Yes -> skip zeroing */
.Lwram_l_loop:
    mov.l   r2, @r0             /* Write 0 at current position */
    add     #4, r0              /* Advance 4 bytes */
    cmp/hs  r1, r0              /* Reached end? */
    bf      .Lwram_l_loop       /* No -> continue */

    /* -------------------------------------------------------------------
     * 4. Call main()
     * -------------------------------------------------------------------
     * early_init is called inside sat_init(), not here.
     * This allows user's main() to execute first if desired.
     * ------------------------------------------------------------------- */
.Lmain_call:
    mov.l   .Lboot_trace, r0
    tst     r0, r0
    bt      .Lno_ready_trace
    mov.l   .Lcache_through, r1
    or      r1, r0
    mov     #3, r1
    mov.l   r1, @(12,r0)
    mov.l   r1, @(16,r0)
    mov.l   @(20,r0), r1
    mov     #8, r2
    or      r2, r1
    mov.l   r1, @(20,r0)
.Lno_ready_trace:
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
.Lmaster_vbr: .long 0x06000000  /* BIOS-managed Master vector table */
.Lbss_start: .long __bss_start   /* BSS section start (linker symbol) */
.Lbss_end:   .long __bss_end     /* BSS section end (linker symbol) */
.Lwram_l_start: .long __wram_l_start /* .wram_l start (linker symbol) */
.Lwram_l_end:   .long __wram_l_end   /* .wram_l end (linker symbol) */
.Lmain:      .long _main         /* Address of main() function */
    .weak _sat_boot_trace_record
.Lboot_trace: .long _sat_boot_trace_record
.Lcache_through: .long 0x20000000
