#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/asset.h"
#include "saturn/file.h"
#include "src/core/file_asset_runtime.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

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

    const uint8_t texture_data[] = {1u, 2u, 3u};
    sat_asset_desc_t desc = {};
    desc.logical_path = "assets/player.png";
    desc.data = texture_data;
    desc.size = sizeof(texture_data);
    desc.width = 8u;
    desc.height = 8u;
    desc.kind = SAT_ASSET_TEXTURE;
    sat_asset_t asset{};
    OK(sat_asset_register(&desc, &asset) == SAT_OK);
    sat_asset_t opened{};
    OK(sat_asset_open("./assets/player.png", &opened) == SAT_OK);
    sat_asset_info_t info{};
    OK(sat_asset_info(opened, &info) == SAT_OK && info.kind == SAT_ASSET_TEXTURE &&
       info.data == texture_data && info.size == sizeof(texture_data));
    const sat_asset_t stale_asset = asset;
    OK(sat_asset_close(asset) == SAT_OK);
    OK(sat_asset_info(stale_asset, &info) == SAT_ERR_INVALID_ARG);
    std::puts("file asset logic: OK");
    return 0;
}
