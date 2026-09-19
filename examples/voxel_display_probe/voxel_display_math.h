/* Pure VDP2 RBG0 bitmap probe maths, shared with host tests.
 * Layout: 512x256 INDEX8 bitmap in VRAM-A0, 48-word rotation table in A1.
 * Only the top-left 160x112 source rectangle is populated. Hardware kx/ky
 * = 0.5 shows that rectangle over a 320x224 screen without CPU upscaling. */
#ifndef EXAMPLES_VOXEL_DISPLAY_PROBE_MATH_H
#define EXAMPLES_VOXEL_DISPLAY_PROBE_MATH_H
#include <stdint.h>

#define VOXEL_PROBE_SRC_W 160u
#define VOXEL_PROBE_SRC_H 112u
#define VOXEL_PROBE_BITMAP_W 512u
#define VOXEL_PROBE_BITMAP_H 256u
#define VOXEL_PROBE_BITMAP_BASE_WORD 0x00000u
#define VOXEL_PROBE_PARAMS_BASE_WORD 0x10000u

static inline void voxel_probe_rbg0_build_params(uint16_t out[48]) {
    for (uint16_t i = 0; i < 48u; ++i) out[i] = 0u;
    /* Sega VDP2 manual 6.1/6.3: Sx = Xst + dX*Hcnt + dXst*Vcnt,
     * Sy = Yst + dY*Hcnt + dYst*Vcnt. A=E=1 and Px=Py=Cx=Cy=Mx=My=0
     * => tex_x=kx*Hcnt and tex_y=ky*Vcnt. */
    out[8] = 1u;       /* dYst: screen -> bitmap row increment */
    out[10] = 1u;      /* dX:   screen -> bitmap column increment */
    out[14] = 1u;      /* matrix A */
    out[22] = 1u;      /* matrix E */
    out[39] = 0x8000u; /* kx 0.5: scale 160 source pixels to 320 */
    out[41] = 0x8000u; /* ky 0.5: scale 112 source pixels to 224 */
}

static inline uint32_t voxel_probe_rbg0_row_offset(uint16_t row) {
    return VOXEL_PROBE_BITMAP_BASE_WORD +
        (uint32_t)row * (VOXEL_PROBE_BITMAP_W / 2u);
}

/* Pack VDP2 bitmap INDEX8 in pixel scan order: high byte is x, low is x+1.
 * Only even source widths are supported by this probe. */
static inline void voxel_probe_rbg0_pack_row(
    const uint8_t* row, uint16_t width, uint16_t* words) {
    for (uint16_t x = 0u; x < width; x += 2u) {
        words[x >> 1u] = (uint16_t)(((uint16_t)row[x] << 8u) | row[x + 1u]);
    }
}

#endif /* EXAMPLES_VOXEL_DISPLAY_PROBE_MATH_H */
