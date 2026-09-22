#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/ram_cart.h"
#include "src/hal/storage/ram_cart.hpp"

namespace {
alignas(64) uint8_t dram[2][2u * 1024u * 1024u] = {};
uint8_t cart_id = 0xFFu;
unsigned configuration_writes = 0u;
}

namespace saturn::hal::ram_cart {
uint8_t probe_id() { return cart_id; }
void configure() { ++configuration_writes; }
uint8_t* bank_base(uint8_t i) { assert(i < 2u); return dram[i]; }
}

int main() {
    sat_ram_cart_info_t info{};
    assert(sat_ram_cart_info(&info) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_ram_cart_init() == SAT_ERR_NOT_CONNECTED);
    assert(configuration_writes == 0u);
    cart_id = 0x5Cu;
    assert(sat_ram_cart_init() == SAT_OK);
    assert(sat_ram_cart_init() == SAT_OK);
    assert(configuration_writes == 1u);
    assert(sat_ram_cart_info(&info) == SAT_OK);
    assert(info.type == SAT_RAM_CART_4MB && info.capacity == 4u * 1024u * 1024u &&
           info.bank_count == 2u && info.free_bytes == info.capacity);
    assert(sat_ram_cart_alloc(8u, 0u) == nullptr);
    assert(sat_ram_cart_alloc(3u * 1024u * 1024u, 4u) == nullptr);
    auto* first = static_cast<uint8_t*>(sat_ram_cart_alloc(64u, 32u));
    assert(first == dram[0]);
    sat_ram_cart_buffer_t big{};
    constexpr uint32_t kSize = 3u * 1024u * 1024u;
    assert(sat_ram_cart_buffer_alloc(kSize, 32u, &big) == SAT_OK);
    assert(big.bank[0] == dram[0] + 64u && big.bank_bytes[0] == 2u * 1024u * 1024u - 64u);
    assert(big.bank[1] == dram[1] && big.bank_bytes[1] == kSize - big.bank_bytes[0]);
    uint8_t data[8] = {9,8,7,6,5,4,3,2};
    uint8_t result[8] = {};
    assert(sat_ram_cart_buffer_write_at(&big, big.bank_bytes[0] - 4u, data, 8u) == SAT_OK);
    assert(sat_ram_cart_buffer_read_at(&big, big.bank_bytes[0] - 4u, result, 8u) == SAT_OK);
    for (unsigned i = 0; i < 8; ++i) assert(data[i] == result[i]);
    assert(sat_ram_cart_buffer_write_at(&big, kSize - 2u, data, 8u) == SAT_ERR_INVALID_ARG);
    assert(sat_ram_cart_buffer_read_at(&big, 1u, nullptr, 8u) == SAT_ERR_INVALID_ARG);
    const uint32_t free_before = [&] { assert(sat_ram_cart_info(&info) == SAT_OK); return info.free_bytes; }();
    sat_ram_cart_buffer_t impossible{};
    assert(sat_ram_cart_buffer_alloc(2u * 1024u * 1024u, 32u, &impossible) == SAT_ERR_CAPACITY);
    assert(sat_ram_cart_info(&info) == SAT_OK && info.free_bytes == free_before);
    sat_ram_cart_reset();
    assert(sat_ram_cart_buffer_read_at(&big, 0u, result, sizeof(result)) == SAT_ERR_INVALID_ARG);
    assert(sat_ram_cart_info(&info) == SAT_OK && info.free_bytes == info.capacity);
    assert(sat_ram_cart_buffer_alloc(4u * 1024u * 1024u, 32u, &big) == SAT_OK);
    assert(big.bank_bytes[0] == 2u * 1024u * 1024u &&
           big.bank_bytes[1] == 2u * 1024u * 1024u);
    assert(sat_ram_cart_buffer_write_at(&big, big.size, nullptr, 0u) == SAT_OK);
    std::puts("RAM cart 4MB API: OK");
    return 0;
}
