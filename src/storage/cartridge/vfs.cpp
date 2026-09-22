#include "saturn/ram_cart.h"

extern "C" sat_result_t sat_ram_cart_file_read_at(
    void* context, uint32_t offset, void* destination,
    uint32_t bytes, uint32_t* out_read) {
    if (out_read == nullptr || context == nullptr) return SAT_ERR_INVALID_ARG;
    *out_read = 0u;
    const auto* buffer = static_cast<const sat_ram_cart_buffer_t*>(context);
    if (offset > buffer->size) return SAT_ERR_INVALID_ARG;
    const uint32_t available = buffer->size - offset;
    const uint32_t count = bytes < available ? bytes : available;
    SAT_TRY(sat_ram_cart_buffer_read_at(buffer, offset, destination, count));
    *out_read = count;
    return SAT_OK;
}
