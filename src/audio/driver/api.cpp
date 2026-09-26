#include "saturn/sound_driver.h"

#include "src/hal/scsp/driver.hpp"
#include "src/hal/scsp/scsp.hpp"

namespace {

namespace hal = saturn::hal::scsp::driver;
namespace logic = saturn::hal::scsp::driver_logic;

sat_result_t push_result(bool pushed) {
    if (pushed) return SAT_OK;
    return hal::running() ? SAT_ERR_CAPACITY : SAT_ERR_NOT_INITIALIZED;
}

}  // namespace

extern "C" sat_result_t sat_sound_driver_start(void) {
    if (!saturn::hal::scsp::is_ready()) return SAT_ERR_NOT_INITIALIZED;
    return hal::start() ? SAT_OK : SAT_ERR_TIMEOUT;
}

extern "C" sat_result_t sat_sound_driver_stop(void) {
    hal::stop();
    return SAT_OK;
}

extern "C" uint8_t sat_sound_driver_running(void) {
    return hal::running() ? 1u : 0u;
}

extern "C" sat_result_t sat_sound_driver_info(sat_sound_driver_info_t* out_info) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    hal::Info info;
    hal::read_info(&info);
    *out_info = {};
    out_info->running = info.running ? 1u : 0u;
    out_info->version = info.version;
    out_info->heartbeat = info.heartbeat;
    out_info->queued = info.queued;
    out_info->executed = info.executed;
    out_info->max_lateness_ticks = info.max_lateness;
    out_info->last_lateness_ticks = info.last_lateness;
    out_info->tick = info.tick;
    return SAT_OK;
}

extern "C" uint32_t sat_sound_driver_tick(void) {
    return hal::tick();
}

extern "C" uint32_t sat_sound_driver_ticks_from_ms(uint32_t milliseconds) {
    return logic::ticks_from_ms(milliseconds);
}

extern "C" uint32_t sat_sound_driver_ms_from_ticks(uint32_t ticks) {
    return logic::ms_from_ticks(ticks);
}

extern "C" sat_result_t sat_sound_driver_schedule_write(uint32_t due_tick, uint16_t register_offset,
                                                        uint16_t value) {
    if (!logic::register_ok(register_offset)) return SAT_ERR_INVALID_ARG;
    return push_result(hal::push_write(due_tick, register_offset, value));
}

extern "C" sat_result_t sat_sound_driver_schedule_key(uint32_t due_tick, uint8_t slot, uint8_t on) {
    if (slot >= saturn::hal::scsp::kSlotCount) return SAT_ERR_INVALID_ARG;
    if (!hal::running()) return SAT_ERR_NOT_INITIALIZED;
    const uint16_t word = saturn::hal::scsp::timed_key_word(slot, on != 0u);
    return push_result(hal::push_write(due_tick, static_cast<uint32_t>(slot) * 0x20u, word));
}

extern "C" sat_result_t sat_sound_driver_schedule_marker(uint32_t due_tick) {
    return push_result(hal::push_marker(due_tick));
}

extern "C" sat_result_t sat_sound_driver_read_log(uint32_t index, uint16_t* out_tick, uint16_t* out_lateness) {
    if (out_tick == nullptr || out_lateness == nullptr) return SAT_ERR_INVALID_ARG;
    if (!hal::read_log(index, out_tick, out_lateness)) return SAT_ERR_NOT_INITIALIZED;
    return SAT_OK;
}

extern "C" sat_result_t sat_sound_driver_clear_counters(void) {
    if (!hal::running()) return SAT_ERR_NOT_INITIALIZED;
    hal::clear_counters();
    return SAT_OK;
}
