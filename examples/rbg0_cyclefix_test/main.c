/* rbg0_cyclefix_test - Manually set correct cycle patterns for RBG0 bitmap */
#include <stdint.h>
#include "saturn/saturn.h"

#define V_REG(off)   (*(volatile uint16_t*)(0x25F80000u + (off)))
#define V_WORD(w_off) (((volatile uint16_t*)0x25E00000u)[w_off])
#define CRAM_WORD(i)  (((volatile uint16_t*)0x25F00000u)[i])

int main(void) {
    sat_video_config_t cfg = {320, 224, 1, 0};
    uint32_t i;

    sat_init(&cfg);

    /* Blue palette, red for pixel 1 */
    for(i=0; i<256; i++) CRAM_WORD(i) = 0x03E0;
    CRAM_WORD(1) = 0x7C00;

    /* Bitmap: solid blue with palette 1 */
    for(i=0; i<(512*256)/2; i++) {
        V_WORD(i) = 0x0101;
    }

    /* Rotation params at A1, all zeros = identity */
    for(i=0; i<48; i++) {
        V_WORD(0x10000 + i) = 0;
    }

    /* dYst = 1.0 (advance Y by 1 per scanline) */
    V_WORD(0x10000 + 8) = 1;   /* dYst integer */
    V_WORD(0x10000 + 9) = 0;   /* dYst fraction */
    
    /* dX = 1.0 (advance X by 1 per pixel) */
    V_WORD(0x10000 + 10) = 1;  /* dX integer */
    V_WORD(0x10000 + 11) = 0;  /* dX fraction */

    /* Configure VDP2 registers */
    V_REG(0x000E) = 0x1103;   /* RAMCTL: A0=bitmap(11), A1=coeff(01) */
    
    /* Cycle patterns: A0 gets RBG0 bitmap reads (9) + CPU (E) */
    V_REG(0x0010) = 0x9E9E;   /* CYCA0L: T0=bitmap, T1=CPU, T2=bitmap, T3=CPU */
    V_REG(0x0012) = 0x9E9E;   /* CYCA0U: T4=bitmap, T5=CPU, T6=bitmap, T7=CPU */
    
    /* A1 gets RBG0 param reads (8) + CPU (E) */
    V_REG(0x0014) = 0x8E8E;   /* CYCA1L */
    V_REG(0x0016) = 0x8E8E;   /* CYCA1U */
    
    /* B0/B1: CPU only */
    V_REG(0x0018) = 0xEEEE;   /* CYCB0L */
    V_REG(0x001A) = 0xEEEE;   /* CYCB1U */
    
    /* RBG0 bitmap mode: 256 colors, 512x256 */
    V_REG(0x002A) = 0x1200;   /* CHCTLB: R0BMEN=1, R0CHCN=001(256c), R0BMSZ=0(512x256) */
    
    /* Bitmap bank = A0 */
    V_REG(0x003E) = 0x0000;   /* MPOFR */
    
    /* RPTA = A1 base (0x10000 words = 0x20000 bytes) */
    /* RPTA format: high nibble = bank, low byte = offset/0x100 */
    /* 0x10000 words = bank 0 (A0)... wait, 0x10000 is A1! */
    /* A1 starts at 0x10000 words = byte offset 0x20000 */
    /* RPTA = (bank << 12) | (offset >> 8) & 0xFFF... hmm */
    /* Actually RPTA = ((word_offset >> 16) << 12) | ((word_offset >> 8) & 0xFFF)? */
    /* Let's try: RPTAU = high bits, RPTAL = low bits */
    V_REG(0x00B8) = 0x0001;   /* RPTAU: bank = A1 = 1 */
    V_REG(0x00BA) = 0x0000;   /* RPTAL: offset 0x10000 >> 1 = 0x8000? No... */
    
    V_REG(0x00B0) = 0x0000;   /* RPMD: param A */
    V_REG(0x00FC) = 0x0007;   /* PRIR: priority 7 */
    
    /* Enable RBG0 */
    V_REG(0x0020) = 0x0010;   /* BGON: R0ON=1 */

    while (1) {
        sat_wait_vblank();
    }

    return 0;
}
