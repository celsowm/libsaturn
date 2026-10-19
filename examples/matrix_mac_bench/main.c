/* Experiment only: exact signed three-term dot / matrix / parent chain.
 * SH-1/SH-2 manual 6.31: MAC.W needs SR.S=0 for 64-bit accumulation.
 * The assembly declares MACH/MACL clobbers; interrupt handlers must preserve
 * them. The SDK SCU interrupt_handler wrappers are checked in disassembly.
 * No public math API or existing consumer is changed by this benchmark. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/time.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/int_math.h"
#include "example_util.h"

typedef struct { int16_t m[3][3]; } matrix_t;
typedef void (*kernel_t)(const matrix_t*,const matrix_t*,matrix_t*);
static int16_t finish(int64_t sum) {
    int64_t q=sat_floor_shift_i64(sum,12);
    return (int16_t)(q < -32768 ? -32768 : q > 32767 ? 32767 : q);
}
/* Clamping the full sum before narrowing preserves every final i16 value.
 * This moves the proven bound ahead of the shift, without narrowing products
 * or the accumulation. The selected Q12 bounds themselves fit signed i32. */
static int16_t finish_narrow(int64_t sum) {
    if(sum>INT64_C(134213632))return 32767;
    if(sum< -INT64_C(134217728))return -32768;
    return (int16_t)sat_floor_shift_i32((int32_t)sum,12);
}
static __attribute__((noinline)) void matrix_c(const matrix_t* a,
    const matrix_t* b,matrix_t* out) {
    matrix_t r;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j) {
        int64_t sum=0;
        for(unsigned k=0;k<3;++k)sum+=(int64_t)a->m[i][k]*b->m[k][j];
        r.m[i][j]=finish(sum);
    }
    *out=r;
}
static __attribute__((noinline)) void matrix_c_narrow(const matrix_t* a,
    const matrix_t* b,matrix_t* out) {
    matrix_t r;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j) {
        int64_t sum=0;
        for(unsigned k=0;k<3;++k)sum+=(int64_t)a->m[i][k]*b->m[k][j];
        r.m[i][j]=finish_narrow(sum);
    }
    *out=r;
}
static uint32_t sr_read(void) {
    uint32_t v;__asm__ volatile("stc sr,%0":"=r"(v));return v;
}
static void sr_write(uint32_t v) {
    __asm__ volatile("ldc %0,sr"::"r"(v):"memory","t");
}
static int64_t dot_mac(const int16_t* a,const int16_t* b) {
    uint32_t hi,lo;
    __asm__ volatile("clrmac\n\tmac.w @%2+,@%3+\n\tmac.w @%2+,@%3+\n\t"
                     "mac.w @%2+,@%3+\n\tsts mach,%0\n\tsts macl,%1"
        :"=&r"(hi),"=&r"(lo),"+&r"(a),"+&r"(b)
        ::"mach","macl","memory");
    return (int64_t)(int32_t)hi*INT64_C(4294967296)+(int64_t)lo;
}
static __attribute__((noinline)) void matrix_mac(const matrix_t* a,
    const matrix_t* b,matrix_t* out) {
    matrix_t columns,r;
    const uint32_t saved=sr_read();
    sr_write(saved&~2u);
    /* Include transpose cost, full-width sum, floor, clamp and alias-safe
     * output publication in every timed call. No prepacked-input advantage. */
    for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k)
        columns.m[j][k]=b->m[k][j];
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
        r.m[i][j]=finish(dot_mac(a->m[i],columns.m[j]));
    *out=r;
    sr_write(saved);
}
static __attribute__((noinline)) void matrix_mac_narrow(const matrix_t* a,
    const matrix_t* b,matrix_t* out) {
    matrix_t columns,r;
    const uint32_t saved=sr_read();
    sr_write(saved&~2u);
    /* Include transpose cost, full-width sum, floor, clamp and alias-safe
     * output publication in every timed call. No prepacked-input advantage. */
    for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k)
        columns.m[j][k]=b->m[k][j];
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
        r.m[i][j]=finish_narrow(dot_mac(a->m[i],columns.m[j]));
    *out=r;
    sr_write(saved);
}
static kernel_t volatile kernels[4]={matrix_c,matrix_mac,matrix_c_narrow,matrix_mac_narrow};
static matrix_t high_inputs[2];
static matrix_t low_inputs[2] __attribute__((section(".wram_l")));
static sat_ascii_font_t font;
static uint32_t random_word=0x12345678u;
static uint32_t rng(void) {
    random_word=random_word*1664525u+1013904223u;return random_word;
}
static int16_t sample(void) {
    uint32_t v=rng()>>16;return (int16_t)(v<32768?v:(int32_t)v-65536);
}
static void fill(matrix_t* a,matrix_t* b,unsigned pattern) {
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j) {
        a->m[i][j]=pattern==0?-32768:pattern==1?32767:
            pattern==2?(i==j?4096:0):sample();
        b->m[i][j]=pattern==0?-32768:pattern==1?-32768:
            pattern==2?(i==j?-4096:0):sample();
    }
}
/* Independent division/remainder oracle, no shared shift or clamp helper. */
static void reference(const matrix_t* a,const matrix_t* b,matrix_t* out) {
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j) {
        int64_t sum=(int64_t)a->m[i][0]*b->m[0][j]+
            (int64_t)a->m[i][1]*b->m[1][j]+(int64_t)a->m[i][2]*b->m[2][j];
        int64_t q=sum/4096;
        if(sum<0 && sum%4096)--q;
        out->m[i][j]=(int16_t)(q>32767?32767:q< -32768?-32768:q);
    }
}
static int equal(const matrix_t* a,const matrix_t* b) {
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
        if(a->m[i][j]!=b->m[i][j])return 0;
    return 1;
}
static uint32_t checksum(const matrix_t* a) {
    uint32_t v=0;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
        v=v*33u+(uint16_t)a->m[i][j];
    return v;
}
static uint32_t median(const volatile uint32_t* values) {
    uint32_t sorted[5];
    for(unsigned i=0;i<5;++i) {
        uint32_t v=values[i];unsigned p=i;
        while(p && sorted[p-1]>v){sorted[p]=sorted[p-1];--p;}sorted[p]=v;
    }
    return sorted[2];
}
typedef struct { uint32_t kernel,region,chain,repetitions,ticks[5],checksum; } row_t;
typedef struct {
    uint32_t magic,version,frequency,done,errors,checks,ccr,rows;
    row_t samples[16];
} results_t;
volatile results_t g_matrix_mac_bench;
static void qualify(void) {
    const uint32_t original=sr_read();
    for(unsigned n=0;n<1024;++n) {
        matrix_t a,b,expected;
        fill(&a,&b,n);reference(&a,&b,&expected);
        sr_write(original&~2u);
        for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j) {
            int16_t column[3]={b.m[0][j],b.m[1][j],b.m[2][j]};
            int64_t sum=(int64_t)a.m[i][0]*column[0]+
                (int64_t)a.m[i][1]*column[1]+(int64_t)a.m[i][2]*column[2];
            if(dot_mac(a.m[i],column)!=sum)++g_matrix_mac_bench.errors;
            ++g_matrix_mac_bench.checks;
        }
        for(unsigned saturation=0;saturation<2;++saturation)
            for(unsigned kind=0;kind<4;++kind)for(unsigned alias=0;alias<3;++alias) {
                struct { uint32_t before;matrix_t value;uint32_t after; } guarded;
                matrix_t aa=a,bb=b;
                guarded.before=0xabcdef01u;guarded.after=0x12345678u;
                matrix_t* dest=alias==1?&aa:alias==2?&bb:&guarded.value;
                sr_write((original&~2u)|(saturation*2u));
                kernels[kind](&aa,&bb,dest);
                uint32_t restored=sr_read();
                if(!equal(dest,&expected) || (restored&2u)!=saturation*2u ||
                   guarded.before!=0xabcdef01u || guarded.after!=0x12345678u)
                    ++g_matrix_mac_bench.errors;
                ++g_matrix_mac_bench.checks;
            }
    }
    sr_write(original);
}
int main(void) {
    sat_example_must(sat_app_init_default());
    g_matrix_mac_bench.magic=0x4d4d4231u;
    g_matrix_mac_bench.version=1;
    g_matrix_mac_bench.frequency=sat_time_frequency_hz();
    g_matrix_mac_bench.done=g_matrix_mac_bench.errors=g_matrix_mac_bench.checks=0;
    g_matrix_mac_bench.ccr=*(volatile uint8_t*)0xfffffe92u;
    g_matrix_mac_bench.rows=16;
    qualify();
    fill(&high_inputs[0],&high_inputs[1],3);
    for(unsigned n=0;n<2;++n)for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
        high_inputs[n].m[i][j]/=8; /* representative Q12 input magnitude */
    low_inputs[0]=high_inputs[0];low_inputs[1]=high_inputs[1];
    for(unsigned kind=0;kind<4;++kind)for(unsigned region=0;region<2;++region)
        for(unsigned chain=0;chain<2;++chain) {
            volatile row_t* row=&g_matrix_mac_bench.samples[kind*4+region*2+chain];
            const matrix_t* inputs=region?low_inputs:high_inputs;
            row->kernel=kind;row->region=region;row->chain=chain?18:1;
            row->repetitions=chain?8:64;
            for(unsigned round=0;round<5;++round) {
                uint32_t hash=0;matrix_t result;
                const uint32_t start=(uint32_t)sat_time_ticks();
                for(unsigned rep=0;rep<row->repetitions;++rep) {
                    result=inputs[0];
                    for(unsigned link=0;link<row->chain;++link)
                        kernels[kind](&result,&inputs[1],&result);
                    hash=hash*33u+checksum(&result);
                }
                row->ticks[round]=(uint32_t)sat_time_ticks()-start;
                row->checksum=hash;
            }
        }
    for(unsigned i=4;i<16;++i)
        if(g_matrix_mac_bench.samples[i%4].checksum!=g_matrix_mac_bench.samples[i].checksum)
            ++g_matrix_mac_bench.errors;
    g_matrix_mac_bench.done=1;
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font,SAT_COLOR_WHITE,0,0));
    uint32_t frame=0;
    for(;;) {
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,0));
        sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&font,
            "EXACT MATRIX C / MAC.W",8,8,8,0,0));
        static const char* const labels[]={"C64 ","MAC64 ","C32 ","MAC32 "};
        for(unsigned i=0;i<4;++i)
            sat_example_must(sat_ascii_font_draw_label_u32(&font,labels[i],
                median(g_matrix_mac_bench.samples[i*4+1].ticks),8,40+(int)i*24,8,0,0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font,"CHECKS ",g_matrix_mac_bench.checks,8,160,8,0,0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font,"ERRORS ",g_matrix_mac_bench.errors,8,184,8,0,0));
        sat_example_must(sat_ascii_font_draw_label_u32(&font,"FRAME ",++frame,8,208,8,0,0));
        sat_example_must(sat_app_frame_end());
    }
}
