#include "src/hal/storage/backup.hpp"

#include <stddef.h>
#include <stdint.h>

#include "src/hal/smpc/smpc.hpp"

namespace saturn::hal::bup {

namespace {

constexpr uintptr_t kBupVectorPointer = 0x06000354u;
constexpr uintptr_t kBupInitPointer = 0x06000358u;
constexpr uint32_t kInternalConfigIndex = 0u;
constexpr uint16_t kInternalUnitId = 1u;

alignas(4) volatile uint32_t g_library[4096] = {};
alignas(4) uint32_t g_work[2048] = {};
Config g_configs[3] = {};
bool g_initialized = false;

Result map_result(int32_t value) {
    switch (value) {
        case 0: return Result::Ok;
        case 1: return Result::NotConnected;
        case 2: return Result::Unformatted;
        case 3: return Result::WriteProtected;
        case 4: return Result::NoSpace;
        case 5: return Result::NotFound;
        case 6: return Result::Found;
        case 7: return Result::NoMatch;
        case 8: return Result::Broken;
        default: return Result::TransportError;
    }
}

uint32_t read_long(uintptr_t address) {
    return *reinterpret_cast<volatile uint32_t*>(address);
}

template <typename Fn>
Fn init_entry() {
    const uint32_t address = read_long(kBupInitPointer);
    return address == 0u ? nullptr : reinterpret_cast<Fn>(static_cast<uintptr_t>(address));
}

template <typename Fn>
Fn vector_entry(uint32_t offset) {
    const uint32_t base = read_long(kBupVectorPointer);
    if (base == 0u) return nullptr;
    const uint32_t address =
        *reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(base + offset));
    return address == 0u ? nullptr : reinterpret_cast<Fn>(static_cast<uintptr_t>(address));
}

class ResetGuard {
public:
    ResetGuard() : active_(saturn::hal::smpc::reset_disable()) {}

    bool active() const { return active_; }

    bool release() {
        if (!active_) return true;
        if (!saturn::hal::smpc::reset_enable()) return false;
        active_ = false;
        return true;
    }

    ~ResetGuard() {
        if (active_) {
            (void)saturn::hal::smpc::reset_enable();
        }
    }

private:
    bool active_;
};

bool ready() {
    return g_initialized;
}

}  // namespace

Result init(Config out_configs[3]) {
    if (out_configs == nullptr) return Result::TransportError;

    if (!g_initialized) {
        using Fn = void (*)(volatile uint32_t*, uint32_t*, Config*);
        Fn fn = init_entry<Fn>();
        if (fn == nullptr) return Result::TransportError;

        ResetGuard guard;
        if (!guard.active()) return Result::TransportError;
        fn(g_library, g_work, g_configs);
        if (!guard.release()) return Result::TransportError;

        if (g_configs[kInternalConfigIndex].unit_id != kInternalUnitId) {
            return Result::NotConnected;
        }
        g_initialized = true;
    }

    for (uint32_t i = 0u; i < 3u; ++i) out_configs[i] = g_configs[i];
    return Result::Ok;
}

Result select_partition(uint32_t device, uint16_t partition) {
    if (!ready()) return Result::TransportError;
    using Fn = int32_t (*)(uint32_t, uint16_t);
    Fn fn = vector_entry<Fn>(4u);
    return fn == nullptr ? Result::TransportError : map_result(fn(device, partition));
}

Result format(uint32_t device) {
    if (!ready()) return Result::TransportError;
    using Fn = int32_t (*)(uint32_t);
    Fn fn = vector_entry<Fn>(8u);
    if (fn == nullptr) return Result::TransportError;

    ResetGuard guard;
    if (!guard.active()) return Result::TransportError;
    const Result result = map_result(fn(device));
    if (!guard.release() && result == Result::Ok) return Result::TransportError;
    return result;
}

Result stat(uint32_t device, uint32_t prospective_data_size, Stat* out_stat) {
    if (!ready() || out_stat == nullptr) return Result::TransportError;
    using Fn = int32_t (*)(uint32_t, uint32_t, Stat*);
    Fn fn = vector_entry<Fn>(12u);
    return fn == nullptr
        ? Result::TransportError
        : map_result(fn(device, prospective_data_size, out_stat));
}

int32_t directory(
    uint32_t device,
    const char* pattern,
    uint16_t capacity,
    Dir* out_entries
) {
    if (!ready() || pattern == nullptr || (capacity != 0u && out_entries == nullptr)) {
        return 0;
    }
    using Fn = int32_t (*)(uint32_t, uint8_t*, uint16_t, Dir*);
    Fn fn = vector_entry<Fn>(28u);
    if (fn == nullptr) return 0;
    return fn(
        device,
        reinterpret_cast<uint8_t*>(const_cast<char*>(pattern)),
        capacity,
        out_entries);
}

Result write(
    uint32_t device,
    Dir* entry,
    const void* data,
    uint8_t overwrite
) {
    if (!ready() || entry == nullptr || data == nullptr) return Result::TransportError;
    using Fn = int32_t (*)(uint32_t, Dir*, const volatile uint8_t*, uint8_t);
    Fn fn = vector_entry<Fn>(16u);
    if (fn == nullptr) return Result::TransportError;

    ResetGuard guard;
    if (!guard.active()) return Result::TransportError;
    /* Sega's flag is inverted: zero overwrites; non-zero refuses. */
    const uint8_t no_overwrite = overwrite != 0u ? 0u : 1u;
    const Result result = map_result(
        fn(device, entry, static_cast<const volatile uint8_t*>(data), no_overwrite));
    if (!guard.release() && result == Result::Ok) return Result::TransportError;
    return result;
}

Result read(uint32_t device, const char* name, void* data) {
    if (!ready() || name == nullptr || data == nullptr) return Result::TransportError;
    using Fn = int32_t (*)(uint32_t, uint8_t*, volatile uint8_t*);
    Fn fn = vector_entry<Fn>(20u);
    if (fn == nullptr) return Result::TransportError;
    return map_result(fn(
        device,
        reinterpret_cast<uint8_t*>(const_cast<char*>(name)),
        static_cast<volatile uint8_t*>(data)));
}

Result remove(uint32_t device, const char* name) {
    if (!ready() || name == nullptr) return Result::TransportError;
    using Fn = int32_t (*)(uint32_t, uint8_t*);
    Fn fn = vector_entry<Fn>(24u);
    if (fn == nullptr) return Result::TransportError;

    ResetGuard guard;
    if (!guard.active()) return Result::TransportError;
    const Result result = map_result(
        fn(device, reinterpret_cast<uint8_t*>(const_cast<char*>(name))));
    if (!guard.release() && result == Result::Ok) return Result::TransportError;
    return result;
}

Result verify(uint32_t device, const char* name, const void* data) {
    if (!ready() || name == nullptr || data == nullptr) return Result::TransportError;
    using Fn = int32_t (*)(uint32_t, uint8_t*, const volatile uint8_t*);
    Fn fn = vector_entry<Fn>(32u);
    if (fn == nullptr) return Result::TransportError;
    return map_result(fn(
        device,
        reinterpret_cast<uint8_t*>(const_cast<char*>(name)),
        static_cast<const volatile uint8_t*>(data)));
}

}  // namespace saturn::hal::bup
