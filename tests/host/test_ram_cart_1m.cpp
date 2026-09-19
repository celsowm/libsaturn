#include <cassert>
#include <cstdint>
#include <cstdio>
#include "saturn/ram_cart.h"
#include "src/core/ram_cart_logic.hpp"
#include "src/hal/ram_cart.hpp"

namespace {
alignas(64) uint8_t dram[2][512u * 1024u] = {};
unsigned initialized = 0u;
}
namespace saturn::hal::ram_cart {
uint8_t probe_id() { return 0x5Au; }
void configure() { ++initialized; }
uint8_t* bank_base(uint8_t i) { assert(i < 2); return dram[i]; }
}
int main() {
    using namespace saturn::core::ram_cart_logic;
    static_assert(geometry(0x5Au).bytes_per_bank == 512u * 1024u);
    static_assert(geometry(0x5Cu).bytes_per_bank == 2u * 1024u * 1024u);
    static_assert(geometry(0xFFu).banks == 0u);
    assert(sat_ram_cart_init() == SAT_OK && initialized == 1u);
    sat_ram_cart_info_t info{};
    assert(sat_ram_cart_info(&info) == SAT_OK &&
           info.type == SAT_RAM_CART_1MB && info.capacity == 1024u * 1024u);
    assert(sat_ram_cart_alloc(513u * 1024u, 4u) == nullptr);
    assert(sat_ram_cart_alloc(512u * 1024u, 4u) == dram[0]);
    assert(sat_ram_cart_alloc(512u * 1024u, 4u) == dram[1]);
    assert(sat_ram_cart_alloc(1u, 4u) == nullptr);
    sat_ram_cart_reset();
    sat_ram_cart_buffer_t whole{};
    assert(sat_ram_cart_buffer_alloc(1024u * 1024u, 4u, &whole) == SAT_OK);
    assert(whole.bank_bytes[0] == 512u * 1024u &&
           whole.bank_bytes[1] == 512u * 1024u);
    std::puts("RAM cart 1MB API: OK");
}
