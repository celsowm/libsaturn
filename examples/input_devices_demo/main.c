/* Input devices and SMPC services: device kinds behind each port, 3D Control
 * Pad axes and triggers, mouse motion, the event queue, and the real-time
 * clock. The counters in g_input_demo are what harness/run-input-devices.ps1
 * checks; the screen shows the same numbers.
 *
 * Press START+A on port 1 to run the RTC and SMEM round trips. They write the
 * clock and the four battery-backed bytes and put the original values back,
 * but a real console's settings are the user's: nothing writes them until
 * that chord. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/smpc.h"
#include "saturn/time.h"

#define INPUT_DEMO_MAGIC 0x49444531u /* "IDE1" */
#define HISTORY 6u

typedef struct port_results {
    uint32_t kind;
    uint32_t peripheral_id;
    uint32_t data_size;
    uint32_t axis_count;
    uint32_t multitap_id;
    uint32_t tap_count;
    uint32_t held;
    uint32_t x;
    uint32_t y;
    uint32_t l;
    uint32_t r;
    uint32_t has_triggers;
    int32_t mouse_dx_total;
    int32_t mouse_dy_total;
    uint32_t mouse_buttons;
    uint32_t axis_history[HISTORY];   /* distinct (x<<24|y<<16|l<<8|r) values, in order */
    uint32_t axis_history_count;
} port_results_t;

/* Read by harness/tests/test_input_devices.py from a Work RAM dump. */
typedef struct input_demo_results {
    uint32_t magic;
    uint32_t polls;
    uint32_t poll_errors;
    port_results_t port[2];
    uint32_t axis_events;
    uint32_t button_events;
    uint32_t connect_events;
    uint32_t mutate_done;
    uint32_t status_ok;
    uint32_t rtc_ok;             /* the read-back time matched what was set */
    uint32_t rtc_restored;
    uint32_t smem_ok;
    uint32_t smem_restored;
    uint32_t bad_date_refused;
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t weekday;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    uint32_t rtc_set_flag;
    uint32_t area_code;
    uint32_t reset_enable_ok;
    uint32_t tap_kind[SAT_PAD_TAP_MAX];   /* port 1: kind of each device slot */
} input_demo_results_t;

volatile input_demo_results_t g_input_demo;

static sat_ascii_font_t font;
static uint8_t g_mutated;

static void record_axis_history(port_results_t* p, uint32_t packed) {
    if (p->axis_history_count >= HISTORY) return;
    if (p->axis_history_count != 0u &&
        p->axis_history[p->axis_history_count - 1u] == packed) return;
    p->axis_history[p->axis_history_count++] = packed;
}

static void poll_port(uint8_t port) {
    volatile port_results_t* r = &g_input_demo.port[port];
    sat_device_info_t info;
    sat_pad_state_t pad;
    if (sat_input_device_info(port, 0u, &info) != SAT_OK) return;
    r->kind = info.kind;
    r->peripheral_id = info.peripheral_id;
    r->data_size = info.data_size;
    r->axis_count = info.axis_count;
    r->multitap_id = info.multitap_id;
    r->tap_count = info.tap_count;
    if (sat_pad_tap_state(port, 0u, &pad) == SAT_OK) r->held = pad.held;
    if (info.kind == SAT_DEVICE_ANALOG_PAD) {
        sat_analog_state_t a;
        if (sat_pad_analog(port, 0u, &a) == SAT_OK) {
            r->x = a.x;
            r->y = a.y;
            r->l = a.l;
            r->r = a.r;
            r->has_triggers = a.has_triggers;
            record_axis_history((port_results_t*)r,
                                ((uint32_t)a.x << 24) | ((uint32_t)a.y << 16) |
                                ((uint32_t)a.l << 8) | a.r);
        }
    } else if (info.kind == SAT_DEVICE_MOUSE) {
        sat_mouse_state_t m;
        if (sat_mouse_read(port, 0u, &m) == SAT_OK) {
            r->mouse_dx_total += m.dx;
            r->mouse_dy_total += m.dy;
            r->mouse_buttons = m.buttons;
        }
    }
}

/* Kind of every device slot on port 1 (a multitap gives up to six). */
static void poll_taps(void) {
    uint8_t tap;
    for (tap = 0u; tap < SAT_PAD_TAP_MAX; ++tap) {
        sat_device_info_t info;
        if (sat_input_device_info(0u, tap, &info) == SAT_OK) g_input_demo.tap_kind[tap] = info.kind;
    }
}

static void drain_events(void) {
    sat_event_t e;
    while (sat_event_poll(&e) == 1) {
        if (e.type == SAT_EVENT_AXIS) ++g_input_demo.axis_events;
        else if (e.type == SAT_EVENT_BUTTON_DOWN || e.type == SAT_EVENT_BUTTON_UP) ++g_input_demo.button_events;
        else if (e.type == SAT_EVENT_PAD_CONNECTED || e.type == SAT_EVENT_PAD_DISCONNECTED) ++g_input_demo.connect_events;
    }
}

static uint32_t time_matches(const sat_rtc_time_t* a, const sat_rtc_time_t* b, uint32_t slack) {
    if (a->year != b->year || a->month != b->month || a->day != b->day) return 0u;
    const int32_t sa = (int32_t)a->hour * 3600 + (int32_t)a->minute * 60 + a->second;
    const int32_t sb = (int32_t)b->hour * 3600 + (int32_t)b->minute * 60 + b->second;
    const int32_t d = sa > sb ? sa - sb : sb - sa;
    return (uint32_t)d <= slack;
}

/* Writes the clock and SMEM, reads them back, then puts both back. */
static void mutate_tests(void) {
    sat_smpc_status_t before;
    sat_rtc_time_t test = {2026u, 9u, 25u, 0u, 21u, 5u, 9u};
    sat_rtc_time_t back;
    sat_rtc_time_t bad = {2026u, 2u, 30u, 0u, 0u, 0u, 0u};
    uint8_t smem_test[4] = {0xA1u, 0xB2u, 0xC3u, 0xD4u};
    uint8_t smem_back[4];
    const uint32_t start_ms = sat_time_ms();
    uint32_t i;
    g_mutated = 1u;
    if (sat_smpc_status(&before) != SAT_OK) return;
    g_input_demo.status_ok = 1u;
    g_input_demo.area_code = before.area_code;
    g_input_demo.rtc_set_flag = before.rtc_set;

    g_input_demo.bad_date_refused = sat_rtc_set(&bad) == SAT_ERR_INVALID_ARG;
    g_input_demo.rtc_ok = sat_rtc_set(&test) == SAT_OK && sat_rtc_get(&back) == SAT_OK &&
                          time_matches(&back, &test, 2u) && back.weekday == 5u;
    /* Original time plus what passed since. */
    {
        sat_rtc_time_t restore = before.time;
        uint32_t seconds = (sat_time_ms() - start_ms) / 1000u + 1u;
        uint32_t total = (uint32_t)restore.hour * 3600u + (uint32_t)restore.minute * 60u +
                         restore.second + seconds;
        if (total < 86400u) {   /* a day rollover is left to the SMPC on the next boot */
            restore.hour = (uint8_t)(total / 3600u);
            restore.minute = (uint8_t)((total / 60u) % 60u);
            restore.second = (uint8_t)(total % 60u);
        }
        g_input_demo.rtc_restored = sat_rtc_set(&restore) == SAT_OK;
    }

    g_input_demo.smem_ok = sat_smem_set(smem_test) == SAT_OK && sat_smem_get(smem_back) == SAT_OK;
    for (i = 0u; i < 4u; ++i) g_input_demo.smem_ok = g_input_demo.smem_ok && smem_back[i] == smem_test[i];
    g_input_demo.smem_restored = sat_smem_set(before.smem) == SAT_OK &&
                                 sat_smem_get(smem_back) == SAT_OK;
    for (i = 0u; i < 4u; ++i) g_input_demo.smem_restored = g_input_demo.smem_restored && smem_back[i] == before.smem[i];

    g_input_demo.reset_enable_ok = sat_smpc_reset_enable(before.reset_disabled ? 0 : 1) == SAT_OK &&
                                   sat_smpc_reset_enable(before.reset_disabled ? 1 : 0) == SAT_OK;
    g_input_demo.mutate_done = 1u;
}

static void read_clock(void) {
    sat_rtc_time_t t;
    if (sat_rtc_get(&t) != SAT_OK) return;
    g_input_demo.year = t.year;
    g_input_demo.month = t.month;
    g_input_demo.day = t.day;
    g_input_demo.weekday = t.weekday;
    g_input_demo.hour = t.hour;
    g_input_demo.minute = t.minute;
    g_input_demo.second = t.second;
}

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(
            &font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_input_demo.magic = INPUT_DEMO_MAGIC;
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLUE, &pad) != SAT_OK) break;
        /* sat_app_frame_begin polled port 1 only; this reads both ports once per
         * frame, so port 2's mouse motion is counted exactly once per poll. */
        ++g_input_demo.polls;
        if (sat_input_poll() != SAT_OK) ++g_input_demo.poll_errors;
        poll_port(0u);
        poll_port(1u);
        poll_taps();
        drain_events();
        if (!g_mutated && (pad.held & (SAT_PAD_START | SAT_PAD_A)) == (SAT_PAD_START | SAT_PAD_A)) {
            mutate_tests();
        }
        if ((g_input_demo.polls & 31u) == 0u) read_clock();
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "INPUT DEVICES", 8, 8, 8, 0u, 0u);
        {
            char taps[24] = "TAPS ";
            uint8_t k;
            for (k = 0u; k < SAT_PAD_TAP_MAX; ++k) taps[5 + k] = (char)('0' + g_input_demo.tap_kind[k]);
            taps[11] = '\0';
            (void)sat_ascii_font_draw_text_screen_indexed8(&font, taps, 152, 24, 8, 0u, 0u);
            (void)sat_ascii_font_draw_label_u32(&font, "N ", g_input_demo.port[0].tap_count,
                                                152, 40, 8, 0u, 0u);
        }
        line("P1 KIND ", g_input_demo.port[0].kind, 24);
        line("P1 ID   ", g_input_demo.port[0].peripheral_id, 40);
        line("P1 X    ", g_input_demo.port[0].x, 56);
        line("P1 Y    ", g_input_demo.port[0].y, 72);
        line("P2 KIND ", g_input_demo.port[1].kind, 96);
        line("MOUSE DX", (uint32_t)g_input_demo.port[1].mouse_dx_total, 112);
        line("YEAR    ", g_input_demo.year, 136);
        line("SECOND  ", g_input_demo.second, 152);
        line("RTC OK  ", g_input_demo.rtc_ok, 168);
        (void)sat_app_frame_end();
    }
    (void)sat_shutdown();
    return 0;
}
