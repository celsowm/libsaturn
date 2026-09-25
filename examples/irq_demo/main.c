/* SCU interrupts: per-source counters driven by real handlers, and a busy
 * wait that proves sat_time_ms keeps counting while nothing calls it. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/irq.h"
#include "saturn/time.h"

#define IRQ_DEMO_MAGIC 0x49525131u /* "IRQ1" */
#define TIMER0_LINE 112u
#define BUSY_VBLANKS 90u           /* 1.5 s at 60 Hz */

/* Read by harness/tests/test_irq_demo.py from a Work RAM dump. */
typedef struct irq_demo_results {
    uint32_t magic;
    uint32_t active;
    uint32_t busy_done;
    uint32_t busy_ms;
    uint32_t busy_vblanks;
    uint32_t vblank_in;
    uint32_t vblank_out;
    uint32_t timer0;
    uint32_t sprite_end;
    uint32_t handler_vblank_out;
    uint32_t handler_timer0;
    uint32_t handler_sprite_end;
} irq_demo_results_t;

volatile irq_demo_results_t g_irq_demo;

static sat_ascii_font_t font;

static void counter(void* user) {
    volatile uint32_t* value = (volatile uint32_t*)user;
    *value = *value + 1u;
}

static void number(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

/* Spins for BUSY_VBLANKS display frames without touching the clock: before
 * the VBlank-IN handler observed the FRT, the 16-bit counter wrapped unseen
 * every ~0.3 s here and sat_time_ms came back short. */
static void busy_wait_test(void) {
    const uint32_t start_ms = sat_time_ms();
    const uint32_t start = sat_irq_count(SAT_IRQ_VBLANK_IN);
    while (sat_irq_count(SAT_IRQ_VBLANK_IN) - start < BUSY_VBLANKS) {
    }
    g_irq_demo.busy_ms = sat_time_ms() - start_ms;
    g_irq_demo.busy_vblanks = sat_irq_count(SAT_IRQ_VBLANK_IN) - start;
    g_irq_demo.busy_done = 1u;
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(
            &font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_irq_demo.magic = IRQ_DEMO_MAGIC;
    g_irq_demo.active = (uint32_t)sat_irq_active();
    uint8_t ok = g_irq_demo.active != 0u;
    if (ok) {
        ok = sat_irq_set_timer0_line(TIMER0_LINE) == SAT_OK &&
             sat_irq_set_handler(SAT_IRQ_TIMER0, counter,
                 (void*)&g_irq_demo.handler_timer0) == SAT_OK &&
             sat_irq_set_handler(SAT_IRQ_VBLANK_OUT, counter,
                 (void*)&g_irq_demo.handler_vblank_out) == SAT_OK &&
             sat_irq_set_handler(SAT_IRQ_SPRITE_END, counter,
                 (void*)&g_irq_demo.handler_sprite_end) == SAT_OK;
    }
    uint32_t frame = 0u;
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK,
                ok ? SAT_COLOR_GREEN : SAT_COLOR_RED, &pad) != SAT_OK) break;
        if (ok && ++frame == 30u) busy_wait_test();
        g_irq_demo.vblank_in = sat_irq_count(SAT_IRQ_VBLANK_IN);
        g_irq_demo.vblank_out = sat_irq_count(SAT_IRQ_VBLANK_OUT);
        g_irq_demo.timer0 = sat_irq_count(SAT_IRQ_TIMER0);
        g_irq_demo.sprite_end = sat_irq_count(SAT_IRQ_SPRITE_END);
        (void)sat_ascii_font_draw_text_screen_indexed8(
            &font, ok ? "SCU INTERRUPTS: ACTIVE" : "SCU INTERRUPTS: POLLING",
            8, 8, 8, 0u, 0u);
        number("VBLANK-IN  ", g_irq_demo.vblank_in, 32);
        number("VBLANK-OUT ", g_irq_demo.vblank_out, 48);
        number("TIMER0 L112 ", g_irq_demo.timer0, 64);
        number("SPRITE END ", g_irq_demo.sprite_end, 80);
        number("TIME MS    ", sat_time_ms(), 104);
        if (g_irq_demo.busy_done) {
            number("BUSY 90 VBLANKS MS ", g_irq_demo.busy_ms, 128);
        }
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return ok ? 0 : 1;
}
