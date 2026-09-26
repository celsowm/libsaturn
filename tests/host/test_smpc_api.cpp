#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/smpc.h"
#include "src/core/runtime/state.hpp"
#include "src/hal/smpc/smpc.hpp"

namespace {
using namespace saturn::hal::smpc;

SmpcStatus g_status{};
bool g_read_ok = true;
bool g_write_ok = true;
RtcTime g_set_time{};
uint32_t g_set_time_calls = 0u;
uint8_t g_smem[4] = {};
bool g_reset_enabled = false;
uint32_t g_reset_calls = 0u;
}

namespace saturn::hal::smpc {
bool read_status(SmpcStatus* out) {
    if (out == nullptr || !g_read_ok) return false;
    *out = g_status;
    return true;
}
bool set_time(const RtcTime& t) {
    ++g_set_time_calls;
    g_set_time = t;
    return g_write_ok;
}
bool set_smem(const uint8_t smem[4]) {
    if (!g_write_ok) return false;
    for (int i = 0; i < 4; ++i) g_smem[i] = smem[i];
    return true;
}
bool reset_enable() { ++g_reset_calls; g_reset_enabled = true; return g_write_ok; }
bool reset_disable() { ++g_reset_calls; g_reset_enabled = false; return g_write_ok; }
}

int main() {
    using saturn::core::g_state;

    sat_smpc_status_t status{};
    sat_rtc_time_t t{};
    uint8_t smem[4] = {1, 2, 3, 4};
    g_state = {};
    assert(sat_smpc_status(&status) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_rtc_set(&t) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_smem_set(smem) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_smpc_reset_enable(1) == SAT_ERR_NOT_INITIALIZED);
    g_state.initialized = true;

    /* Status and clock come straight from the parsed INTBACK block. */
    g_status.time_set = true;
    g_status.reset_disabled = true;
    g_status.cartridge_code = 2u;
    g_status.area_code = 4u;
    g_status.system_status1 = 0x34u;
    g_status.smem[0] = 9u;
    g_status.smem[3] = 8u;
    g_status.time = {2026u, 9u, 25u, 5u, 21u, 5u, 9u};
    assert(sat_smpc_status(&status) == SAT_OK);
    assert(status.rtc_set == 1u && status.reset_disabled == 1u);
    assert(status.cartridge_code == 2u && status.area_code == 4u && status.system_status1 == 0x34u);
    assert(status.smem[0] == 9u && status.smem[3] == 8u);
    assert(status.time.year == 2026u && status.time.weekday == 5u && status.time.second == 9u);
    assert(sat_rtc_get(&t) == SAT_OK);
    assert(t.year == 2026u && t.month == 9u && t.day == 25u && t.hour == 21u && t.minute == 5u);
    uint8_t got[4] = {};
    assert(sat_smem_get(got) == SAT_OK && got[0] == 9u && got[3] == 8u);

    g_read_ok = false;
    assert(sat_smpc_status(&status) == SAT_ERR_IO);
    assert(sat_rtc_get(&t) == SAT_ERR_IO);
    assert(sat_smem_get(got) == SAT_ERR_IO);
    g_read_ok = true;
    assert(sat_smpc_status(nullptr) == SAT_ERR_INVALID_ARG);
    assert(sat_rtc_get(nullptr) == SAT_ERR_INVALID_ARG);
    assert(sat_smem_get(nullptr) == SAT_ERR_INVALID_ARG);

    /* sat_rtc_set refuses impossible dates before the SMPC is touched. */
    t = {2024u, 2u, 29u, 0u, 23u, 59u, 59u};
    assert(sat_rtc_set(&t) == SAT_OK);
    assert(g_set_time_calls == 1u && g_set_time.year == 2024u && g_set_time.day == 29u);
    const sat_rtc_time_t bad[] = {
        {2023u, 2u, 29u, 0u, 0u, 0u, 0u},   /* not a leap year */
        {2024u, 4u, 31u, 0u, 0u, 0u, 0u},
        {2024u, 13u, 1u, 0u, 0u, 0u, 0u},
        {2100u, 1u, 1u, 0u, 0u, 0u, 0u},
        {1979u, 1u, 1u, 0u, 0u, 0u, 0u},
        {2024u, 1u, 1u, 0u, 24u, 0u, 0u},
        {2024u, 1u, 1u, 0u, 0u, 60u, 0u},
        {2024u, 1u, 1u, 0u, 0u, 0u, 60u},
    };
    for (const sat_rtc_time_t& b : bad) assert(sat_rtc_set(&b) == SAT_ERR_INVALID_ARG);
    assert(g_set_time_calls == 1u);
    assert(sat_rtc_set(nullptr) == SAT_ERR_INVALID_ARG);
    g_write_ok = false;
    assert(sat_rtc_set(&t) == SAT_ERR_IO);
    g_write_ok = true;

    assert(sat_smem_set(smem) == SAT_OK && g_smem[0] == 1u && g_smem[3] == 4u);
    assert(sat_smem_set(nullptr) == SAT_ERR_INVALID_ARG);
    g_write_ok = false;
    assert(sat_smem_set(smem) == SAT_ERR_IO);
    assert(sat_smpc_reset_enable(0) == SAT_ERR_IO);
    g_write_ok = true;
    assert(sat_smpc_reset_enable(1) == SAT_OK && g_reset_enabled);
    assert(sat_smpc_reset_enable(0) == SAT_OK && !g_reset_enabled);

    std::printf("PASS: test_smpc_api.cpp\n");
    return 0;
}
