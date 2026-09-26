#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "saturn/scu_dsp.h"
#include "src/hal/scu/dsp.hpp"
#include "src/hal/scu/dsp_logic.hpp"
#include "src/hal/scu/dsp_transform_dma_words.h"
#include "src/hal/scu/dsp_transform_words.h"

/* A stand-in DSP: program and data RAM as arrays, the port semantics of the
 * manual, and a "run" that recognizes the built-in program and computes what
 * the real one computes (the assembler simulator proves the program itself). */
namespace {

namespace logic = saturn::hal::scu::dsp_logic;

uint32_t g_program[256];
uint32_t g_data[4][64];
uint32_t g_pc = 0u;
uint32_t g_data_address = 0u;
bool g_running = false;
bool g_ended_flag = false;
unsigned g_start_calls = 0u;
unsigned g_status_reads_before_finish = 0u;      /* polls a run takes to finish */
unsigned g_pending_polls = 0u;
bool g_never_finishes = false;

const void* g_dma_pointers[8];
unsigned g_dma_pointer_count = 0u;
unsigned g_invalidations = 0u;
uint32_t g_entry = 0u;

bool is_builtin() {
    for (uint32_t i = 0u; i < kDspTransformProgramCount; ++i) {
        if (g_program[kDspTransformProgramAddress + i] != kDspTransformProgram[i]) return false;
    }
    for (uint32_t i = 0u; i < kDspTransformDmaProgramCount; ++i) {
        if (g_program[kDspTransformDmaProgramAddress + i] != kDspTransformDmaProgram[i]) return false;
    }
    return true;
}

void run_transform_body();

void run_program() {
    if (!is_builtin()) return;
    if (g_entry == kDspTransformDmaProgramAddress) {
        // the DMA variant: parameters in RAM 3, data through host memory
        const int32_t* src = static_cast<const int32_t*>(g_dma_pointers[g_data[3][0]]);
        int32_t* dst = static_cast<int32_t*>(const_cast<void*>(g_dma_pointers[g_data[3][1]]));
        assert(g_data[3][2] == g_data[3][3]);
        const uint32_t vectors = g_data[3][2] / 3u;
        assert(vectors == g_data[3][63] + 1u);
        for (uint32_t i = 0u; i < g_data[3][2]; ++i) g_data[1][i] = static_cast<uint32_t>(src[i]);
        run_transform_body();
        for (uint32_t i = 0u; i < g_data[3][2]; ++i) dst[i] = static_cast<int32_t>(g_data[2][i]);
        return;
    }
    run_transform_body();
}

void run_transform_body() {
    const uint32_t count = g_data[3][63] + 1u;
    for (uint32_t v = 0u; v < count; ++v) {
        const int64_t x = static_cast<int32_t>(g_data[1][3 * v]);
        const int64_t y = static_cast<int32_t>(g_data[1][3 * v + 1]);
        const int64_t z = static_cast<int32_t>(g_data[1][3 * v + 2]);
        for (uint32_t r = 0u; r < 3u; ++r) {
            const int64_t total = static_cast<int32_t>(g_data[0][3 * r]) * x +
                                  static_cast<int32_t>(g_data[0][3 * r + 1]) * y +
                                  static_cast<int32_t>(g_data[0][3 * r + 2]) * z;
            g_data[2][3 * v + r] = static_cast<uint32_t>(total);
        }
    }
}

}  // namespace

namespace saturn::hal::scu::dsp {

bool dma_range_ok(const void* p, uint32_t bytes) {
    return p != nullptr && bytes != 0u && (reinterpret_cast<uintptr_t>(p) & 3u) == 0u && (bytes & 3u) == 0u;
}

uint32_t dma_word_address(const void* p) {
    for (unsigned i = 0u; i < g_dma_pointer_count; ++i) {
        if (g_dma_pointers[i] == p) return i;
    }
    g_dma_pointers[g_dma_pointer_count] = p;
    return g_dma_pointer_count++;
}

void invalidate(const void*, uint32_t) {
    ++g_invalidations;
}

dsp_logic::Status status() {
    if (g_running) {
        if (g_never_finishes) return dsp_logic::decode_status(dsp_logic::kPpafExecute);
        if (g_pending_polls > 0u) {
            --g_pending_polls;
            return dsp_logic::decode_status(dsp_logic::kPpafExecute | (g_pc & 0xFFu));
        }
        g_running = false;
        run_program();
        g_ended_flag = true;
    }
    const uint32_t word = (g_ended_flag ? dsp_logic::kPpafEnded : 0u) | (g_pc & 0xFFu);
    g_ended_flag = false;
    return dsp_logic::decode_status(word);
}

void load_program(const uint32_t* words, uint32_t count, uint32_t at) {
    for (uint32_t i = 0u; i < count; ++i) g_program[at + i] = words[i];
}

void write_data(uint32_t bank, uint32_t offset, const uint32_t* words, uint32_t count) {
    g_data_address = (bank << 6u) | offset;
    for (uint32_t i = 0u; i < count; ++i) {
        g_data[(g_data_address >> 6u) & 3u][g_data_address & 63u] = words[i];
        ++g_data_address;
    }
}

void read_data(uint32_t bank, uint32_t offset, uint32_t* words, uint32_t count) {
    g_data_address = (bank << 6u) | offset;
    for (uint32_t i = 0u; i < count; ++i) {
        words[i] = g_data[(g_data_address >> 6u) & 3u][g_data_address & 63u];
        ++g_data_address;
    }
}

void start(uint32_t entry) {
    ++g_start_calls;
    g_entry = entry;
    g_pc = entry;
    g_running = true;
    g_pending_polls = g_status_reads_before_finish;
}

void stop() {
    g_running = false;
}

bool wait_finished(uint32_t timeout_ticks) {
    for (uint32_t i = 0u; i <= timeout_ticks; ++i) {
        if (dsp_logic::finished(status())) return true;
    }
    return false;
}

}  // namespace saturn::hal::scu::dsp

static void logic_words_follow_the_manual() {
    using namespace logic;
    assert(start_word(0u) == 0x00018000u);
    assert(start_word(0x12u) == 0x00018012u);
    assert(load_pc_word(0x40u) == 0x00008040u);
    assert(data_address_word(2u, 5u) == 0x85u && data_address_word(3u, 63u) == 0xFFu);
    assert(program_range_ok(0u, 256u) && !program_range_ok(1u, 256u) && !program_range_ok(257u, 0u));
    assert(data_range_ok(3u, 63u, 1u) && !data_range_ok(3u, 63u, 2u) && !data_range_ok(4u, 0u, 1u));
    const Status s = decode_status(kPpafExecute | kPpafEnded | kPpafZero | kPpafDma | 0x21u);
    assert(s.running && s.ended && s.zero && s.dma && !s.carry && !s.sign && !s.overflow && s.pc == 0x21u);
    assert(!finished(s));
    assert(finished(decode_status(kPpafEnded)));
    assert(!finished(decode_status(kPpafDma)));
    assert(transform_count_ok(1u) && transform_count_ok(21u) && !transform_count_ok(0u) && !transform_count_ok(22u));
    assert(kTransformMaxVertices * 3u <= kBankWords - 1u + 1u);
}

static void raw_access_and_refusals() {
    uint32_t words[4] = {1u, 2u, 3u, 4u};
    uint32_t back[4] = {};
    assert(sat_dsp_load_program(nullptr, 1u, 0u) == SAT_ERR_INVALID_ARG);
    assert(sat_dsp_load_program(words, 4u, 253u) == SAT_ERR_INVALID_ARG);
    assert(sat_dsp_load_program(words, 4u, 250u) == SAT_OK && g_program[253] == 4u);
    assert(sat_dsp_write_data(4u, 0u, words, 1u) == SAT_ERR_INVALID_ARG);
    assert(sat_dsp_write_data(1u, 62u, words, 3u) == SAT_ERR_INVALID_ARG);
    assert(sat_dsp_write_data(1u, 10u, words, 4u) == SAT_OK);
    assert(sat_dsp_read_data(1u, 10u, back, 4u) == SAT_OK && std::memcmp(words, back, sizeof(words)) == 0);
    assert(sat_dsp_start(256u) == SAT_ERR_INVALID_ARG);
    sat_dsp_status_t st{};
    assert(sat_dsp_status(nullptr) == SAT_ERR_INVALID_ARG && sat_dsp_status(&st) == SAT_OK && st.running == 0u);

    // while running, the RAMs and a second start are refused
    g_status_reads_before_finish = 8u;
    assert(sat_dsp_start(0u) == SAT_OK && g_running);
    assert(sat_dsp_write_data(1u, 0u, words, 1u) == SAT_ERR_BUSY);
    assert(sat_dsp_read_data(1u, 0u, back, 1u) == SAT_ERR_BUSY);
    assert(sat_dsp_load_program(words, 1u, 0u) == SAT_ERR_BUSY);
    assert(sat_dsp_start(0u) == SAT_ERR_BUSY);
    assert(sat_dsp_running() == 1u);
    assert(sat_dsp_wait(10u) == SAT_OK && sat_dsp_running() == 0u);
    // a program that never ends is reported and can be stopped
    g_never_finishes = true;
    assert(sat_dsp_start(0u) == SAT_OK);
    assert(sat_dsp_wait(5u) == SAT_ERR_TIMEOUT);
    assert(sat_dsp_stop() == SAT_OK);
    g_never_finishes = false;
    assert(sat_dsp_wait(5u) == SAT_OK);
    g_status_reads_before_finish = 0u;
}

static void transform_matches_the_fixed_point_reference() {
    const int32_t one = 0x10000;
    const int32_t matrix[9] = {one / 2, -one, 3, 7, one, -5, 100, 200, -one};
    int32_t in[3 * 50];
    int32_t out[3 * 50] = {};
    for (int i = 0; i < 150; ++i) in[i] = (i * 37) % 401 - 200;

    // not usable before the matrix is set
    assert(sat_dsp_transform_begin(in, 1u) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_dsp_transform_set_matrix(nullptr) == SAT_ERR_INVALID_ARG);
    assert(sat_dsp_transform_set_matrix(matrix) == SAT_OK);
    assert(is_builtin());

    assert(sat_dsp_transform_begin(nullptr, 1u) == SAT_ERR_INVALID_ARG);
    assert(sat_dsp_transform_begin(in, 0u) == SAT_ERR_INVALID_ARG);
    assert(sat_dsp_transform_begin(in, SAT_DSP_TRANSFORM_MAX_VERTICES + 1u) == SAT_ERR_INVALID_ARG);
    assert(sat_dsp_transform_end(out, 1u, 10u) == SAT_ERR_INVALID_ARG);      // nothing in flight

    g_status_reads_before_finish = 2u;
    assert(sat_dsp_transform_begin(in, 5u) == SAT_OK);
    assert(sat_dsp_transform_begin(in, 5u) == SAT_ERR_BUSY);
    assert(sat_dsp_transform_end(out, 4u, 10u) == SAT_ERR_INVALID_ARG);       // wrong count
    assert(sat_dsp_transform_end(out, 5u, 10u) == SAT_OK);
    for (int v = 0; v < 5; ++v) {
        for (int r = 0; r < 3; ++r) {
            const int64_t total = static_cast<int64_t>(matrix[3 * r]) * in[3 * v] +
                                  static_cast<int64_t>(matrix[3 * r + 1]) * in[3 * v + 1] +
                                  static_cast<int64_t>(matrix[3 * r + 2]) * in[3 * v + 2];
            assert(out[3 * v + r] == static_cast<int32_t>(total));
        }
    }

    // any count, in runs of at most 21
    const unsigned starts_before = g_start_calls;
    std::memset(out, 0, sizeof(out));
    assert(sat_dsp_transform_vertices(in, out, 50u, 10u) == SAT_OK);
    assert(g_start_calls - starts_before == 3u);                               // 21 + 21 + 8
    for (int v = 0; v < 50; ++v) {
        const int64_t total = static_cast<int64_t>(matrix[3]) * in[3 * v] +
                              static_cast<int64_t>(matrix[4]) * in[3 * v + 1] +
                              static_cast<int64_t>(matrix[5]) * in[3 * v + 2];
        assert(out[3 * v + 1] == static_cast<int32_t>(total));
    }
    assert(sat_dsp_transform_vertices(in, out, 0u, 10u) == SAT_OK);

    // a timeout leaves the run pending; the same count can still be collected
    g_never_finishes = true;
    assert(sat_dsp_transform_begin(in, 2u) == SAT_OK);
    assert(sat_dsp_transform_end(out, 2u, 3u) == SAT_ERR_TIMEOUT);
    g_never_finishes = false;
    assert(sat_dsp_transform_end(out, 2u, 3u) == SAT_OK);

    // the DMA variant: the DSP moves the data itself
    {
        alignas(4) static int32_t dma_in[3 * 21 * 2];
        alignas(4) static int32_t dma_out[3 * 21 * 2];
        for (int i = 0; i < 126; ++i) dma_in[i] = (i * 53) % 601 - 300;
        assert(sat_dsp_transform_begin_dma(nullptr, dma_out, 1u) == SAT_ERR_INVALID_ARG);
        assert(sat_dsp_transform_begin_dma(dma_in, dma_out, 22u) == SAT_ERR_INVALID_ARG);
        assert(sat_dsp_transform_begin_dma(reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(dma_in) + 2u),
                                           dma_out, 1u) == SAT_ERR_INVALID_ARG);       // not longword aligned
        assert(sat_dsp_transform_end_dma(10u) == SAT_ERR_INVALID_ARG);              // nothing in flight
        g_status_reads_before_finish = 2u;
        assert(sat_dsp_transform_begin_dma(dma_in, dma_out, 7u) == SAT_OK);
        assert(sat_dsp_transform_begin_dma(dma_in, dma_out, 7u) == SAT_ERR_BUSY);
        assert(sat_dsp_transform_end(out, 7u, 10u) == SAT_ERR_INVALID_ARG);         // wrong end call
        const unsigned invalidated = g_invalidations;
        assert(sat_dsp_transform_end_dma(10u) == SAT_OK && g_invalidations == invalidated + 1u);
        for (int v = 0; v < 7; ++v) {
            const int64_t total = static_cast<int64_t>(matrix[6]) * dma_in[3 * v] +
                                  static_cast<int64_t>(matrix[7]) * dma_in[3 * v + 1] +
                                  static_cast<int64_t>(matrix[8]) * dma_in[3 * v + 2];
            assert(dma_out[3 * v + 2] == static_cast<int32_t>(total));
        }
        // many vectors, in runs
        std::memset(dma_out, 0, sizeof(dma_out));
        assert(sat_dsp_transform_vertices_dma(dma_in, dma_out, 42u, 10u) == SAT_OK);
        assert(dma_out[3 * 41] == static_cast<int32_t>(static_cast<int64_t>(matrix[0]) * dma_in[3 * 41] +
                                                       static_cast<int64_t>(matrix[1]) * dma_in[3 * 41 + 1] +
                                                       static_cast<int64_t>(matrix[2]) * dma_in[3 * 41 + 2]));
        g_status_reads_before_finish = 0u;
    }

    // loading another program means the transform must be set up again
    uint32_t nop = 0u;
    assert(sat_dsp_load_program(&nop, 1u, 0u) == SAT_OK);
    assert(sat_dsp_transform_begin(in, 1u) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_dsp_transform_set_matrix(matrix) == SAT_OK && is_builtin());
    g_status_reads_before_finish = 0u;
}

int main() {
    logic_words_follow_the_manual();
    raw_access_and_refusals();
    transform_matches_the_fixed_point_reference();
    std::puts("PASS: test_scu_dsp_api.cpp");
    return 0;
}
