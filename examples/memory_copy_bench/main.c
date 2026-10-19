/* Normal-RAM libc copy probe: real SH-2 ABI, all modulo-four alignments,
 * five local FRT intervals per case, guards checked outside timed work. */
#include <stddef.h>
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/time.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "example_util.h"

extern void* memcpy(void*,const void*,size_t);
typedef void* (*copy_fn_t)(void*,const void*,size_t);
static copy_fn_t volatile copy_function=memcpy;
enum { BUFFER=736, CASES=512, ROUNDS=5 };
static uint8_t high_source[BUFFER] __attribute__((aligned(4)));
static uint8_t high_destination[BUFFER] __attribute__((aligned(4)));
static uint8_t low_source[BUFFER] __attribute__((aligned(4),section(".wram_l")));
static uint8_t low_destination[BUFFER] __attribute__((aligned(4),section(".wram_l")));
typedef struct {
    uint32_t source_region,destination_region,source_offset,destination_offset;
    uint32_t bytes,repetitions,ticks[ROUNDS],checksum,status;
} sample_t;
typedef struct {
    uint32_t magic,version,frequency,count,done,errors,ccr,rows;
    sample_t samples[CASES];
} results_t;
volatile results_t g_memory_copy_bench __attribute__((section(".wram_l")));

static sat_ascii_font_t font;
static uint32_t median(const volatile uint32_t* values) {
    uint32_t sorted[ROUNDS];
    for(unsigned i=0;i<ROUNDS;++i) {
        uint32_t value=values[i];unsigned p=i;
        while(p && sorted[p-1]>value){sorted[p]=sorted[p-1];--p;}
        sorted[p]=value;
    }
    return sorted[ROUNDS/2];
}
int main(void) {
    if(sat_app_init_default()!=SAT_OK)for(;;){}
    g_memory_copy_bench.magic=0x4d434231u;
    g_memory_copy_bench.version=1u;
    g_memory_copy_bench.frequency=sat_time_frequency_hz();
    g_memory_copy_bench.count=g_memory_copy_bench.done=g_memory_copy_bench.errors=0u;
    g_memory_copy_bench.rows=CASES;
    g_memory_copy_bench.ccr=*(volatile uint8_t*)0xfffffe92u;
    for(unsigned i=0;i<BUFFER;++i)high_source[i]=low_source[i]=(uint8_t)(i*37u+19u);
    const uint32_t lengths[]={0,2,8,18,32,98,183,704};
    for(unsigned sr=0;sr<2;++sr)for(unsigned dr=0;dr<2;++dr)
        for(unsigned so=0;so<4;++so)for(unsigned d=0;d<4;++d)
            for(unsigned n=0;n<8;++n) {
        const uint8_t* source=(sr?low_source:high_source)+16+so;
        uint8_t* base=dr?low_destination:high_destination;
        uint8_t* destination=base+16+d;
        const uint32_t length=lengths[n],repetitions=length<=32u?512u:32u;
        volatile sample_t* row=&g_memory_copy_bench.samples[g_memory_copy_bench.count];
        row->source_region=sr;row->destination_region=dr;
        row->source_offset=so;row->destination_offset=d;
        row->bytes=length;row->repetitions=repetitions;row->status=0u;row->checksum=0u;
        for(unsigned round=0;round<ROUNDS;++round) {
            for(unsigned i=0;i<BUFFER;++i)base[i]=0xa5u;
            const uint16_t start=sat_time_frc();
            for(uint32_t repeat=0;repeat<repetitions;++repeat)
                if(copy_function(destination,source,length)!=destination)row->status=1u;
            row->ticks[round]=(uint16_t)(sat_time_frc()-start);
            uint32_t checksum=0;
            for(unsigned i=0;i<BUFFER;++i) {
                const uint8_t expected=i>=16u+d && i<16u+d+length
                    ?source[i-16u-d]:0xa5u;
                if(base[i]!=expected)row->status=1u;
                checksum=checksum*33u+base[i];
            }
            row->checksum=checksum;
        }
        if(row->status)++g_memory_copy_bench.errors;
        ++g_memory_copy_bench.count;
    }
    g_memory_copy_bench.done=1u;
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,0,0));
    for(;;) {
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,0));
        sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
            &font,"MEMCPY 18 BYTES / 512 CALLS",8,8,8,0,0));
        sat_example_must(sat_ascii_font_draw_label_u32(
            &font,"FRT HZ ",g_memory_copy_bench.frequency,8,24,8,0,0));
        static const unsigned cases[]={3,19,11,147,275,403};
        static const char* const labels[]={"HH 0/0 ","HH 0/2 ","HH 0/1 ",
            "HL 0/2 ","LH 0/2 ","LL 0/2 "};
        for(unsigned i=0;i<6;++i)
            sat_example_must(sat_ascii_font_draw_label_u32(&font,labels[i],
                median(g_memory_copy_bench.samples[cases[i]].ticks),8,48+20*(int)i,8,0,0));
        sat_example_must(sat_ascii_font_draw_label_u32(
            &font,"ERRORS ",g_memory_copy_bench.errors,8,184,8,0,0));
        sat_example_must(sat_app_frame_end());
    }
}
