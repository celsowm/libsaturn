/* ===========================================================================
 * early_init.c - Saturn Hardware Early Initialization
 * ===========================================================================
 *
 * This function is called by sat_init() BEFORE any engine-specific
 * configuration. It prepares the hardware to a known state.
 *
 * Why is it needed:
 *   - Saturn's BIOS does NOT initialize VDP1/VDP2 completely
 *   - Without this init, hardware may be in undefined state
 *   - libyaul does this in ___sys_init(), JoEngine in jo_core_init()
 *
 * Hardware addresses (all via uncached region 0x20000000):
 *   - VDP2 regs:    0x25F80000 (0x20000000 | 0x05F80000)
 *   - VDP1 regs:    0x25D00000 (0x20000000 | 0x05D00000)
 *   - VDP1 VRAM:    0x25C00000 (0x20000000 | 0x05C00000)
 *   - VDP2 VRAM:    0x25E00000 (0x20000000 | 0x05E00000)
 *   - SCU regs:     0x20C00000
 *   - SMPC regs:    0x20100000
 *
 * Why use uncached addresses (0x2x...):
 *   - Cached access (0x0x...) can cause problems with hardware
 *   - Uncached region ensures writes reach immediately
 *   - VDP1/VDP2 registers need uncached access to work properly
 *
 * Note on array indices:
 *   Pointers are `unsigned short*` (16 bits), so each index = 2 bytes.
 *   Example: vdp2[7] = offset 0x0E (14 bytes = index 7 * 2)
 * =========================================================================== */

__attribute__((used))
void _saturn_early_init(void) {
    /* Pointers to hardware registers (uncached region) */
    volatile unsigned short* const vdp2 = (volatile unsigned short*)0x25F80000u;
    volatile unsigned short* const vdp1 = (volatile unsigned short*)0x25D00000u;
    volatile unsigned int* const vdp1_vram = (volatile unsigned int*)0x25C00000u;
    volatile unsigned short* const scu = (volatile unsigned short*)0x20C00000u;
    int i;

    /* =========================================================================
     * 1. SCU (System Control Unit) - Interrupt and DMA controller
     * =========================================================================
     * Offset 0x00 (index 0): ICU - Interrupt Control Unit
     * Offset 0x04 (index 2): IMR - Interrupt Mask Register
     * ========================================================================= */
    scu[0] = 0;   /* ICU: clear pending interrupt requests */
    scu[2] = 0;   /* IMR: mask all interrupts */

    /* =========================================================================
     * 2. VDP2 (Video Display Processor 2) - Backgrounds and scroll
     * =========================================================================
     * Step 1: Disable display to avoid glitches during config
     * ========================================================================= */
    vdp2[0] = 0x0000;   /* TVMD (offset 0x00): Display OFF
                         *   Bit 15 (DISP) = 0: disable video output
                         *   Bit 8 (BDCLMD) = 0: backdrop color mode */

    /* Small delay to ensure VDP2 has processed the command */
    for (i = 0; i < 1000; i++) { }

    /* =========================================================================
     * Step 2: Configure VRAM and bus cycle
     * ========================================================================= */
    vdp2[7] = 0x1100;   /* RAMCTL (offset 0x0E, index 7):
                         *   Bits 15-14: VRAM mode (01 = 512K mode)
                         *   Bit 12: 1 = VRAM A/B dual port
                         *   Bit 8: 1 = VRAM access priority */

    /* Cycle patterns: control access to VRAM between CPU, VDP1, VDP2, DMA
     * Each nibble (4 bits) = access priority for a clock slot
     * 0xE = CPU has access, other devices blocked
     * 0x4 = VDP2 has access for reading
     * Safe config: CPU dominant during init */
    for (i = 0; i < 8; i++) {
        vdp2[8 + i] = 0xEEEE;   /* CYCA0L-U, CYCA1L-U, CYCB0L-U, CYCB1L-U
                                 * Offsets 0x10-0x1E (indices 8-15) */
    }

    /* =========================================================================
     * Step 3: Disable all backgrounds
     * ========================================================================= */
    vdp2[16] = 0x0000;  /* BGON (offset 0x20, index 16):
                         *   Bits 0-7: enable NBG0-3, RBG0
                         *   0 = all disabled */

    /* =========================================================================
     * Step 4: Reset scroll and zoom
     * ========================================================================= */
    vdp2[56] = 0;       /* SCXIN0 (offset 0x70, index 56): Scroll X integer NBG0 */
    vdp2[57] = 0;       /* SCXDN0 (offset 0x72, index 57): Scroll X fraction NBG0 */
    vdp2[58] = 0;       /* SCYIN0 (offset 0x74, index 58): Scroll Y integer NBG0 */
    vdp2[59] = 0;       /* SCYDN0 (offset 0x76, index 59): Scroll Y fraction NBG0 */

    /* =========================================================================
     * Step 5: Configure layer priorities
     * ========================================================================= */
    vdp2[120] = 0x0606; /* PRISA (offset 0xF0, index 120):
                         *   Priority: NBG3 > NBG2 > NBG1 > NBG0
                         *   0x0606 = default priority */
    vdp2[124] = 0x0001; /* PRINA (offset 0xF8, index 124):
                         *   Priority: CC (color calculation) > Sprites > Backdrop */

    /* Delay to stabilize configuration */
    for (i = 0; i < 1000; i++) { }

    /* =========================================================================
     * Step 6: Enable display with backdrop color
     * ========================================================================= */
    vdp2[0] = 0x8100;   /* TVMD (offset 0x00):
                         *   Bit 15 (DISP) = 1: enable video output
                         *   Bit 8 (BDCLMD) = 1: backdrop color enable
                         *   Backdrop color defined in table in VRAM */

    /* Delay for VDP2 to process */
    for (i = 0; i < 1000; i++) { }

    /* =========================================================================
     * 3. VDP1 (Video Display Processor 1) - Sprites and polygons
     * =========================================================================
     * Disable and clear VDP1 to avoid pending commands
     * ========================================================================= */
    vdp1[0] = 0x0000;   /* TVMR (offset 0x00): TV Mode
                         *   Bit 15 = 0: VDP1 disable
                         *   Bit 3 = 0: do not reset */
    vdp1[1] = 0x0000;   /* FBCR (offset 0x02): Frame Buffer Control */
    vdp1[2] = 0x0000;   /* PTMR (offset 0x04): Process Timer
                         *   0 = stop command processing */
    vdp1[3] = 0x0000;   /* EWDR (offset 0x06): End Write Data */
    vdp1[4] = 0x0000;   /* EWLR (offset 0x08): End Write Left */
    vdp1[5] = 0x0000;   /* EWRR (offset 0x0A): End Write Right */

    /* =========================================================================
     * 4. Mark end of command list in VDP1 VRAM
     * =========================================================================
     * "end" command (0x80000000) indicates end of command list.
     * VDP1 stops processing when it encounters this command.
     * ========================================================================= */
    vdp1_vram[0] = 0x80000000u;  /* End-of-command marker
                                  *   Bit 31 = 1: last command
                                  *   Bits 30-28 = 0: NOP command */
}
