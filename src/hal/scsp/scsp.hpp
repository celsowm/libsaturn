#ifndef SATURN_HAL_SCSP_HPP
#define SATURN_HAL_SCSP_HPP

#include <stdint.h>

namespace saturn::hal::scsp {

constexpr uint32_t kSoundRamBytes = 0x80000u;
constexpr uint32_t kSystemReservedBytes = 0x0B000u;
constexpr uint8_t kSlotCount = 32u;

struct SlotConfig {
    uint32_t start_address;
    uint32_t sample_rate;
    uint16_t sample_count;
    uint16_t loop_start;
    uint16_t loop_end;
    uint32_t pitch_scale_q16;
    uint8_t pcm8;
    uint8_t loop;
    uint8_t total_level;
    uint8_t direct_level;
    uint8_t pan;
};

bool init();
// Busy-waits at least `count` output samples (44.1 kHz) of the SCSP.
void wait_samples(uint32_t count);
void shutdown();
bool is_ready();

bool upload(uint32_t offset, const void* data, uint32_t byte_count);
// One 16-bit word of Sound RAM (the SH-2 accesses it in 16-bit units).
uint16_t sound_word(uint32_t byte_offset);
void set_sound_word(uint32_t byte_offset, uint16_t value);
// Puts the idle 68000 stub back (reset vectors and a BRA.S -2) and restarts the
// sound CPU, e.g. after the resident driver was stopped.
bool restore_idle_68k();
void clear_sound_ram(uint32_t offset, uint32_t byte_count);

bool configure_slot(uint8_t slot, const SlotConfig& config);
// Read CA through the MSLC slot monitor. The returned value is the current
// sample offset in the SCSP's native 4096-sample units.
bool read_current_sample_block(uint8_t slot, uint8_t* out_block);
void key_on(uint8_t slot);
// Sets KYONB without KYONEX; execute_key_transitions() then keys on every
// armed slot on the same sample, e.g. both channels of a stereo stream.
void arm_key_on(uint8_t slot);
void execute_key_transitions();
void key_off(uint8_t slot);
// A key transition executed later by the sound driver: updates the shadow of the
// slot control word and returns the word (with KYONEX) to write to slot register
// 0x00 at that time. The KYONB bit is set for `on`, cleared otherwise.
uint16_t timed_key_word(uint8_t slot, bool on);
// TL=0xFF: silences a slot whose envelope release may still read Sound RAM.
void mute_slot(uint8_t slot);
void set_slot_level_pan(uint8_t slot, uint8_t total_level, uint8_t direct_level, uint8_t pan);
void stop_all_slots();
void set_master_volume(uint8_t level);
// DSP routing. A slot sends its output to MIXS 0 at `level` (0 = none, 7 = full);
// slots configured later start with the default send. EFREG n comes back through slot
// n's effect level and pan (slot register 0x16, low byte), whatever plays on that slot.
void set_default_effect_send(uint8_t level);
void set_effect_send(uint8_t slot, uint8_t level);
void set_effect_return(uint8_t efreg, uint8_t level, uint8_t pan);

uint16_t encode_pitch(uint32_t sample_rate, uint32_t pitch_scale_q16);
uint8_t encode_pan(int16_t pan);

}  // namespace saturn::hal::scsp

#endif
