/* vdp2_infinite_plan - ZERO-TEST (Addressing) */
#include <stdint.h>
#include "saturn/saturn.h"
#include "vdp2_infinite_plan/seamless_sea.h"

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

    /* Map: All Tile 0 */
    for(i=0; i<4096; i++) V_WORD(0x20000 + i) = 0;

    /* ZERO THE ENTIRE TABLE BANK */
    for(i=0; i<8192; i++) V_WORD(0x10000 + i) = 0;

    V_REG(0x000E) = 0x013F;
    V_REG(0x0010) = 0x8888; V_REG(0x0012) = 0x8888;
    V_REG(0x0014) = 0xCCCC; V_REG(0x0016) = 0xCCCC;
    V_REG(0x0018) = 0x4444; V_REG(0x001A) = 0x4444;
    V_REG(0x002A) = 0x0001; V_REG(0x0038) = 0x0000;
    V_REG(0x003A) = 0x0040; V_REG(0x0050) = 0x0000;
    V_REG(0x00B8) = 0x0002; V_REG(0x00BA) = 0x0000;
    V_REG(0x00B0) = 0x0000; V_REG(0x00FC) = 0x0007;
    V_REG(0x0020) = 0x0010;

    while(1) {
        sat_app_frame_begin(0, 0, &pad);
        sat_end_frame();
    }
    return 0;
}
