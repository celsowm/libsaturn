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
/* NOTE: sat_vdp2_nbg0_init leaves NBG0 at priority 7, ABOVE the sprite
 * layer the VDP1 draws into, which is what a program showing nothing but a
 * background wants. If the VDP1 output has to appear on top -- a background
 * behind a 3D scene, say -- call sat_vdp2_nbg0_set_priority() afterwards
 * with a value below the sprite priority. */
sat_result_t sat_vdp2_nbg0_init(const sat_vdp2_nbg0_config_t* config);
sat_result_t sat_vdp2_nbg0_set_scroll(const sat_vdp2_scroll_t* scroll);
sat_result_t sat_vdp2_nbg0_set_enabled(uint8_t enable);

/* Layer priority, 0-7. The highest number is drawn in front, and equal
 * priorities are resolved by a fixed hardware order rather than by the
 * order anything was set up in -- so a layer that must stay behind the
 * VDP1 needs a strictly lower number, not an equal one.
 *
 * Priority 0 hides the layer completely. That is a legitimate way to turn
 * one off, but it looks identical to a layer that was never initialised,
 * so prefer sat_vdp2_nbg0_set_enabled() when hiding is what you mean. */
sat_result_t sat_vdp2_nbg0_set_priority(uint8_t priority);

/* Priority of the sprite layer, i.e. of everything the VDP1 draws. This is
 * one setting for the whole VDP1 output, not a per-sprite one. */
sat_result_t sat_vdp2_sprite_set_priority(uint8_t priority);

/* ------------------------------------------------------------------ */
/* NBG0 tiled image upload                                             */
/* ------------------------------------------------------------------ */
/* Cells for one 64x64-cell NBG0 plane. Pass a buffer of this many uint16_t
 * as the scratch argument below; the library allocates nothing itself. */
#define SAT_VDP2_NBG0_MAP_CELLS (64u * 64u)

/* Upload a linear indexed8 image as NBG0 character data, and build the map
 * that reassembles it.
 *
 * The image is REPEATED across the plane, so a source smaller than
 * 512x512 tiles rather than leaving a gap -- which is what you want from a
 * seamless texture and is worth knowing if yours is not seamless.
 *
 * Both dimensions must be multiples of 8 (VDP2 cells are 8x8) and at most
 * 512 pixels (the plane is 64 cells across). Where the character data lands
 * in VRAM is not a parameter: a 1-word pattern name can only reach one
 * fixed window, and the plane index chosen at sat_vdp2_nbg0_init() decides
 * how much of it is left, so the layout has exactly one sensible answer and
 * the function picks it. An image too large to fit under the map returns
 * SAT_ERR_CAPACITY.
 *
 * palette_id selects one of the EIGHT CRAM banks of 256 colours (0-7);
 * upload the palette to word offset palette_id * 256 with
 * sat_vdp2_palette_upload(). Anything above 7 is SAT_ERR_INVALID_ARG,
 * because a 256-colour address only has three bits of bank in it: the
 * colour RAM address is palette number bits 6-4 followed by the eight-bit
 * dot code, so palette number bits 3-0 -- the ones a 1-word pattern name
 * carries -- select nothing at all. The bank is therefore written to
 * PNCN0's supplementary palette field by this call, which is why choosing
 * it is not a separate step.
 *
 * map_scratch must hold SAT_VDP2_NBG0_MAP_CELLS entries; it is used to
 * stage the pattern names and is not read afterwards. */
sat_result_t sat_vdp2_nbg0_upload_indexed8(
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t palette_id,
    uint16_t* map_scratch
);

/* ------------------------------------------------------------------ */
/* Palette & VRAM                                                      */
/* ------------------------------------------------------------------ */
sat_result_t sat_vdp2_palette_upload(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset);
/* Hi-res (640/704-wide) modes only: the 256-colour CRAM bank (0-7, word
 * offset bank * 256) that the 8-bit VDP1 framebuffer codes index. Replayed
 * by sat_vdp2_layers_commit(). */
sat_result_t sat_vdp2_sprite_palette_bank_set(uint8_t bank);
sat_result_t sat_vdp2_vram_write_words(uint32_t word_offset, const uint16_t* words, uint32_t word_count);

/* Pixel generator for an INDEX8 VDP2 bitmap. The callback is invoked once
 * for each (x,y) and returns its palette index. It must be deterministic and
 * cannot use the row scratch storage passed to the upload call. */
typedef uint8_t (*sat_vdp2_indexed8_pixel_fn)(void* user, uint16_t x, uint16_t y);
/* Optional progress callback, invoked AFTER each complete row was committed.
 * Useful for rendering a loading indicator without a full bitmap in WRAM. */
typedef void (*sat_vdp2_bitmap_row_fn)(void* user, uint16_t rows_complete);

/* Convert a generated INDEX8 bitmap into VDP2 big-endian pixel pairs and
 * upload it with the existing validated word writer; games never address
 * 0x25E00000 directly. The caller owns a buffer of width/2 uint16_t words.
 * Any even width (<=1024) and height (<=1024) may be used for a bitmap
 * subregion; the RBG0 display mode itself must be configured separately.
 *
 * Validates the WHOLE VRAM interval and scratch length before invoking the
 * pixel callback or writing a single row. No heap or full-frame RAM copy.
 * Do not overlap bitmap, rotation, coefficient or NBG0 VRAM allocations.
 * On hardware failure, previously written rows are not rolled back. */
sat_result_t sat_vdp2_bitmap_upload_indexed8(
    uint32_t base_word, uint16_t width, uint16_t height,
    sat_vdp2_indexed8_pixel_fn pixel_fn,
    sat_vdp2_bitmap_row_fn row_fn,
    void* user, uint16_t* row_words, uint32_t row_word_capacity
);


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
/* Backdrop / BACK screen color                                        */
/* ------------------------------------------------------------------ */
sat_result_t sat_vdp2_set_backdrop_color(uint16_t rgb555);

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
sat_result_t sat_vdp2_rbg0_set_transparent_code_enabled(uint8_t enable);
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
/* Re-applies the composed NBG0/RBG0 state during VBlank. */
sat_result_t sat_vdp2_layers_commit(void);
sat_result_t sat_vdp2_rbg0_set_rotation_read_control(uint16_t rprctl);
sat_result_t sat_vdp2_rbg0_set_coefficient_control(uint16_t ktctl);
sat_result_t sat_vdp2_rbg0_set_ktaof(uint16_t ktaof);
sat_result_t sat_vdp2_rbg0_set_priority(uint8_t priority);
sat_result_t sat_vdp2_rbg0_set_sprite_priority(uint8_t priority);

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

/* ------------------------------------------------------------------ */
/* RBG0 Mode-7 high-level init                                         */
/* ------------------------------------------------------------------ */
typedef struct sat_vdp2_rbg0_mode7_config {
    sat_vdp2_rbg0_bitmap_size_t bitmap_size;
    sat_vdp2_color_mode_t color_mode;
    uint32_t bitmap_base_word;
    uint32_t rot_param_base_word;
    uint16_t back_color_rgb555;
    uint8_t rbg0_priority;
    uint8_t sprite_priority;
} sat_vdp2_rbg0_mode7_config_t;

/* One-shot setup for a Mode-7 style RBG0 floor.
 * Configures bitmap mode, coefficient table (2-word, KMD=0),
 * priorities, backdrop color, and enables the layer.
 * The caller must still upload the bitmap, generate the coefficient
 * table, and build the rotation parameter table.
 */
sat_result_t sat_vdp2_rbg0_mode7_init(const sat_vdp2_rbg0_mode7_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_H */
