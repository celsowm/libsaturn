#include "saturn/abus.h"

#include "saturn/ram_cart.h"
#include "src/hal/storage/abus.hpp"
#include "src/storage/cartridge/abus_logic.hpp"

namespace {

namespace logic = saturn::core::abus_logic;

sat_abus_kind_t to_kind(logic::Kind kind) {
    switch (kind) {
        case logic::Kind::None: return SAT_ABUS_NONE;
        case logic::Kind::Dram1Mb: return SAT_ABUS_DRAM_1MB;
        case logic::Kind::Dram4Mb: return SAT_ABUS_DRAM_4MB;
        case logic::Kind::BackupMemory: return SAT_ABUS_BACKUP_MEMORY;
        case logic::Kind::Unknown: return SAT_ABUS_UNKNOWN;
    }
    return SAT_ABUS_UNKNOWN;
}

logic::Kind current_kind() {
    return logic::classify(saturn::hal::abus::read_id());
}

}  // namespace

extern "C" sat_result_t sat_abus_detect(sat_abus_info_t* out_info) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    const uint8_t id = saturn::hal::abus::read_id();
    const logic::Kind kind = logic::classify(id);
    *out_info = {};
    out_info->kind = to_kind(kind);
    out_info->id = id;
    out_info->backup_bytes = kind == logic::Kind::BackupMemory ? logic::backup_capacity(id) : 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_abus_read(sat_abus_area_t area, uint32_t offset, void* destination,
                                      uint32_t bytes) {
    if (destination == nullptr && bytes != 0u) return SAT_ERR_INVALID_ARG;
    if (!logic::range_ok(static_cast<uint32_t>(area), offset, bytes)) return SAT_ERR_INVALID_ARG;
    const logic::Kind kind = current_kind();
    if (kind == logic::Kind::None) return SAT_ERR_NOT_CONNECTED;
    if (!logic::read_ok(kind)) return SAT_ERR_UNSUPPORTED;
    saturn::hal::abus::read(static_cast<uint32_t>(area), offset, destination, bytes);
    return SAT_OK;
}

extern "C" sat_result_t sat_abus_write(sat_abus_area_t area, uint32_t offset, const void* source,
                                       uint32_t bytes) {
    if (source == nullptr && bytes != 0u) return SAT_ERR_INVALID_ARG;
    if (!logic::range_ok(static_cast<uint32_t>(area), offset, bytes)) return SAT_ERR_INVALID_ARG;
    const logic::Kind kind = current_kind();
    if (kind == logic::Kind::None) return SAT_ERR_NOT_CONNECTED;
    if (!logic::write_ok(kind, static_cast<uint32_t>(area), offset, bytes)) {
        return bytes == 0u ? SAT_OK : SAT_ERR_UNSUPPORTED;
    }
    /* The A-Bus must be set up for the DRAM before it is written: the RAM cart
     * module does that once, after checking the ID. */
    SAT_TRY(sat_ram_cart_init());
    saturn::hal::abus::write(static_cast<uint32_t>(area), offset, source, bytes);
    return SAT_OK;
}
