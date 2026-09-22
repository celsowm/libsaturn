#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

enum {
    kRequestCommand = 1u,
    kResponseCommand = 2u,
    kStartupTimeoutTicks = 60000u,
    kResponseTimeoutFrames = 120u,
};

static volatile uint32_t g_slave_requests = 0u;

static void slave_program(void* context) {
    (void)context;
    for (;;) {
        sat_dual_sh2_slave_heartbeat();

        if (sat_dual_sh2_slave_stop_requested() != 0u) break;

        sat_dual_sh2_message_t request = {0};
        if (sat_dual_sh2_slave_receive(&request) == SAT_OK) {
            ++g_slave_requests;
            const uint32_t result = request.argument0 * 2u;
            while (sat_dual_sh2_slave_send(
                       kResponseCommand, request.argument0, result) == SAT_ERR_BUSY) {
                sat_dual_sh2_slave_heartbeat();
                if (sat_dual_sh2_slave_stop_requested() != 0u) return;
            }
            (void)sat_dual_sh2_slave_signal_master();
        }
    }
}

static const char* state_text(sat_dual_sh2_state_t state) {
    switch (state) {
        case SAT_DUAL_SH2_STARTING: return "SLAVE STARTING";
        case SAT_DUAL_SH2_READY: return "SLAVE READY";
        case SAT_DUAL_SH2_STOPPING: return "SLAVE STOPPING";
        case SAT_DUAL_SH2_FAULT: return "SLAVE ERROR";
        default: return "SLAVE OFFLINE";
    }
}

static void draw_line(const sat_ascii_font_t* font, const char* text, int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(font, text, 8, y, 8, 0u, 0u);
}

static void draw_value(const sat_ascii_font_t* font, const char* label,
                       uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    sat_ascii_font_t font;
    sat_pad_state_t pad = {0};
    uint32_t master_counter = 0u;
    uint32_t requests_sent = 0u;
    uint32_t responses_received = 0u;
    uint32_t errors = 0u;
    uint32_t last_input = 0u;
    uint32_t last_result = 0u;
    uint32_t waiting_since = 0u;
    uint8_t waiting = 0u;
    uint8_t validation_pass = 1u;

    sat_example_must(sat_app_init_default());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u));

    if (sat_dual_sh2_configure_slave(slave_program, 0) != SAT_OK ||
        sat_dual_sh2_start(kStartupTimeoutTicks) != SAT_OK) {
        validation_pass = 0u;
        ++errors;
    }

    for (;;) {
        ++master_counter;

        if (sat_dual_sh2_available() != 0u && waiting == 0u &&
            (master_counter % 30u) == 0u) {
            last_input = 100u;
            if (sat_dual_sh2_send(kRequestCommand, last_input, 0u) == SAT_OK) {
                ++requests_sent;
                waiting = 1u;
                waiting_since = sat_frame_count();
            } else {
                ++errors;
                validation_pass = 0u;
            }
        }

        if (waiting != 0u) {
            sat_dual_sh2_message_t response = {0};
            const sat_result_t result = sat_dual_sh2_receive(&response);
            if (result == SAT_OK) {
                ++responses_received;
                last_result = response.argument1;
                if (response.command != kResponseCommand ||
                    response.argument0 != last_input ||
                    response.argument1 != last_input * 2u) {
                    ++errors;
                    validation_pass = 0u;
                }
                waiting = 0u;
            } else if (sat_frame_count() - waiting_since >= kResponseTimeoutFrames) {
                ++errors;
                validation_pass = 0u;
                waiting = 0u;
            }
        }

        if (sat_dual_sh2_state() == SAT_DUAL_SH2_FAULT) validation_pass = 0u;

        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));
        draw_line(&font, "LIBSATURN - DUAL SH-2", 8);
        draw_line(&font, "MASTER: RUNNING", 24);
        draw_line(&font, state_text(sat_dual_sh2_state()), 36);
        draw_value(&font, "MASTER COUNTER ", master_counter, 56);
        draw_value(&font, "SLAVE COUNTER  ", sat_dual_sh2_slave_heartbeat_count(), 68);
        draw_value(&font, "REQUESTS SENT   ", requests_sent, 84);
        draw_value(&font, "RESPONSES       ", responses_received, 96);
        draw_value(&font, "LAST INPUT      ", last_input, 112);
        draw_value(&font, "LAST RESULT     ", last_result, 124);
        draw_line(&font, validation_pass != 0u ? "VALIDATION: PASS" : "VALIDATION: FAIL", 140);
        draw_value(&font, "ERRORS          ", errors, 152);
        draw_line(&font, "MASTER AND SLAVE RUN INDEPENDENTLY", 176);
        sat_example_must(sat_app_frame_end());

        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }

    (void)sat_dual_sh2_stop(kStartupTimeoutTicks);
    return 0;
}
