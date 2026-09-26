#include "saturn/smpc.h"

#include "src/core/runtime/state.hpp"
#include "src/hal/smpc/smpc.hpp"

namespace {

namespace smpc = saturn::hal::smpc;

sat_rtc_time_t to_public(const smpc::RtcTime& t) {
    sat_rtc_time_t out{};
    out.year = t.year;
    out.month = t.month;
    out.day = t.day;
    out.weekday = t.weekday;
    out.hour = t.hour;
    out.minute = t.minute;
    out.second = t.second;
    return out;
}

}  // namespace

extern "C" sat_result_t sat_smpc_status(sat_smpc_status_t* out_status) {
    SAT_TRY(saturn::core::require_initialized());
    if (out_status == nullptr) return SAT_ERR_INVALID_ARG;
    smpc::SmpcStatus st{};
    if (!smpc::read_status(&st)) return SAT_ERR_IO;
    *out_status = {};
    out_status->rtc_set = st.time_set ? 1u : 0u;
    out_status->reset_disabled = st.reset_disabled ? 1u : 0u;
    out_status->cartridge_code = st.cartridge_code;
    out_status->area_code = st.area_code;
    out_status->system_status1 = st.system_status1;
    out_status->system_status2 = st.system_status2;
    for (uint8_t i = 0u; i < 4u; ++i) out_status->smem[i] = st.smem[i];
    out_status->time = to_public(st.time);
    return SAT_OK;
}

extern "C" sat_result_t sat_rtc_get(sat_rtc_time_t* out_time) {
    if (out_time == nullptr) return SAT_ERR_INVALID_ARG;
    sat_smpc_status_t status{};
    SAT_TRY(sat_smpc_status(&status));
    *out_time = status.time;
    return SAT_OK;
}

extern "C" sat_result_t sat_rtc_set(const sat_rtc_time_t* time) {
    SAT_TRY(saturn::core::require_initialized());
    if (time == nullptr) return SAT_ERR_INVALID_ARG;
    smpc::RtcTime t{};
    t.year = time->year;
    t.month = time->month;
    t.day = time->day;
    t.hour = time->hour;
    t.minute = time->minute;
    t.second = time->second;
    if (!smpc::valid_time(t)) return SAT_ERR_INVALID_ARG;
    return smpc::set_time(t) ? SAT_OK : SAT_ERR_IO;
}

extern "C" sat_result_t sat_smem_set(const uint8_t smem[4]) {
    SAT_TRY(saturn::core::require_initialized());
    if (smem == nullptr) return SAT_ERR_INVALID_ARG;
    return smpc::set_smem(smem) ? SAT_OK : SAT_ERR_IO;
}

extern "C" sat_result_t sat_smem_get(uint8_t out_smem[4]) {
    if (out_smem == nullptr) return SAT_ERR_INVALID_ARG;
    sat_smpc_status_t status{};
    SAT_TRY(sat_smpc_status(&status));
    for (uint8_t i = 0u; i < 4u; ++i) out_smem[i] = status.smem[i];
    return SAT_OK;
}

extern "C" sat_result_t sat_smpc_reset_enable(int enabled) {
    SAT_TRY(saturn::core::require_initialized());
    const bool ok = enabled != 0 ? smpc::reset_enable() : smpc::reset_disable();
    return ok ? SAT_OK : SAT_ERR_IO;
}
