#include <cstdio>
#include <cstdlib>

#include "src/hal/vdp1.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    using namespace saturn::hal::vdp1;

    Command commands[8]{};
    begin_frame(commands, 8u);

    UserClipRequest clip{10u, 20u, 100u, 80u};
    OK(push_user_clip(clip) == SAT_OK);
    OK(commands[2].ctrl == 0x0008u);
    OK(commands[2].xa == 10 && commands[2].ya == 20);
    OK(commands[2].xc == 100 && commands[2].yc == 80);

    SpriteRequest sprite{};
    sprite.width = 8u;
    sprite.height = 8u;
    sprite.user_clip = true;
    OK(push_sprite(sprite) == SAT_OK);
    OK((commands[3].pmod & 0x0400u) != 0u);
    OK((commands[3].pmod & 0x0200u) == 0u);

    UserClipRequest invalid_order{20u, 20u, 10u, 30u};
    OK(push_user_clip(invalid_order) == SAT_ERR_INVALID_ARG);
    UserClipRequest outside{0u, 0u, 320u, 223u};
    OK(push_user_clip(outside) == SAT_ERR_INVALID_ARG);

    Command small[3]{};
    begin_frame(small, 3u);
    OK(push_user_clip(clip) == SAT_ERR_CAPACITY);

    std::puts("vdp1 clip: OK");
    return 0;
}
