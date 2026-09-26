#include "saturn/scu_dsp.h"

#include "src/hal/scu/dsp.hpp"
#include "src/hal/scu/dsp_transform_dma_words.h"
#include "src/hal/scu/dsp_transform_words.h"

namespace {

namespace hal = saturn::hal::scu::dsp;
namespace logic = saturn::hal::scu::dsp_logic;

/* What the program RAM holds, so the transform is loaded once. */
enum class Resident : uint8_t { Unknown, Transform };
Resident g_resident = Resident::Unknown;
bool g_matrix_set = false;
uint32_t g_pending = 0u;      /* vectors of a transform run in flight */
int32_t* g_pending_out = nullptr;   /* the DMA variant: where its results land (null: the plain variant) */

sat_result_t require_idle() {
    return logic::finished(hal::status()) ? SAT_OK : SAT_ERR_BUSY;
}

}  // namespace

extern "C" sat_result_t sat_dsp_load_program(const uint32_t* words, uint32_t count, uint32_t at) {
    if (words == nullptr || count == 0u || !logic::program_range_ok(at, count)) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_idle());
    hal::load_program(words, count, at);
    g_resident = Resident::Unknown;
    g_matrix_set = false;
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_write_data(uint32_t bank, uint32_t offset, const uint32_t* words,
                                           uint32_t count) {
    if (words == nullptr || count == 0u || !logic::data_range_ok(bank, offset, count)) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_idle());
    hal::write_data(bank, offset, words, count);
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_read_data(uint32_t bank, uint32_t offset, uint32_t* words, uint32_t count) {
    if (words == nullptr || count == 0u || !logic::data_range_ok(bank, offset, count)) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_idle());
    hal::read_data(bank, offset, words, count);
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_start(uint32_t entry) {
    if (entry >= logic::kProgramWords) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_idle());
    hal::start(entry);
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_stop(void) {
    hal::stop();
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_status(sat_dsp_status_t* out_status) {
    if (out_status == nullptr) return SAT_ERR_INVALID_ARG;
    const logic::Status s = hal::status();
    out_status->running = s.running ? 1u : 0u;
    out_status->ended = s.ended ? 1u : 0u;
    out_status->dma = s.dma ? 1u : 0u;
    out_status->overflow = s.overflow ? 1u : 0u;
    out_status->carry = s.carry ? 1u : 0u;
    out_status->zero = s.zero ? 1u : 0u;
    out_status->sign = s.sign ? 1u : 0u;
    out_status->pc = s.pc;
    return SAT_OK;
}

extern "C" uint8_t sat_dsp_running(void) {
    return logic::finished(hal::status()) ? 0u : 1u;
}

extern "C" sat_result_t sat_dsp_wait(uint32_t timeout_ticks) {
    return hal::wait_finished(timeout_ticks) ? SAT_OK : SAT_ERR_TIMEOUT;
}

extern "C" sat_result_t sat_dsp_transform_set_matrix(const int32_t matrix[9]) {
    if (matrix == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_idle());
    if (g_resident != Resident::Transform) {
        hal::load_program(kDspTransformProgram, kDspTransformProgramCount, kDspTransformProgramAddress);
        hal::load_program(kDspTransformDmaProgram, kDspTransformDmaProgramCount,
                          kDspTransformDmaProgramAddress);
        g_resident = Resident::Transform;
    }
    uint32_t words[logic::kTransformMatrixWords];
    for (uint32_t i = 0u; i < logic::kTransformMatrixWords; ++i) words[i] = static_cast<uint32_t>(matrix[i]);
    hal::write_data(0u, 0u, words, logic::kTransformMatrixWords);
    g_matrix_set = true;
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_transform_begin(const int32_t* xyz, uint32_t count) {
    if (xyz == nullptr || !logic::transform_count_ok(count)) return SAT_ERR_INVALID_ARG;
    if (g_resident != Resident::Transform || !g_matrix_set) return SAT_ERR_NOT_INITIALIZED;
    if (g_pending != 0u) return SAT_ERR_BUSY;
    SAT_TRY(require_idle());
    uint32_t words[3u * logic::kTransformMaxVertices];
    for (uint32_t i = 0u; i < 3u * count; ++i) words[i] = static_cast<uint32_t>(xyz[i]);
    hal::write_data(1u, 0u, words, 3u * count);
    const uint32_t last = count - 1u;
    hal::write_data(logic::kTransformCountBank, logic::kTransformCountOffset, &last, 1u);
    hal::start(kDspTransformProgramAddress);
    g_pending = count;
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_transform_end(int32_t* out_xyz, uint32_t count, uint32_t timeout_ticks) {
    if (out_xyz == nullptr || count == 0u) return SAT_ERR_INVALID_ARG;
    if (g_pending == 0u || count != g_pending || g_pending_out != nullptr) return SAT_ERR_INVALID_ARG;
    if (!hal::wait_finished(timeout_ticks)) return SAT_ERR_TIMEOUT;
    uint32_t words[3u * logic::kTransformMaxVertices];
    hal::read_data(2u, 0u, words, 3u * count);
    for (uint32_t i = 0u; i < 3u * count; ++i) out_xyz[i] = static_cast<int32_t>(words[i]);
    g_pending = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_transform_begin_dma(const int32_t* xyz, int32_t* out_xyz, uint32_t count) {
    if (xyz == nullptr || out_xyz == nullptr || !logic::transform_count_ok(count)) return SAT_ERR_INVALID_ARG;
    if (!hal::dma_range_ok(xyz, 12u * count) || !hal::dma_range_ok(out_xyz, 12u * count)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (g_resident != Resident::Transform || !g_matrix_set) return SAT_ERR_NOT_INITIALIZED;
    if (g_pending != 0u) return SAT_ERR_BUSY;
    SAT_TRY(require_idle());
    const uint32_t words = 3u * count;
    const uint32_t params[logic::kTransformDmaParamWords] = {
        hal::dma_word_address(xyz), hal::dma_word_address(out_xyz), words, words};
    hal::write_data(logic::kTransformCountBank, 0u, params, logic::kTransformDmaParamWords);
    const uint32_t last = count - 1u;
    hal::write_data(logic::kTransformCountBank, logic::kTransformCountOffset, &last, 1u);
    hal::start(kDspTransformDmaProgramAddress);
    g_pending = count;
    g_pending_out = out_xyz;
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_transform_end_dma(uint32_t timeout_ticks) {
    if (g_pending == 0u || g_pending_out == nullptr) return SAT_ERR_INVALID_ARG;
    if (!hal::wait_finished(timeout_ticks)) return SAT_ERR_TIMEOUT;
    hal::invalidate(g_pending_out, 12u * g_pending);
    g_pending = 0u;
    g_pending_out = nullptr;
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_transform_vertices_dma(const int32_t* xyz, int32_t* out_xyz, uint32_t count,
                                                       uint32_t timeout_ticks) {
    if (xyz == nullptr || out_xyz == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint32_t done = 0u; done < count;) {
        const uint32_t n = count - done < logic::kTransformMaxVertices ? count - done
                                                                        : logic::kTransformMaxVertices;
        SAT_TRY(sat_dsp_transform_begin_dma(xyz + 3u * done, out_xyz + 3u * done, n));
        SAT_TRY(sat_dsp_transform_end_dma(timeout_ticks));
        done += n;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_dsp_transform_vertices(const int32_t* xyz, int32_t* out_xyz, uint32_t count,
                                                   uint32_t timeout_ticks) {
    if (xyz == nullptr || out_xyz == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint32_t done = 0u; done < count;) {
        const uint32_t n = count - done < logic::kTransformMaxVertices ? count - done
                                                                        : logic::kTransformMaxVertices;
        SAT_TRY(sat_dsp_transform_begin(xyz + 3u * done, n));
        SAT_TRY(sat_dsp_transform_end(out_xyz + 3u * done, n, timeout_ticks));
        done += n;
    }
    return SAT_OK;
}
