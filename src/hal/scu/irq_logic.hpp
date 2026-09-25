#ifndef SATURN_HAL_SCU_IRQ_LOGIC_HPP
#define SATURN_HAL_SCU_IRQ_LOGIC_HPP

#include <stdint.h>

/* Pure SCU interrupt policy: no register access, so host tests cover it.
 * Source numbers are the interrupt status bit numbers of the SCU manual
 * (table 2.1); vector = 0x40 + source. */
namespace saturn::hal::scu::irq_logic {

constexpr uint8_t kSourceCount = 14u;
constexpr uint8_t kVectorBase = 0x40u;
constexpr uint8_t kVblankIn = 0u;
constexpr uint8_t kTimer0 = 3u;
constexpr uint8_t kTimer1 = 4u;
/* Every internal source masked; bit 15 (A-bus) always is. */
constexpr uint32_t kImsAllMasked = 0x0000BFFFu;
constexpr uint32_t kInternalBits = 0x00003FFFu;
/* An NTSC frame has 263 lines; T0C values past it never match. */
constexpr uint16_t kTimer0MaxLine = 263u;
/* T1S is 9 bits; 0 counts as 512, which no line is long enough for. */
constexpr uint16_t kTimer1MaxDots = 0x01FFu;

/* SH-2 interrupt level of each source (SCU manual, table 2.1). */
constexpr uint8_t kLevels[kSourceCount] = {
    0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x8, 0x6, 0x6, 0x5, 0x3, 0x2};

inline uint8_t level(uint8_t source) {
    return source < kSourceCount ? kLevels[source] : 0u;
}

inline uint8_t vector(uint8_t source) {
    return static_cast<uint8_t>(kVectorBase + source);
}

/* IMS for a set of enabled sources: 1 masks, so enabled bits are cleared. */
inline uint32_t compose_ims(uint16_t enabled) {
    return kImsAllMasked & ~(static_cast<uint32_t>(enabled) & kInternalBits);
}

/* The SR interrupt mask that admits every enabled source and nothing below
 * it: a source is taken when its level is ABOVE the mask. With nothing
 * enabled every level stays masked (15). */
inline uint8_t sr_mask(uint16_t enabled) {
    uint8_t lowest = 0x10u;
    for (uint8_t s = 0u; s < kSourceCount; ++s) {
        if ((enabled & (1u << s)) != 0u && kLevels[s] < lowest) lowest = kLevels[s];
    }
    return lowest > 0xFu ? 0xFu : static_cast<uint8_t>(lowest - 1u);
}

/* Timer 0 or timer 1 needs TENB; the others do not. */
inline bool timers_needed(uint16_t enabled) {
    return (enabled & ((1u << kTimer0) | (1u << kTimer1))) != 0u;
}

/* T1MD register value: bit 8 selects "only on the timer 0 line", bit 0 is
 * the timer enable for both timers. */
inline uint32_t compose_t1md(bool enable, bool timer0_line_only) {
    return (timer0_line_only ? 0x0100u : 0u) | (enable ? 0x0001u : 0u);
}

/* VBlank-IN is the frame clock once interrupts run. No VBlank-IN for two
 * whole frames of FRT time means the host is not delivering it: fall back
 * to polling TVSTAT rather than hang. Uncalibrated (0) never stalls here;
 * the caller bounds that case by spin count. */
inline bool vblank_stalled(uint64_t waited_ticks, uint16_t ticks_per_frame) {
    return ticks_per_frame != 0u &&
           waited_ticks > 2u * static_cast<uint64_t>(ticks_per_frame);
}

/* Display frames between two VBlank-IN counts, wrap-safe; a wait always
 * spans at least one. */
inline uint32_t frames_between(uint32_t then, uint32_t now) {
    const uint32_t frames = now - then;
    return frames == 0u ? 1u : frames;
}

/* 16-bit FRT reading folded into a 64-bit total. Correct only while two
 * observations are less than one FRT wrap apart -- the reason the VBlank-IN
 * handler observes it every frame. */
inline uint64_t extend_ticks(uint64_t total, uint16_t* last, uint16_t now) {
    const uint16_t delta = static_cast<uint16_t>(now - *last);
    *last = now;
    return total + delta;
}

}  // namespace saturn::hal::scu::irq_logic

#endif
