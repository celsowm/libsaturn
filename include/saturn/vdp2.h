#ifndef SATURN_VDP2_H
#define SATURN_VDP2_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Character size & color mode enums                                   */
/* ------------------------------------------------------------------ */
typedef enum sat_vdp2_char_size {
    SAT_VDP2_CHAR_SIZE_1X1 = 0,
    SAT_VDP2_CHAR_SIZE_2X2 = 1
} sat_vdp2_char_size_t;

typedef enum sat_vdp2_color_mode {
    SAT_VDP2_COLOR_MODE_16 = 0,
    SAT_VDP2_COLOR_MODE_256 = 1,
    SAT_VDP2_COLOR_MODE_2048 = 2,
    SAT_VDP2_COLOR_MODE_32768 = 3,
    SAT_VDP2_COLOR_MODE_16770000 = 4
} sat_vdp2_color_mode_t;

/* ------------------------------------------------------------------ */
/* NBG0 config                                                         */
/* ------------------------------------------------------------------ */
typedef struct sat_vdp2_nbg0_config {
    sat_vdp2_char_size_t char_size;
    sat_vdp2_color_mode_t color_mode;
    uint16_t map_plane_index;
    uint8_t transparent_code_enabled;
    uint8_t reserved;
} sat_vdp2_nbg0_config_t;

/* ------------------------------------------------------------------ */
/* Scroll state                                                        */
/* ------------------------------------------------------------------ */
typedef struct sat_vdp2_scroll {
    uint16_t x_integer;
    uint16_t x_fraction;
    uint16_t y_integer;
    uint16_t y_fraction;
} sat_vdp2_scroll_t;

/* ------------------------------------------------------------------ */
/* Map region                                                          */
/* ------------------------------------------------------------------ */
typedef struct sat_vdp2_map_region {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} sat_vdp2_map_region_t;

/* ------------------------------------------------------------------ */
/* NBG0 initialization & control                                       */
/* ------------------------------------------------------------------ */
sat_result_t sat_vdp2_nbg0_init(const sat_vdp2_nbg0_config_t* config);
sat_result_t sat_vdp2_nbg0_set_scroll(const sat_vdp2_scroll_t* scroll);
sat_result_t sat_vdp2_nbg0_set_enabled(uint8_t enable);

/* ------------------------------------------------------------------ */
/* Palette & VRAM                                                      */
/* ------------------------------------------------------------------ */
sat_result_t sat_vdp2_palette_upload(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset);
sat_result_t sat_vdp2_vram_write_words(uint32_t word_offset, const uint16_t* words, uint32_t word_count);

/* ------------------------------------------------------------------ */
/* Map fill & region write                                             */
/* ------------------------------------------------------------------ */
sat_result_t sat_vdp2_nbg0_map_fill(uint16_t pattern_name);
sat_result_t sat_vdp2_nbg0_map_write_region(
    const uint16_t* pattern_names,
    const sat_vdp2_map_region_t* region,
    uint16_t source_stride
);

/* ------------------------------------------------------------------ */
/* VBlank wait helpers (VDP2-specific)                                 */
/* ------------------------------------------------------------------ */
sat_result_t sat_vdp2_wait_vblank_start(void);
sat_result_t sat_vdp2_wait_vblank_end(void);

/* ------------------------------------------------------------------ */
/* RBG0 (Rotation Background 0) - Infinite plane support              */
/* ------------------------------------------------------------------ */

/* RBG0 bitmap size modes */
typedef enum sat_vdp2_rbg0_bitmap_size {
    SAT_VDP2_RBG0_BITMAP_256x256   = 0x00,
    SAT_VDP2_RBG0_BITMAP_512x256   = 0x01,
    SAT_VDP2_RBG0_BITMAP_1024x256  = 0x02,
    SAT_VDP2_RBG0_BITMAP_256x512   = 0x04,
    SAT_VDP2_RBG0_BITMAP_512x512   = 0x05,
    SAT_VDP2_RBG0_BITMAP_1024x512  = 0x06,
    SAT_VDP2_RBG0_BITMAP_1024x1024 = 0x07
} sat_vdp2_rbg0_bitmap_size_t;

/* RBG0 rotation parameter mode */
typedef enum sat_vdp2_rbg0_param_mode {
    SAT_VDP2_RBG0_PARAM_A     = 0x00,
    SAT_VDP2_RBG0_PARAM_B     = 0x01,
    SAT_VDP2_RBG0_PARAM_COEFF = 0x02,
    SAT_VDP2_RBG0_PARAM_WINDOW = 0x03
} sat_vdp2_rbg0_param_mode_t;

/* RBG0 configuration */
typedef struct sat_vdp2_rbg0_config {
    sat_vdp2_rbg0_bitmap_size_t bitmap_size;
    sat_vdp2_color_mode_t color_mode;
    uint32_t bitmap_base_word;
    uint32_t rot_param_base_word;
} sat_vdp2_rbg0_config_t;

/* RBG0 initialization & control */
sat_result_t sat_vdp2_rbg0_init(const sat_vdp2_rbg0_config_t* config);
sat_result_t sat_vdp2_rbg0_set_enabled(uint8_t enable);
sat_result_t sat_vdp2_rbg0_set_param_mode(sat_vdp2_rbg0_param_mode_t mode);
uint16_t sat_vdp2_rbg0_last_bgon_written(void);
uint16_t sat_vdp2_rbg0_last_ramctl_written(void);
uint16_t sat_vdp2_rbg0_last_chctlb_written(void);
uint16_t sat_vdp2_rbg0_last_mpofr_written(void);
uint16_t sat_vdp2_rbg0_last_rptau_written(void);
uint16_t sat_vdp2_rbg0_last_rptal_written(void);
uint16_t sat_vdp2_rbg0_last_rprctl_written(void);
uint16_t sat_vdp2_rbg0_last_ktctl_written(void);
uint16_t sat_vdp2_rbg0_last_rpmd_written(void);
uint16_t sat_vdp2_rbg0_last_prir_written(void);
uint16_t sat_vdp2_rbg0_last_bmpnb_written(void);
uint16_t sat_vdp2_rbg0_last_plsz_written(void);

/* Re-apply all RBG0 configuration registers during VBlank.
 * After sat_init() the display is enabled, so VDP2 register writes
 * outside VBlank are silently dropped by the hardware.  Call this
 * function once per frame inside your VBlank handler (i.e. after
 * sat_wait_vblank() returns) to ensure RBG0 registers take effect.
 */
sat_result_t sat_vdp2_rbg0_commit(void);
sat_result_t sat_vdp2_rbg0_set_rotation_read_control(uint16_t rprctl);
sat_result_t sat_vdp2_rbg0_set_coefficient_control(uint16_t ktctl);

/* Rotation parameter setup */
sat_result_t sat_vdp2_rbg0_set_scroll(uint32_t rot_param_word_offset,
                                       int32_t xst_int, int32_t xst_frac,
                                       int32_t yst_int, int32_t yst_frac);
sat_result_t sat_vdp2_rbg0_set_vertical_increments(uint32_t rot_param_word_offset,
                                                    int32_t dxst_int, int32_t dxst_frac,
                                                    int32_t dyst_int, int32_t dyst_frac);
sat_result_t sat_vdp2_rbg0_set_coordinate_increments(uint32_t rot_param_word_offset,
                                                       int32_t dx_int, int32_t dx_frac,
                                                       int32_t dy_int, int32_t dy_frac);
/* Rotation matrix setup (for perspective/3D effects).
 * Angles are interpreted as degrees in X/Y/Z order.
 */
sat_result_t sat_vdp2_rbg0_set_rotation_matrix(uint32_t rot_param_word_offset,
                                                int32_t angle_x, int32_t angle_y, int32_t angle_z);
/* Viewpoint and center coordinates are provided as 16.16 fixed-point and
 * written to the table as integer-only fields.
 */
sat_result_t sat_vdp2_rbg0_set_viewpoint(uint32_t rot_param_word_offset,
                                          int32_t px, int32_t py, int32_t pz);
sat_result_t sat_vdp2_rbg0_set_center(uint32_t rot_param_word_offset,
                                       int32_t cx, int32_t cy, int32_t cz);
/* Scaling coefficients are interpreted as 16.16 fixed-point. */
sat_result_t sat_vdp2_rbg0_set_scaling(uint32_t rot_param_word_offset,
                                        int32_t kx, int32_t ky);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_H */
