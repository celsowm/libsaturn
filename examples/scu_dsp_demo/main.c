/* SCU DSP: a batch 3x3 transform on the DSP, compared with the SH-2 fixed-point
 * path, overlapped with SH-2 work, and a probe of DSP-initiated DMA.
 * g_dsp_demo holds what harness/tests/test_scu_dsp.py checks. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/scu_dsp.h"

#include "dma_probe_r1w1.h"
#include "dma_probe_r1w2.h"
#include "dma_probe_r2w1.h"
#include "dma_probe_r2w2.h"

#define DSP_DEMO_MAGIC 0x44535031u /* "DSP1" */
#define VERTICES 420u
#define TIMEOUT 60000u
#define PROBE_WORDS 8u

typedef struct dsp_demo_results {
    uint32_t magic;
    uint32_t matrix_status;
    uint32_t transform_status;        /* sat_dsp_transform_vertices, as -result */
    uint32_t mismatches;              /* DSP result differs from the SH-2 reference */
    uint32_t dsp_ticks;               /* FRT ticks for VERTICES on the DSP, data moved by the CPU */
    uint32_t sh2_ticks;               /* the same on the SH-2 */
    uint32_t overlap_status;
    uint32_t overlap_iterations;      /* SH-2 loop iterations while one DSP run was in flight */
    uint32_t overlap_ticks;
    uint32_t overlap_mismatches;
    uint32_t ended_seen;              /* the DSP-end flag was seen at the status port */
    uint32_t wait_status;             /* a plain start/wait/read round trip */
    uint32_t raw_value;
    uint32_t dma_status;              /* the DMA variant: sat_dsp_transform_vertices_dma, as -result */
    uint32_t dma_mismatches;
    uint32_t dma_ticks;               /* FRT ticks for VERTICES with the DSP moving the data */
    uint32_t dma_overlap_status;
    uint32_t dma_overlap_iterations;  /* SH-2 loop iterations while one DMA run was in flight */
    uint32_t dma_overlap_mismatches;
    uint32_t probe_correct[4];        /* words of the DMA probe that came back right: r1w1 r1w2 r2w1 r2w2 */
    uint32_t probe_status[4];
    uint32_t frames;
} dsp_demo_results_t;

volatile dsp_demo_results_t g_dsp_demo;

static sat_ascii_font_t font;
static int32_t g_in[3u * VERTICES];
static int32_t g_dsp_out[3u * VERTICES];
static int32_t g_sh2_out[3u * VERTICES];
static int32_t g_dma_out[3u * VERTICES];
static uint32_t g_probe_src[PROBE_WORDS] __attribute__((aligned(16)));
static uint32_t g_probe_dst[PROBE_WORDS] __attribute__((aligned(16)));

static uint16_t frt_counter(void) {
    volatile uint8_t* const high = (volatile uint8_t*)0xFFFFFE12u;
    volatile uint8_t* const low = (volatile uint8_t*)0xFFFFFE13u;
    const uint16_t h = *high;
    return (uint16_t)((h << 8u) | *low);
}

/* The path the DSP replaces: three 64-bit products per output component. */
static void transform_sh2(const int32_t* m, const int32_t* in, int32_t* out, uint32_t count) {
    for (uint32_t v = 0u; v < count; ++v) {
        const int64_t x = in[3u * v];
        const int64_t y = in[3u * v + 1u];
        const int64_t z = in[3u * v + 2u];
        for (uint32_t r = 0u; r < 3u; ++r) {
            out[3u * v + r] = (int32_t)((int64_t)m[3u * r] * x + (int64_t)m[3u * r + 1u] * y +
                                        (int64_t)m[3u * r + 2u] * z);
        }
    }
}

static uint32_t count_mismatches(const int32_t* a, const int32_t* b, uint32_t words) {
    uint32_t bad = 0u;
    for (uint32_t i = 0u; i < words; ++i) bad += a[i] != b[i];
    return bad;
}

static void probe_dma(uint32_t variant, const uint32_t* program, uint32_t count) {
    volatile uint32_t* dst = (volatile uint32_t*)((uintptr_t)g_probe_dst | 0x20000000u);
    uint32_t params[2];
    uint32_t correct = 0u;
    for (uint32_t i = 0u; i < PROBE_WORDS; ++i) {
        g_probe_src[i] = 0xA5000000u | (variant << 8u) | i;
        dst[i] = 0u;
    }
    params[0] = (uint32_t)(uintptr_t)g_probe_src >> 2u;
    params[1] = (uint32_t)(uintptr_t)g_probe_dst >> 2u;
    g_dsp_demo.probe_status[variant] = (uint32_t)(-sat_dsp_load_program(program, count, 0u));
    if (g_dsp_demo.probe_status[variant] == 0u) {
        g_dsp_demo.probe_status[variant] = (uint32_t)(-sat_dsp_write_data(3u, 0u, params, 2u));
    }
    if (g_dsp_demo.probe_status[variant] == 0u) {
        g_dsp_demo.probe_status[variant] = (uint32_t)(-sat_dsp_start(0u));
    }
    if (g_dsp_demo.probe_status[variant] == 0u) {
        g_dsp_demo.probe_status[variant] = (uint32_t)(-sat_dsp_wait(TIMEOUT));
    }
    for (uint32_t i = 0u; i < PROBE_WORDS; ++i) correct += dst[i] == g_probe_src[i];
    g_dsp_demo.probe_correct[variant] = correct;
}

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    /* A rotation about Y by 30 degrees, scaled by 3/2 (16.16), plus a shear. */
    static const int32_t matrix[9] = {84934, 0, 49152, 1000, 98304, -2000, -49152, 0, 84934};
    uint16_t start;
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_dsp_demo.magic = DSP_DEMO_MAGIC;

    for (uint32_t i = 0u; i < 3u * VERTICES; ++i) g_in[i] = (int32_t)((i * 2654435761u) >> 23u) - 256;

    /* a bare round trip through the ports and back */
    {
        const uint32_t value = 0x12345678u;
        uint32_t back = 0u;
        g_dsp_demo.wait_status = (uint32_t)(-sat_dsp_write_data(2u, 33u, &value, 1u));
        if (g_dsp_demo.wait_status == 0u) {
            g_dsp_demo.wait_status = (uint32_t)(-sat_dsp_read_data(2u, 33u, &back, 1u));
        }
        g_dsp_demo.raw_value = back;
    }

    g_dsp_demo.matrix_status = (uint32_t)(-sat_dsp_transform_set_matrix(matrix));

    /* All vectors on the DSP, and on the SH-2 for the reference and the time. */
    start = frt_counter();
    g_dsp_demo.transform_status =
        (uint32_t)(-sat_dsp_transform_vertices(g_in, g_dsp_out, VERTICES, TIMEOUT));
    g_dsp_demo.dsp_ticks = (uint16_t)(frt_counter() - start);
    start = frt_counter();
    transform_sh2(matrix, g_in, g_sh2_out, VERTICES);
    g_dsp_demo.sh2_ticks = (uint16_t)(frt_counter() - start);
    g_dsp_demo.mismatches = count_mismatches(g_dsp_out, g_sh2_out, 3u * VERTICES);

    /* One run in flight while the SH-2 does its own work. */
    {
        uint32_t iterations = 0u;
        uint32_t ended = 0u;
        static int32_t overlapped[3u * SAT_DSP_TRANSFORM_MAX_VERTICES];
        sat_dsp_status_t status;
        start = frt_counter();
        g_dsp_demo.overlap_status = (uint32_t)(-sat_dsp_transform_begin(g_in, SAT_DSP_TRANSFORM_MAX_VERTICES));
        if (g_dsp_demo.overlap_status == 0u) {
            do {
                ++iterations;
                if (sat_dsp_status(&status) == SAT_OK && status.ended != 0u) ended = 1u;
            } while (status.running != 0u || status.dma != 0u);
            g_dsp_demo.overlap_status = (uint32_t)(-sat_dsp_transform_end(overlapped, SAT_DSP_TRANSFORM_MAX_VERTICES, TIMEOUT));
        }
        g_dsp_demo.overlap_ticks = (uint16_t)(frt_counter() - start);
        g_dsp_demo.overlap_iterations = iterations;
        g_dsp_demo.ended_seen = ended;
        g_dsp_demo.overlap_mismatches = count_mismatches(overlapped, g_sh2_out, 3u * SAT_DSP_TRANSFORM_MAX_VERTICES);
    }

    /* The DSP moves the data itself: the SH-2 only starts each run. */
    start = frt_counter();
    g_dsp_demo.dma_status =
        (uint32_t)(-sat_dsp_transform_vertices_dma(g_in, g_dma_out, VERTICES, TIMEOUT));
    g_dsp_demo.dma_ticks = (uint16_t)(frt_counter() - start);
    g_dsp_demo.dma_mismatches = count_mismatches(g_dma_out, g_sh2_out, 3u * VERTICES);
    {
        uint32_t iterations = 0u;
        sat_dsp_status_t status = {0};
        for (uint32_t i = 0u; i < 3u * SAT_DSP_TRANSFORM_MAX_VERTICES; ++i) g_dma_out[i] = 0;
        g_dsp_demo.dma_overlap_status =
            (uint32_t)(-sat_dsp_transform_begin_dma(g_in, g_dma_out, SAT_DSP_TRANSFORM_MAX_VERTICES));
        if (g_dsp_demo.dma_overlap_status == 0u) {
            do {
                ++iterations;
                (void)sat_dsp_status(&status);
            } while (status.running != 0u || status.dma != 0u);
            g_dsp_demo.dma_overlap_status = (uint32_t)(-sat_dsp_transform_end_dma(TIMEOUT));
        }
        g_dsp_demo.dma_overlap_iterations = iterations;
        g_dsp_demo.dma_overlap_mismatches = count_mismatches(g_dma_out, g_sh2_out, 3u * SAT_DSP_TRANSFORM_MAX_VERTICES);
    }

    probe_dma(0u, kDmaProbeR1W1, kDmaProbeR1W1Count);
    probe_dma(1u, kDmaProbeR1W2, kDmaProbeR1W2Count);
    probe_dma(2u, kDmaProbeR2W1, kDmaProbeR2W1Count);
    probe_dma(3u, kDmaProbeR2W2, kDmaProbeR2W2Count);

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_GREEN, &pad) != SAT_OK) break;
        ++g_dsp_demo.frames;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "SCU DSP", 8, 8, 8, 0u, 0u);
        line("MISMATCHES ", g_dsp_demo.mismatches, 24);
        line("DSP TICKS ", g_dsp_demo.dsp_ticks, 40);
        line("SH2 TICKS ", g_dsp_demo.sh2_ticks, 56);
        line("OVERLAP ITER ", g_dsp_demo.overlap_iterations, 72);
        line("DMA MISMATCH ", g_dsp_demo.dma_mismatches, 88);
        line("DMA TICKS ", g_dsp_demo.dma_ticks, 104);
        line("PROBE R1W1 ", g_dsp_demo.probe_correct[0], 128);
        line("PROBE R1W2 ", g_dsp_demo.probe_correct[1], 144);
        line("PROBE R2W1 ", g_dsp_demo.probe_correct[2], 160);
        line("PROBE R2W2 ", g_dsp_demo.probe_correct[3], 176);
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
