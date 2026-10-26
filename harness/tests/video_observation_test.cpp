// GPL-3.0: procedural display-change contracts; no firmware or game assets.
#include "video_observation.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <limits>

int main() {
    using namespace harness::video;
    Crop parsed;
    assert(parse_crop("0:61:320:135",parsed));
    assert(parsed.x==0 && parsed.y==61 && parsed.width==320 && parsed.height==135);
    for(const char* bad:{"", "0:0:1", "0:0:1:1:1", "0:0:0:1", "-1:0:1:1",
                         "0:0:1:garbage", "0:0:4294967296:1", "0:0:1:1x", " 0:0:1:1"})
        assert(!parse_crop(bad,parsed));
    std::array<uint32_t,12> frame{};
    Observer all;
    auto first=all.observe(frame,4,3);
    assert(!first.comparable && !first.compared_pixels && !first.changed_pixels);
    auto same=all.observe(frame,4,3);
    assert(same.comparable && same.compared_pixels==12 && !same.changed_pixels);
    assert(same.rgb_fingerprint==first.rgb_fingerprint);
    frame[0]=0xff000000; // Alpha-only writes are not visible RGB changes.
    assert(!all.observe(frame,4,3).changed_pixels);
    frame[0]=0x000000ff;
    auto changed=all.observe(frame,4,3);
    assert(changed.changed_pixels==1 && changed.rgb_fingerprint!=first.rgb_fingerprint);
    assert(!all.observe(frame,4,3).changed_pixels); // Repeated drawing is not a new image.
    frame[0]=0;
    assert(all.observe(frame,4,3).changed_pixels==1); // Returning to an old image is a transition.
    Observer crop({1,1,2,1});
    crop.observe(frame,4,3);
    frame[0]=7;
    assert(!crop.observe(frame,4,3).changed_pixels); // Excluded HUD/border.
    frame[5]=1;
    assert(crop.observe(frame,4,3).changed_pixels==1);
    auto resized=all.observe(frame,3,4);
    assert(resized.dimensions_changed && !resized.comparable);
    assert(all.observe(frame,3,4).comparable);
    auto invalid=[&](auto action) {
        bool rejected=false;
        try { action(); } catch(const std::invalid_argument&) { rejected=true; }
        assert(rejected);
    };
    invalid([&] { all.observe(frame,0,0); });
    invalid([&] { all.observe(frame,4,4); });
    invalid([&] { Observer({4,0,1,1}).observe(frame,4,3); });
    invalid([&] { Observer({0,0,0,1}).observe(frame,4,3); });
    invalid([&] { Observer({std::numeric_limits<uint32_t>::max(),0,2,1}).observe(frame,4,3); });
    return 0;
}
