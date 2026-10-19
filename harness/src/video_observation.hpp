// GPL-3.0: exact RGB changes in the composited video, independent of draw counters.
#pragma once
#include <charconv>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace harness::video {
struct Crop {
    uint32_t x=0,y=0,width=0,height=0; // all zero means the entire output image
};
inline bool parse_crop(std::string_view text,Crop& out) {
    Crop parsed;
    uint32_t* fields[]={&parsed.x,&parsed.y,&parsed.width,&parsed.height};
    for(unsigned i=0;i<4;++i) {
        const size_t colon=text.find(':');
        const auto word=text.substr(0,colon);
        if(word.empty())return false;
        const auto result=std::from_chars(word.data(),word.data()+word.size(),*fields[i]);
        if(result.ec!=std::errc{} || result.ptr!=word.data()+word.size() ||
           (i<3 && colon==std::string_view::npos) || (i==3 && colon!=std::string_view::npos))return false;
        if(i<3)text.remove_prefix(colon+1);
    }
    if(!parsed.width || !parsed.height)return false;
    out=parsed;return true;
}
struct Observation {
    Crop crop;
    uint64_t rgb_fingerprint=0;
    uint32_t compared_pixels=0,changed_pixels=0;
    bool comparable=false,dimensions_changed=false;
};

// Keep one crop, not an unbounded frame history. Equality is checked pixel by
// pixel: the fingerprint is diagnostic and never decides whether an image
// changed. Alpha does not participate, matching png_writer's visible RGB.
class Observer {
public:
    explicit Observer(Crop crop={}): requested_(crop) {}
    Observation observe(std::span<const uint32_t> pixels,uint32_t width,uint32_t height) {
        if(!width || !height || uint64_t(width)*height!=pixels.size())
            throw std::invalid_argument("incomplete composited video frame");
        Crop crop=requested_;
        if(!crop.x && !crop.y && !crop.width && !crop.height)crop={0,0,width,height};
        if(!crop.width || !crop.height || crop.x>width || crop.y>height ||
           crop.width>width-crop.x || crop.height>height-crop.y)
            throw std::invalid_argument("video crop is outside the composited frame");
        Observation out;
        out.crop=crop;
        out.dimensions_changed=!previous_.empty() && (width!=width_ || height!=height_);
        out.comparable=!previous_.empty() && !out.dimensions_changed;
        const size_t count=size_t(crop.width)*crop.height;
        if(!out.comparable)previous_.resize(count);
        const uint64_t fnv_offset=14695981039346656037ull,fnv_prime=1099511628211ull;
        out.rgb_fingerprint=fnv_offset;
        size_t index=0;
        for(uint32_t y=crop.y;y<crop.y+crop.height;++y) {
            for(uint32_t x=crop.x;x<crop.x+crop.width;++x,++index) {
                const uint32_t rgb=pixels[size_t(y)*width+x]&0x00ffffffu;
                if(out.comparable && rgb!=previous_[index])++out.changed_pixels;
                previous_[index]=rgb;
                // Ymir uses little-channel-order words: low byte is red.
                for(unsigned shift=0;shift<24;shift+=8)
                    out.rgb_fingerprint=(out.rgb_fingerprint^((rgb>>shift)&0xffu))*fnv_prime;
            }
        }
        out.compared_pixels=out.comparable?static_cast<uint32_t>(count):0;
        width_=width;height_=height;return out;
    }
private:
    Crop requested_;
    std::vector<uint32_t> previous_;
    uint32_t width_=0,height_=0;
};
} // namespace harness::video
