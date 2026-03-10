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

#ifdef __cplusplus
}
#endif

#endif

