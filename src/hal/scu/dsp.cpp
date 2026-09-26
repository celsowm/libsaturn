#include "src/hal/scu/dsp.hpp"

#include "src/hal/sh2/cache.hpp"
#include "src/hal/sh2/frt.hpp"

namespace saturn::hal::scu::dsp {

namespace {

namespace logic = saturn::hal::scu::dsp_logic;

inline volatile uint32_t& reg(uint32_t address) {
    return *reinterpret_cast<volatile uint32_t*>(address);
}

}  // namespace

logic::Status status() {
    return logic::decode_status(reg(logic::kRegPpaf));
}

void load_program(const uint32_t* words, uint32_t count, uint32_t at) {
    /* LEF with EX clear sets the program counter; the port then writes
     * program RAM at the counter and steps it. */
    reg(logic::kRegPpaf) = logic::load_pc_word(at);
    for (uint32_t i = 0u; i < count; ++i) reg(logic::kRegPpd) = words[i];
}

void write_data(uint32_t bank, uint32_t offset, const uint32_t* words, uint32_t count) {
    reg(logic::kRegPda) = logic::data_address_word(bank, offset);
    for (uint32_t i = 0u; i < count; ++i) reg(logic::kRegPdd) = words[i];
}

void read_data(uint32_t bank, uint32_t offset, uint32_t* words, uint32_t count) {
    reg(logic::kRegPda) = logic::data_address_word(bank, offset);
    for (uint32_t i = 0u; i < count; ++i) words[i] = reg(logic::kRegPdd);
}

void start(uint32_t entry) {
    reg(logic::kRegPpaf) = logic::start_word(entry);
}

void stop() {
    reg(logic::kRegPpaf) = 0u;
}

bool dma_range_ok(const void* p, uint32_t bytes) {
    const uintptr_t raw = reinterpret_cast<uintptr_t>(p);
    if (p == nullptr || bytes == 0u || (raw & 3u) != 0u || (bytes & 3u) != 0u) return false;
    /* any cache alias of Work RAM-H: fold to the physical address */
    const uint32_t physical = static_cast<uint32_t>(raw) & 0x07FFFFFFu;
    return saturn::hal::sh2::cache::is_supported_work_ram(0x06000000u | (physical & 0x000FFFFFu), 1u) &&
           (physical & 0x06000000u) == 0x06000000u && (physical & 0x000FFFFFu) + bytes <= 0x00100000u;
}

uint32_t dma_word_address(const void* p) {
    return (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)) & 0x07FFFFFCu) >> 2u;
}

void invalidate(const void* p, uint32_t bytes) {
    const uint32_t cached = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)) & 0x07FFFFFFu;
    (void)saturn::hal::sh2::cache::invalidate_range(cached, bytes);
}

bool wait_finished(uint32_t timeout_ticks) {
    const uint16_t begin = saturn::hal::sh2::frt::counter();
    for (;;) {
        if (logic::finished(status())) return true;
        if (static_cast<uint16_t>(saturn::hal::sh2::frt::counter() - begin) >= timeout_ticks) {
            return logic::finished(status());
        }
    }
}

}  // namespace saturn::hal::scu::dsp
