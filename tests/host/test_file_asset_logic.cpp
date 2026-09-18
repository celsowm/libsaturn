#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/asset.h"
#include "saturn/file.h"
#include "src/core/file_asset_runtime.hpp"
#include "src/core/palette_registry.hpp"
#include "src/core/runtime_state.hpp"
#include "src/core/texture_runtime.hpp"
#include "src/hal/vdp1.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace saturn::hal::vdp1 {

sat_result_t upload_palette(const uint16_t*, uint16_t) { return SAT_OK; }

sat_result_t upload_texture_indexed8_pitched(
    const uint8_t*, uint16_t width, uint16_t height, uint16_t pitch, uint16_t* out_srca) {
    if (out_srca == nullptr || width == 0u || height == 0u || pitch < width) return SAT_ERR_INVALID_ARG;
    *out_srca = 0x2000u;
    return SAT_OK;
}

sat_result_t update_texture_indexed8_pitched(
    uint16_t, const uint8_t*, uint16_t width, uint16_t height, uint16_t pitch) {
    return width == 0u || height == 0u || pitch < width ? SAT_ERR_INVALID_ARG : SAT_OK;
}

}  // namespace saturn::hal::vdp1

extern "C" sat_result_t sat_sound_create(sat_sound_t*, const sat_sound_desc_t*) {
    return SAT_ERR_UNSUPPORTED;
}

extern "C" sat_result_t sat_font_init(
    sat_font_t*, sat_texture_t, const sat_font_glyph_t*, uint16_t, uint16_t, uint16_t) {
    return SAT_ERR_UNSUPPORTED;
}

int main() {
    char normalized[SAT_FILE_PATH_MAX] = {};
    OK(saturn::core::normalize_path("./assets\\player//sprite.bin", normalized, sizeof(normalized)) == SAT_OK);
    OK(std::strcmp(normalized, "assets/player/sprite.bin") == 0);
    OK(saturn::core::normalize_path("assets/../secret.bin", normalized, sizeof(normalized)) == SAT_ERR_INVALID_ARG);
    OK(saturn::core::normalize_path("./", normalized, sizeof(normalized)) == SAT_ERR_INVALID_ARG);

    saturn::core::file_asset_runtime_reset(saturn::core::g_file_asset_runtime);
    const uint8_t bytes[] = {10u, 20u, 30u, 40u, 50u};
    OK(sat_file_register_blob("data\\sample.bin", bytes, sizeof(bytes)) == SAT_OK);
    sat_file_t file{};
    OK(sat_file_open("./data/sample.bin", &file) == SAT_OK);
    uint32_t size = 0u;
    OK(sat_file_size(file, &size) == SAT_OK && size == sizeof(bytes));
    uint8_t out[3] = {};
    uint32_t read = 0u;
    OK(sat_file_read(file, out, 3u, &read) == SAT_OK && read == 3u);
    OK(std::memcmp(out, bytes, 3u) == 0);
    OK(sat_file_seek(file, -1, SAT_FILE_SEEK_END) == SAT_OK);
    OK(sat_file_read(file, out, 3u, &read) == SAT_OK && read == 1u && out[0] == 50u);
    const sat_file_t stale = file;
    OK(sat_file_close(file) == SAT_OK);
    OK(sat_file_tell(stale, &size) == SAT_ERR_INVALID_ARG);
    OK(sat_file_open("missing.bin", &file) == SAT_ERR_NOT_FOUND);

    uint8_t texture_data[8u * 8u] = {};
    uint16_t palette[256u] = {};
    sat_asset_desc_t desc = {};
    desc.logical_path = "assets/player.png";
    desc.data = texture_data;
    desc.size = sizeof(texture_data);
    desc.pitch = 8u;
    desc.width = 8u;
    desc.height = 8u;
    desc.palette_rgb555 = palette;
    desc.palette_count = 256u;
    desc.format = SAT_PIXEL_INDEX8;
    desc.kind = SAT_ASSET_TEXTURE;
    sat_asset_t asset{};
    OK(sat_asset_register(&desc, &asset) == SAT_OK);
    sat_asset_t opened{};
    OK(sat_asset_open("./assets/player.png", &opened) == SAT_OK);
    sat_asset_info_t info{};
    OK(sat_asset_info(opened, &info) == SAT_OK && info.kind == SAT_ASSET_TEXTURE &&
       info.data == texture_data && info.size == sizeof(texture_data));
    saturn::core::g_state = {};
    saturn::core::g_state.initialized = true;
    saturn::core::palette_registry_reset(saturn::core::g_palette_registry);
    saturn::core::texture_registry_reset(saturn::core::g_texture_registry);
    sat_texture_t texture{};
    OK(sat_texture_load("assets/player.png", &texture) == SAT_OK);
    sat_texture_info_t texture_info{};
    OK(sat_texture_info(texture, &texture_info) == SAT_OK &&
       texture_info.width == 8u && texture_info.height == 8u);
    const uint8_t raw_data[] = {7u, 8u, 9u};
    sat_asset_desc_t raw_desc = {};
    raw_desc.logical_path = "data/raw.bin";
    raw_desc.data = raw_data;
    raw_desc.size = sizeof(raw_data);
    raw_desc.kind = SAT_ASSET_DATA;
    sat_asset_t raw_asset{};
    OK(sat_asset_register(&raw_desc, &raw_asset) == SAT_OK);
    const void* loaded_data = nullptr;
    uint32_t loaded_size = 0u;
    OK(sat_asset_load_data("data/raw.bin", &loaded_data, &loaded_size) == SAT_OK &&
       loaded_data == raw_data && loaded_size == sizeof(raw_data));
    OK(sat_asset_load_data("assets/player.png", &loaded_data, &loaded_size) == SAT_ERR_UNSUPPORTED);
    OK(sat_texture_destroy(texture) == SAT_OK);
    const sat_asset_t stale_asset = asset;
    OK(sat_asset_close(asset) == SAT_OK);
    OK(sat_asset_info(stale_asset, &info) == SAT_ERR_INVALID_ARG);
    std::puts("file asset logic: OK");
    return 0;
}
