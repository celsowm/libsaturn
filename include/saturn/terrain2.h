#ifndef SATURN_TERRAIN2_H
#define SATURN_TERRAIN2_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/collide2d.h"
#include "saturn/math2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Oriented 2D terrain queries over read-only tile profiles.
 *
 * Terrain2 answers "how far to the nearest support along this direction, where
 * is the contact, which way does the surface face" for any caller: enemies,
 * projectiles, items and characters use it alike. It does not depend on the
 * renderer, the character controller or any scheduler.
 *
 * Coordinates are whole world pixels, X right and Y down. Tiles are 8 x 8
 * pixels. Angles follow saturn/math2d.h (256 steps per turn, 0 = +X, 64 = +Y).
 *
 * Surface angle. A profile's `angle` is the direction of travel along its
 * surface with the solid on the right-hand side of that travel, so a flat floor
 * is 0, a flat ceiling 128, and a wall whose solid lies to its right 192. The
 * outward normal is (sin a, -cos a); the tangent is (cos a, sin a). Flipping a
 * tile in X maps the angle to -a, flipping in Y maps it to 128 - a.
 *
 * Ownership. Every pointer in sat_terrain_map2_t is borrowed and read-only. The
 * caller keeps profiles, metatiles and layer cells alive and unchanged for as
 * long as the map is used; the runtime allocates nothing. Queries are pure:
 * the same map and arguments always give the same result, in bounded time
 * (at most range / 8 + 2 tile lookups per probe).
 */

#define SAT_TERRAIN2_TILE_PX 8
#define SAT_TERRAIN2_MAX_LAYERS 4

/* Tile word: 10-bit profile index, flips, and 4 bits the game may use freely. */
#define SAT_TERRAIN2_TILE_INDEX_MASK 0x03FFu
#define SAT_TERRAIN2_TILE_FLIP_X 0x0400u
#define SAT_TERRAIN2_TILE_FLIP_Y 0x0800u
#define SAT_TERRAIN2_TILE_USER_MASK 0xF000u

/* Profile flags. Bits 4-7 belong to the game and come back in hit.flags. */
#define SAT_TERRAIN2_ONE_WAY 0x01u
#define SAT_TERRAIN2_FLAG_USER_MASK 0xF0u

/* One tile's collision shape.
 *
 * column[x] is the solid extent of pixel column x, row[y] that of pixel row y,
 * each in -8..8. A positive value anchors the solid to the bottom (columns) or
 * right (rows) edge of the tile and gives its length in pixels; a negative value
 * anchors it to the top or left edge; 0 is empty and +-8 is full. Columns are
 * authoritative for vertical probes and the pixel mask, rows for horizontal
 * probes, so both tables must describe the same shape
 * (sat_terrain_profile2_validate checks this).
 *
 * `category` is a game-defined bit set matched against a query's category_mask.
 * `material` is returned untouched. Size: 22 bytes. */
typedef struct sat_terrain_profile2 {
    int8_t column[8];
    int8_t row[8];
    uint16_t material;
    uint8_t angle;
    uint8_t flags;
    uint8_t category;
    uint8_t reserved;
} sat_terrain_profile2_t;

typedef enum sat_terrain_outside2 {
    SAT_TERRAIN2_OUTSIDE_EMPTY = 0, /* beyond the map there is nothing */
    SAT_TERRAIN2_OUTSIDE_SOLID = 1, /* beyond the map everything is solid */
    SAT_TERRAIN2_OUTSIDE_CLAMP = 2  /* the edge tiles repeat outwards */
} sat_terrain_outside2_t;

/* A map is a metatile table plus up to four logical layers of metatile indices.
 * Each metatile is (1 << metatile_shift) x (1 << metatile_shift) tile words, so
 * tile lookup never divides. Which layer is "foreground" or "background" is the
 * game's business: a query simply names a layer index. */
typedef struct sat_terrain_map2 {
    const sat_terrain_profile2_t* profiles;
    const uint16_t* metatiles;
    const uint16_t* layers[SAT_TERRAIN2_MAX_LAYERS];
    uint16_t profile_count;
    uint16_t metatile_count;
    uint16_t cols; /* map width in metatiles */
    uint16_t rows; /* map height in metatiles */
    uint8_t layer_count;
    uint8_t metatile_shift;
    uint8_t outside;
    uint8_t reserved;
    int32_t width_px;
    int32_t height_px;
} sat_terrain_map2_t;

/* Filtering applied before any narrow-phase work. A profile takes part in a
 * query only if (profile.category & category_mask) != 0 and none of its flag
 * bits are set in ignore_flags. A one-way profile additionally takes part only
 * when the probe direction opposes its surface normal, whatever its orientation. */
typedef struct sat_terrain_query2 {
    uint8_t layer;
    uint8_t category_mask;
    uint8_t ignore_flags;
    uint8_t reserved;
} sat_terrain_query2_t;

static inline sat_terrain_query2_t sat_terrain_query2_default(void) {
    sat_terrain_query2_t q;
    q.layer = 0;
    q.category_mask = 0xFFu;
    q.ignore_flags = 0;
    q.reserved = 0;
    return q;
}

typedef struct sat_terrain_hit2 {
    sat_vec2_t point;    /* contact point on the surface boundary, 16.16 pixels */
    sat_vec2_t normal;   /* unit, pointing out of the solid towards the probe */
    sat_vec2_t tangent;  /* unit, travel direction with the solid on the right */
    sat_fx16_t distance; /* probe: signed whole pixels (see sat_terrain2_probe);
                            cast: fraction of the cast vector in [0, 1] */
    uint32_t flags;      /* profile flags */
    uint32_t material;   /* profile material */
    int32_t tile_x;      /* tile coordinates of the surface pixel */
    int32_t tile_y;
    uint16_t collider_id; /* profile index, or 0xFFFF for the solid outside */
    uint8_t layer;
    uint8_t angle;       /* surface angle of `normal`/`tangent` (sat_angle_t) */
} sat_terrain_hit2_t;

/* Probe directions are quadrants: 0 = +X, 1 = +Y (down), 2 = -X, 3 = -Y (up),
 * the same numbering as sat_angle_quadrant(). */
enum {
    SAT_TERRAIN2_DIR_RIGHT = 0,
    SAT_TERRAIN2_DIR_DOWN = 1,
    SAT_TERRAIN2_DIR_LEFT = 2,
    SAT_TERRAIN2_DIR_UP = 3
};

/* Fills a profile from eight column heights (each -8..8, same meaning as
 * column[]), deriving the row table. Rows are single runs anchored to a side, so
 * a shape whose row has a hole (a valley or a hill) fails with SAT_ERR_INVALID_ARG:
 * split it across tiles. */
sat_result_t sat_terrain_profile2_from_columns(sat_terrain_profile2_t* out,
    const int8_t heights[8], uint8_t angle, uint8_t flags, uint8_t category,
    uint16_t material);

/* SAT_OK when every value is in range and the column and row tables describe
 * the same 64-pixel mask; SAT_ERR_INVALID_ARG otherwise. */
sat_result_t sat_terrain_profile2_validate(const sat_terrain_profile2_t* profile);

/* Bytes of read-only data a map of this shape needs (profiles, metatiles and
 * `layers` layers of cols x rows indices), for resource planning. */
sat_result_t sat_terrain_map2_requirements(uint16_t profile_count,
    uint16_t metatile_count, uint8_t metatile_shift, uint16_t cols,
    uint16_t rows, uint8_t layers, uint32_t* out_bytes);

/* Binds a map to caller-owned data. metatile_shift is 0..5. Layers start empty
 * (layer_count 0); add each with sat_terrain_map2_add_layer. */
sat_result_t sat_terrain_map2_init(sat_terrain_map2_t* map,
    const sat_terrain_profile2_t* profiles, uint16_t profile_count,
    const uint16_t* metatiles, uint16_t metatile_count, uint8_t metatile_shift,
    uint16_t cols, uint16_t rows);
/* `cells` holds cols * rows metatile indices, row-major. SAT_ERR_CAPACITY once
 * four layers are present. */
sat_result_t sat_terrain_map2_add_layer(sat_terrain_map2_t* map, const uint16_t* cells);
sat_result_t sat_terrain_map2_set_outside(sat_terrain_map2_t* map, sat_terrain_outside2_t policy);
/* Checks every layer cell and tile word against the table sizes. Debug and
 * tool aid; O(map size). */
sat_result_t sat_terrain_map2_validate(const sat_terrain_map2_t* map);

/* Whether the pixel is solid for this query (columns decide). A point query has
 * no direction, so one-way profiles never count as solid here. A NULL query means
 * sat_terrain_query2_default() for every function below. */
int sat_terrain2_solid_at(const sat_terrain_map2_t* map, int32_t x, int32_t y,
    const sat_terrain_query2_t* query);

/* Tile-level sample: the profile under pixel (x, y), with its normal and
 * tangent, whether or not that exact pixel is solid. SAT_ERR_NOT_FOUND when the
 * tile is empty or filtered out. hit.distance is 0. */
sat_result_t sat_terrain2_sample(const sat_terrain_map2_t* map, int32_t x, int32_t y,
    const sat_terrain_query2_t* query, sat_terrain_hit2_t* out);

/* Axis-aligned support probe, the fast path.
 *
 * Starting at the sensor pixel (x, y) and moving along `dir`:
 *   - sensor pixel free: distance = number of free pixels between it and the
 *     first solid pixel (0 means the next pixel is solid, i.e. the sensor is
 *     touching), found within `range` pixels;
 *   - sensor pixel solid: distance = -(length of the solid run that ends at the
 *     sensor pixel, counted backwards), so -1 means one pixel deep, found within
 *     `range` pixels.
 * Either way, moving the sensor by `distance` along `dir` leaves it touching.
 * hit.point is the boundary between the last free and first solid pixel on the
 * sensor's line, at the pixel centre across the probe.
 *
 * Returns SAT_OK with *out filled, SAT_ERR_NOT_FOUND when no surface lies within
 * `range`, SAT_ERR_INVALID_ARG for a bad map, layer, dir or range (1..255). */
sat_result_t sat_terrain2_probe(const sat_terrain_map2_t* map, int32_t x, int32_t y,
    uint8_t dir, int32_t range, const sat_terrain_query2_t* query,
    sat_terrain_hit2_t* out);

/* Longest cast, in pixel steps along the dominant axis. */
#define SAT_TERRAIN2_CAST_MAX_STEPS 512

/* Arbitrary-direction cast: steps one pixel at a time along the dominant axis
 * of `delta` (16.16 pixels) and reports the first solid pixel, or SAT_ERR_NOT_FOUND.
 * hit.distance is the fraction of `delta` travelled (0 when the origin is already
 * solid). This is the slower general form; use sat_terrain2_probe for support
 * queries. SAT_ERR_INVALID_ARG if the cast is longer than SAT_TERRAIN2_CAST_MAX_STEPS. */
sat_result_t sat_terrain2_cast(const sat_terrain_map2_t* map, sat_vec2_t origin,
    sat_vec2_t delta, const sat_terrain_query2_t* query, sat_terrain_hit2_t* out);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_TERRAIN2_H */
