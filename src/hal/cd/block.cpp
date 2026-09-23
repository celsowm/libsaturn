#include "saturn/cd_block.h"

#include <stddef.h>

namespace {

struct Command {
    uint16_t cr1;
    uint16_t cr2;
    uint16_t cr3;
    uint16_t cr4;
};

#if defined(__sh__) || defined(__SH2__)
volatile uint16_t& reg(uint32_t address) {
    return *reinterpret_cast<volatile uint16_t*>(address);
}

uint16_t read_reg(uint32_t address) { return reg(address); }
void write_reg(uint32_t address, uint16_t value) { reg(address) = value; }
#else
uint16_t read_reg(uint32_t) { return 0u; }
void write_reg(uint32_t, uint16_t) {}
#endif

uint32_t timeout_for(const sat_cd_block_t* block) {
    return block->timeout_iterations == 0u
        ? SAT_CD_BLOCK_DEFAULT_TIMEOUT : block->timeout_iterations;
}

/* The HAL knows only this explicit per-block hook, never an audio symbol.
 * Suppress recursive callbacks during one service invocation. */
inline void pump_progress(sat_cd_block_t* block) {
    if (block->progress == nullptr || block->progress_active != 0u) return;
    block->progress_active = 1u;
    (void)block->progress(block->progress_context);
    block->progress_active = 0u;
}

sat_result_t wait_hirq(sat_cd_block_t* block, uint16_t mask) {
    for (uint32_t i = 0u; i < timeout_for(block); ++i) {
        if ((read_reg(SAT_CD_BLOCK_HIRQ) & mask) != 0u) return SAT_OK;
        if ((i & 0x0FFFu) == 0u) pump_progress(block);
    }
    return SAT_ERR_TIMEOUT;
}

void write_command(const Command& command) {
    write_reg(SAT_CD_BLOCK_CR1, command.cr1);
    write_reg(SAT_CD_BLOCK_CR2, command.cr2);
    write_reg(SAT_CD_BLOCK_CR3, command.cr3);
    write_reg(SAT_CD_BLOCK_CR4, command.cr4);
}

Command read_command() {
    return {
        read_reg(SAT_CD_BLOCK_CR1),
        read_reg(SAT_CD_BLOCK_CR2),
        read_reg(SAT_CD_BLOCK_CR3),
        read_reg(SAT_CD_BLOCK_CR4)
    };
}

sat_result_t execute(
    sat_cd_block_t* block,
    const Command& command,
    uint16_t wait_flags,
    Command* out_response
) {
    if (block == nullptr || out_response == nullptr) return SAT_ERR_INVALID_ARG;
    const uint16_t hirq = read_reg(SAT_CD_BLOCK_HIRQ);
    if ((hirq & SAT_CD_BLOCK_HIRQ_CMOK) == 0u) return SAT_ERR_BUSY;
    write_reg(
        SAT_CD_BLOCK_HIRQ,
        static_cast<uint16_t>(~(wait_flags | SAT_CD_BLOCK_HIRQ_CMOK)));
    write_command(command);
    SAT_TRY(wait_hirq(block, SAT_CD_BLOCK_HIRQ_CMOK));
    *out_response = read_command();
    const uint8_t status = static_cast<uint8_t>(out_response->cr1 >> 8);
    if (status == 0xFFu) return SAT_ERR_IO;
    if ((status & 0x80u) != 0u) return SAT_ERR_BUSY;
    return SAT_OK;
}

sat_result_t execute_and_wait(
    sat_cd_block_t* block,
    const Command& command,
    uint16_t wait_flags
) {
    Command response{};
    SAT_TRY(execute(block, command, wait_flags, &response));
    if (wait_flags != 0u) SAT_TRY(wait_hirq(block, wait_flags));
    return SAT_OK;
}

sat_result_t get_ready_sectors(sat_cd_block_t* block, uint16_t* out_count) {
    if (out_count == nullptr) return SAT_ERR_INVALID_ARG;
    Command response{};
    SAT_TRY(wait_hirq(block, SAT_CD_BLOCK_HIRQ_CMOK));
    SAT_TRY(execute(block, {0x5100u, 0u, 0u, 0u}, 0u, &response));
    *out_count = response.cr4;
    return SAT_OK;
}

sat_result_t transfer_words(
    uint8_t* destination,
    uint32_t bytes
) {
    if ((bytes & 1u) != 0u) return SAT_ERR_INVALID_ARG;
#if defined(__sh__) || defined(__SH2__)
    volatile uint16_t* dtr = &reg(SAT_CD_BLOCK_DTR);
#endif
    for (uint32_t i = 0u; i < bytes; i += 2u) {
#if defined(__sh__) || defined(__SH2__)
        const uint16_t word = *dtr;
        destination[i] = static_cast<uint8_t>(word >> 8);
        destination[i + 1u] = static_cast<uint8_t>(word);
#else
        destination[i] = 0u;
        destination[i + 1u] = 0u;
#endif
    }
    return SAT_OK;
}

sat_result_t read_impl(
    sat_cd_block_t* block,
    uint32_t lba,
    uint32_t sector_count,
    uint8_t* destination
) {
    if (block == nullptr || block->initialized == 0u || destination == nullptr ||
        sector_count == 0u || sector_count > 0xFFFFFFu || lba > 0xFFFFFFu - 150u ||
        sector_count > 0xFFFFFFu) return SAT_ERR_INVALID_ARG;

    const uint32_t fad = lba + 150u;
    SAT_TRY(execute_and_wait(block, {0x6000u, 0u, 0u, 0u}, SAT_CD_BLOCK_HIRQ_ESEL));
    SAT_TRY(execute_and_wait(block, {0x4800u, 0u, 0u, 0u}, SAT_CD_BLOCK_HIRQ_ESEL));
    SAT_TRY(execute_and_wait(block, {0x3000u, 0u, 0u, 0u}, SAT_CD_BLOCK_HIRQ_ESEL));
    /* CSCT/PEND are level flags and may still describe the BIOS' last
     * operation.  They must not satisfy the first data-ready poll. */
    write_reg(
        SAT_CD_BLOCK_HIRQ,
        static_cast<uint16_t>(~(SAT_CD_BLOCK_HIRQ_PEND | SAT_CD_BLOCK_HIRQ_CSCT) |
                              SAT_CD_BLOCK_HIRQ_CMOK));
    SAT_TRY(execute_and_wait(
        block,
        {
            static_cast<uint16_t>(0x1080u | ((fad >> 16) & 0xFFu)),
            static_cast<uint16_t>(fad),
            static_cast<uint16_t>(0x0080u | ((sector_count >> 16) & 0xFFu)),
            static_cast<uint16_t>(sector_count)
        },
        0u));

    uint32_t remaining = sector_count;
    while (remaining != 0u) {
        pump_progress(block);
        uint16_t ready = 0u;
        for (;;) {
            SAT_TRY(get_ready_sectors(block, &ready));
            if (ready != 0u) break;
            SAT_TRY(wait_hirq(block, SAT_CD_BLOCK_HIRQ_CSCT));
        }
        const uint32_t take = ready < remaining ? ready : remaining;
        SAT_TRY(execute_and_wait(
            block,
            {0x6300u, 0u, 0u, static_cast<uint16_t>(take)},
            SAT_CD_BLOCK_HIRQ_DRDY));
        SAT_TRY(transfer_words(destination, take * SAT_CD_SECTOR_BYTES));
        SAT_TRY(execute_and_wait(block, {0x0600u, 0u, 0u, 0u}, 0u));
        pump_progress(block);
        destination += take * SAT_CD_SECTOR_BYTES;
        remaining -= take;
    }
    return SAT_OK;
}

sat_result_t read_callback(void* context, uint32_t lba, uint32_t count, void* destination) {
    return sat_cd_block_read_sectors(
        static_cast<sat_cd_block_t*>(context), lba, count, destination);
}

}  // namespace

extern "C" sat_result_t sat_cd_block_init(
    sat_cd_block_t* out_block,
    uint32_t timeout_iterations
) {
    if (out_block == nullptr) return SAT_ERR_INVALID_ARG;
    *out_block = {};
    out_block->timeout_iterations = timeout_iterations == 0u
        ? SAT_CD_BLOCK_DEFAULT_TIMEOUT : timeout_iterations;
#if !defined(__sh__) && !defined(__SH2__)
    return SAT_ERR_UNSUPPORTED;
#else
    Command response{};
    const auto command = [&](Command command, uint16_t flags) -> sat_result_t {
        return execute_and_wait(out_block, command, flags);
    };
    SAT_TRY(command({0x7500u, 0u, 0u, 0u}, SAT_CD_BLOCK_HIRQ_EFLS));
    SAT_TRY(command({0x0400u, 0u, 0u, 0x040Fu}, 0u));
    SAT_TRY(command({0x0600u, 0u, 0u, 0u}, 0u));
    SAT_TRY(command({0x48FCu, 0u, 0u, 0u}, SAT_CD_BLOCK_HIRQ_ESEL));
    (void)response;
    out_block->initialized = 1u;
    return SAT_OK;
#endif
}

extern "C" sat_result_t sat_cd_block_set_progress_service(
    sat_cd_block_t* block, sat_cd_block_progress_fn fn, void* context) {
    if (block == nullptr) return SAT_ERR_INVALID_ARG;
    if (block->progress_active != 0u) return SAT_ERR_BUSY;
    block->progress = fn;
    block->progress_context = fn != nullptr ? context : nullptr;
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_block_read_sectors(
    sat_cd_block_t* block,
    uint32_t lba,
    uint32_t sector_count,
    void* destination
) {
    if (destination == nullptr || sector_count == 0u) return SAT_ERR_INVALID_ARG;
    return read_impl(block, lba, sector_count, static_cast<uint8_t*>(destination));
}

extern "C" sat_result_t sat_cd_block_bind_device(
    sat_cd_block_t* block,
    sat_cd_device_t* out_device,
    uint32_t sector_count
) {
    if (block == nullptr || out_device == nullptr || block->initialized == 0u) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    return sat_cd_device_init(out_device, read_callback, block, sector_count);
}
