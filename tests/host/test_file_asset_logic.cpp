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

namespace {

struct BackendBlob {
    const uint8_t* data;
    uint32_t size;
};

sat_result_t backend_read_at(
    void* context,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
) {
    if (context == nullptr || destination == nullptr || out_read == nullptr) return SAT_ERR_INVALID_ARG;
    const BackendBlob& blob = *static_cast<const BackendBlob*>(context);
    if (offset > blob.size) return SAT_ERR_IO;
    const uint32_t available = blob.size - offset;
    const uint32_t count = bytes < available ? bytes : available;
    const uint32_t partial = count > 2u ? 2u : count;
    std::memcpy(destination, blob.data + offset, partial);
    *out_read = partial;
    return SAT_OK;
}

}  // namespace

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
    BackendBlob backend{bytes, sizeof(bytes)};
    OK(sat_file_register_backend("data/backend.bin", sizeof(bytes), backend_read_at,
                                 &backend) == SAT_OK);
    sat_file_t backend_file{};
    OK(sat_file_open("data/backend.bin", &backend_file) == SAT_OK);
    OK(sat_file_seek(backend_file, 0, SAT_FILE_SEEK_SET) == SAT_OK);
    uint8_t backend_out[4] = {};
    OK(sat_file_read(backend_file, backend_out, sizeof(backend_out), &read) == SAT_OK &&
       read == 2u && backend_out[0] == 10u && backend_out[1] == 20u);
    OK(sat_file_read(backend_file, backend_out, sizeof(backend_out), &read) == SAT_OK &&
       read == 2u && backend_out[0] == 30u && backend_out[1] == 40u);
    OK(sat_file_close(backend_file) == SAT_OK);
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
    sat_asset_desc_t physical_desc = {};
    physical_desc.logical_path = "data/cd-raw.bin";
    physical_desc.source_path = "ASSETS/CD-RAW.BIN";
    physical_desc.size = sizeof(raw_data);
    physical_desc.kind = SAT_ASSET_DATA;
    sat_asset_t physical_asset{};
    OK(sat_asset_register(&physical_desc, &physical_asset) == SAT_OK);
    OK(sat_asset_info(physical_asset, &info) == SAT_OK &&
       std::strcmp(info.source_path, "ASSETS/CD-RAW.BIN") == 0 &&
       info.data == nullptr && info.size == sizeof(raw_data));
    OK(sat_asset_load_data("data/cd-raw.bin", &loaded_data, &loaded_size) == SAT_ERR_IO);
    OK(sat_file_register_blob("ASSETS/CD-RAW.BIN", raw_data, sizeof(raw_data)) == SAT_OK);
    uint8_t physical_out[4] = {};
    uint32_t physical_read = 0u;
    OK(sat_asset_read_at("data/cd-raw.bin", 1u, physical_out, sizeof(physical_out), &physical_read) == SAT_OK &&
       physical_read == 2u && physical_out[0] == 8u && physical_out[1] == 9u);
    OK(sat_asset_read_at("data/cd-raw.bin", 0u, physical_out, 1u, &physical_read) == SAT_OK &&
       physical_read == 1u && physical_out[0] == 7u);
    sat_asset_cache_stats_t cache_stats{};
    OK(sat_asset_cache_stats(&cache_stats) == SAT_OK && cache_stats.used == 1u &&
       cache_stats.fills == 1u && cache_stats.hits >= 1u);
    uint8_t streamed_data[4096u] = {};
    for (uint32_t i = 0u; i < sizeof(streamed_data); ++i) {
        streamed_data[i] = static_cast<uint8_t>(i ^ 0x5Au);
    }
    OK(sat_file_register_blob("ASSETS/STREAM.BIN", streamed_data, sizeof(streamed_data)) == SAT_OK);
    sat_asset_desc_t streamed_desc = {};
    streamed_desc.logical_path = "data/stream.bin";
    streamed_desc.source_path = "ASSETS/STREAM.BIN";
    streamed_desc.size = sizeof(streamed_data);
    streamed_desc.kind = SAT_ASSET_DATA;
    OK(sat_asset_register(&streamed_desc, nullptr) == SAT_ERR_INVALID_ARG);
    sat_asset_t streamed_asset{};
    OK(sat_asset_register(&streamed_desc, &streamed_asset) == SAT_OK);
    sat_asset_prefetch_t prefetch{};
    OK(sat_asset_prefetch_submit("data/stream.bin", 100u, 3000u, &prefetch) == SAT_OK);
    sat_asset_prefetch_state_t prefetch_state{};
    sat_result_t prefetch_result = SAT_ERR_BUSY;
    uint32_t cached_bytes = 0u;
    OK(sat_asset_prefetch_status(prefetch, &prefetch_state, &prefetch_result, &cached_bytes) == SAT_OK &&
       prefetch_state == SAT_ASSET_PREFETCH_PENDING);
    OK(sat_asset_prefetch_update() == SAT_OK);
    OK(sat_asset_prefetch_update() == SAT_OK);
    OK(sat_asset_prefetch_status(prefetch, &prefetch_state, &prefetch_result, &cached_bytes) == SAT_OK &&
       prefetch_state == SAT_ASSET_PREFETCH_COMPLETE && prefetch_result == SAT_OK &&
       cached_bytes == 4096u);
    uint8_t streamed_out[3000u] = {};
    OK(sat_asset_read_at("data/stream.bin", 100u, streamed_out, sizeof(streamed_out), &physical_read) == SAT_OK &&
       physical_read == sizeof(streamed_out) && std::memcmp(streamed_out, streamed_data + 100u,
                                                             sizeof(streamed_out)) == 0);
    const uint32_t stream_first_block = 100u / SAT_ASSET_CACHE_BLOCK_BYTES;
    const uint32_t stream_last_block = (100u + 3000u - 1u) / SAT_ASSET_CACHE_BLOCK_BYTES;
    const uint32_t stream_cache_blocks = stream_last_block - stream_first_block + 1u;
    OK(sat_asset_cache_stats(&cache_stats) == SAT_OK && cache_stats.prefetch_completed == 1u &&
       cache_stats.used == 1u + stream_cache_blocks);

    /* A full 64 KiB cache block must remain readable on a cache hit. This
     * guards SAT_ASSET_CACHE_BLOCK_BYTES == 65536 against truncating the
     * cached valid-byte count to uint16_t (65536 -> 0). */
    static uint8_t full_block_data[SAT_ASSET_CACHE_BLOCK_BYTES] = {};
    for (uint32_t i = 0u; i < sizeof(full_block_data); ++i) {
        full_block_data[i] = static_cast<uint8_t>((i * 13u + 7u) & 0xFFu);
    }
    OK(sat_file_register_blob("ASSETS/FULL-BLOCK.BIN", full_block_data, sizeof(full_block_data)) == SAT_OK);
    sat_asset_desc_t full_block_desc = {};
    full_block_desc.logical_path = "data/full-block.bin";
    full_block_desc.source_path = "ASSETS/FULL-BLOCK.BIN";
    full_block_desc.size = sizeof(full_block_data);
    full_block_desc.kind = SAT_ASSET_DATA;
    sat_asset_t full_block_asset{};
    OK(sat_asset_register(&full_block_desc, &full_block_asset) == SAT_OK);
    uint8_t full_block_probe[4] = {};
    OK(sat_asset_read_at("data/full-block.bin", SAT_ASSET_CACHE_BLOCK_BYTES - 4u,
                         full_block_probe, sizeof(full_block_probe), &physical_read) == SAT_OK &&
       physical_read == sizeof(full_block_probe) &&
       std::memcmp(full_block_probe, full_block_data + SAT_ASSET_CACHE_BLOCK_BYTES - 4u,
                   sizeof(full_block_probe)) == 0);
    std::memset(full_block_probe, 0, sizeof(full_block_probe));
    OK(sat_asset_read_at("data/full-block.bin", 0u, full_block_probe,
                         sizeof(full_block_probe), &physical_read) == SAT_OK &&
       physical_read == sizeof(full_block_probe) &&
       std::memcmp(full_block_probe, full_block_data, sizeof(full_block_probe)) == 0);
    sat_asset_desc_t missing_desc = streamed_desc;
    missing_desc.logical_path = "data/missing-stream.bin";
    missing_desc.source_path = "ASSETS/MISSING.BIN";
    sat_asset_t missing_asset{};
    OK(sat_asset_register(&missing_desc, &missing_asset) == SAT_OK);
    sat_asset_prefetch_t failed_prefetch{};
    OK(sat_asset_prefetch_submit("data/missing-stream.bin", 0u, 1u, &failed_prefetch) == SAT_OK);
    OK(sat_asset_prefetch_update() == SAT_ERR_NOT_FOUND);
    OK(sat_asset_prefetch_status(failed_prefetch, &prefetch_state, &prefetch_result, &cached_bytes) == SAT_OK &&
       prefetch_state == SAT_ASSET_PREFETCH_FAILED && prefetch_result == SAT_ERR_NOT_FOUND);
    OK(sat_asset_prefetch_cancel(failed_prefetch) == SAT_OK);
    OK(sat_asset_load_data("assets/player.png", &loaded_data, &loaded_size) == SAT_ERR_UNSUPPORTED);
    OK(sat_texture_destroy(texture) == SAT_OK);
    const sat_asset_t stale_asset = asset;
    OK(sat_asset_close(asset) == SAT_OK);
    OK(sat_asset_info(stale_asset, &info) == SAT_ERR_INVALID_ARG);
    std::puts("file asset logic: OK");
    return 0;
}
