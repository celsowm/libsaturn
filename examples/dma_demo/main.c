/* SCU DMA acceptance: every legal route, an indirect list, read-back,
 * cache coherence, the CPU fallback and a timing comparison. Each check
 * reads the destination back and compares it with what the CPU path
 * produces, so the result does not depend on how fast the emulator is. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/dma.h"
#include "saturn/font.h"
#include "saturn/irq.h"
#include "saturn/time.h"

#define DMA_DEMO_MAGIC 0x444D4131u /* "DMA1" */
#define WORDS 8192u                /* 32 KiB source pattern */
#define BYTES (WORDS * 4u)
#define VDP1_SCRATCH 0x25C70000u   /* top 64 KiB of VDP1 VRAM: no texture lives here */
#define VDP2_SCRATCH 0x25E60000u
#define SCSP_SCRATCH 0x25A60000u
#define BENCH_ROUNDS 16u

/* Read by harness/tests/test_dma_demo.py from a Work RAM dump. */
typedef struct dma_demo_results {
    uint32_t magic;
    uint32_t done;
    uint32_t irq_active;
    uint32_t direct_vdp1;
    uint32_t direct_vdp2;
    uint32_t direct_scsp;
    uint32_t matches_cpu_path;
    uint32_t readback;
    uint32_t indirect;
    uint32_t indirect_end_irq;
    uint32_t cache_coherent;
    uint32_t illegal_rejected;
    uint32_t ram_copy_on_cpu;
    uint32_t small_copy_on_cpu;
    uint32_t cpu_ms;
    uint32_t dma_ms;
    uint32_t scu_transfers;
    uint32_t timeouts;
    uint32_t illegal;
} dma_demo_results_t;

volatile dma_demo_results_t g_dma_demo;

static uint32_t g_src[WORDS] __attribute__((aligned(16)));
static uint32_t g_back[WORDS] __attribute__((aligned(16)));
static volatile uint32_t g_end_irqs;

static sat_ascii_font_t font;

static void on_dma1_end(void* user) {
    (void)user;
    g_end_irqs = g_end_irqs + 1u;
}

static void fill_pattern(uint32_t seed) {
    for (uint32_t i = 0u; i < WORDS; ++i) {
        seed = seed * 1664525u + 1013904223u;
        g_src[i] = seed ^ (i << 7u);
    }
}

/* Compares 16-bit reads of a B-bus region with g_src. */
static uint32_t region_matches(uint32_t address, const uint32_t* expect, uint32_t bytes) {
    const volatile uint16_t* r = (const volatile uint16_t*)address;
    const uint16_t* e = (const uint16_t*)expect;
    for (uint32_t i = 0u; i < bytes / 2u; ++i) {
        if (r[i] != e[i]) return 0u;
    }
    return 1u;
}

static void clear_region(uint32_t address, uint32_t bytes) {
    volatile uint16_t* r = (volatile uint16_t*)address;
    for (uint32_t i = 0u; i < bytes / 2u; ++i) r[i] = 0u;
}

static uint32_t words_equal(const uint32_t* a, const uint32_t* b, uint32_t words) {
    for (uint32_t i = 0u; i < words; ++i) {
        if (a[i] != b[i]) return 0u;
    }
    return 1u;
}

static uint32_t direct_uploads(void) {
    clear_region(VDP1_SCRATCH, BYTES);
    clear_region(VDP2_SCRATCH, BYTES);
    clear_region(SCSP_SCRATCH, BYTES);
    fill_pattern(1u);
    g_dma_demo.direct_vdp1 =
        sat_dma_copy((void*)VDP1_SCRATCH, g_src, BYTES) == SAT_OK &&
        sat_dma_scu_capable((void*)VDP1_SCRATCH, g_src, BYTES) &&
        region_matches(VDP1_SCRATCH, g_src, BYTES);
    g_dma_demo.direct_vdp2 =
        sat_dma_copy((void*)VDP2_SCRATCH, g_src, BYTES) == SAT_OK &&
        region_matches(VDP2_SCRATCH, g_src, BYTES);
    g_dma_demo.direct_scsp =
        sat_dma_copy((void*)SCSP_SCRATCH, g_src, BYTES) == SAT_OK &&
        region_matches(SCSP_SCRATCH, g_src, BYTES);
    return g_dma_demo.direct_vdp1 && g_dma_demo.direct_vdp2 && g_dma_demo.direct_scsp;
}

/* The same copy on the SCU and on the CPU must leave the same bytes. */
static void compare_with_cpu_path(void) {
    fill_pattern(2u);
    clear_region(VDP1_SCRATCH, BYTES);
    (void)sat_dma_copy((void*)VDP1_SCRATCH, g_src, BYTES);
    (void)sat_dma_copy(g_back, (void*)VDP1_SCRATCH, BYTES); /* B-bus to Work RAM-H */
    g_dma_demo.readback = words_equal(g_src, g_back, WORDS);
    sat_dma_set_enabled(0);
    clear_region(VDP2_SCRATCH, BYTES);
    (void)sat_dma_copy((void*)VDP2_SCRATCH, g_src, BYTES);
    sat_dma_set_enabled(1);
    g_dma_demo.matches_cpu_path =
        region_matches(VDP1_SCRATCH, g_src, BYTES) && region_matches(VDP2_SCRATCH, g_src, BYTES);
}

/* Three transfers, one start, one end interrupt. */
static void indirect_list(void) {
    clear_region(VDP1_SCRATCH, 0x3000u);
    fill_pattern(3u);
    sat_dma_transfer_t list[3] = {
        {&g_src[0], (void*)(VDP1_SCRATCH + 0x0000u), 0x200u},
        {&g_src[256], (void*)(VDP1_SCRATCH + 0x1000u), 0x100u},
        {&g_src[512], (void*)(VDP1_SCRATCH + 0x2000u), 0x400u},
    };
    const uint32_t before = g_end_irqs;
    uint8_t ok = sat_dma_start_list(1u, list, 3u) == SAT_OK && sat_dma_wait(1u) == SAT_OK;
    ok = ok && region_matches(VDP1_SCRATCH + 0x0000u, &g_src[0], 0x200u) &&
         region_matches(VDP1_SCRATCH + 0x1000u, &g_src[256], 0x100u) &&
         region_matches(VDP1_SCRATCH + 0x2000u, &g_src[512], 0x400u);
    g_dma_demo.indirect = ok;
    for (uint32_t i = 0u; i < 2000000u && g_end_irqs == before; ++i) {
    }
    g_dma_demo.indirect_end_irq = g_end_irqs - before;
}

/* The SCU writes Work RAM behind the cache: a stale line would show the old
 * fill instead of the VRAM contents. */
static void cache_coherence(void) {
    fill_pattern(4u);
    (void)sat_dma_copy((void*)VDP1_SCRATCH, g_src, 0x1000u);
    for (uint32_t i = 0u; i < 1024u; ++i) g_back[i] = 0xA5A5A5A5u;
    uint32_t sum = 0u;
    for (uint32_t i = 0u; i < 1024u; ++i) sum += g_back[i]; /* pulls the lines in */
    (void)sum;
    uint8_t ok = sat_dma_copy(g_back, (void*)VDP1_SCRATCH, 0x1000u) == SAT_OK;
    for (uint32_t i = 0u; i < 1024u; ++i) ok = ok && g_back[i] == g_src[i];
    g_dma_demo.cache_coherent = ok;
}

static void fallbacks(void) {
    sat_dma_stats_t s;
    /* Work RAM-H to itself is not a legal route: refused, then copied by CPU. */
    g_dma_demo.illegal_rejected =
        sat_dma_start(0u, g_src, g_back, 256u) == SAT_ERR_UNSUPPORTED &&
        sat_dma_start(0u, (void*)VDP2_SCRATCH, g_back, 256u) == SAT_ERR_UNSUPPORTED &&
        sat_dma_start(0u, g_src, (void*)VDP1_SCRATCH, 6u) == SAT_ERR_INVALID_ARG &&
        sat_dma_start(1u, g_src, (void*)VDP1_SCRATCH, 0x2000u) == SAT_ERR_INVALID_ARG;
    fill_pattern(5u);
    (void)sat_dma_copy(g_back, g_src, 1024u);
    sat_dma_get_stats(&s);
    g_dma_demo.ram_copy_on_cpu = s.last_path == SAT_DMA_PATH_CPU && words_equal(g_src, g_back, 256u);
    (void)sat_dma_copy((void*)VDP1_SCRATCH, g_src, 32u);
    sat_dma_get_stats(&s);
    g_dma_demo.small_copy_on_cpu =
        s.last_path == SAT_DMA_PATH_CPU && region_matches(VDP1_SCRATCH, g_src, 32u);
}

static uint32_t timed_uploads(void) {
    const uint32_t start = sat_time_ms();
    for (uint32_t i = 0u; i < BENCH_ROUNDS; ++i) (void)sat_dma_copy((void*)VDP1_SCRATCH, g_src, BYTES);
    return sat_time_ms() - start;
}

static void run_tests(void) {
    g_dma_demo.irq_active = (uint32_t)sat_irq_active();
    if (g_dma_demo.irq_active) {
        (void)sat_irq_set_handler(SAT_IRQ_DMA1_END, on_dma1_end, 0);
    }
    (void)direct_uploads();
    compare_with_cpu_path();
    indirect_list();
    cache_coherence();
    fallbacks();
    fill_pattern(6u);
    sat_dma_set_enabled(0);
    g_dma_demo.cpu_ms = timed_uploads();
    sat_dma_set_enabled(1);
    g_dma_demo.dma_ms = timed_uploads();
    sat_dma_stats_t s;
    sat_dma_get_stats(&s);
    g_dma_demo.scu_transfers = s.scu_transfers;
    g_dma_demo.timeouts = s.timeouts;
    g_dma_demo.illegal = s.illegal;
    g_dma_demo.done = 1u;
}

static uint32_t all_ok(void) {
    return g_dma_demo.direct_vdp1 && g_dma_demo.direct_vdp2 && g_dma_demo.direct_scsp &&
           g_dma_demo.matches_cpu_path && g_dma_demo.readback && g_dma_demo.indirect &&
           g_dma_demo.cache_coherent && g_dma_demo.illegal_rejected &&
           g_dma_demo.ram_copy_on_cpu && g_dma_demo.small_copy_on_cpu &&
           g_dma_demo.timeouts == 0u && g_dma_demo.illegal == 0u;
}

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(
            &font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_dma_demo.magic = DMA_DEMO_MAGIC;
    uint32_t frame = 0u;
    for (;;) {
        sat_pad_state_t pad = {0};
        const uint32_t ok = g_dma_demo.done ? all_ok() : 1u;
        if (sat_app_frame_begin(SAT_COLOR_BLACK,
                ok ? SAT_COLOR_GREEN : SAT_COLOR_RED, &pad) != SAT_OK) break;
        if (++frame == 30u) run_tests();
        (void)sat_ascii_font_draw_text_screen_indexed8(
            &font, "SCU DMA", 8, 8, 8, 0u, 0u);
        if (g_dma_demo.done) {
            line("VDP1 DIRECT  ", g_dma_demo.direct_vdp1, 32);
            line("VDP2 DIRECT  ", g_dma_demo.direct_vdp2, 48);
            line("SCSP DIRECT  ", g_dma_demo.direct_scsp, 64);
            line("SAME AS CPU  ", g_dma_demo.matches_cpu_path, 80);
            line("INDIRECT     ", g_dma_demo.indirect, 96);
            line("CACHE OK     ", g_dma_demo.cache_coherent, 112);
            line("CPU MS       ", g_dma_demo.cpu_ms, 136);
            line("DMA MS       ", g_dma_demo.dma_ms, 152);
        }
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
