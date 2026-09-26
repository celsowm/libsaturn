#ifndef SATURN_CORE_H
#define SATURN_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Fixed-point math type                                               */
/* ------------------------------------------------------------------ */
typedef int32_t sat_fx16_t;

#define SAT_FX16_ONE ((sat_fx16_t)0x00010000)

/* ------------------------------------------------------------------ */
/* Result codes                                                        */
/* ------------------------------------------------------------------ */
typedef enum sat_result {
    SAT_OK = 0,
    SAT_ERR_INVALID_ARG = -1,
    SAT_ERR_NOT_INITIALIZED = -2,
    SAT_ERR_CAPACITY = -3,
    SAT_ERR_UNSUPPORTED = -4,
    SAT_ERR_IO = -5,
    /* The requested logical object or glyph is not present. */
    SAT_ERR_NOT_FOUND = -6,
    SAT_ERR_BUSY = -7,
    SAT_ERR_TIMEOUT = -8,
    SAT_ERR_NOT_CONNECTED = -9,
    SAT_ERR_UNFORMATTED = -10,
    SAT_ERR_WRITE_PROTECTED = -11,
    SAT_ERR_ALREADY_EXISTS = -12,
    SAT_ERR_VERIFY_FAILED = -13,
    SAT_ERR_VERSION = -14
} sat_result_t;

/* ------------------------------------------------------------------ */
/* Video config (needed early by core init)                            */
/* ------------------------------------------------------------------ */
/* width 320, or 640/704 for hi-res; height 224.
 *
 * `ntsc` picks the frame-rate the library's time base assumes: SAT_VIDEO_NTSC
 * (60 frames per second), SAT_VIDEO_PAL (50) or SAT_VIDEO_AUTO, which reads the
 * console's own standard from VDP2 TVSTAT (sat_video_is_pal()). The picture
 * is always the 224-line mode, which a PAL console shows at 50 Hz with borders;
 * the 240 and 256-line PAL modes are not offered. sat_time_ms(), the audio
 * clock and every frame-based duration follow the chosen rate, so a program
 * that hard-codes SAT_VIDEO_NTSC runs slow on a PAL console.
 *
 * Hi-res doubles the horizontal resolution, and the VDP1 framebuffer drops
 * to 8 bits/pixel. VDP1 output is then palette codes 0-255 into one
 * 256-colour CRAM bank (sat_vdp2_sprite_palette_bank_set()): code 0 is
 * transparent, 0xFE is the sprite shadow code, and RGB textures, RGB
 * polygon colours, Gouraud shading and VDP1 colour calculation are not
 * available (VDP1 manual 1.3, 6.4). Polygon colours and LUT4 table entries
 * write their low byte as the code; indexed textures write their texel.
 * sat_set_clear_color() leaves the erase transparent: set the backdrop. */
#define SAT_VIDEO_PAL 0u
#define SAT_VIDEO_NTSC 1u
#define SAT_VIDEO_AUTO 2u

typedef struct sat_video_config {
    uint16_t width;
    uint16_t height;
    uint8_t ntsc;
    uint8_t reserved;
} sat_video_config_t;

/* ------------------------------------------------------------------ */
/* Core lifecycle                                                      */
/* ------------------------------------------------------------------ */
sat_result_t sat_init(const sat_video_config_t* config);
/* Idempotently invalidates runtime handles and shuts down optional audio
 * state before returning. Explicit subsystem shutdown calls remain valid. */
sat_result_t sat_shutdown(void);

/* ------------------------------------------------------------------ */
/* Error handling helpers                                              */
/* ------------------------------------------------------------------ */
#define SAT_TRY(expr) do { \
    sat_result_t sat__st = (expr); \
    if (sat__st != SAT_OK) { \
        return sat__st; \
    } \
} while (0)

#define SAT_PANIC_IF_ERROR(expr) do { \
    sat_result_t sat__st = (expr); \
    if (sat__st != SAT_OK) { \
        while (1) { } \
    } \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* SATURN_CORE_H */
