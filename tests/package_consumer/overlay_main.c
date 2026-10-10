#include <saturn/saturn.h>
#include <stdint.h>

extern const uint8_t ovl_a_table[12], ovl_b_table[20], resident_table[8];
extern const uint8_t __ovl_a_start[], __ovl_a_end[], __ovl_b_start[], __ovl_b_end[];

volatile const uint8_t *g_keep[] = { ovl_a_table, ovl_b_table, resident_table, __ovl_a_start,
                                     __ovl_a_end, __ovl_b_start, __ovl_b_end };

int main(void) {
    sat_pad_state_t pad;

    g_keep[0] = g_keep[1];  /* the table must be reachable, or --gc-sections drops it and its data */

    if (sat_app_init_default() != SAT_OK) {
        for (;;) {
        }
    }
    for (;;) {
        if (sat_app_frame_begin(SAT_COLOR_BLUE, SAT_COLOR_BLACK, &pad) != SAT_OK) {
            continue;
        }
        (void)sat_app_frame_end();
    }
}
