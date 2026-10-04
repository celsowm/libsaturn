#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/cdfs.h"
#include "saturn/file.h"
#include "saturn/loader.h"
#include "saturn/vdp1.h"
#include "saturn/video.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {

constexpr uint32_t kSectorCount = 32u;
uint8_t g_image[kSectorCount * SAT_CD_SECTOR_BYTES] = {};

void put_le32(uint8_t* destination, uint32_t value) {
    destination[0] = static_cast<uint8_t>(value);
    destination[1] = static_cast<uint8_t>(value >> 8);
    destination[2] = static_cast<uint8_t>(value >> 16);
    destination[3] = static_cast<uint8_t>(value >> 24);
}

uint8_t add_record(uint8_t* directory, uint16_t offset, uint32_t lba, uint32_t size,
                  uint8_t flags, const char* name, uint8_t name_length) {
    uint8_t length = static_cast<uint8_t>(33u + name_length + ((name_length & 1u) == 0u ? 1u : 0u));
    directory[offset] = length;
    put_le32(directory + offset + 2u, lba);
    put_le32(directory + offset + 10u, size);
    directory[offset + 25u] = flags;
    directory[offset + 32u] = name_length;
    for (uint8_t i = 0u; i < name_length; ++i) directory[offset + 33u + i] = name[i];
    return length;
}

sat_result_t read_image(void*, uint32_t lba, uint32_t count, void* destination) {
    if (destination == nullptr || count == 0u || lba >= kSectorCount ||
        count > kSectorCount - lba) return SAT_ERR_IO;
    std::memcpy(destination, g_image + lba * SAT_CD_SECTOR_BYTES,
                count * SAT_CD_SECTOR_BYTES);
    return SAT_OK;
}

void make_image() {
    std::memset(g_image, 0, sizeof(g_image));
    uint8_t* pvd = g_image + 16u * SAT_CD_SECTOR_BYTES;
    pvd[0] = 1u;
    pvd[1] = 'C'; pvd[2] = 'D'; pvd[3] = '0'; pvd[4] = '0'; pvd[5] = '1'; pvd[6] = 1u;
    pvd[156u] = 34u;
    put_le32(pvd + 158u, 20u);
    put_le32(pvd + 166u, SAT_CD_SECTOR_BYTES);
    pvd[181u] = 2u;
    pvd[182u] = 0u;

    uint8_t* root = g_image + 20u * SAT_CD_SECTOR_BYTES;
    uint16_t offset = 0u;
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "\0", 1u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "\1", 1u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 21u, SAT_CD_SECTOR_BYTES, 2u, "ASSETS", 6u));
    (void)add_record(root, offset, 22u, 5u, 0u, "ROOT.TXT;1", 10u);

    uint8_t* assets = g_image + 21u * SAT_CD_SECTOR_BYTES;
    (void)add_record(assets, 0u, 21u, SAT_CD_SECTOR_BYTES, 2u, "\0", 1u);
    (void)add_record(assets, 34u, 21u, SAT_CD_SECTOR_BYTES, 2u, "\1", 1u);
    (void)add_record(assets, 68u, 23u, SAT_CD_SECTOR_BYTES + 2u, 0u, "PLAYER.BIN;1", 12u);
    for (uint32_t i = 0u; i < SAT_CD_SECTOR_BYTES; ++i) g_image[23u * SAT_CD_SECTOR_BYTES + i] = static_cast<uint8_t>(i);
    g_image[24u * SAT_CD_SECTOR_BYTES] = 0xA0u;
    g_image[24u * SAT_CD_SECTOR_BYTES + 1u] = 0xA1u;
    std::memcpy(g_image + 22u * SAT_CD_SECTOR_BYTES, "hello", 5u);
}

struct Progress {
    uint32_t calls = 0u;
    uint32_t last_done = 0u;
    uint32_t total = 0u;
    bool monotonic = true;
};

void on_progress(void* context, uint32_t done, uint32_t total) {
    Progress* p = static_cast<Progress*>(context);
    if (done < p->last_done) p->monotonic = false;
    p->last_done = done;
    p->total = total;
    ++p->calls;
}

int g_frames = 0, g_rects = 0, g_back = 0;
uint16_t g_last_rect_width = 0u;

}  // namespace

extern "C" sat_result_t sat_begin_frame(void) { ++g_frames; return SAT_OK; }
extern "C" sat_result_t sat_end_frame(void) { return SAT_OK; }
extern "C" sat_result_t sat_wait_vblank(void) { return SAT_OK; }
extern "C" sat_result_t sat_vdp2_back_color_set(uint16_t) { ++g_back; return SAT_OK; }
extern "C" sat_result_t sat_draw_rect_screen(int16_t, int16_t, uint16_t w, uint16_t, uint16_t) {
    ++g_rects;
    g_last_rect_width = w;
    return SAT_OK;
}

int main() {
    make_image();
    sat_cd_device_t device{};
    OK(sat_cd_device_init(&device, read_image, nullptr, kSectorCount) == SAT_OK);
    sat_cdfs_volume_t volume{};
    OK(sat_cdfs_mount(&volume, &device) == SAT_OK);

    // PLAYER.BIN is a sector and two bytes: bytes 0..255 repeating, then A0 A1.
    uint8_t first[100] = {};
    uint8_t second[SAT_CD_SECTOR_BYTES - 100u + 2u] = {};
    uint8_t hello[5] = {};
    Progress progress;
    const sat_load_item_t items[3] = {
        { "assets/player.bin", SAT_CD_SECTOR_BYTES + 2u, 0u, 100u, first },
        { "assets/player.bin", 0u, 100u, 0u, second },
        { "ROOT.TXT", 0u, 0u, 5u, hello },
    };
    // A one-sector chunk makes every item go through more than one read.
    OK(sat_load_files(&volume, items, 3u, SAT_CD_SECTOR_BYTES, on_progress, &progress) == SAT_OK);
    OK(first[0] == 0u && first[99] == 99u);
    OK(second[0] == 100u && second[SAT_CD_SECTOR_BYTES - 101u] == 0xFFu);
    OK(second[SAT_CD_SECTOR_BYTES - 100u] == 0xA0u && second[SAT_CD_SECTOR_BYTES - 99u] == 0xA1u);
    OK(hello[0] == 'h' && hello[4] == 'o');
    OK(progress.total == 100u + (SAT_CD_SECTOR_BYTES - 98u) + 5u);
    OK(progress.last_done == progress.total && progress.monotonic && progress.calls >= 4u);

    // Validation happens before any byte moves.
    uint8_t untouched[8] = { 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u };
    const sat_load_item_t bad_size[2] = {
        { "ROOT.TXT", 0u, 0u, 5u, untouched },
        { "assets/player.bin", 99u, 0u, 0u, untouched },
    };
    OK(sat_load_files(&volume, bad_size, 2u, 0u, nullptr, nullptr) == SAT_ERR_VERSION);
    OK(untouched[0] == 7u);
    const sat_load_item_t missing = { "nope.bin", 0u, 0u, 0u, untouched };
    OK(sat_load_files(&volume, &missing, 1u, 0u, nullptr, nullptr) == SAT_ERR_NOT_FOUND);
    const sat_load_item_t past_end = { "ROOT.TXT", 0u, 3u, 9u, untouched };
    OK(sat_load_files(&volume, &past_end, 1u, 0u, nullptr, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_load_files(&volume, nullptr, 0u, 0u, nullptr, nullptr) == SAT_ERR_INVALID_ARG);

    // The loading screen draws one frame per callback; the fill follows done/total.
    sat_loading_screen_t screen{};
    sat_loading_screen_default(&screen);
    sat_loading_screen_progress(&screen, 0u, 100u);
    OK(g_frames == 1 && g_back == 1 && g_rects == 2);
    sat_loading_screen_progress(&screen, 50u, 100u);
    OK(g_frames == 2 && g_rects == 5 && g_last_rect_width == screen.width / 2u);
    sat_loading_screen_progress(&screen, 5u, 0u);
    OK(g_frames == 2);
    std::puts("loader: OK");
    return 0;
}
