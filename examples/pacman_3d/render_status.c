#include "render_status.h"

static int g_overflow;

void p3d_note(sat_result_t status) {
    if (status != SAT_OK && status != SAT_ERR_UNSUPPORTED) {
        g_overflow = 1;
    }
}

void p3d_note_overflow(void) {
    g_overflow = 1;
}

int p3d_render_overflowed(void) {
    return g_overflow;
}
