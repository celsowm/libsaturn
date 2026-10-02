/* Minimal Saturn program that consumes an *installed* LibSaturn package.
 *
 * It only exists to prove the package contract: headers resolve from the
 * install prefix, the runtime links with the installed linker script and the
 * startup objects survive the link. It uses the public umbrella header and
 * nothing from the repository tree. */
#include <saturn/saturn.h>

#if !defined(SATURN_VERSION_STRING) || !defined(SATURN_VERSION_AT_LEAST)
#error "the installed package must provide <saturn/version.h>"
#endif

int main(void) {
    sat_pad_state_t pad;

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
