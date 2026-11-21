#ifndef SATURN_WATER_VIEW_H
#define SATURN_WATER_VIEW_H

#include "saturn/vdp2_color_offset.h"
#include "saturn/water_regions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The camera going under water: which region it is in (with hysteresis), a
 * fade of a VDP2 colour offset toward a tint over a fixed number of frames
 * and back, and the painter-pass order for the view.
 *
 * Call sat_water_view_update once per frame with the camera position. It is
 * pure: it only reports what to do. sat_water_view_apply writes that to the
 * VDP2 colour offset. The view owns the offset bank and the layer mask it is
 * given only while the fade is above zero, and releases the layers when the
 * fade returns to zero; nothing else may use that bank at the same time (the
 * library cannot check). */
typedef struct sat_water_view_config {
    const sat_water_regions_t* regions;
    sat_fx16_t hysteresis;                 /* >= 0, as sat_water_volume_immersion */
    sat_vdp2_color_offset_bank_t bank;
    uint8_t layers;                        /* SAT_VDP2_LAYER_* mask, nonzero */
    sat_vdp2_color_offset_t tint;          /* offset at full fade */
    uint8_t fade_frames;                   /* frames from clear to full, >= 1 */
} sat_water_view_config_t;

typedef struct sat_water_view {
    sat_water_view_config_t config;
    uint16_t region;                       /* SAT_WATER_REGION_NONE or the region */
    uint8_t fade;                          /* 0..fade_frames */
} sat_water_view_t;

typedef struct sat_water_view_state {
    uint16_t region;                       /* region the camera is in, or NONE */
    uint8_t underwater;
    uint8_t fade;                          /* 0..fade_frames */
    sat_vdp2_color_offset_t offset;        /* the tint scaled by fade */
    uint8_t set_offset;                    /* write `offset` (fade above zero) */
    uint8_t release;                       /* the fade just returned to zero */
} sat_water_view_state_t;

/* Painter passes for the three layers of a water scene, ascending is drawn
 * first. Above the surface: the submerged geometry, the water, then the dry.
 * Under it the order mirrors, since the dry world lies beyond the water. */
typedef struct sat_water_view_passes {
    uint16_t below, water, above;
} sat_water_view_passes_t;

sat_result_t sat_water_view_init(sat_water_view_t* view, const sat_water_view_config_t* config);
sat_result_t sat_water_view_update(sat_water_view_t* view, const sat_vec3_t* eye,
    sat_water_view_state_t* out);
sat_water_view_passes_t sat_water_view_passes(uint8_t underwater);

/* Writes the colour offset (SAT_ERR_* from the VDP2 layer calls pass through).
 * With state->release it disables the offset on the view's layers. */
sat_result_t sat_water_view_apply(const sat_water_view_t* view,
    const sat_water_view_state_t* state);

#ifdef __cplusplus
}
#endif
#endif
