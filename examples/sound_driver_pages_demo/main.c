/* The paged score of the resident 68000 sound driver: a long, note-dense score (a few thousand register writes,
 * far past the 1024-event looping score) played by the 68000 while the SH-2 only refills pages.
 * The SH-2 builds each page of 512 events as a generated tune (a rising and falling scale on eight slots), and
 * publishes it as soon as the driver has freed a page; the 68000 plays the pages in order, the tick of every event
 * counted from one stream base, so nothing drifts. g_pages_demo holds what harness/tests can check. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/audio.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/sound_driver.h"
#include "saturn/video.h"

#define DEMO_MAGIC SAT_FOURCC('S', 'D', 'P', '1')
#define TONE_SAMPLES 441u            /* one cycle at 44100 Hz = 100 Hz when looped */
#define VOICES 8u
#define NOTES 400u                   /* four events each: 1600 events, over three pages */
#define NOTE_TICKS 12u               /* a new note every 12 ticks (70 ms) */
#define HOLD_TICKS 9u
#define TICK_SAMPLES 256u

typedef struct pages_demo_results {
    uint32_t magic, audio_status, start_status, create_status, pages_status;
    uint32_t events_total, pages_total;
    uint32_t pages_published, pages_played, executed, late_events, max_lateness, underrun_ticks, bad_sequence, refused;
    uint32_t done, max_burst, frames;
} pages_demo_results_t;

volatile pages_demo_results_t g_pages_demo;

static sat_ascii_font_t font;
static int16_t g_tone[TONE_SAMPLES];
static sat_sound_score_event_t g_page[SAT_SOUND_DRIVER_PAGE_EVENTS];
static uint32_t g_next_note;
static uint32_t g_next_page;

/* Pitch words for a major scale over one octave: OCT 0 = 100 Hz, FNS from 2^(k/12). */
static const uint16_t kScale[8] = {0, 125, 266, 343, 510, 698, 909, 0x0800};

static uint16_t pitch_of(uint32_t note) {
    const uint32_t step = note % 14u;                          /* up the scale and back down */
    return kScale[step < 8u ? step : 14u - step];
}

/* Fills g_page with whole notes; returns the count and the end time (the next note's first event). */
static uint16_t build_page(uint32_t* end, uint8_t* last) {
    uint16_t count = 0u;
    while (g_next_note < NOTES && count + 4u <= SAT_SOUND_DRIVER_PAGE_EVENTS) {
        const uint32_t n = g_next_note++;
        const uint32_t slot = n % VOICES;
        const uint32_t t = 4u + n * NOTE_TICKS;
        const uint16_t ctrl = 0x0020u;                          /* PCM16, forward loop, SA 0x3000 high bits 0 */
        const uint32_t base = slot * 0x20u;
        g_page[count++] = (sat_sound_score_event_t){t - 2u, (uint16_t)(base + 0x10u), pitch_of(n)};
        g_page[count++] = (sat_sound_score_event_t){t - 2u, (uint16_t)(base + 0x0Cu), (uint16_t)(0x20u + (n % 5u) * 8u)};
        g_page[count++] = (sat_sound_score_event_t){t, (uint16_t)base, (uint16_t)(ctrl | 0x0800u | 0x1000u)};
        g_page[count++] = (sat_sound_score_event_t){t + HOLD_TICKS, (uint16_t)base, (uint16_t)(ctrl | 0x1000u)};
    }
    *last = g_next_note >= NOTES;
    *end = *last ? 4u + NOTES * NOTE_TICKS + 20u : 4u + g_next_note * NOTE_TICKS - 2u;
    return count;
}

static void slots_init(void) {
    volatile uint16_t* regs = (volatile uint16_t*)0x25B00000u;
    for (uint32_t s = 0u; s < VOICES; ++s) {
        volatile uint16_t* r = regs + s * 0x10u;
        r[0x00 / 2] = 0x0020u;
        r[0x02 / 2] = 0x3000u;                                  /* the first sat_sound_create lands at 0x3000 */
        r[0x04 / 2] = 0u;
        r[0x06 / 2] = TONE_SAMPLES;
        r[0x08 / 2] = 0x001Fu;
        r[0x0A / 2] = 0x3C1Fu;
        r[0x0C / 2] = 0x00FFu;
        r[0x0E / 2] = 0u;
        r[0x10 / 2] = 0u;
        r[0x12 / 2] = 0u;
        r[0x14 / 2] = 0u;
        r[0x16 / 2] = 0xE000u;                                  /* DISDL 7, centre */
    }
    regs[0] = 0x1000u;
}

static void publish_when_free(void) {
    sat_sound_pages_info_t info = {0};
    while (g_next_note < NOTES && sat_sound_driver_pages_info(&info) == SAT_OK && info.pages_free != 0u) {
        uint32_t end = 0u;
        uint8_t last = 0u;
        const uint16_t count = build_page(&end, &last);
        if (sat_sound_driver_page_publish(g_page, count, 0u, end, last) != SAT_OK) break;
        ++g_next_page;
        g_pages_demo.events_total += count;
    }
}

int main(void) {
    sat_sound_t tone = {0};
    sat_sound_desc_t desc = {0};
    sat_sound_pages_info_t info = {0};

    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_pages_demo.magic = DEMO_MAGIC;
    for (uint32_t i = 0u; i < TONE_SAMPLES; ++i) {
        /* a triangle keeps the demo free of math libraries */
        const int32_t phase = (int32_t)i * 4 / (int32_t)TONE_SAMPLES;       /* quarter 0..3 */
        const int32_t within = ((int32_t)i * 4 % (int32_t)TONE_SAMPLES) * 8000 / (int32_t)TONE_SAMPLES;
        g_tone[i] = (int16_t)(phase == 0 ? within : phase == 1 ? 8000 - within : phase == 2 ? -within : within - 8000);
    }
    desc.samples = g_tone;
    desc.sample_count = TONE_SAMPLES;
    desc.sample_rate = 44100u;
    desc.format = SAT_AUDIO_PCM_S16;
    g_pages_demo.audio_status = (uint32_t)(-sat_audio_init());
    if (g_pages_demo.audio_status == 0u) g_pages_demo.start_status = (uint32_t)(-sat_sound_driver_start());
    if (g_pages_demo.audio_status == 0u && g_pages_demo.start_status == 0u) {
        slots_init();
        g_pages_demo.create_status = (uint32_t)(-sat_sound_create(&tone, &desc));
        g_pages_demo.pages_status = (uint32_t)(-sat_sound_driver_pages_start(TICK_SAMPLES, 40u));
        publish_when_free();
    }
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad) != SAT_OK) break;
        ++g_pages_demo.frames;
        publish_when_free();
        if (sat_sound_driver_pages_info(&info) == SAT_OK) {
            g_pages_demo.pages_published = info.pages_published;
            g_pages_demo.pages_played = info.pages_played;
            g_pages_demo.executed = info.executed;
            g_pages_demo.late_events = info.late_events;
            g_pages_demo.max_lateness = info.max_lateness_ticks;
            g_pages_demo.underrun_ticks = info.underrun_ticks;
            g_pages_demo.bad_sequence = info.bad_sequence;
            g_pages_demo.refused = info.refused;
            g_pages_demo.done = info.done;
            g_pages_demo.max_burst = info.max_burst;
        }
        g_pages_demo.pages_total = g_next_page;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "PAGED SCORE", 8, 8, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "EVENTS ", g_pages_demo.executed, 8, 24, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "PAGES ", g_pages_demo.pages_played, 8, 40, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "LATE ", g_pages_demo.late_events, 8, 56, 8, 0u, 0u);
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_sound_driver_pages_stop();
    (void)sat_sound_driver_stop();
    (void)sat_shutdown();
    return 0;
}
