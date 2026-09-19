#include "saturn/ram_cart.h"

#include "saturn/memory.h"
#include "src/core/ram_cart_logic.hpp"
#include "src/hal/ram_cart.hpp"

namespace {
sat_arena_t g_banks[2] = {};
sat_ram_cart_type_t g_type = SAT_RAM_CART_NONE;
uint32_t g_generation = 1u;

bool active_buffer(const sat_ram_cart_buffer_t* buffer, uint32_t offset,
                   const void* data, uint32_t bytes) {
    return g_type != SAT_RAM_CART_NONE && buffer != nullptr &&
        buffer->generation == g_generation && buffer->generation != 0u &&
        (data != nullptr || bytes == 0u) && offset <= buffer->size &&
        bytes <= buffer->size - offset &&
        buffer->bank_bytes[0] <= buffer->size &&
        buffer->bank_bytes[1] == buffer->size - buffer->bank_bytes[0] &&
        (buffer->bank_bytes[0] == 0u || buffer->bank[0] != nullptr) &&
        (buffer->bank_bytes[1] == 0u || buffer->bank[1] != nullptr);
}

void invalidate_generation() {
    ++g_generation;
    if (g_generation == 0u) g_generation = 1u;
}
} // namespace

extern "C" sat_result_t sat_ram_cart_init(void) {
    if (g_type != SAT_RAM_CART_NONE) return SAT_OK;
    const uint8_t id = saturn::hal::ram_cart::probe_id();
    const auto geometry = saturn::core::ram_cart_logic::geometry(id);
    if (geometry.banks == 0u) return SAT_ERR_NOT_CONNECTED;

    saturn::hal::ram_cart::configure();
    for (uint8_t i = 0; i < geometry.banks; ++i) {
        SAT_TRY(sat_arena_init(&g_banks[i], saturn::hal::ram_cart::bank_base(i),
                               geometry.bytes_per_bank));
    }
    g_type = (id == 0x5Cu) ? SAT_RAM_CART_4MB : SAT_RAM_CART_1MB;
    invalidate_generation();
    return SAT_OK;
}

extern "C" sat_result_t sat_ram_cart_info(sat_ram_cart_info_t* out_info) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    *out_info = {};
    out_info->type = g_type;
    if (g_type == SAT_RAM_CART_NONE) return SAT_ERR_NOT_INITIALIZED;
    for (const auto& bank : g_banks) {
        out_info->capacity += static_cast<uint32_t>(bank.capacity);
        out_info->free_bytes += static_cast<uint32_t>(bank.capacity - bank.offset);
        ++out_info->bank_count;
    }
    return SAT_OK;
}

extern "C" void* sat_ram_cart_alloc(size_t size, size_t align) {
    if (g_type == SAT_RAM_CART_NONE || size == 0u ||
        !saturn::core::ram_cart_logic::valid_alignment(align)) return nullptr;
    for (auto& bank : g_banks) {
        if (void* ptr = sat_arena_alloc(&bank, size, align)) return ptr;
    }
    return nullptr;
}

extern "C" sat_result_t sat_ram_cart_buffer_alloc(
    uint32_t size, size_t align, sat_ram_cart_buffer_t* out_buffer) {
    if (out_buffer == nullptr || size == 0u ||
        !saturn::core::ram_cart_logic::valid_alignment(align)) return SAT_ERR_INVALID_ARG;
    *out_buffer = {};
    if (g_type == SAT_RAM_CART_NONE) return SAT_ERR_NOT_INITIALIZED;
    const sat_arena_t before[2] = {g_banks[0], g_banks[1]};
    sat_ram_cart_buffer_t result = {};
    uint32_t remaining = size;
    for (uint8_t i = 0; i < 2u && remaining > 0u; ++i) {
        const size_t available = saturn::core::ram_cart_logic::available_after_alignment(
            g_banks[i].memory, g_banks[i].offset, g_banks[i].capacity, align);
        const size_t take = available < remaining ? available : remaining;
        if (take == 0u) continue;
        void* ptr = sat_arena_alloc(&g_banks[i], take, align);
        if (ptr == nullptr) break;
        result.bank[i] = static_cast<uint8_t*>(ptr);
        result.bank_bytes[i] = static_cast<uint32_t>(take);
        remaining -= static_cast<uint32_t>(take);
    }
    if (remaining != 0u) {
        g_banks[0] = before[0];
        g_banks[1] = before[1];
        return SAT_ERR_CAPACITY;
    }
    result.size = size;
    result.generation = g_generation;
    *out_buffer = result;
    return SAT_OK;
}

extern "C" sat_result_t sat_ram_cart_buffer_write_at(
    const sat_ram_cart_buffer_t* buffer, uint32_t offset,
    const void* source, uint32_t bytes) {
    if (!active_buffer(buffer, offset, source, bytes)) return SAT_ERR_INVALID_ARG;
    const auto* input = static_cast<const uint8_t*>(source);
    uint32_t remaining = bytes;
    for (uint8_t i = 0; i < 2u && remaining != 0u; ++i) {
        const uint32_t part = buffer->bank_bytes[i];
        if (offset >= part) { offset -= part; continue; }
        const uint32_t chunk = (part - offset) < remaining ? part - offset : remaining;
        volatile uint8_t* output = buffer->bank[i] + offset;
        for (uint32_t j = 0; j < chunk; ++j) output[j] = input[j];
        input += chunk;
        remaining -= chunk;
        offset = 0u;
    }
    return remaining == 0u ? SAT_OK : SAT_ERR_IO;
}

extern "C" sat_result_t sat_ram_cart_buffer_read_at(
    const sat_ram_cart_buffer_t* buffer, uint32_t offset,
    void* destination, uint32_t bytes) {
    if (!active_buffer(buffer, offset, destination, bytes)) return SAT_ERR_INVALID_ARG;
    auto* output = static_cast<uint8_t*>(destination);
    uint32_t remaining = bytes;
    for (uint8_t i = 0; i < 2u && remaining != 0u; ++i) {
        const uint32_t part = buffer->bank_bytes[i];
        if (offset >= part) { offset -= part; continue; }
        const uint32_t chunk = (part - offset) < remaining ? part - offset : remaining;
        const volatile uint8_t* input = buffer->bank[i] + offset;
        for (uint32_t j = 0; j < chunk; ++j) output[j] = input[j];
        output += chunk;
        remaining -= chunk;
        offset = 0u;
    }
    return remaining == 0u ? SAT_OK : SAT_ERR_IO;
}

extern "C" void sat_ram_cart_reset(void) {
    if (g_type == SAT_RAM_CART_NONE) return;
    for (auto& bank : g_banks) sat_arena_reset(&bank);
    invalidate_generation();
}
