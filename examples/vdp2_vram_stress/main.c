/* vdp2_vram_stress: the transactional VDP2 resource plan under load.
 *
 * The plan (sat_vdp2_plan_* in saturn/vdp2_alloc.h) owns the semantic ledger
 * over the byte free list: what each range is for, which banks it may live
 * in, and an all-or-nothing transaction. This demo drives it through every
 * error path the plan documentation promises, on a visible four-layer
 * picture, and records the outcome of each step in g_vram_stress for the
 * harness (harness/tests/test_vdp2_vram_stress.py):
 *
 *   scenario 0  a legal four-layer layout: pattern names spread one bank
 *               each (A0, A1, B0), NBG3 sharing A0's tail, B1 left free.
 *   scenario 1  an oversized bitmap reservation is refused with CAPACITY
 *               and the rollback restores the exact free space.
 *   scenario 2  a whole-bank claim: pattern names in the claimed bank are
 *               refused with BUSY, a line scroll table there is accepted.
 *   scenario 3  claiming a bank that hosts committed NBG reservations is
 *               refused with BUSY and changes nothing.
 *   scenario 4  fragmentation: a block larger than any hole is refused
 *               although the total free space would allow it.
 *   scenario 5  reclaim and reconfigure: everything is released and a
 *               second, different layout (two banks instead of three) is
 *               planned, committed and configured live.
 *
 * The screen shows one colour band per layer (the layer's data lives in
 * that bank) and a per-bank usage bar; every refusal leaves the display
 * registers untouched, which the harness checks by comparing the cycle
 * pattern snapshot taken after scenario 0 with the one at the end.
 *
 * Controls: A advances to the next scenario at once, B toggles the debug
 * numbers overlay, START exits. The scenarios also advance by themselves
 * at fixed frames so an unattended emulator run sees all of them. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/render2d.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_alloc.h"
#include "saturn/vdp2_layers.h"

#define VRS_MAGIC SAT_FOURCC('V', 'R', 'S', '1')
#define SCENARIOS 6u

typedef struct vram_stress_results {
    uint32_t magic;
    uint32_t scenario_ran;          /* bit per scenario, 0x3F when all ran */
    uint32_t planned_layout_ok;     /* the first plan commit */
    uint32_t layers_configured[4];
    uint32_t names_bases[4];        /* 0, 0x20000, 0x40000, 0x2040-ish */
    uint32_t free_before_rollback;  /* total free bytes, summed over banks */
    uint32_t free_after_rollback;
    uint32_t oversized_refused;     /* SAT_ERR_CAPACITY */
    uint32_t names_in_claimed_bank; /* SAT_ERR_BUSY */
    uint32_t table_in_claimed_bank; /* SAT_OK */
    uint32_t claim_over_live;       /* SAT_ERR_BUSY */
    uint32_t banks_released;        /* SAT_OK */
    uint32_t fragmentation_detected; /* 1: big reserve refused, total free larger */
    uint32_t largest_hole;          /* sat_vdp2_vram_largest_free() at that point */
    uint32_t total_free;            /* summed over banks at that point */
    uint32_t reconfigured_ok;       /* second layout: commit + 4x configure */
    uint32_t names_bases_b[4];
    uint32_t cycle_after_first[8];  /* snapshot once layout A is committed */
    uint32_t cycle_mid[8];          /* after every refusal scenario: equal */
    uint32_t cycle_at_end[8];       /* after the reconfiguration: different */
    uint32_t refusals;              /* plan telemetry at the end */
    uint32_t last_status;
    uint32_t free_bytes_per_bank[4];
    uint32_t frames;
} vram_stress_results_t;

volatile vram_stress_results_t g_vram_stress;

static sat_ascii_font_t font;
static uint16_t g_names[64u * 64u];
static uint16_t g_cells[2u * 16u];
static uint32_t g_debug_overlay = 1u;

#define NAMES_BYTES 0x2000u
#define CHARS_BYTES 0x40u

static const uint32_t AUTO_FRAME[SCENARIOS] = {0u, 90u, 120u, 150u, 180u, 210u};

static uint32_t total_free_bytes(void) {
    sat_vdp2_plan_usage_t usage;
    sat_vdp2_plan_usage(&usage);
    return usage.free_bytes_per_bank[0] + usage.free_bytes_per_bank[1]
         + usage.free_bytes_per_bank[2] + usage.free_bytes_per_bank[3];
}

/* Cell 0 transparent, cell 1 solid dot 1 (4bpp). */
static void build_cells(void) {
    for (uint32_t i = 0u; i < 16u; ++i) {
        g_cells[i] = 0u;
        g_cells[16u + i] = 0x1111u;
    }
}

/* Name rows [band, band + count) show the layer's colour; the rest is
 * transparent so the four bands stack without covering each other. */
static void fill_names(uint32_t band, uint32_t count, uint32_t palette) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            const uint16_t cell = (y >= band && y < band + count)
                ? (uint16_t)((palette << 12u) | 1u) : 0u;
            g_names[y * 64u + x] = cell;
        }
    }
}

static void upload_palettes(void) {
    static const uint16_t colours[4] = {
        SAT_RGB555(29, 6, 6), SAT_RGB555(6, 29, 6),
        SAT_RGB555(6, 10, 31), SAT_RGB555(29, 6, 29)};
    uint16_t pal[16];
    for (uint32_t b = 0u; b < 4u; ++b) {
        for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
        pal[1] = colours[b];
        (void)sat_vdp2_palette_upload(pal, 16u, (1u + b) * 16u);
    }
}

/* Writes one layer's names and cells at the planned addresses and
 * configures it. `bank` is the SAT_VDP2_BANK_* its characters read. */
static sat_result_t setup_layer(uint32_t layer, uint32_t names, uint32_t chars,
                                uint8_t bank, uint8_t priority,
                                uint32_t band, uint32_t palette) {
    fill_names(band, 2u, palette);
    SAT_TRY(sat_vdp2_vram_write_words(names / 2u, g_names, 64u * 64u));
    SAT_TRY(sat_vdp2_vram_write_words(chars / 2u, g_cells, 2u * 16u));
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = names;
    c.char_base_address = chars;
    c.char_bank_mask = bank;
    c.priority = priority;
    return sat_vdp2_layer_configure(&c);
}

/* Scenario 0: names spread one bank each (0x20000 alignment); character
 * windows are pinned 0x8000-aligned inside the same bank as the layer's
 * declared char_bank_mask (the base must be 32 KiB aligned and a window
 * straddling a bank edge could not be read by one configuration). NBG3
 * packs into A0 so B1 stays free of NBG kinds and can be claimed later.
 * Deterministic bases the harness asserts on:
 *   names 0x00000, 0x20000, 0x40000, 0x02000
 *   chars 0x08000 (A0), 0x28000 (A1), 0x48000 (B0), 0x10000 (A0). */
static void scenario_legal_layout(void) {
    sat_vdp2_plan_reset();
    sat_vdp2_vram_alloc_reset();
    upload_palettes();
    if (sat_vdp2_plan_begin() != SAT_OK) return;
    static const uint32_t align[4] = {0x20000u, 0x20000u, 0x20000u, 0x2000u};
    static const uint32_t chars_at[4] = {0x08000u, 0x28000u, 0x48000u, 0x10000u};
    static const uint8_t bank[4] = {
        SAT_VDP2_BANK_A0, SAT_VDP2_BANK_A1, SAT_VDP2_BANK_B0, SAT_VDP2_BANK_A0};
    static const uint32_t band[4] = {3u, 7u, 11u, 15u};
    for (uint32_t l = 0u; l < 4u; ++l) {
        uint32_t names = 0u;
        if (sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_PATTERN_NAMES, NAMES_BYTES,
                                  align[l], &names) != SAT_OK
            || sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_CHARACTERS,
                                         chars_at[l], CHARS_BYTES) != SAT_OK) {
            (void)sat_vdp2_plan_rollback();
            return;
        }
        g_vram_stress.names_bases[l] = names;
        g_vram_stress.layers_configured[l] =
            setup_layer(l, names, chars_at[l], bank[l],
                        (uint8_t)(1u + l), band[l], 1u + l);
    }
    g_vram_stress.planned_layout_ok = sat_vdp2_plan_commit();
    {
        uint16_t cycle[8];
        (void)sat_vdp2_layer_cycle_patterns(cycle);
        for (uint32_t i = 0u; i < 8u; ++i) {
            g_vram_stress.cycle_after_first[i] = cycle[i];
        }
    }
}

/* Scenario 1: no run of unclaimed banks can host a 0x40000 bitmap any
 * more; the refusal must restore the exact free space. */
static void scenario_oversized(void) {
    g_vram_stress.free_before_rollback = total_free_bytes();
    uint32_t base = 0u;
    if (sat_vdp2_plan_begin() == SAT_OK) {
        g_vram_stress.oversized_refused =
            sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_BITMAP, 0x40000u,
                                  0x20000u, &base);
        (void)sat_vdp2_plan_rollback();
    }
    g_vram_stress.free_after_rollback = total_free_bytes();
}

/* Scenario 2: B1 hosts no NBG kinds, so it can be claimed. Pattern names
 * are then refused inside it; a line scroll table is accepted (manual
 * 5.3: no cycle pattern needed), and the claim is released again. */
static void scenario_bank_claim(void) {
    if (sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_B1) != SAT_OK) return;
    /* Pattern names inside the claimed bank are refused even inside a
     * transaction, and the refusal stages nothing. */
    if (sat_vdp2_plan_begin() == SAT_OK) {
        g_vram_stress.names_in_claimed_bank =
            sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_PATTERN_NAMES,
                                     0x60000u, NAMES_BYTES);
        (void)sat_vdp2_plan_rollback();
    }
    if (sat_vdp2_plan_begin() == SAT_OK) {
        const sat_result_t table = sat_vdp2_plan_reserve_at(
            SAT_VDP2_RESERVE_LINE_SCROLL, 0x60040u, 0x400u);
        g_vram_stress.table_in_claimed_bank = table;
        if (table == SAT_OK) {
            (void)sat_vdp2_plan_commit();
        } else {
            (void)sat_vdp2_plan_rollback();
        }
    }
    g_vram_stress.banks_released =
        sat_vdp2_plan_release_banks(SAT_VDP2_BANK_B1);
}

/* Scenario 3: B0 hosts NBG2's committed names and characters, so a claim
 * is refused and nothing changes. */
static void scenario_claim_conflict(void) {
    g_vram_stress.claim_over_live =
        sat_vdp2_plan_claim_banks(SAT_VDP2_BANK_B0);
}

/* Scenario 4: after committing two scratch blocks and releasing the
 * first, no single hole can host a 0x38000 request although the total
 * free space is larger: fragmentation, reported as CAPACITY. */
static void scenario_fragmentation(void) {
    uint32_t a = 0u, b = 0u, big = 0u;
    if (sat_vdp2_plan_begin() == SAT_OK
        && sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x8000u, 2u, &a) == SAT_OK
        && sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x8000u, 2u, &b) == SAT_OK
        && sat_vdp2_plan_commit() == SAT_OK
        && sat_vdp2_plan_release(a) == SAT_OK) {
        g_vram_stress.total_free = total_free_bytes();
        g_vram_stress.largest_hole = sat_vdp2_vram_largest_free();
        if (sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_SCRATCH, 0x38000u, 2u, &big)
            == SAT_ERR_CAPACITY
            && g_vram_stress.total_free > 0x38000u) {
            g_vram_stress.fragmentation_detected = 1u;
        }
        (void)sat_vdp2_plan_release(b);
    }
}

/* Scenario 5: everything is reclaimed and a second legal layout is
 * planned: four names in two banks (A0 + A1), different priorities, and
 * the picture keeps drawing. */
static void scenario_reconfigure(void) {
    for (uint32_t l = 0u; l < 4u; ++l) {
        (void)sat_vdp2_layer_release((sat_vdp2_layer_t)l);
    }
    (void)sat_vdp2_plan_release_all();
    static const uint32_t names_b[4] = {0x00000u, 0x02000u, 0x20000u, 0x22000u};
    /* Layout B mirrors the proven per-bank shape of layout A: each
     * layer's 0x8000-aligned character window sits in the same bank as
     * its names (A0 for the first two layers, A1 for the last two), so
     * every bank fetches two pattern-name and two character streams -
     * the arrangement table 3.4's allowed timings can always place. */
    static const uint32_t chars_b_at[4] = {0x08000u, 0x10000u, 0x28000u, 0x30000u};
    static const uint8_t bank_b[4] = {
        SAT_VDP2_BANK_A0, SAT_VDP2_BANK_A0, SAT_VDP2_BANK_A1, SAT_VDP2_BANK_A1};
    static const uint32_t band_b[4] = {3u, 7u, 11u, 15u};
    uint32_t ok = 1u;
    if (sat_vdp2_plan_begin() != SAT_OK) {
        g_vram_stress.reconfigured_ok = (uint32_t)SAT_ERR_CAPACITY;
        return;
    }
    for (uint32_t l = 0u; l < 4u; ++l) {
        if (sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_PATTERN_NAMES,
                                     names_b[l], NAMES_BYTES) != SAT_OK
            || sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_CHARACTERS,
                                        chars_b_at[l], CHARS_BYTES) != SAT_OK) {
            ok = 0u;
        }
        g_vram_stress.names_bases_b[l] = names_b[l];
    }
    if (sat_vdp2_plan_commit() != SAT_OK) ok = 0u;
    if (ok) {
        for (uint32_t l = 0u; l < 4u; ++l) {
            if (setup_layer(l, names_b[l], chars_b_at[l], bank_b[l],
                            (uint8_t)(4u - l), band_b[l], 1u + l) != SAT_OK) {
                ok = 0u;
            }
        }
    }
    g_vram_stress.reconfigured_ok = ok ? (uint32_t)SAT_OK : (uint32_t)SAT_ERR_CAPACITY;
}

static void run_scenario(uint32_t index) {
    switch (index) {
        case 0u: scenario_legal_layout(); break;
        case 1u: scenario_oversized(); break;
        case 2u: scenario_bank_claim(); break;
        case 3u: scenario_claim_conflict(); break;
        case 4u: scenario_fragmentation(); break;
        default: scenario_reconfigure(); break;
    }
    g_vram_stress.scenario_ran |= 1u << index;
}

static void draw_bank_bars(void) {
    sat_vdp2_plan_usage_t usage;
    sat_vdp2_plan_usage(&usage);
    static const sat_color_t bank_colour[4] = {
        {232, 48, 48, 255}, {48, 232, 48, 255},
        {48, 80, 248, 255}, {232, 48, 232, 255}};
    const sat_color_t track_colour = {48, 48, 48, 255};
    for (uint32_t b = 0u; b < 4u; ++b) {
        const uint32_t x = 8u + b * 78u;
        sat_rect_t track = {x, 196u, 70u, 8u};
        (void)sat_fill_rect(&track, track_colour);
        const uint32_t reserved = usage.reserved_bytes_per_bank[b];
        uint32_t width = 70u;   /* a fully plan-managed bank shows full */
        if (reserved <= 0x20000u) {
            width = (reserved * 70u) / 0x20000u;
        }
        if (width > 0u) {
            sat_rect_t fill = {x, 196u, (uint16_t)width, 8u};
            (void)sat_fill_rect(&fill, bank_colour[b]);
        }
    }
}

static void draw_debug_overlay(void) {
    sat_vdp2_plan_usage_t usage;
    sat_vdp2_plan_usage(&usage);
    static const char* const names[4] = {"A0", "A1", "B0", "B1"};
    for (uint32_t b = 0u; b < 4u; ++b) {
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, names[b],
                                                      8u + b * 78u, 148u, 8u, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "F",
                                            usage.free_bytes_per_bank[b] >> 10u,
                                            28u + b * 78u, 148u, 8u, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "R",
                                            usage.reserved_bytes_per_bank[b] >> 10u,
                                            56u + b * 78u, 148u, 8u, 0u, 0u);
    }
    (void)sat_ascii_font_draw_label_u32(&font, "SCEN ",
                                        g_vram_stress.scenario_ran, 8u, 160u, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&font, "REFUSE ",
                                        usage.refusals, 8u, 172u, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&font, "LAST ",
                                        (uint32_t)usage.last_status, 90u, 172u, 8u, 0u, 0u);
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) {
        return 1;
    }
    build_cells();
    g_vram_stress.magic = VRS_MAGIC;

    run_scenario(0u);
    uint32_t next = 1u;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_vram_stress.frames;

        if (next < SCENARIOS) {
            if ((pad.pressed & SAT_PAD_A) != 0u
                || g_vram_stress.frames >= AUTO_FRAME[next]) {
                run_scenario(next);
                ++next;
            }
        }
        if ((pad.pressed & SAT_PAD_B) != 0u) g_debug_overlay ^= 1u;

        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 VRAM STRESS",
                                                      8u, 4u, 8u, 0u, 0u);
        draw_bank_bars();
        if (g_debug_overlay) draw_debug_overlay();
        (void)sat_ascii_font_draw_label_u32(&font, "FRAME ", g_vram_stress.frames,
                                            8u, 208u, 8u, 0u, 0u);

        if (g_vram_stress.frames == 200u) {
            /* After every refusal scenario, before the reconfiguration:
             * the display registers must be exactly what layout A set. */
            uint16_t cycle[8];
            (void)sat_vdp2_layer_cycle_patterns(cycle);
            for (uint32_t i = 0u; i < 8u; ++i) {
                g_vram_stress.cycle_mid[i] = cycle[i];
            }
        }
        if (g_vram_stress.frames == 250u) {
            /* The end snapshot: refusals must never have touched the
             * display registers the first layout committed. */
            uint16_t cycle[8];
            (void)sat_vdp2_layer_cycle_patterns(cycle);
            for (uint32_t i = 0u; i < 8u; ++i) {
                g_vram_stress.cycle_at_end[i] = cycle[i];
            }
            sat_vdp2_plan_usage_t usage;
            sat_vdp2_plan_usage(&usage);
            g_vram_stress.refusals = usage.refusals;
            g_vram_stress.last_status = (uint32_t)usage.last_status;
            for (uint32_t b = 0u; b < 4u; ++b) {
                g_vram_stress.free_bytes_per_bank[b] = usage.free_bytes_per_bank[b];
            }
        }
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
