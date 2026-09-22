#include "saturn/vdp2.h"

/* Streaming procedural 8-bit bitmap upload, backed by the validated VDP2
 * word writer. No direct VDP2 MMIO, heap allocation or full-frame RAM copy. */
extern "C" sat_result_t sat_vdp2_bitmap_upload_indexed8(
    uint32_t base_word, uint16_t width, uint16_t height,
    sat_vdp2_indexed8_pixel_fn pixel_fn,
    sat_vdp2_bitmap_row_fn row_fn,
    void* user, uint16_t* row_words, uint32_t row_word_capacity
) {
    constexpr uint32_t kVdp2Words=262144u; /* 512 KiB / 2 */
    if(pixel_fn==nullptr || row_words==nullptr || width<2u ||
       width>1024u || (width&1u)!=0u || height==0u || height>1024u)
        return SAT_ERR_INVALID_ARG;
    const uint32_t row_size=static_cast<uint32_t>(width)/2u;
    const uint32_t total=row_size*height;
    if(row_word_capacity<row_size) return SAT_ERR_CAPACITY;
    if(base_word>=kVdp2Words || total>kVdp2Words-base_word)
        return SAT_ERR_INVALID_ARG;

    for(uint16_t y=0u;y<height;++y) {
        for(uint16_t x=0u;x<width;x=static_cast<uint16_t>(x+2u)) {
            const uint16_t first=pixel_fn(user,x,y);
            const uint16_t second=pixel_fn(user,static_cast<uint16_t>(x+1u),y);
            row_words[x/2u]=static_cast<uint16_t>((first<<8u)|second);
        }
        const sat_result_t st=sat_vdp2_vram_write_words(
            base_word+static_cast<uint32_t>(y)*row_size,
            row_words,row_size);
        if(st!=SAT_OK) return st;
        if(row_fn!=nullptr) row_fn(user,static_cast<uint16_t>(y+1u));
    }
    return SAT_OK;
}
