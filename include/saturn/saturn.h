#ifndef SATURN_SATURN_H
#define SATURN_SATURN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t sat_fx16_t;

#define SAT_FX16_ONE ((sat_fx16_t)0x00010000)
#define SAT_PAD_UP    ((uint16_t)(1u << 12))
#define SAT_PAD_DOWN  ((uint16_t)(1u << 13))
#define SAT_PAD_LEFT  ((uint16_t)(1u << 14))
#define SAT_PAD_RIGHT ((uint16_t)(1u << 15))
#define SAT_PAD_START ((uint16_t)(1u << 3))
#define SAT_PAD_A     ((uint16_t)(1u << 2))
#define SAT_PAD_B     ((uint16_t)(1u << 4))
#define SAT_PAD_C     ((uint16_t)(1u << 5))
#define SAT_PAD_X     ((uint16_t)(1u << 6))
#define SAT_PAD_Y     ((uint16_t)(1u << 7))
#define SAT_PAD_Z     ((uint16_t)(1u << 8))
#define SAT_PAD_L     ((uint16_t)(1u << 9))
#define SAT_PAD_R     ((uint16_t)(1u << 10))

typedef enum sat_result {
    SAT_OK = 0,
    SAT_ERR_INVALID_ARG = -1,
    SAT_ERR_NOT_INITIALIZED = -2,
    SAT_ERR_CAPACITY = -3,
    SAT_ERR_UNSUPPORTED = -4
} sat_result_t;

typedef struct sat_video_config {
    uint16_t width;
    uint16_t height;
    uint8_t ntsc;
    uint8_t reserved;
} sat_video_config_t;

typedef struct sat_texture {
    uint16_t srca;
    uint16_t width;
    uint16_t height;
    uint16_t palette;
    uint16_t valid;
    uint16_t reserved;
} sat_texture_t;

typedef struct sat_sprite_cmd {
    sat_fx16_t x;
    sat_fx16_t y;
    uint16_t width;
    uint16_t height;
    const sat_texture_t* texture;
    uint16_t palette_override;
    uint16_t flags;
} sat_sprite_cmd_t;

typedef struct sat_pad_state {
    uint16_t held;
    uint16_t pressed;
    uint16_t released;
} sat_pad_state_t;

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

typedef struct sat_vdp2_nbg0_config {
    sat_vdp2_char_size_t char_size;
    sat_vdp2_color_mode_t color_mode;
    uint16_t map_plane_index;
    uint8_t transparent_code_enabled;
    uint8_t reserved;
} sat_vdp2_nbg0_config_t;

typedef struct sat_vdp2_scroll {
    uint16_t x_integer;
    uint16_t x_fraction;
    uint16_t y_integer;
    uint16_t y_fraction;
} sat_vdp2_scroll_t;

typedef struct sat_vdp2_map_region {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} sat_vdp2_map_region_t;

sat_result_t sat_init(const sat_video_config_t* config);
sat_result_t sat_shutdown(void);

sat_result_t sat_begin_frame(void);
sat_result_t sat_end_frame(void);
sat_result_t sat_wait_vblank(void);

sat_result_t sat_pad_poll(sat_pad_state_t* out_state);
uint16_t sat_pad_held(void);

sat_result_t sat_tex_upload_indexed8(
    sat_texture_t* out_texture,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    const uint16_t* palette_rgb555,
    uint16_t palette_index
);

sat_result_t sat_draw_sprite(const sat_sprite_cmd_t* cmd);
sat_result_t sat_set_clear_color(uint16_t rgb555);

// VDP2 API (breaking v0) for background/text workflows.
sat_result_t sat_vdp2_nbg0_init(const sat_vdp2_nbg0_config_t* config);
sat_result_t sat_vdp2_nbg0_set_scroll(const sat_vdp2_scroll_t* scroll);
sat_result_t sat_vdp2_nbg0_set_enabled(uint8_t enable);
sat_result_t sat_vdp2_palette_upload(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset);
sat_result_t sat_vdp2_vram_write_words(uint32_t word_offset, const uint16_t* words, uint32_t word_count);
sat_result_t sat_vdp2_nbg0_map_fill(uint16_t pattern_name);
sat_result_t sat_vdp2_nbg0_map_write_region(
    const uint16_t* pattern_names,
    const sat_vdp2_map_region_t* region,
    uint16_t source_stride
);
sat_result_t sat_vdp2_wait_vblank_start(void);
sat_result_t sat_vdp2_wait_vblank_end(void);
sat_result_t sat_vdp2_back_color_set(uint16_t rgb555);

#ifdef __cplusplus
}
#endif

#endif
