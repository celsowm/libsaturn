/* tvstat_probe - diagnostic example: does the guest ever observe TVSTAT change?
 *
 * Not a demo. It exists to answer one question that cannot be answered from
 * the host side, because the host can only sample VDP2 registers between
 * emulated frames -- always at the same point in the vertical phase, where the
 * VBLANK flag naturally reads the same value every time.
 *
 * The whole libsaturn frame pacing rests on saturn::hal::scu::wait_vblank()
 * spinning until TVSTAT.VBLANK changes, with a 2,000,000-iteration escape
 * hatch. If the flag never changes, every frame silently burns that entire
 * budget and the program crawls. This records, from inside the guest, whether
 * the flag moves at all -- before and after sat_init, since sat_init
 * reprograms TVMD and could be what stops the CRTC.
 *
 * Read the counters back out of a work-RAM dump:
 *   probe.exe ... --dump-wram-high wram.bin
 * and look up the symbol addresses with sh2eb-elf-nm.
 */
#include <stdint.h>

#include "saturn/core.h"

/* VDP2 TVSTAT, through the uncached mirror (0x2xxxxxxx). Deliberately NOT via
 * the HAL: this must observe the raw register with no library layer in the
 * way. */
static volatile uint16_t* const TVSTAT = (volatile uint16_t*)0x25F80004u;

#define TVSTAT_VBLANK 0x0008u

/* Roughly a few emulated frames' worth of polling at a handful of cycles per
 * iteration -- long enough that a toggling flag is certain to be seen. */
#define SAMPLE_ITERATIONS 2000000u

typedef struct tvstat_result {
    uint32_t iterations;   /* reads performed */
    uint32_t changes;      /* times the whole register value changed */
    uint32_t vblank_edges; /* times the VBLANK bit specifically flipped */
    uint16_t first;
    uint16_t last;
    uint16_t seen_or;      /* OR of every value seen */
    uint16_t seen_and;     /* AND of every value seen */
} tvstat_result_t;

/* Not static: keeps them out of reach of -O2's dead-store elimination and
 * makes them easy to find with nm. */
volatile tvstat_result_t g_before_init;
volatile tvstat_result_t g_after_init;
volatile uint32_t g_init_status = 0xFFFFFFFFu;
volatile uint32_t g_done = 0u;

static void sample(volatile tvstat_result_t* out) {
    uint16_t prev = *TVSTAT;
    uint16_t seen_or = prev;
    uint16_t seen_and = prev;
    uint32_t changes = 0u;
    uint32_t edges = 0u;
    uint32_t i;

    out->first = prev;
    for (i = 0; i < SAMPLE_ITERATIONS; ++i) {
        const uint16_t v = *TVSTAT;
        seen_or = (uint16_t)(seen_or | v);
        seen_and = (uint16_t)(seen_and & v);
        if (v != prev) {
            ++changes;
            if (((v ^ prev) & TVSTAT_VBLANK) != 0u) {
                ++edges;
            }
            prev = v;
        }
    }
    out->iterations = i;
    out->changes = changes;
    out->vblank_edges = edges;
    out->last = prev;
    out->seen_or = seen_or;
    out->seen_and = seen_and;
}

int main(void) {
    sat_video_config_t video = {320, 224, 1u, 0u};

    /* Before touching the library at all: whatever state the BIOS left. */
    sample(&g_before_init);

    g_init_status = (uint32_t)sat_init(&video);

    /* After sat_init has reprogrammed TVMD and the rest of the CRTC. */
    sample(&g_after_init);

    g_done = 0xD01E0000u; /* completion marker */
    while (1) {
    }
    return 0;
}
