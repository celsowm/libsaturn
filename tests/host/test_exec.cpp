#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/cdfs.h"
#include "saturn/exec.h"
#include "src/storage/cd/exec_hal.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace logic = saturn::storage::exec::logic;

namespace {

/* A small ISO: PVD at 16, root directory at 20, files from 22. GAME.BIN and
 * HUGE.BIN are generated rather than stored (lba >= kGeneratedLba). */
constexpr uint32_t kStoredSectors = 32u;
constexpr uint32_t kGeneratedLba = 1000u;
constexpr uint32_t kSectorCount = 4000u;
uint8_t g_image[kStoredSectors * SAT_CD_SECTOR_BYTES] = {};

alignas(4) uint8_t g_staging[SAT_EXEC_MAX_BYTES] = {};
logic::Handoff g_handoff = {};
int g_stops = 0;
int g_reads = 0;
int g_failures_left = 0;     /* reads to fail before succeeding */
bool g_fail_always = false;
uint32_t g_started_bytes = 0u;

struct Started {};
struct Reset {};

uint8_t generated_byte(uint32_t lba, uint32_t offset) {
    return static_cast<uint8_t>((lba * 7u) ^ (offset * 13u) ^ (offset >> 8));
}

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

sat_result_t read_sectors(void*, uint32_t lba, uint32_t count, void* destination) {
    if (destination == nullptr || count == 0u || lba >= kSectorCount ||
        count > kSectorCount - lba) return SAT_ERR_IO;
    if (lba >= 22u) {
        ++g_reads;
        if (g_fail_always) return SAT_ERR_IO;
        if (g_failures_left > 0) {
            --g_failures_left;
            return SAT_ERR_IO;
        }
    }
    uint8_t* out = static_cast<uint8_t*>(destination);
    for (uint32_t s = 0; s < count; ++s, ++lba, out += SAT_CD_SECTOR_BYTES) {
        if (lba < kStoredSectors) {
            std::memcpy(out, g_image + lba * SAT_CD_SECTOR_BYTES, SAT_CD_SECTOR_BYTES);
        } else {
            for (uint32_t i = 0; i < SAT_CD_SECTOR_BYTES; ++i) out[i] = generated_byte(lba, i);
        }
    }
    return SAT_OK;
}

constexpr uint32_t kSmallBytes = 5000u;                 /* 3 sectors at 22 */
constexpr uint32_t kGameBytes = 200u * SAT_CD_SECTOR_BYTES - 100u;  /* 200 sectors: two reads */

void make_image() {
    std::memset(g_image, 0, sizeof(g_image));
    uint8_t* pvd = g_image + 16u * SAT_CD_SECTOR_BYTES;
    pvd[0] = 1u;
    pvd[1] = 'C'; pvd[2] = 'D'; pvd[3] = '0'; pvd[4] = '0'; pvd[5] = '1'; pvd[6] = 1u;
    pvd[156u] = 34u;
    put_le32(pvd + 158u, 20u);
    put_le32(pvd + 166u, SAT_CD_SECTOR_BYTES);

    uint8_t* root = g_image + 20u * SAT_CD_SECTOR_BYTES;
    uint16_t offset = 0u;
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "\0", 1u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "\1", 1u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 22u, kSmallBytes, 0u, "0.BIN;1", 7u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, kGeneratedLba, kGameBytes, 0u, "GAME.BIN;1", 10u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, kGeneratedLba, SAT_EXEC_MAX_BYTES + 1u, 0u, "HUGE.BIN;1", 10u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 26u, 0u, 0u, "EMPTY.BIN;1", 11u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "DIR", 3u));
    (void)offset;
    for (uint32_t i = 0; i < 3u * SAT_CD_SECTOR_BYTES; ++i) {
        g_image[22u * SAT_CD_SECTOR_BYTES + i] = static_cast<uint8_t>(i * 3u + 1u);
    }
}

sat_cd_device_t g_device;
sat_cdfs_volume_t g_volume;

void mount() {
    OK(sat_cd_device_init(&g_device, read_sectors, nullptr, kSectorCount) == SAT_OK);
    OK(sat_cdfs_mount(&g_volume, &g_device) == SAT_OK);
}

void reset_counters() {
    g_stops = 0;
    g_reads = 0;
    g_failures_left = 0;
    g_fail_always = false;
    g_started_bytes = 0u;
    std::memset(g_staging, 0xEE, sizeof(g_staging));
}

/* Runs sat_exec and reports how it ended: 'S' started the image, 'R' reset, 'E' returned. */
char run(const char* path, const void* arg, uint32_t bytes, sat_result_t* out_rc = nullptr) {
    try {
        const sat_result_t rc = sat_exec(&g_volume, path, arg, bytes);
        if (out_rc != nullptr) *out_rc = rc;
        return 'E';
    } catch (const Started&) {
        return 'S';
    } catch (const Reset&) {
        return 'R';
    }
}

}  // namespace

/* Only reached with a bound CD block, which the fake transport never is. */
extern "C" sat_result_t sat_cd_stream_finish(sat_cd_block_t*) { return SAT_ERR_UNSUPPORTED; }
extern "C" sat_result_t sat_cd_block_set_progress_service(sat_cd_block_t*, sat_cd_block_progress_fn, void*) {
    return SAT_ERR_UNSUPPORTED;
}

namespace saturn::storage::exec {
void* staging() { return g_staging; }
logic::Handoff* handoff() { return &g_handoff; }
sat_cd_block_t* bound_block(const sat_cd_device_t*) { return nullptr; }
void stop_machine() { ++g_stops; }
void start_image(uint32_t bytes) {
    g_started_bytes = bytes;
    throw Started{};
}
void reset_machine() { throw Reset{}; }
}  // namespace saturn::storage::exec

static void test_work_ram_low_ranges() {
    OK(!logic::touches_work_ram_low(0x001FFFFFu, 1u));
    OK(logic::touches_work_ram_low(0x001FFFFFu, 2u));
    OK(logic::touches_work_ram_low(0x00200000u, 0u));
    OK(logic::touches_work_ram_low(0x002FFFFFu, 1u));
    OK(logic::touches_work_ram_low(0x20280000u, 4u));  /* cache-through mirror */
    OK(!logic::touches_work_ram_low(0x00300000u, 4u));
    OK(!logic::touches_work_ram_low(0x06000000u, 0x1000u));
    OK(!logic::touches_work_ram_low(0x26004000u, 4u));
    OK(logic::sectors_for(1u) == 1u && logic::sectors_for(2048u) == 1u && logic::sectors_for(2049u) == 2u);
}

static void test_handoff_seal() {
    logic::Handoff h;
    const uint8_t arg[3] = {1, 2, 3};
    logic::seal(h, arg, 3u);
    OK(logic::valid(h) && h.bytes == 3u && h.data[2] == 3u && h.data[3] == 0u);
    h.data[1] ^= 0x40u;
    OK(!logic::valid(h));               /* a changed byte fails the check */
    logic::seal(h, nullptr, 0u);
    OK(logic::valid(h) && h.bytes == 0u);
    logic::forget(h);
    OK(!logic::valid(h));
    std::memset(&h, 0, sizeof(h));
    OK(!logic::valid(h));               /* zeroed RAM is no hand-off */
}

static void test_rejects_before_stopping() {
    reset_counters();
    const uint8_t arg[SAT_EXEC_ARG_MAX + 1u] = {};
    OK(sat_exec(nullptr, "0.BIN", nullptr, 0u) == SAT_ERR_INVALID_ARG);
    OK(sat_exec(&g_volume, nullptr, nullptr, 0u) == SAT_ERR_INVALID_ARG);
    OK(sat_exec(&g_volume, "0.BIN", nullptr, 1u) == SAT_ERR_INVALID_ARG);
    OK(sat_exec(&g_volume, "0.BIN", arg, SAT_EXEC_ARG_MAX + 1u) == SAT_ERR_CAPACITY);
    sat_result_t rc = SAT_OK;
    OK(run("NOPE.BIN", nullptr, 0u, &rc) == 'E' && rc == SAT_ERR_NOT_FOUND);
    OK(run("DIR", nullptr, 0u, &rc) == 'E' && rc == SAT_ERR_INVALID_ARG);
    OK(run("EMPTY.BIN", nullptr, 0u, &rc) == 'E' && rc == SAT_ERR_INVALID_ARG);
    OK(run("HUGE.BIN", nullptr, 0u, &rc) == 'E' && rc == SAT_ERR_CAPACITY);
    sat_cdfs_volume_t unmounted = {};
    OK(sat_exec(&unmounted, "0.BIN", nullptr, 0u) == SAT_ERR_INVALID_ARG);
    OK(g_stops == 0 && g_reads == 0);
    OK(!logic::valid(g_handoff));       /* nothing sealed either */
}

static void test_starts_small_image() {
    reset_counters();
    const char arg[] = "SA2";
    OK(run("0.BIN", arg, 4u) == 'S');
    OK(g_stops == 1 && g_started_bytes == kSmallBytes && g_reads == 1);
    for (uint32_t i = 0; i < 3u * SAT_CD_SECTOR_BYTES; ++i) {
        OK(g_staging[i] == static_cast<uint8_t>(i * 3u + 1u));  /* whole sectors staged */
    }
    OK(g_staging[3u * SAT_CD_SECTOR_BYTES] == 0xEEu);

    char out[SAT_EXEC_ARG_MAX] = {};
    uint32_t bytes = 99u;
    OK(sat_exec_arg(out, 2u, &bytes) == SAT_ERR_CAPACITY && bytes == 4u);  /* kept */
    OK(sat_exec_arg(nullptr, 0u, &bytes) == SAT_ERR_CAPACITY && bytes == 4u);
    OK(sat_exec_arg(nullptr, 4u, &bytes) == SAT_ERR_INVALID_ARG);
    OK(sat_exec_arg(out, sizeof(out), &bytes) == SAT_OK && bytes == 4u && std::strcmp(out, "SA2") == 0);
    OK(sat_exec_arg(out, sizeof(out), &bytes) == SAT_ERR_NOT_FOUND);  /* read once */
}

static void test_no_argument_still_counts() {
    reset_counters();
    OK(run("0.BIN", nullptr, 0u) == 'S');
    uint32_t bytes = 99u;
    OK(sat_exec_arg(nullptr, 0u, &bytes) == SAT_OK && bytes == 0u);
    OK(sat_exec_arg(nullptr, 0u, &bytes) == SAT_ERR_NOT_FOUND);
}

static void test_large_image_in_chunks() {
    reset_counters();
    OK(run("GAME.BIN", nullptr, 0u) == 'S');
    OK(g_started_bytes == kGameBytes && g_reads == 2);   /* 128 + 72 sectors */
    for (uint32_t s = 0; s < 200u; ++s) {
        const uint8_t* sector = g_staging + s * SAT_CD_SECTOR_BYTES;
        OK(sector[0] == generated_byte(kGeneratedLba + s, 0u));
        OK(sector[2047] == generated_byte(kGeneratedLba + s, 2047u));
    }
    (void)sat_exec_arg(nullptr, 0u, nullptr);
}

static void test_retries_then_resets() {
    reset_counters();
    g_failures_left = 2;                 /* two bad reads, the third attempt lands */
    OK(run("0.BIN", nullptr, 0u) == 'S' && g_reads == 3);
    (void)sat_exec_arg(nullptr, 0u, nullptr);

    reset_counters();
    g_fail_always = true;                /* past the point of no return: reset */
    OK(run("0.BIN", nullptr, 0u) == 'R' && g_stops == 1 && g_reads == 3);
    (void)sat_exec_arg(nullptr, 0u, nullptr);
}

int main() {
    make_image();
    mount();
    test_work_ram_low_ranges();
    test_handoff_seal();
    test_rejects_before_stopping();
    test_starts_small_image();
    test_no_argument_still_counts();
    test_large_image_in_chunks();
    test_retries_then_resets();
    std::puts("test_exec: OK");
    return 0;
}
