#include "saturn/save_schema.h"

namespace {
constexpr uint16_t kHeaderSize = sizeof(sat_save_schema_header_t);
constexpr uint32_t kFnvOffset = 2166136261u;
constexpr uint32_t kFnvPrime = 16777619u;
}

extern "C" uint32_t sat_save_schema_checksum(const void* payload,
                                               uint32_t payload_size) {
    if (!payload && payload_size) return 0u;
    const auto* bytes = static_cast<const uint8_t*>(payload);
    uint32_t hash = kFnvOffset;
    for (uint32_t i = 0; i < payload_size; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
    return hash;
}

extern "C" sat_result_t sat_save_schema_requirements(
    uint32_t payload_size, uint32_t* out_wire_size) {
    if (!out_wire_size) return SAT_ERR_INVALID_ARG;
    if (payload_size > UINT32_MAX - static_cast<uint32_t>(kHeaderSize))
        return SAT_ERR_CAPACITY;
    *out_wire_size = static_cast<uint32_t>(kHeaderSize) + payload_size;
    return SAT_OK;
}

extern "C" sat_result_t sat_save_schema_encode(
    sat_save_schema_header_t* out, uint32_t magic, uint16_t version,
    const void* payload, uint32_t payload_size) {
    if (!out || (!payload && payload_size) || !magic || !version)
        return SAT_ERR_INVALID_ARG;
    out->magic = magic;
    out->version = version;
    out->header_size = kHeaderSize;
    out->payload_size = payload_size;
    out->checksum = sat_save_schema_checksum(payload, payload_size);
    return SAT_OK;
}

extern "C" sat_result_t sat_save_schema_validate(
    const sat_save_schema_header_t* header, uint32_t expected_magic,
    uint16_t expected_version, const void* payload,
    uint32_t payload_capacity, uint32_t* out_payload_size) {
    if (!header || !expected_magic || !expected_version ||
        (!payload && header->payload_size) || !out_payload_size)
        return SAT_ERR_INVALID_ARG;
    if (header->header_size != kHeaderSize || header->magic != expected_magic)
        return SAT_ERR_VERIFY_FAILED;
    if (header->version != expected_version) return SAT_ERR_VERSION;
    if (header->payload_size > payload_capacity) return SAT_ERR_CAPACITY;
    if (sat_save_schema_checksum(payload, header->payload_size) != header->checksum)
        return SAT_ERR_VERIFY_FAILED;
    *out_payload_size = header->payload_size;
    return SAT_OK;
}
