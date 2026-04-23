/* rbg0_direct_test - Write VDP2 registers directly like vdp2_infinite_plan */
#include <stdint.h>
#include "saturn/saturn.h"

#define V_REG(off)   (*(volatile uint16_t*)(0x25F80000u + (off)))
#define V_WORD(w_off) (((volatile uint16_t*)0x25E00000u)[w_off])
#define CRAM_WORD(i)  (((volatile uint16_t*)0x25F00000u)[i])

int main(void) {
    sat_video_config_t cfg = {320, 224, 1, 0};
    uint32_t i;

    sat_init(&cfg);

    /* Blue palette */
    for(i=0; i<256; i++) CRAM_WORD(i) = 0x03E0;
    CRAM_WORD(1) = 0x7C00; /* Red */

    /* Tile 0: One red dot pattern */
    for(i=0; i<32; i++) V_WORD(i) = 0x0101;

    /* Map: All Tile 0 at offset 0x20000 (VRAM B0) */
    for(i=0; i<4096; i++) V_WORD(0x20000 + i) = 0;

    /* Zero rotation parameter table at offset 0x10000 (VRAM A1) */
    for(i=0; i<8192; i++) V_WORD(0x10000 + i) = 0;

    /* Configure VDP2 registers exactly like vdp2_infinite_plan */
    V_REG(0x000E) = 0x013F;   /* RAMCTL */
    V_REG(0x0010) = 0x8888;   /* CYCA0L */
    V_REG(0x0012) = 0x8888;   /* CYCA0U */
    V_REG(0x0014) = 0xCCCC;   /* CYCA1L */
    V_REG(0x0016) = 0xCCCC;   /* CYCA1U */
    V_REG(0x0018) = 0x4444;   /* CYCB0L */
    V_REG(0x001A) = 0x4444;   /* CYCB1U */
    V_REG(0x002A) = 0x0001;   /* CHCTLB - RBG0 cell mode, 256 colors */
    V_REG(0x0038) = 0x0000;   /* RNCN0 */
    V_REG(0x003A) = 0x0040;   /* PLSZ */
    V_REG(0x0050) = 0x0000;   /* MPABRA */
    V_REG(0x00B8) = 0x0002;   /* RPTAU */
    V_REG(0x00BA) = 0x0000;   /* RPTAL */
    V_REG(0x00B0) = 0x0000;   /* RPMD */
    V_REG(0x00FC) = 0x0007;   /* PRIR */
    V_REG(0x0020) = 0x0010;   /* BGON - Enable RBG0 */

    while (1) {
        sat_wait_vblank();
    }

    return 0;
}
