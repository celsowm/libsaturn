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

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_H */
