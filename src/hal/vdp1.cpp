#include "src/hal/vdp1.hpp"

namespace saturn::hal::vdp1 {

namespace {

volatile uint16_t& TVMR = *reinterpret_cast<volatile uint16_t*>(0x05D00000);
volatile uint16_t& FBCR = *reinterpret_cast<volatile uint16_t*>(0x05D00002);
volatile uint16_t& PTMR = *reinterpret_cast<volatile uint16_t*>(0x05D00004);
volatile uint16_t& EWDR = *reinterpret_cast<volatile uint16_t*>(0x05D00006);
volatile uint16_t& EWLR = *reinterpret_cast<volatile uint16_t*>(0x05D00008);
volatile uint16_t& EWRR = *reinterpret_cast<volatile uint16_t*>(0x05D0000A);

volatile uint16_t* const VDP2_CRAM = reinterpret_cast<volatile uint16_t*>(0x05F00000);
volatile uint32_t* const VDP1_VRAM_32 = reinterpret_cast<volatile uint32_t*>(0x05C80000);
volatile uint8_t* const VDP1_VRAM_8 = reinterpret_cast<volatile uint8_t*>(0x05C80000);

constexpr uint32_t kVramSize = 512u * 1024u;
constexpr uint32_t kCommandAreaBytes = 16u * 1024u;

Command* g_cmd_buffer = nullptr;
uint16_t g_cmd_capacity = 0;
uint16_t g_cmd_count = 0;
uint32_t g_texture_cursor = kCommandAreaBytes;
uint16_t g_width = 320;
uint16_t g_height = 224;

inline void copy_words_to_vram(const Command* src, uint32_t count_commands) {
    const uint32_t dwords = (count_commands * sizeof(Command)) / sizeof(uint32_t);
    const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
    for (uint32_t i = 0; i < dwords; ++i) {
        VDP1_VRAM_32[i] = s[i];
    }
}

}  // namespace

void init(uint16_t width, uint16_t height, uint16_t clear_color) {
    g_width = width;
    g_height = height;
    g_texture_cursor = kCommandAreaBytes;

    TVMR = 0x0000;
    FBCR = 0x0000;
    PTMR = 0x0000;
    EWDR = clear_color;
    EWLR = 0x0000;
    EWRR = static_cast<uint16_t>(((width / 8u) << 9u) | height);

    VDP1_VRAM_32[0] = 0x80000000u;
    VDP1_VRAM_32[1] = 0x00000000u;
    VDP1_VRAM_32[2] = 0x00000000u;
    VDP1_VRAM_32[3] = 0x00000000u;

    PTMR = 0x0002;
}

void set_clear_color(uint16_t rgb555) {
    EWDR = rgb555;
}

void begin_frame(Command* command_buffer, uint16_t capacity) {
    g_cmd_buffer = command_buffer;
    g_cmd_capacity = capacity;
    g_cmd_count = 0;
}

sat_result_t push_sprite(const SpriteRequest& req) {
    if (g_cmd_buffer == nullptr) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    if (req.width == 0 || req.height == 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((req.width & 7u) != 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if (g_cmd_count + 1u >= g_cmd_capacity) {
        return SAT_ERR_CAPACITY;
    }

    Command& cmd = g_cmd_buffer[g_cmd_count++];
    cmd.ctrl = 0x0002;
    cmd.link = 0;
    cmd.pmod = 0x00C0;
    cmd.colr = static_cast<uint16_t>(req.palette << 8u);
    cmd.srca = req.srca;
    cmd.size = static_cast<uint16_t>(((req.width / 8u) << 8u) | req.height);
    cmd.xa = req.x;
    cmd.ya = req.y;
    cmd.xb = static_cast<int16_t>(req.x + static_cast<int16_t>(req.width));
    cmd.yb = req.y;
    cmd.xc = static_cast<int16_t>(req.x + static_cast<int16_t>(req.width));
    cmd.yc = static_cast<int16_t>(req.y + static_cast<int16_t>(req.height));
    cmd.xd = req.x;
    cmd.yd = static_cast<int16_t>(req.y + static_cast<int16_t>(req.height));
    cmd.grda = 0;
    cmd.pad = 0;
    return SAT_OK;
}

void submit() {
    if (g_cmd_buffer == nullptr) {
        return;
    }
    if (g_cmd_count >= g_cmd_capacity) {
        g_cmd_count = static_cast<uint16_t>(g_cmd_capacity - 1u);
    }

    Command& end = g_cmd_buffer[g_cmd_count++];
    end.ctrl = 0x8000;
    end.link = 0;
    end.pmod = 0;
    end.colr = 0;
    end.srca = 0;
    end.size = 0;
    end.xa = 0;
    end.ya = 0;
    end.xb = 0;
    end.yb = 0;
    end.xc = 0;
    end.yc = 0;
    end.xd = 0;
    end.yd = 0;
    end.grda = 0;
    end.pad = 0;

    copy_words_to_vram(g_cmd_buffer, g_cmd_count);
}

sat_result_t upload_palette(const uint16_t* palette_rgb555, uint16_t palette_index) {
    if (palette_rgb555 == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t base = static_cast<uint32_t>(palette_index) * 256u;
    for (uint32_t i = 0; i < 256u; ++i) {
        VDP2_CRAM[base + i] = palette_rgb555[i];
    }
    return SAT_OK;
}

sat_result_t upload_texture_indexed8(const uint8_t* pixels, uint16_t width, uint16_t height, uint16_t* out_srca) {
    if (pixels == nullptr || out_srca == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (width == 0 || height == 0 || (width & 7u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    uint32_t size = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
    g_texture_cursor = (g_texture_cursor + 7u) & ~7u;
    if (g_texture_cursor + size > kVramSize) {
        return SAT_ERR_CAPACITY;
    }

    const uint32_t start = g_texture_cursor;
    for (uint32_t i = 0; i < size; ++i) {
        VDP1_VRAM_8[start + i] = pixels[i];
    }

    *out_srca = static_cast<uint16_t>(start >> 3u);
    g_texture_cursor += size;
    return SAT_OK;
}

}  // namespace saturn::hal::vdp1

