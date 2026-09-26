#include "src/hal/scsp/dsp.hpp"

#include "src/hal/scsp/dsp_logic.hpp"
#include "src/hal/scsp/scsp.hpp"

namespace saturn::hal::scsp::dsp {

namespace {

constexpr uintptr_t kRegisterBase = 0x25B00000u;
constexpr uintptr_t kRingRegister = kRegisterBase + 0x402u;
constexpr uintptr_t kCoefBase = kRegisterBase + 0x700u;
constexpr uintptr_t kMadrsBase = kRegisterBase + 0x780u;
constexpr uintptr_t kProgramBase = kRegisterBase + 0x800u;

constexpr uint32_t kSettleSamples = 4u;

bool g_running = false;

inline volatile uint16_t* reg(uintptr_t address) {
    return reinterpret_cast<volatile uint16_t*>(address);
}

void clear_program() {
    for (uint32_t i = 0u; i < dsp_logic::kProgramWords; ++i) *reg(kProgramBase + i * 2u) = 0u;
}

}  // namespace

bool load(const Program& program) {
    if (!is_ready() || program.steps == nullptr || program.step_count == 0u ||
        program.step_count > dsp_logic::kProgramSteps ||
        !dsp_logic::ring_placement_ok(program.ring_offset, program.ring_length)) {
        return false;
    }
    clear_program();
    g_running = false;
    // The DSP runs while the program is written, so a half-written program executes: its memory
    // writes (of whatever the registers held) would land in the ring. The program is therefore
    // written with every gain at zero, given a few samples to run whole and flush its pending
    // accesses, and only then is the ring cleared and the gains set.
    for (uint32_t i = 0u; i < dsp_logic::kCoefCount; ++i) *reg(kCoefBase + i * 2u) = 0u;
    for (uint32_t i = 0u; i < dsp_logic::kMadrsCount; ++i) {
        *reg(kMadrsBase + i * 2u) = program.madrs != nullptr ? program.madrs[i] : 0u;
    }
    *reg(kRingRegister) = dsp_logic::ring_register(program.ring_offset, program.ring_length);
    // Last step first: the core sizes the program by the last non-empty step it sees.
    for (uint32_t step = program.step_count; step-- > 0u;) {
        for (uint32_t word = 0u; word < 4u; ++word) {
            *reg(kProgramBase + (step * 4u + word) * 2u) = program.steps[step][word];
        }
    }
    wait_samples(kSettleSamples);
    clear_sound_ram(program.ring_offset, dsp_logic::ring_bytes(program.ring_length));
    wait_samples(kSettleSamples);
    for (uint32_t i = 0u; i < dsp_logic::kCoefCount; ++i) {
        const uint16_t value = program.coef != nullptr ? program.coef[i] : 0u;
        *reg(kCoefBase + i * 2u) = dsp_logic::coefficient_register(value);
    }
    g_running = true;
    return true;
}

void stop() {
    if (!is_ready()) return;
    clear_program();
    g_running = false;
}

bool running() {
    return g_running;
}

bool set_coef(uint32_t index, uint16_t coefficient13) {
    if (!is_ready() || index >= dsp_logic::kCoefCount) return false;
    *reg(kCoefBase + index * 2u) = dsp_logic::coefficient_register(coefficient13);
    return true;
}

bool set_madrs(uint32_t index, uint16_t value) {
    if (!is_ready() || index >= dsp_logic::kMadrsCount) return false;
    *reg(kMadrsBase + index * 2u) = value;
    return true;
}

}  // namespace saturn::hal::scsp::dsp
