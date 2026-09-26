#ifndef SATURN_HAL_SCU_DSP_LOGIC_HPP
#define SATURN_HAL_SCU_DSP_LOGIC_HPP

#include <stdint.h>

/* Pure SCU DSP policy: register words, ranges and the built-in transform's
 * memory layout (SCU manual chapters 2 and 4). No register access. */
namespace saturn::hal::scu::dsp_logic {

constexpr uint32_t kProgramWords = 256u;
constexpr uint32_t kBanks = 4u;
constexpr uint32_t kBankWords = 64u;

/* DSP_PPAF (program control), DSP_PPD (program RAM port), DSP_PDA (data RAM
 * address) and DSP_PDD (data RAM port). Longword access. */
constexpr uint32_t kRegPpaf = 0x25FE0080u;
constexpr uint32_t kRegPpd = 0x25FE0084u;
constexpr uint32_t kRegPda = 0x25FE0088u;
constexpr uint32_t kRegPdd = 0x25FE008Cu;

constexpr uint32_t kPpafLoadEnable = 1u << 15;   /* LEF: load the address field into the PC */
constexpr uint32_t kPpafExecute = 1u << 16;      /* EX: run (reads 0 again once the program ended) */
constexpr uint32_t kPpafEnded = 1u << 18;        /* E: ENDI executed (reading clears it) */
constexpr uint32_t kPpafOverflow = 1u << 19;
constexpr uint32_t kPpafCarry = 1u << 20;
constexpr uint32_t kPpafZero = 1u << 21;
constexpr uint32_t kPpafSign = 1u << 22;
constexpr uint32_t kPpafDma = 1u << 23;          /* T0: a DSP DMA is still moving data */

constexpr uint32_t start_word(uint32_t entry) {
    return kPpafExecute | kPpafLoadEnable | (entry & 0xFFu);
}

constexpr uint32_t load_pc_word(uint32_t address) {
    return kPpafLoadEnable | (address & 0xFFu);
}

constexpr bool program_range_ok(uint32_t at, uint32_t count) {
    return at <= kProgramWords && count <= kProgramWords - at;
}

constexpr bool data_range_ok(uint32_t bank, uint32_t offset, uint32_t count) {
    return bank < kBanks && offset <= kBankWords && count <= kBankWords - offset;
}

/* PDA: bits 7-6 select the bank, 5-0 the word; the port steps to the next word
 * after every access (and on into the next bank). */
constexpr uint32_t data_address_word(uint32_t bank, uint32_t offset) {
    return ((bank & 3u) << 6u) | (offset & 0x3Fu);
}

struct Status {
    bool running;     /* the program is executing */
    bool ended;       /* ENDI ran (raises the DSP-end interrupt) */
    bool overflow;
    bool carry;
    bool zero;
    bool sign;
    bool dma;         /* a DSP DMA transfer is in progress */
    uint8_t pc;
};

constexpr Status decode_status(uint32_t ppaf) {
    return Status{(ppaf & kPpafExecute) != 0u, (ppaf & kPpafEnded) != 0u, (ppaf & kPpafOverflow) != 0u,
                  (ppaf & kPpafCarry) != 0u, (ppaf & kPpafZero) != 0u, (ppaf & kPpafSign) != 0u,
                  (ppaf & kPpafDma) != 0u, static_cast<uint8_t>(ppaf & 0xFFu)};
}

/* Nothing is left to wait for once the program stopped and its DMA drained. */
constexpr bool finished(const Status& s) {
    return !s.running && !s.dma;
}

/* The built-in 3x3 transform (dsp_transform.dsp): matrix words 0-8 of RAM 0,
 * three input words per vector in RAM 1 and three output words in RAM 2, the
 * vector count minus one in word 63 of RAM 3. */
constexpr uint32_t kTransformMatrixWords = 9u;
constexpr uint32_t kTransformMaxVertices = 21u;    /* 63 words of a 64-word bank */
constexpr uint32_t kTransformCountBank = 3u;
constexpr uint32_t kTransformCountOffset = 63u;

/* The DMA variant (dsp_transform_dma.dsp) sits at program address 32 and takes
 * its parameters in RAM 3: words 0 and 1 the source and destination word
 * addresses, words 2 and 3 the transfer length in words (three per vector). */
constexpr uint32_t kTransformDmaParamWords = 4u;

constexpr bool transform_count_ok(uint32_t vertices) {
    return vertices >= 1u && vertices <= kTransformMaxVertices;
}

}  // namespace saturn::hal::scu::dsp_logic

#endif  // SATURN_HAL_SCU_DSP_LOGIC_HPP
