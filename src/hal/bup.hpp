#ifndef SATURN_HAL_BUP_HPP
#define SATURN_HAL_BUP_HPP

#include <stdint.h>

namespace saturn::hal::bup {

enum class Result : int32_t {
    Ok = 0,
    NotConnected = 1,
    Unformatted = 2,
    WriteProtected = 3,
    NoSpace = 4,
    NotFound = 5,
    Found = 6,
    NoMatch = 7,
    Broken = 8,
    TransportError = -1
};

struct Config {
    uint16_t unit_id;
    uint16_t partitions;
};

struct Stat {
    uint32_t total_size;
    uint32_t total_blocks;
    uint32_t block_size;
    uint32_t free_size;
    uint32_t free_blocks;
    uint32_t fit_count;
};

struct Dir {
    uint8_t filename[12];
    uint8_t comment[11];
    uint8_t language;
    uint32_t date;
    uint32_t data_size;
    uint16_t block_size;
};

static_assert(sizeof(Config) == 4u);
static_assert(sizeof(Stat) == 24u);
static_assert(sizeof(Dir) == 36u);

Result init(Config out_configs[3]);
Result select_partition(uint32_t device, uint16_t partition);
Result format(uint32_t device);
Result stat(uint32_t device, uint32_t prospective_data_size, Stat* out_stat);

/* BUP_Dir returns a match count, not a BUP error:
 *   0  -> no matches
 *   N  -> N matches, all fit in capacity
 *  -N  -> N total matches, first capacity entries were written */
int32_t directory(
    uint32_t device,
    const char* pattern,
    uint16_t capacity,
    Dir* out_entries);

Result write(
    uint32_t device,
    Dir* entry,
    const void* data,
    uint8_t overwrite);
Result read(uint32_t device, const char* name, void* data);
Result remove(uint32_t device, const char* name);
Result verify(uint32_t device, const char* name, const void* data);

}  // namespace saturn::hal::bup

#endif /* SATURN_HAL_BUP_HPP */
