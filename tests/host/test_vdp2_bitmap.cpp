#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "saturn/vdp2.h"

#define CHECK(expr) do { if(!(expr)) { std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#expr); std::exit(1); } } while(0)
#define EQ(a,b) CHECK((a)==(b))

static uint16_t g_row[8][512];
static uint32_t g_offset[8],g_words[8];
static uint16_t g_pixel_reads,g_rows_written,g_progress;
static sat_result_t g_write_status=SAT_OK;
static uint16_t g_fail_row=0xffffu;
extern "C" sat_result_t sat_vdp2_vram_write_words(
    uint32_t offset,const uint16_t* words,uint32_t count
) {
    const uint16_t row=g_rows_written++;
    CHECK(row<8u);
    g_offset[row]=offset;
    g_words[row]=count;
    for(uint32_t i=0;i<count;++i) g_row[row][i]=words[i];
    return row==g_fail_row?g_write_status:SAT_OK;
}
static uint8_t pixel(void*,uint16_t x,uint16_t y) {
    ++g_pixel_reads;
    return static_cast<uint8_t>(10u*y+x);
}
static void complete(void*,uint16_t row) {
    ++g_progress;
    EQ(row,g_progress);
}
static void reset() {
    g_pixel_reads=g_rows_written=g_progress=0u;
    g_write_status=SAT_OK;
    g_fail_row=0xffffu;
}
static void correct_indexed8_pairs_and_row_offsets() {
    reset();
    uint16_t scratch[4]={};
    EQ(sat_vdp2_bitmap_upload_indexed8(
        0x10000u,6u,3u,pixel,complete,nullptr,scratch,4u),SAT_OK);
    EQ(g_pixel_reads,18u);
    EQ(g_rows_written,3u);
    EQ(g_progress,3u);
    for(uint16_t y=0u;y<3u;++y) {
        EQ(g_offset[y],0x10000u+y*3u);
        EQ(g_words[y],3u);
        for(uint16_t pair=0u;pair<3u;++pair)
            EQ(g_row[y][pair],
               static_cast<uint16_t>(((10u*y+pair*2u)<<8u)|
                                      (10u*y+pair*2u+1u)));
    }
}
static void capacity_and_invalid_shapes_are_atomic() {
    reset();
    uint16_t scratch[4]={};
    EQ(sat_vdp2_bitmap_upload_indexed8(
        0u,10u,2u,pixel,complete,nullptr,scratch,4u),SAT_ERR_CAPACITY);
    EQ(sat_vdp2_bitmap_upload_indexed8(
        0u,3u,2u,pixel,complete,nullptr,scratch,4u),SAT_ERR_INVALID_ARG);
    EQ(sat_vdp2_bitmap_upload_indexed8(
        0u,0u,2u,pixel,complete,nullptr,scratch,4u),SAT_ERR_INVALID_ARG);
    EQ(sat_vdp2_bitmap_upload_indexed8(
        0u,4u,0u,pixel,complete,nullptr,scratch,4u),SAT_ERR_INVALID_ARG);
    EQ(g_pixel_reads,0u);
    EQ(g_rows_written,0u);
    EQ(g_progress,0u);
}
static void bounds_check_entire_bitmap_before_start() {
    reset();
    uint16_t scratch[4]={};
    EQ(sat_vdp2_bitmap_upload_indexed8(
        262141u,4u,2u,pixel,complete,nullptr,scratch,4u),
        SAT_ERR_INVALID_ARG);
    EQ(sat_vdp2_bitmap_upload_indexed8(
        262144u,2u,1u,pixel,complete,nullptr,scratch,4u),
        SAT_ERR_INVALID_ARG);
    EQ(g_pixel_reads,0u);
    EQ(g_rows_written,0u);
}
static void last_word_of_vram_is_legal() {
    reset();
    uint16_t scratch[4]={};
    EQ(sat_vdp2_bitmap_upload_indexed8(
        262142u,4u,1u,pixel,nullptr,nullptr,scratch,4u),SAT_OK);
    EQ(g_row[0][0],1u);
    EQ(g_row[0][1],0x0203u);
    EQ(g_offset[0],262142u);
    EQ(g_progress,0u);
}
static void hardware_error_stops_remaining_rows_without_false_progress() {
    reset();
    uint16_t scratch[4]={};
    g_fail_row=1u;
    g_write_status=SAT_ERR_IO;
    EQ(sat_vdp2_bitmap_upload_indexed8(
        0u,4u,4u,pixel,complete,nullptr,scratch,4u),SAT_ERR_IO);
    EQ(g_pixel_reads,8u);
    EQ(g_rows_written,2u);
    EQ(g_progress,1u);
}
static void null_callbacks_and_scratch_rejected() {
    reset();
    uint16_t scratch[4]={};
    EQ(sat_vdp2_bitmap_upload_indexed8(
        0u,4u,1u,nullptr,complete,nullptr,scratch,4u),SAT_ERR_INVALID_ARG);
    EQ(sat_vdp2_bitmap_upload_indexed8(
        0u,4u,1u,pixel,complete,nullptr,nullptr,4u),SAT_ERR_INVALID_ARG);
    EQ(g_pixel_reads,0u);
    EQ(g_rows_written,0u);
}
int main() {
    correct_indexed8_pairs_and_row_offsets();
    capacity_and_invalid_shapes_are_atomic();
    bounds_check_entire_bitmap_before_start();
    last_word_of_vram_is_legal();
    hardware_error_stops_remaining_rows_without_false_progress();
    null_callbacks_and_scratch_rejected();
    puts("test_vdp2_bitmap: 6 tests passed");
    return 0;
}
