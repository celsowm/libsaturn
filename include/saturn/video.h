#ifndef SATURN_VIDEO_H
#define SATURN_VIDEO_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Frame lifecycle                                                     */
/* ------------------------------------------------------------------ */
sat_result_t sat_begin_frame(void);
sat_result_t sat_end_frame(void);
sat_result_t sat_wait_vblank(void);

/* The logical picture size sat_init set up: 320/352/640/704 by 224/240/256.
 * Before sat_init both read 0. In interlaced modes this is one field's size;
 * the output picture is twice as tall. Use these instead of a private
 * SCREEN_W/SCREEN_H, so an example follows the video mode it asked for. */
uint16_t sat_video_width(void);
uint16_t sat_video_height(void);
/* 1 when the picture is 640 or 704 wide (hi-res). */
uint8_t sat_video_is_hires(void);

/* One display mode as sat_init receives it. `reserved` keeps the struct a
 * whole number of words on both compilers; set it to 0. */
typedef struct sat_video_mode {
    uint16_t width;
    uint16_t height;
    uint8_t interlace;
    uint8_t reserved;
} sat_video_mode_t;

/* Checks a mode without touching the display. SAT_OK when this console can
 * show it, SAT_ERR_UNSUPPORTED for a legal VDP2 mode this library does not
 * implement (double-density interlace, or 256 lines on an NTSC console) and
 * SAT_ERR_INVALID_ARG for a width/height/interlace value the VDP2 cannot
 * express. Only sat_init() changes the mode: the VDP2 manual requires the
 * horizontal-resolution switch to go through the BIOS and a reset when
 * leaving dedicated-monitor modes, so a mode change is a full sat_shutdown()
 * plus sat_init(). */
sat_result_t sat_video_mode_supported(const sat_video_mode_t* mode);

/* The interlace setting sat_init resolved: SAT_VIDEO_INTERLACE_*. */
uint8_t sat_video_interlace_mode(void);
/* 1 when the active mode is single-density interlace. */
uint8_t sat_video_is_interlaced(void);

/* The TVMD register last written by the library (TV screen mode: DISP, LSMD,
 * VRESO, HRESO). Read-only diagnostics for a HUD or harness; the picture's
 * actual size comes from sat_video_width/height. TVMD is a register the VDP2
 * documents as readable, but the library returns its own shadow so the value
 * is also available on emulators that do not model the read. */
uint16_t sat_video_tvmd(void);
/* The live TVSTAT word (VBLANK bit 3, HBLANK bit 2, PAL bit 0). */
uint16_t sat_video_tvstat(void);

/* 1 when the console runs the PAL standard (VDP2 TVSTAT bit 0), 0 for NTSC.
 * Readable at any time, before sat_init as well. */
uint8_t sat_video_is_pal(void);
/* 1 when the library's time base is 60 frames per second, 0 for 50. This is
 * the standard sat_init resolved (SAT_VIDEO_AUTO becomes the console's own). */
uint8_t sat_video_is_ntsc_timing(void);
/* The frame rate the library is using: 60 for NTSC timing, 50 for PAL.
 * Prefer this over the is_ntsc_timing ternary when you need the rate as a
 * number (frame->ms conversion, per-second budgets, animation periods). */
uint32_t sat_video_refresh_hz(void);

/* Display frames (VBlanks) elapsed since sat_init.
 *
 * Unlike counting sat_wait_vblank calls, this keeps counting while the
 * program is busy: a frame that takes three VBlanks to build advances it by
 * three. Time-based animation should step by its deltas, or it plays in slow
 * motion exactly when the scene is heaviest. Measured with the SH-2
 * free-running timer, calibrated against real VBlank edges during sat_init;
 * a single program frame longer than about 18 NTSC frames undercounts. */
uint32_t sat_frame_count(void);

/* ------------------------------------------------------------------ */
/* Clear / backdrop color                                              */
/* ------------------------------------------------------------------ */

/* Sets an OPAQUE VDP1 erase color. The erase clears the sprite framebuffer at
 * the start of each frame. Bit 15 (the RGB code) is added automatically, so the
 * erased framebuffer COVERS the VDP2 backdrop and any VDP2 layer below the
 * sprite layer. Call sat_vdp1_set_erase_transparent() to restore the default.
 */
sat_result_t sat_set_clear_color(uint16_t rgb555);

/* Restores the transparent VDP1 framebuffer erase (the default set by sat_init).
 * The framebuffer is filled with the "no data" code, so the VDP2 backdrop and
 * VDP2 layers below the sprite layer are visible wherever no sprite is drawn.
 */
sat_result_t sat_vdp1_set_erase_transparent(void);

/* Sets VDP2 backdrop color. This color appears when
 * no VDP1/VDP2 layer draws on top. With transparent VDP1 erase
 * (default), this is the visible color on screen.
 */
sat_result_t sat_vdp2_back_color_set(uint16_t rgb555);

/* Enables or disables automatic VDP1 erase for the entire frame.
 * enable=1: VDP1 erases each frame with the current erase color
 *           (transparent by default, opaque after sat_set_clear_color)
 * enable=0: VDP1 does NOT erase (framebuffer keeps previous frame data)
 * Default: enabled.
 */
sat_result_t sat_vdp1_set_erase_enabled(uint8_t enable);

/* ------------------------------------------------------------------ */
/* When the sprite framebuffer changes                                 */
/* ------------------------------------------------------------------ */

typedef enum sat_vdp1_frame_mode {
    /* Default. The VDP1 swaps framebuffers at every field and plots the last
     * submitted list right after the swap (1-cycle mode, PTMR auto). A list
     * sent by sat_end_frame during field k is drawn in field k+1 and shown in
     * field k+2: one field after VDP2 changes made in the VBlank that ends
     * field k, so sprites trail the scroll layers by a frame. */
    SAT_VDP1_FRAME_AUTO = 0,
    /* sat_end_frame starts plotting at once (PTMR 01); the VBlank-IN after the
     * list has ended asks for the swap (FBCR manual change), so a list drawn
     * in time appears in field k+1, with the VDP2 state committed in the same
     * VBlank. A field with no new or no finished list keeps showing the
     * previous one instead of an erased or half-drawn framebuffer, which
     * suits programs slower than 60 fps (sat_vdp1_frame_stats counts those
     * fields as late_swaps); the next sat_end_frame waits for an outstanding
     * swap. sat_wait_vblank returns only in the VBlank in which the last
     * list's swap was asked for (a list that ends during the VBlank gets it
     * there; a later one costs whole fields), so VDP2 state written right
     * after it always appears with that list's sprites. The erase is a polygon at the top of each list, filled with the
     * erase colour (sat_set_clear_color / sat_vdp1_set_erase_transparent);
     * with sat_vdp1_set_erase_enabled(0) the list draws over the picture from
     * two lists back. Needs the SCU VBlank-IN interrupt (on by default);
     * without it the waits poll for the VBlank instead. */
    SAT_VDP1_FRAME_ON_SUBMIT = 1
} sat_vdp1_frame_mode_t;

/* Takes effect from the next sat_begin_frame. SAT_ERR_INVALID_ARG for an
 * unknown mode. */
sat_result_t sat_vdp1_set_frame_mode(sat_vdp1_frame_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VIDEO_H */
