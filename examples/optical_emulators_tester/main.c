#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/time.h"
#include "saturn/fmt.h"
#include "src/core/startup/boot_trace.h"
#include "diagnostics.h"

/* Loaded with the application; neither crt0 BSS clearing nor sat_init owns it. */
volatile sat_boot_trace_t sat_boot_trace_record __attribute__((section(".boot_trace"),aligned(4))) =
    {.magic=0x4c534254u,.version=1u,.bytes=sizeof(sat_boot_trace_t)};
static sat_ascii_font_t font;
static void text(const char* s,int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(&font,s,8,y,8,0,0);
}
static void number(const char* name,uint32_t value,int y) {
    char line[40];sat_fmt_label_u32(name,value,line,sizeof(line),0);text(line,y);
}
int main(void) {
    sat_boot_mark(SAT_BOOT_MAIN,1,SAT_OK);
    sat_boot_mark(SAT_BOOT_INIT_BEGIN,0,SAT_OK);
    const sat_result_t status=sat_app_init_default();
    if(status!=SAT_OK) { sat_boot_mark(SAT_BOOT_INIT_BEGIN,0,status);for(;;){} }
    if(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,0,0)!=SAT_OK)for(;;){}
    g_optical_results.current=g_optical_results.completed=0xffffffffu;
    volatile sat_boot_trace_t* const trace=(volatile sat_boot_trace_t*)
        ((uintptr_t)&sat_boot_trace_record | 0x20000000u);
    unsigned selected=0;int details=0;
    for(;;) {
        sat_pad_state_t pad={0};
        if(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,&pad)!=SAT_OK)continue;
        if(pad.pressed&SAT_PAD_DOWN)selected=(selected+1u)%OET_STEP_COUNT;
        if(pad.pressed&SAT_PAD_UP)selected=(selected+OET_STEP_COUNT-1u)%OET_STEP_COUNT;
        if(pad.pressed&SAT_PAD_C)details^=1;
        if(pad.pressed&SAT_PAD_START){selected=0;details=0;}
        if(pad.pressed&SAT_PAD_A)g_optical_results.current=selected;
        if(pad.pressed&SAT_PAD_B)details=0;
        g_optical_results.selected=selected;
        text("LIBSATURN OPTICAL EMULATORS TESTER",8);
        text(OET_FULL?"FULL / CD TESTS ARE OPT-IN":"MINIMAL / CD COMMANDS DISABLED",24);
        if(details)number("BOOT BITS ",trace->reached,40);
        else number("BUILD ",OET_BUILD_ID,40);
        number("HEARTBEAT ",g_optical_results.frames,56);
        number("BOOT STAGE ",trace->stage,72);
        text(optical_step_name(selected),88);
        if(details) {
            number("HIRQ ",g_optical_results.hirq,104);
            number("CR1 ",g_optical_results.cr1,120);
            number("CR2 ",g_optical_results.cr2,136);
            number("CR3 ",g_optical_results.cr3,152);
            number("CR4 ",g_optical_results.cr4,168);
            number("VALID RESPONSE ",g_optical_results.response_valid,184);
        } else {
            if(g_optical_results.current!=0xffffffffu)text("RUNNING / DEADLINES ACTIVE",104);
            else if(g_optical_results.completed==0xffffffffu && !g_optical_results.failures)text("IDLE / NO CD REQUESTS",104);
            else text(sat_result_name((sat_result_t)g_optical_results.result),104);
            if(g_optical_results.completed==0xffffffffu)text("LAST COMPLETE NONE",120);
            else text(optical_step_name(g_optical_results.completed),120);
            number("TIMEOUTS ",g_optical_results.timeouts,136);
            number("LBA ",g_optical_results.lba,152);
            number("BYTES ",g_optical_results.transferred,168);
            number("WAIT MS ",g_optical_results.elapsed_ms,184);
        }
        text("UP/DOWN SELECT A RUN C DETAILS B BACK",208);
        if(sat_app_frame_end()==SAT_OK) {
            ++g_optical_results.frames;
            if(g_optical_results.frames==1u)sat_boot_mark(SAT_BOOT_FIRST_FRAME,1,SAT_OK);
        }
        /* A command never runs before the first complete diagnostic frame. */
        if(pad.pressed&SAT_PAD_A)(void)optical_run_step(selected);
    }
}
