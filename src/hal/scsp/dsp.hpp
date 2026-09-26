#ifndef SATURN_HAL_SCSP_DSP_HPP
#define SATURN_HAL_SCSP_DSP_HPP

#include <stdint.h>

namespace saturn::hal::scsp::dsp {

struct Program {
    const uint16_t (*steps)[4];   /* step_count steps of four words, most significant word first */
    uint32_t step_count;
    const uint16_t* coef;         /* 64 entries of 13 bits, or nullptr for zeros */
    const uint16_t* madrs;        /* 32 entries, or nullptr for zeros */
    uint8_t ring_length;          /* RBL code 0-3 */
    uint32_t ring_offset;         /* Sound RAM byte offset of the ring, 8 KiB aligned */
};

/* Loads a program: stops the DSP, writes the data registers, clears the ring
 * buffer and starts the program. False when the placement or program is invalid. */
bool load(const Program& program);
/* Stops the DSP (an empty program). */
void stop();
bool running();
/* Live updates; false when the index is out of range. */
bool set_coef(uint32_t index, uint16_t coefficient13);
bool set_madrs(uint32_t index, uint16_t value);

}  // namespace saturn::hal::scsp::dsp

#endif
