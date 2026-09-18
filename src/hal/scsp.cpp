#include "src/hal/scsp.hpp"

#include "src/hal/smpc.hpp"

namespace saturn::hal::scsp {

namespace {

constexpr uintptr_t kSoundRamBase = 0x25A00000u;
constexpr uintptr_t kRegisterBase = 0x25B00000u;
constexpr uintptr_t kCommonControlAddress = 0x25B00400u;
constexpr uintptr_t kDspProgramAddress = 0x25B00800u;
constexpr uint32_t kSlotStride = 0x20u;
constexpr uint16_t kMem4Mb = 0x0200u;
constexpr uint16_t kMasterVolumeMax = 0x000Fu;
constexpr uint16_t kKeyOnExecute = 0x1000u;
constexpr uint16_t kKeyOnBit = 0x0800u;
constexpr uint16_t kPcm8Bit = 0x0010u;
constexpr uint16_t kLoopNormal = 0x0020u;
constexpr uint32_t kNativeRate = 44100u;

bool g_ready = false;
// Slot control (+00) contains write-only fields (including KYONB), so it
// cannot be read-modified-written safely. Keep the programmed state here and
// use it whenever a key transition needs KYONEX.
uint16_t g_slot_control[kSlotCount] = {};

inline volatile uint16_t* slot_word(uint8_t slot, uint32_t offset) {
    return reinterpret_cast<volatile uint16_t*>(
        kRegisterBase + static_cast<uintptr_t>(slot) * kSlotStride + offset
    );
}

inline volatile uint16_t* common_control() {
    return reinterpret_cast<volatile uint16_t*>(kCommonControlAddress);
}

inline volatile uint16_t* sound_ram_word(uint32_t byte_offset) {
    return reinterpret_cast<volatile uint16_t*>(kSoundRamBase + byte_offset);
}

inline void write_sound_word(uint32_t byte_offset, uint16_t value) {
    *sound_ram_word(byte_offset) = value;
}

inline void install_idle_68k_stub() {
    // Reset SSP = 0x0007FFF0, reset PC = 0x00000008, then BRA.S -2.
    // Sound RAM is accessed by the SH-2 in 16-bit units.
    write_sound_word(0x0000u, 0x0007u);
    write_sound_word(0x0002u, 0xFFF0u);
    write_sound_word(0x0004u, 0x0000u);
    write_sound_word(0x0006u, 0x0008u);
    write_sound_word(0x0008u, 0x60FEu);
}

inline void clear_dsp_program() {
    volatile uint16_t* p = reinterpret_cast<volatile uint16_t*>(kDspProgramAddress);
    for (uint32_t i = 0; i < 512u; ++i) {
        p[i] = 0u;
    }
}

inline void clear_slot_registers() {
    for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
        for (uint32_t offset = 0; offset < 0x18u; offset += 2u) {
            *slot_word(slot, offset) = 0u;
        }
    }
}

inline uint8_t clamp_u8(uint32_t value, uint8_t max_value) {
    return static_cast<uint8_t>(value > max_value ? max_value : value);
}

inline void execute_key_transition(uint8_t slot) {
    *slot_word(slot, 0x00u) = g_slot_control[slot];
    *slot_word(slot, 0x00u) = static_cast<uint16_t>(g_slot_control[slot] | kKeyOnExecute);
}

}  // namespace

bool init() {
    if (g_ready) {
        return true;
    }

    // Hold the 68k before replacing vectors/program data. The SH-2 remains
    // able to initialize the sound block while the sound CPU is reset.
    if (!smpc::sound_off()) {
        return false;
    }

    *common_control() = kMem4Mb;
    clear_sound_ram(0u, kSystemReservedBytes);
    install_idle_68k_stub();
    clear_slot_registers();
    for (uint8_t slot = 0u; slot < kSlotCount; ++slot) g_slot_control[slot] = 0u;
    clear_dsp_program();

    if (!smpc::sound_on()) {
        return false;
    }

    *common_control() = static_cast<uint16_t>(kMem4Mb | kMasterVolumeMax);
    g_ready = true;
    return true;
}

void shutdown() {
    if (!g_ready) {
        return;
    }
    stop_all_slots();
    *common_control() = kMem4Mb;
    g_ready = false;
}

bool is_ready() {
    return g_ready;
}

bool upload(uint32_t offset, const void* data, uint32_t byte_count) {
    if (data == nullptr || (offset & 1u) != 0u || offset >= kSoundRamBytes ||
        byte_count > (kSoundRamBytes - offset)) {
        return false;
    }

    // The main SH-2 accesses SCSP sound RAM in 16-bit units. Pack source
    // bytes explicitly so this also works for signed PCM8 payloads.
    const uint8_t* src = static_cast<const uint8_t*>(data);
    uint32_t i = 0u;
    for (; i + 1u < byte_count; i += 2u) {
        const uint16_t word = static_cast<uint16_t>(
            (static_cast<uint16_t>(src[i]) << 8) | src[i + 1u]
        );
        write_sound_word(offset + i, word);
    }
    if (i < byte_count) {
        write_sound_word(offset + i, static_cast<uint16_t>(src[i]) << 8);
    }
    return true;
}

void clear_sound_ram(uint32_t offset, uint32_t byte_count) {
    if ((offset & 1u) != 0u || offset >= kSoundRamBytes) {
        return;
    }
    if (byte_count > (kSoundRamBytes - offset)) {
        byte_count = kSoundRamBytes - offset;
    }
    const uint32_t words = (byte_count + 1u) / 2u;
    for (uint32_t i = 0u; i < words; ++i) {
        write_sound_word(offset + i * 2u, 0u);
    }
}

uint16_t encode_pitch(uint32_t sample_rate, uint32_t pitch_scale_q16) {
    if (sample_rate == 0u || pitch_scale_q16 == 0u) {
        return 0u;
    }

    uint64_t num = static_cast<uint64_t>(sample_rate) * static_cast<uint64_t>(pitch_scale_q16);
    uint64_t den = static_cast<uint64_t>(kNativeRate) << 16;
    int32_t octave = 0;

    while (num < den && octave > -8) {
        num <<= 1;
        --octave;
    }
    while (num >= (den << 1) && octave < 7) {
        den <<= 1;
        ++octave;
    }

    uint32_t fns = 0u;
    if (num > den) {
        fns = static_cast<uint32_t>(((num - den) * 1024u + (den / 2u)) / den);
        if (fns > 1023u) {
            fns = 1023u;
        }
    }

    return static_cast<uint16_t>(((static_cast<uint32_t>(octave) & 0x0Fu) << 11) | fns);
}

uint8_t encode_pan(int16_t pan) {
    if (pan > 15) pan = 15;
    if (pan < -15) pan = -15;
    if (pan == 0) return 0u;
    if (pan > 0) return static_cast<uint8_t>(pan & 0x0F);
    return static_cast<uint8_t>(0x10u | ((-pan) & 0x0F));
}

bool configure_slot(uint8_t slot, const SlotConfig& config) {
    if (!g_ready || slot >= kSlotCount || config.sample_count == 0u || config.start_address >= kSoundRamBytes) {
        return false;
    }

    key_off(slot);

    uint16_t control = static_cast<uint16_t>((config.start_address >> 16) & 0x0Fu);
    if (config.pcm8 != 0u) control |= kPcm8Bit;
    if (config.loop != 0u) control |= kLoopNormal;

    g_slot_control[slot] = control;
    *slot_word(slot, 0x00u) = control;
    *slot_word(slot, 0x02u) = static_cast<uint16_t>(config.start_address & 0xFFFFu);
    *slot_word(slot, 0x04u) = config.loop_start;
    *slot_word(slot, 0x06u) = config.loop_end;

    *slot_word(slot, 0x08u) = 0x001Fu;
    *slot_word(slot, 0x0Au) = 0x3C1Fu;
    *slot_word(slot, 0x0Cu) = static_cast<uint16_t>(config.total_level);
    *slot_word(slot, 0x0Eu) = 0u;
    *slot_word(slot, 0x10u) = encode_pitch(config.sample_rate, config.pitch_scale_q16);
    *slot_word(slot, 0x12u) = 0u;
    *slot_word(slot, 0x14u) = 0u;
    *slot_word(slot, 0x16u) = static_cast<uint16_t>(
        (static_cast<uint16_t>(clamp_u8(config.direct_level, 7u)) << 13) |
        (static_cast<uint16_t>(config.pan & 0x1Fu) << 8)
    );
    return true;
}

void key_on(uint8_t slot) {
    if (!g_ready || slot >= kSlotCount) return;
    g_slot_control[slot] = static_cast<uint16_t>(g_slot_control[slot] | kKeyOnBit);
    execute_key_transition(slot);
}

void key_off(uint8_t slot) {
    if (slot >= kSlotCount) return;
    g_slot_control[slot] = static_cast<uint16_t>(g_slot_control[slot] & ~kKeyOnBit);
    execute_key_transition(slot);
}

void set_slot_level_pan(uint8_t slot, uint8_t total_level, uint8_t direct_level, uint8_t pan) {
    if (!g_ready || slot >= kSlotCount) return;
    *slot_word(slot, 0x0Cu) = static_cast<uint16_t>(total_level);
    *slot_word(slot, 0x16u) = static_cast<uint16_t>(
        (static_cast<uint16_t>(clamp_u8(direct_level, 7u)) << 13) |
        (static_cast<uint16_t>(pan & 0x1Fu) << 8)
    );
}

void set_master_volume(uint8_t level) {
    if (level > 15u) level = 15u;
    *common_control() = static_cast<uint16_t>(kMem4Mb | level);
}

void stop_all_slots() {
    if (!g_ready) return;
    for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
        g_slot_control[slot] = static_cast<uint16_t>(g_slot_control[slot] & ~kKeyOnBit);
        *slot_word(slot, 0x00u) = g_slot_control[slot];
    }
    *slot_word(0u, 0x00u) = static_cast<uint16_t>(g_slot_control[0u] | kKeyOnExecute);
}

}  // namespace saturn::hal::scsp
