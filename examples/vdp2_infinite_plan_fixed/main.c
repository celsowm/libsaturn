/* vdp2_infinite_plan_fixed - Fixed map register pointing */
#include <stdint.h>
#include "saturn/saturn.h"

#define V_REG(off)   (*(volatile uint16_t*)(0x25F80000u + (off)))
#define V_WORD(w_off) (((volatile uint16_t*)0x25E00000u)[w_off])
#define CRAM_WORD(i)  (((volatile uint16_t*)0x25F00000u)[i])

int main(void) {
    sat_video_config_t cfg;
    sat_pad_state_t pad;
    uint32_t i;

    cfg.width = 320; cfg.height = 224; cfg.ntsc = 1; cfg.reserved = 0;
    sat_init(&cfg);

    /* Green Palette */
    for(i=0; i<256; i++) CRAM_WORD(i) = 0x03E0;
    CRAM_WORD(1) = 0x7C00; /* Red */

    /* Tile 0: One red dot */
    for(i=0; i<32; i++) V_WORD(i) = 0x0101;

    /* Map at VRAM B0 (byte offset 0x40000 = word offset 0x20000) */
    /* Map register value = offset / 0x2000 = 0x40000 / 0x2000 = 0x20 */
    for(i=0; i<4096; i++) V_WORD(0x20000 + i) = 0;

    /* ZERO THE ENTIRE TABLE BANK (A1) */
    for(i=0; i<8192; i++) V_WORD(0x10000 + i) = 0;

    /* Set rotation parameters for flat 2D */
    /* dXst = 0, dYst = 1.0 */
    V_WORD(0x10000 + 6) = 0;   /* dXst int */
    V_WORD(0x10000 + 7) = 0;   /* dXst frac */
    V_WORD(0x10000 + 8) = 1;   /* dYst int */
    V_WORD(0x10000 + 9) = 0;   /* dYst frac */
    /* dX = 1.0, dY = 0 */
    V_WORD(0x10000 + 10) = 1;  /* dX int */
    V_WORD(0x10000 + 11) = 0;  /* dX frac */
    V_WORD(0x10000 + 12) = 0;  /* dY int */
    V_WORD(0x10000 + 13) = 0;  /* dY frac */

    V_REG(0x000E) = 0x013F;   /* RAMCTL: all banks = character pattern */
    V_REG(0x0010) = 0x8888;   /* CYCA0L: RBG0 param read */
    V_REG(0x0012) = 0x8888;   /* CYCA0U */
    V_REG(0x0014) = 0xCCCC;   /* CYCA1L: Color RAM */
    V_REG(0x0016) = 0xCCCC;   /* CYCA1U */
    V_REG(0x0018) = 0x4444;   /* CYCB0L: CPU */
    V_REG(0x001A) = 0x4444;   /* CYCB1U */
    V_REG(0x002A) = 0x0001;   /* CHCTLB: RBG0 cell mode, 256 colors */
    V_REG(0x0038) = 0x0000;   /* RNCN0 */
    V_REG(0x003A) = 0x0040;   /* PLSZ: 1x1 page, repeat */
    
    /* FIXED: Map register points to B0 (value 0x20) */
    V_REG(0x0050) = 0x2020;   /* MPABRA: A=0x20, B=0x20 -> VRAM B0 */
    
    /* RPTA: parameter table at A1 (word offset 0x10000) */
    /* RPTAU = bank (1 = A1), RPTAL = offset in bank / 2 */
    V_REG(0x00B8) = 0x0001;   /* RPTAU: A1 */
    V_REG(0x00BA) = 0x0000;   /* RPTAL: offset 0 */
    
    V_REG(0x00B0) = 0x0000;   /* RPMD: param A */
    V_REG(0x00FC) = 0x0007;   /* PRIR: priority 7 */
    V_REG(0x0020) = 0x0010;   /* BGON: Enable RBG0 */

    while(1) {
        sat_app_frame_begin(0, 0, &pad);
        sat_end_frame();
    }
    return 0;
}
