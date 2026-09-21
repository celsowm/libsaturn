#ifndef P3D_RENDER_STATUS_H
#define P3D_RENDER_STATUS_H

/* One flag every drawing module reports into: set when a draw call failed
 * for a reason other than geometry being behind the camera -- the VDP1
 * command list filling up, usually. The HUD shows it, so an overflow reads
 * as "RENDER LIMIT" on screen instead of as geometry that silently
 * vanished. */

#include "saturn/core.h"

/* Records `status`. SAT_ERR_UNSUPPORTED just means the geometry is behind
 * the camera and is not a failure. */
void p3d_note(sat_result_t status);

/* Marks a failure that did not come from a status code -- a bake table
 * running out of room. */
void p3d_note_overflow(void);

int p3d_render_overflowed(void);

#endif /* P3D_RENDER_STATUS_H */
