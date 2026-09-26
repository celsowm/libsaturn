#include "src/hal/scsp/driver.hpp"

#include "src/hal/scsp/scsp.hpp"
#include "src/hal/scsp/sound_driver_image.h"
#include "src/hal/scu/scu.hpp"
#include "src/hal/smpc/smpc.hpp"

namespace saturn::hal::scsp::driver {

namespace {

namespace logic = saturn::hal::scsp::driver_logic;

bool g_running = false;
/* How long the sound CPU gets to come up: this many display frames. */
constexpr uint32_t kStartupFrames = 30u;

uint16_t word(uint32_t offset) {
    return scsp::sound_word(offset);
}

void set_word(uint32_t offset, uint16_t value) {
    scsp::set_sound_word(offset, value);
}

/* The tick is written high word then low word by the 68000 (the low word
 * carries into the high one), so read high, low, high and retry on a change. */
uint32_t read_tick() {
    for (;;) {
        const uint16_t high = word(logic::kTickOffset);
        const uint16_t low = word(logic::kTickOffset + 2u);
        if (word(logic::kTickOffset) == high) return logic::combine_tick(high, low);
    }
}

bool wait_running() {
    const uint32_t per_frame = saturn::hal::scu::ticks_per_frame();
    const uint64_t begin = saturn::hal::scu::elapsed_ticks();
    const uint64_t limit = static_cast<uint64_t>(per_frame != 0u ? per_frame : 1000u) * kStartupFrames;
    for (;;) {
        if ((word(logic::kFlagsOffset) & 1u) != 0u && read_tick() >= 2u) return true;
        if (saturn::hal::scu::elapsed_ticks() - begin > limit) return false;
    }
}

}  // namespace

bool start() {
    if (!scsp::is_ready()) return false;
    if (g_running) return true;
    if (!saturn::hal::smpc::sound_off()) return false;
    /* image first, then a clean mailbox around it */
    for (uint32_t i = 0u; i < kSoundDriverImageWords; ++i) set_word(i * 2u, kSoundDriverImage[i]);
    for (uint32_t offset = logic::kMailbox; offset < logic::kMailbox + logic::kMailboxBytes; offset += 2u) {
        set_word(offset, 0u);
    }
    if (!saturn::hal::smpc::sound_on()) return false;
    g_running = wait_running();
    if (!g_running) (void)scsp::restore_idle_68k();
    return g_running;
}

void stop() {
    if (!g_running) return;
    g_running = false;
    (void)scsp::restore_idle_68k();
}

bool running() {
    return g_running;
}

bool read_info(Info* out) {
    if (out == nullptr) return false;
    *out = {};
    if (!g_running) return true;
    out->running = (word(logic::kFlagsOffset) & 1u) != 0u;
    out->version = word(logic::kVersionOffset);
    out->heartbeat = word(logic::kHeartbeatOffset);
    out->tick = read_tick();
    out->queued = static_cast<uint16_t>(
        logic::ring_count(word(logic::kHeadOffset), word(logic::kTailOffset)));
    out->executed = word(logic::kExecutedOffset);
    out->max_lateness = word(logic::kMaxLateOffset);
    out->last_lateness = word(logic::kLastLateOffset);
    return true;
}

uint32_t tick() {
    return g_running ? read_tick() : 0u;
}

static bool push(uint32_t due_tick, uint16_t op_and_register, uint16_t value) {
    if (!g_running) return false;
    const uint16_t head = word(logic::kHeadOffset);
    const uint16_t tail = word(logic::kTailOffset);
    if (logic::ring_full(head, tail)) return false;
    const uint32_t entry = logic::ring_entry_offset(head);
    set_word(entry + 0u, static_cast<uint16_t>(due_tick >> 16u));
    set_word(entry + 2u, static_cast<uint16_t>(due_tick & 0xFFFFu));
    set_word(entry + 4u, op_and_register);
    set_word(entry + 6u, value);
    /* publish last: the 68000 only looks at slots below the head */
    set_word(logic::kHeadOffset, static_cast<uint16_t>((head + 1u) & logic::kRingMask));
    return true;
}

bool push_write(uint32_t due_tick, uint32_t register_offset, uint16_t value) {
    if (!logic::register_ok(register_offset)) return false;
    return push(due_tick, static_cast<uint16_t>(register_offset), value);
}

bool push_marker(uint32_t due_tick) {
    return push(due_tick, logic::kMarkerBit, 0u);
}

bool read_log(uint32_t index, uint16_t* out_tick, uint16_t* out_lateness) {
    if (!g_running || out_tick == nullptr || out_lateness == nullptr) return false;
    const uint32_t offset = logic::log_entry_offset(index);
    *out_tick = word(offset);
    *out_lateness = word(offset + 2u);
    return true;
}

void clear_counters() {
    if (g_running) set_word(logic::kControlOffset, 1u);
}

}  // namespace saturn::hal::scsp::driver
