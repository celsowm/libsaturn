#ifndef SATURN_HAL_DUAL_SH2_PROTOCOL_HPP
#define SATURN_HAL_DUAL_SH2_PROTOCOL_HPP

#include <stdint.h>

namespace saturn::hal::dual_sh2::protocol {

struct Message {
    uint32_t sequence;
    uint32_t command;
    uint32_t argument0;
    uint32_t argument1;
};

inline uint32_t next_sequence(uint32_t current) {
    const uint32_t next = current + 1u;
    return next == 0u ? 1u : next;
}

inline bool has_message(uint32_t sequence, uint32_t acknowledgment) {
    return sequence != 0u && sequence != acknowledgment;
}

template <typename Slot>
inline bool publish_slot(Slot& slot, uint32_t generation, uint32_t command,
                         uint32_t argument0, uint32_t argument1) {
    if (slot.sequence != slot.acknowledgment) return false;
    slot.command = command;
    slot.argument0 = argument0;
    slot.argument1 = argument1;
    slot.generation = generation;
    slot.sequence = next_sequence(slot.sequence);
    return true;
}

template <typename Slot>
inline bool consume_slot(Slot& slot, uint32_t generation, Message* out_message) {
    const uint32_t sequence = slot.sequence;
    if (!has_message(sequence, slot.acknowledgment)) return false;
    if (slot.generation != generation) {
        slot.acknowledgment = sequence;
        return false;
    }
    if (out_message != nullptr) {
        out_message->sequence = sequence;
        out_message->command = slot.command;
        out_message->argument0 = slot.argument0;
        out_message->argument1 = slot.argument1;
    }
    slot.acknowledgment = sequence;
    return true;
}

}  // namespace saturn::hal::dual_sh2::protocol

#endif
