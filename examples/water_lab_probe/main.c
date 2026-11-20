/* Standalone ownership probe. After sat_init this program owns VDP1 command
 * RAM and FBCR/PTMR. It does not call the SDK frame submission API. */
#include "composition.h"
#include "example_util.h"
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/dma.h"
#include "saturn/time.h"
#include "saturn/vdp2_alloc.h"

enum {
    FB_WIDTH = 512, AUX_X = 320, AUX_WIDTH = 160, AUX_HEIGHT = 112,
    AUX_WORDS = AUX_WIDTH * AUX_HEIGHT,
    TRANSFER_OFFSET = 0x40000,
    SYSTEM_CLIP = 9, USER_CLIP = 8, LOCAL_COORD = 10, POLYGON = 4,
    END = 0x8000, DRAW_REPLACE = 0x00c0, DRAW_INSIDE = 0x0400
};

#define MMIO16(addr) (*(volatile uint16_t*)(uintptr_t)(addr))
#define FBCR MMIO16(0x25d00002u)
#define PTMR MMIO16(0x25d00004u)
#define EDSR MMIO16(0x25d00010u)
#define COPR MMIO16(0x25d00014u)
#define TVSTAT MMIO16(0x25f80004u)
#define VDP1_FB ((volatile uint16_t*)(uintptr_t)0x25c80000u)
#define VDP2_VRAM ((volatile uint16_t*)(uintptr_t)0x25e00000u)

typedef struct vdp1_command {
    uint16_t ctrl, link, mode, color, texture, size;
    int16_t xa, ya, xb, yb, xc, yc, xd, yd;
    uint16_t gouraud, reserved;
} vdp1_command_t;

#define COMMANDS ((volatile vdp1_command_t*)(uintptr_t)0x25c00000u)
#define GUARD SAT_RGB555(29, 2, 22)

static _Alignas(4) uint16_t staged[AUX_WORDS];

volatile struct {
    uint32_t verified;
    uint32_t failures;
    uint32_t slot_reuses;
    uint32_t published;
    uint32_t cpu_max_ticks;
    uint32_t scu_max_ticks;
    uint32_t scu_transfers;
    uint32_t max_draw_ticks;
    uint32_t frequency;
    uint32_t bad_pixel, bad_actual, bad_expected;
    uint32_t rgb_pair_rejected;
    uint32_t stage;
    uint32_t late_draws, late_uploads, partial_uploads;
    uint32_t conversion_max_ticks;
    uint32_t readback_max_ticks, qualified, mode_switches;
} water_lab_probe;

static vdp1_command_t rect(int x0, int y0, int x1, int y1, uint16_t color, uint16_t mode) {
    return (vdp1_command_t){.ctrl = POLYGON, .mode = mode, .color = color,
        .xa = x0, .ya = y0, .xb = x1, .yb = y0,
        .xc = x1, .yc = y1, .xd = x0, .yd = y1};
}

static uint16_t tag(uint32_t serial) {
    /* The high two bits of each channel survive the RGB222 probe palette. */
    const unsigned r = (serial & 3u) * 8u + ((serial >> 6) & 7u);
    const unsigned g = ((serial >> 2) & 3u) * 8u + ((serial >> 9) & 7u);
    const unsigned b = ((serial >> 4) & 3u) * 8u + ((serial >> 12) & 7u);
    return SAT_RGB555(r, g, b);
}

static uint16_t main_color(uint32_t serial) {
    return SAT_RGB555(5 + (serial & 7u) * 3u, (10 + (serial >> 3)) & 31u,
        (18 + (serial >> 8)) & 31u);
}

static void start_draw(uint32_t serial, unsigned bank) {
    volatile vdp1_command_t* const c = COMMANDS + bank;
    c[0] = (vdp1_command_t){.ctrl = LOCAL_COORD};
    c[1] = (vdp1_command_t){.ctrl = SYSTEM_CLIP, .xc = AUX_X - 1, .yc = 223};
    c[2] = rect(0, 0, AUX_X, 223, main_color(serial), DRAW_REPLACE);

    c[3] = (vdp1_command_t){.ctrl = SYSTEM_CLIP, .xc = FB_WIDTH - 1, .yc = 255};
    c[4] = (vdp1_command_t){.ctrl = USER_CLIP, .xa = AUX_X,
        .xc = AUX_X + AUX_WIDTH - 1, .yc = AUX_HEIGHT - 1};
    c[5] = (vdp1_command_t){.ctrl = LOCAL_COORD, .xa = AUX_X};
    /* Two polygons overreach every clip edge. The second leaves a zero area. */
    c[6] = rect(-2, -2, AUX_WIDTH + 2, AUX_HEIGHT + 2, 0, DRAW_REPLACE | DRAW_INSIDE);
    c[7] = rect(-2, -2, 79, AUX_HEIGHT + 2, tag(serial), DRAW_REPLACE | DRAW_INSIDE);
    c[8] = (vdp1_command_t){.ctrl = LOCAL_COORD};
    c[9] = (vdp1_command_t){.ctrl = SYSTEM_CLIP, .xc = AUX_X - 1, .yc = 223};
    c[10] = rect(272, 12, 307, 22, SAT_RGB555(28, 29, 29), DRAW_REPLACE);
    c[11] = (vdp1_command_t){.ctrl = END};
    COMMANDS[0] = (vdp1_command_t){.ctrl = 0x5000, .link = (uint16_t)(bank * 4)};
    (void)c[11].ctrl;
    PTMR = 1;
}

static int verify_pixel(unsigned x, unsigned y, uint16_t expected) {
    const uint16_t actual = VDP1_FB[y * FB_WIDTH + x];
    if (actual == expected) return 1;
    water_lab_probe.bad_pixel = y * FB_WIDTH + x;
    water_lab_probe.bad_actual = actual;
    water_lab_probe.bad_expected = expected;
    return 0;
}

static int verify_bank(uint32_t serial, int exhaustive) {
    for (unsigned y = 0; y < AUX_HEIGHT; ++y) {
        if (!verify_pixel(AUX_X - 1, y, main_color(serial)) ||
            !verify_pixel(AUX_X + AUX_WIDTH, y, GUARD) ||
            !verify_pixel(AUX_X + 79, y, tag(serial)) ||
            !verify_pixel(AUX_X + 80, y, 0)) return 0;
        for (unsigned x = 0; exhaustive && x < AUX_WIDTH; ++x) {
            if (!verify_pixel(AUX_X + x, y, x < 80 ? tag(serial) : 0)) return 0;
        }
    }
    return verify_pixel(AUX_X, AUX_HEIGHT, GUARD);
}

static void seed_guards(void) {
    for (unsigned y = 0; y < AUX_HEIGHT; ++y) {
        VDP1_FB[y * FB_WIDTH + AUX_X + AUX_WIDTH] = GUARD;
    }
    VDP1_FB[AUX_HEIGHT * FB_WIDTH + AUX_X] = GUARD;
}

static void cpu_transfer(void) {
    volatile uint16_t* dst = VDP2_VRAM + TRANSFER_OFFSET / 2;
    for (unsigned y = 0; y < AUX_HEIGHT; ++y) {
        const volatile uint16_t* src = VDP1_FB + y * FB_WIDTH + AUX_X;
        for (unsigned x = 0; x < AUX_WIDTH; ++x) dst[y * FB_WIDTH + x] = src[x];
    }
}

static sat_result_t scu_transfer(unsigned legs) {
    sat_dma_transfer_t list[SAT_DMA_MAX_LIST];
    for (unsigned leg = 0; leg < legs; ++leg) {
        for (unsigned offset = 0; offset < AUX_HEIGHT; offset += SAT_DMA_MAX_LIST) {
            for (unsigned i = 0; i < SAT_DMA_MAX_LIST; ++i) {
                const unsigned y = offset + i;
                const uint16_t* src = (const uint16_t*)VDP1_FB + y * FB_WIDTH + AUX_X;
                uint16_t* dst = (uint16_t*)VDP2_VRAM + TRANSFER_OFFSET / 2 + y * FB_WIDTH;
                list[i] = (sat_dma_transfer_t){leg ? staged + y * AUX_WIDTH : src,
                    leg ? dst : staged + y * AUX_WIDTH, AUX_WIDTH * 2};
            }
            sat_result_t result = sat_dma_start_list(0, list, SAT_DMA_MAX_LIST);
            if (result != SAT_OK) return result;
            result = sat_dma_wait(0);
            if (result != SAT_OK) return result;
        }
    }
    return SAT_OK;
}

static int verify_transfer(uint32_t serial) {
    const volatile uint16_t* vram = VDP2_VRAM + TRANSFER_OFFSET / 2;
    for (unsigned y = 0; y < AUX_HEIGHT; ++y) {
        for (unsigned x = 0; x < AUX_WIDTH; ++x) {
            if (vram[y * FB_WIDTH + x] != (x < 80 ? tag(serial) : 0)) return 0;
        }
    }
    return 1;
}

static void peak(volatile uint32_t* value, uint64_t start) {
    const uint32_t elapsed = (uint32_t)(sat_time_ticks() - start);
    if (elapsed > *value) *value = elapsed;
}

int main(void) {
    sat_example_must(sat_app_init_default());
    water_lab_probe.rgb_pair_rejected = probe_composition_pair_rgb_rejected();
    sat_example_must(sat_vdp2_back_color_set(SAT_RGB555(17, 13, 11)));
    water_lab_probe.stage = 1;
    probe_composition_synthetic();
    probe_composition_init();
    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_BITMAP, TRANSFER_OFFSET, 0x20000));
    sat_example_must(sat_vdp2_plan_commit());
    water_lab_probe.frequency = sat_time_frequency_hz();

    /* Retire initial AUTO, with no nontrivial list left by sat_init. */
    sat_example_must(sat_wait_vblank());
    PTMR = 0;
    FBCR = 2;
    sat_example_must(sat_wait_vblank());
    FBCR = 3;
    sat_example_must(sat_wait_vblank());
    while (TVSTAT & 8u) {}
    for (uint32_t serial = 1; ; ++serial) {
        /* The RGB CPU/SCU comparison deliberately runs outside the timed
         * composition path. Run it again at intervals for whole-bank checks. */
        const int exhaustive = serial <= 16 || (serial % 128u) < 2u;
        if (serial <= 2) seed_guards();
        else if (verify_bank(serial - 2, exhaustive)) ++water_lab_probe.slot_reuses;
        else {
            ++water_lab_probe.failures;
            sat_example_must(SAT_ERR_IO);
        }

        if (serial == 7) {
            ++water_lab_probe.late_draws;
            water_lab_probe.stage = 2;
            sat_example_must(sat_wait_vblank());
        }
        const unsigned bank = (serial & 1u) ? 16u : 32u;
        const uint64_t draw_start = sat_time_ticks();
        start_draw(serial, bank);
        while ((COPR != (bank + 11) * 4u || !(EDSR & 2u)) &&
               sat_time_ticks() - draw_start < water_lab_probe.frequency / 10u) {}
        if (COPR != (bank + 11) * 4u || !(EDSR & 2u)) {
            ++water_lab_probe.failures;
            sat_example_must(SAT_ERR_TIMEOUT);
        }
        peak(&water_lab_probe.max_draw_ticks, draw_start);
        if (!verify_bank(serial, exhaustive)) {
            ++water_lab_probe.failures;
            sat_example_must(SAT_ERR_IO);
        }

        const uint64_t transfer_start = sat_time_ticks();
        if (!exhaustive) {
            sat_example_must(scu_transfer(1));
            peak(&water_lab_probe.readback_max_ticks, transfer_start);
        } else if (serial & 1u) {
            cpu_transfer();
            peak(&water_lab_probe.cpu_max_ticks, transfer_start);
            sat_example_must(scu_transfer(1));
        } else {
            sat_example_must(scu_transfer(2));
            peak(&water_lab_probe.scu_max_ticks, transfer_start);
            sat_dma_stats_t stats;
            sat_dma_get_stats(&stats);
            water_lab_probe.scu_transfers = stats.scu_transfers;
        }
        if (exhaustive && !verify_transfer(serial)) {
            ++water_lab_probe.failures;
            sat_example_must(SAT_ERR_IO);
        }
        ++water_lab_probe.verified;
        if (serial == 11) {
            ++water_lab_probe.late_uploads;
            water_lab_probe.stage = 3;
            sat_example_must(sat_wait_vblank());
        }
        const uint64_t convert_start = sat_time_ticks();
        sat_result_t uploaded = probe_composition_upload(serial, staged, serial == 15 ? 12 : AUX_HEIGHT);
        if (uploaded != SAT_OK) {
            if (serial == 15) {
                ++water_lab_probe.partial_uploads;
                water_lab_probe.stage = 4;
            }
            const uint64_t idle_start = sat_time_ticks();
            while (sat_dma_busy(0) &&
                   sat_time_ticks() - idle_start < water_lab_probe.frequency / 10u) {}
            if (sat_dma_busy(0)) sat_example_must(SAT_ERR_TIMEOUT);
            sat_example_must(sat_wait_vblank());
            sat_example_must(probe_composition_upload(serial, staged, AUX_HEIGHT));
        } else {
            peak(&water_lab_probe.conversion_max_ticks, convert_start);
        }

        sat_example_must(sat_wait_vblank());
        probe_composition_publish(serial);
        FBCR = 3;
        const uint64_t swap_start = sat_time_ticks();
        while (VDP1_FB[AUX_X] == tag(serial) &&
               sat_time_ticks() - swap_start < water_lab_probe.frequency / 10u) {}
        if (VDP1_FB[AUX_X] == tag(serial)) {
            ++water_lab_probe.failures;
            sat_example_must(SAT_ERR_TIMEOUT);
        }
        water_lab_probe.published = serial;
        if (!exhaustive) ++water_lab_probe.qualified;
        water_lab_probe.stage = 5;
        if (serial == 32) {
            /* The scripted exit is serviced after upload and bank change,
             * with no submission in flight while the effect owns no VRAM. */
            probe_composition_release();
            ++water_lab_probe.mode_switches;
            for (unsigned i = 0; i < 3; ++i) sat_example_must(sat_wait_vblank());
            probe_composition_init();
            ++water_lab_probe.mode_switches;
        }
    }
}
