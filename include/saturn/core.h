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

/* A constant as 16.16: SAT_FX16(3) is 3.0 and SAT_FX16(0.25) is 0.25. Meant
 * for integer or floating-point CONSTANTS; a runtime float would pull in the
 * soft-float library on the SH-2. Negative values work. */
#define SAT_FX16(x) ((sat_fx16_t)((x) * 65536))

/* A four-character file/telemetry tag as a big-endian word:
 * SAT_FOURCC('L','I','T','1') == 0x4C495431, what "LIT1" reads as in a hex dump. */
#define SAT_FOURCC(a, b, c, d)     ((uint32_t)(((uint32_t)(uint8_t)(a) << 24) | ((uint32_t)(uint8_t)(b) << 16) |                 ((uint32_t)(uint8_t)(c) << 8) | (uint32_t)(uint8_t)(d)))

/* Small integer helpers examples kept redefining. Arguments evaluate twice. */
#define SAT_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SAT_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SAT_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

/* floor(sqrt(v)) for a 32-bit unsigned value, no libgcc call. */
uint32_t sat_isqrt32(uint32_t v);
/* Triangle wave: 0 .. amplitude .. 0 over `period` steps (period >= 2). */
int32_t sat_triangle_wave(uint32_t step, uint32_t period, int32_t amplitude);

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
/* Width 320, 352, 640 or 704; height 224, 240 or 256 dots. Widths 640/704
 * are the high-resolution modes and drop the VDP1 framebuffer to 8
 * bits/pixel. Height 256 is PAL-only and is refused as SAT_ERR_UNSUPPORTED on
 * an NTSC console (VDP2 manual 2.4, VRESO). `interlace` selects the scanning
 * method:
 *
 *   SAT_VIDEO_INTERLACE_NONE   non-interlaced (every field is a picture)
 *   SAT_VIDEO_INTERLACE_SINGLE single-density interlace (two fields show the
 *                              same picture; output height is twice `height`)
 *   SAT_VIDEO_INTERLACE_DOUBLE double-density interlace (two different
 *                              pictures per frame) - NOT SUPPORTED by this
 *                              library yet; sat_init returns
 *                              SAT_ERR_UNSUPPORTED
 *
 * Single-density interlace is the VDP2's LSMD=10 mode and is valid at 224,
 * 240 and 256 lines (320x448, 320x480, 320x512 and the 352/640/704
 * equivalents). The logical picture height stays `height`: drawing
 * coordinates, layer tables and sat_video_height() describe one field. The
 * VBlank interrupt still fires once per field, so sat_video_refresh_hz()
 * remains 60 (NTSC) or 50 (PAL); a full interlaced frame spans two VBlanks.
 * VDP1 must stay in single-density interlace, which is its DIE=0 state
 * (VDP1 manual 4.1/4.2, table 2.3 of the VDP2 manual).
 *
 * `ntsc` picks the frame-rate the library's time base assumes: SAT_VIDEO_NTSC
 * (60 fields per second), SAT_VIDEO_PAL (50) or SAT_VIDEO_AUTO, which reads
 * the console's own standard from VDP2 TVSTAT (sat_video_is_pal()). It is a
 * timing choice, independent of the display mode, so a 256-line picture still
 * needs a PAL console even when the timing is fixed to SAT_VIDEO_NTSC.
 * sat_time_ms(), the audio clock and every frame-based duration follow the
 * chosen rate, so a program that hard-codes SAT_VIDEO_NTSC runs slow on a PAL
 * console.
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

#define SAT_VIDEO_INTERLACE_NONE 0u
#define SAT_VIDEO_INTERLACE_SINGLE 1u
#define SAT_VIDEO_INTERLACE_DOUBLE 2u

typedef struct sat_video_config {
    uint16_t width;
    uint16_t height;
    uint8_t ntsc;
    /* SAT_VIDEO_INTERLACE_*; the field was named `reserved` before video
     * modes existed, so positional initializers with 0 keep working. */
    uint8_t interlace;
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

/* Human-readable name of a result code ("BUSY", "CAPACITY", ...). Never
 * returns NULL; unknown codes give "UNKNOWN". */
const char* sat_result_name(sat_result_t st);

/* What the last sat_panic() saw, kept in a fixed global so a test, probe or
 * debugger can read it from RAM without OCR-ing the screen (find
 * g_sat_last_panic in the .map). `magic` is SAT_PANIC_MAGIC once written. */
#define SAT_PANIC_MAGIC 0x50414E43u /* 'PANC' */
#define SAT_PANIC_FILE_MAX 32u
typedef struct sat_panic_info {
    uint32_t magic;
    int32_t code;
    int32_t line;
    char file[SAT_PANIC_FILE_MAX];
} sat_panic_info_t;
extern volatile sat_panic_info_t g_sat_last_panic;

/* Fatal-error stop: records the failure in g_sat_last_panic, then shows a
 * full-screen report (error name, source file, line, code) drawn from VDP1
 * rectangles and never returns. Needs only an initialised VDP1; if even that
 * is unavailable it degrades to a silent spin, the old behaviour. */
#if defined(__GNUC__)
__attribute__((noreturn, cold))
#endif
void sat_panic(sat_result_t st, const char* file, int line);

/* Stops with an on-screen report on any non-OK result. Statement-like, safe
 * in an unbraced if/else, and evaluates `expr` exactly once. */
#define SAT_PANIC_IF_ERROR(expr) do { \
    sat_result_t sat__st = (expr); \
    if (sat__st != SAT_OK) { \
        sat_panic(sat__st, __FILE__, __LINE__); \
    } \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* SATURN_CORE_H */
