/* SH7604 manual 10.3/10.4.1: divide latency is 39 cycles and an early
 * register access waits. Only NOPs separate the start write and result read.
 * Compare short/long gap curves; a faster CPU loop is not the criterion. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/time.h"
#include "example_util.h"
#define STRING_(N) #N
#define STRING(N) STRING_(N)
#define DIV32(N) static __attribute__((noinline)) uint32_t div32_##N(void) { \
    uint32_t result; \
    __asm__ volatile("mov.l %2,@%1\n\tmov.l %3,@(4,%1)\n\t.rept " STRING(N) \
        "\n\tnop\n\t.endr\n\tmov.l @(4,%1),%0" \
        : "=&r"(result):"r"(0xffffff00u),"r"(7u),"r"(0x1234567u):"memory"); \
    return result; }
#define DIV64(N) static __attribute__((noinline)) uint32_t div64_##N(void) { \
    uint32_t result; \
    __asm__ volatile("mov.l %2,@%1\n\tmov.l %3,@(16,%1)\n\tmov.l %4,@(20,%1)\n\t.rept " STRING(N) \
        "\n\tnop\n\t.endr\n\tmov.l @(20,%1),%0" \
        : "=&r"(result):"r"(0xffffff00u),"r"(7u),"r"(1u),"r"(7u):"memory"); \
    return result; }
DIV32(0) DIV32(8) DIV32(16) DIV32(32) DIV32(48) DIV32(64)
DIV64(0) DIV64(8) DIV64(16) DIV64(32) DIV64(48) DIV64(64)
typedef uint32_t (*operation_t)(void);
static operation_t const operations[2][6]={
    {div32_0,div32_8,div32_16,div32_32,div32_48,div32_64},
    {div64_0,div64_8,div64_16,div64_32,div64_48,div64_64}};
static const uint32_t gaps[6]={0,8,16,32,48,64};
typedef struct { uint32_t bits,nops,ticks[5],quotient; } row_t;
typedef struct { uint32_t magic,version,frequency,done,errors,tcr,ccr,repetitions;row_t rows[12]; } result_t;
volatile result_t g_division_latency_probe;
static sat_ascii_font_t font;
static uint32_t median(const volatile uint32_t* values) {
    uint32_t sorted[5];
    for(unsigned i=0;i<5;++i) {
        uint32_t value=values[i];unsigned p=i;
        while(p && sorted[p-1]>value){sorted[p]=sorted[p-1];--p;}
        sorted[p]=value;
    }
    return sorted[2];
}
int main(void) {
    sat_example_must(sat_app_init_default());
    g_division_latency_probe.magic=0x444c5031u;
    g_division_latency_probe.version=1;
    g_division_latency_probe.frequency=sat_time_frequency_hz();
    g_division_latency_probe.done=g_division_latency_probe.errors=0;
    g_division_latency_probe.tcr=*(volatile uint8_t*)0xfffffe16u;
    g_division_latency_probe.ccr=*(volatile uint8_t*)0xfffffe92u;
    g_division_latency_probe.repetitions=256;
    for(unsigned width=0;width<2;++width)for(unsigned gap=0;gap<6;++gap) {
        volatile row_t* row=&g_division_latency_probe.rows[width*6+gap];
        row->bits=width?64:32;row->nops=gaps[gap];
        for(unsigned round=0;round<5;++round) {
            uint32_t quotient=0;const uint16_t start=sat_time_frc();
            for(unsigned repeat=0;repeat<256;++repeat)quotient=operations[width][gap]();
            row->ticks[round]=(uint16_t)(sat_time_frc()-start);row->quotient=quotient;
            if(quotient!=(width?613566757u:2726963u))++g_division_latency_probe.errors;
        }
    }
    g_division_latency_probe.done=1;
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,0,0));
    for(;;) {
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,0));
        sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&font,"DIVU GAP / MEDIAN FRT TICKS",8,8,8,0,0));
        sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&font,"32-BIT             64-BIT",8,24,8,0,0));
        for(unsigned width=0;width<2;++width)for(unsigned gap=0;gap<6;++gap) {
            const int x=8+(int)width*160,y=48+(int)gap*20;
            const volatile row_t* row=&g_division_latency_probe.rows[width*6+gap];
            sat_example_must(sat_ascii_font_draw_label_u32(&font,"N",row->nops,x,y,8,0,0));
            sat_example_must(sat_ascii_font_draw_label_u32(&font,":",median(row->ticks),x+40,y,8,0,0));
        }
        sat_example_must(sat_ascii_font_draw_label_u32(&font,"ERRORS ",g_division_latency_probe.errors,8,184,8,0,0));
        sat_example_must(sat_app_frame_end());
    }
}
