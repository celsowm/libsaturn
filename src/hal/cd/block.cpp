#include "saturn/cd_block.h"

#include <stddef.h>

#include "src/hal/cd/block.hpp"
#include "src/hal/cd/progress.hpp"

namespace {

struct Command {
    uint16_t cr1;
    uint16_t cr2;
    uint16_t cr3;
    uint16_t cr4;
};

using CdWord = uint16_t __attribute__((may_alias));

#if defined(SAT_CD_BLOCK_TEST_BUS)
uint16_t read_reg(uint32_t address) { return sat_cd_test_read(address); }
void write_reg(uint32_t address, uint16_t value) { sat_cd_test_write(address, value); }
#elif defined(__sh__) || defined(__SH2__)
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

using saturn::hal::cd::pump_progress;

uint32_t now_ms(const sat_cd_block_t* block) {
    return block->timing.now_ms ? block->timing.now_ms(block->timing.clock_context) : 0u;
}

void begin_operation(sat_cd_block_t* block) {
    const uint32_t timeouts = block->diagnostics.timeout_count;
    block->diagnostics = {};
    block->diagnostics.timeout_count = timeouts;
    block->operation_start_ms = now_ms(block);
}

sat_result_t snapshot(sat_cd_block_t* block, sat_result_t result) {
    auto& d = block->diagnostics;
    d.elapsed_ms = now_ms(block) - block->operation_start_ms;
    d.hirq = read_reg(SAT_CD_BLOCK_HIRQ);
    d.response[0] = read_reg(SAT_CD_BLOCK_CR1);
    d.response[1] = read_reg(SAT_CD_BLOCK_CR2);
    d.response[2] = read_reg(SAT_CD_BLOCK_CR3);
    d.response[3] = read_reg(SAT_CD_BLOCK_CR4);
    if (result == SAT_ERR_TIMEOUT && d.result != SAT_ERR_TIMEOUT) ++d.timeout_count;
    d.result = result;
    return result;
}

bool operation_expired(sat_cd_block_t* block, uint32_t now) {
    return block->timing.now_ms &&
        now - block->operation_start_ms >= block->timing.operation_timeout_ms;
}

sat_result_t wait_hirq(sat_cd_block_t* block, uint16_t mask) {
    block->diagnostics.wait_mask = mask;
    const uint32_t begin = now_ms(block);
    for (uint32_t i = 0u;; ++i) {
        if (block->timing.now_ms) {
            const uint32_t now = now_ms(block);
            if (now - begin >= block->timing.command_timeout_ms || operation_expired(block, now))
                return snapshot(block, SAT_ERR_TIMEOUT);
        } else if (i >= timeout_for(block)) return snapshot(block, SAT_ERR_TIMEOUT);
        if ((read_reg(SAT_CD_BLOCK_HIRQ) & mask) != 0u) return SAT_OK;
        if ((i & 0x0FFFu) == 0u) pump_progress(block);
    }
}

void remember_command(sat_cd_block_t* block, const Command& command) {
    block->diagnostics.response_valid = 0u;
    block->diagnostics.command[0] = command.cr1;
    block->diagnostics.command[1] = command.cr2;
    block->diagnostics.command[2] = command.cr3;
    block->diagnostics.command[3] = command.cr4;
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
    remember_command(block, command);
    if (operation_expired(block, now_ms(block))) return snapshot(block, SAT_ERR_TIMEOUT);
    const uint16_t hirq = read_reg(SAT_CD_BLOCK_HIRQ);
    if ((hirq & SAT_CD_BLOCK_HIRQ_CMOK) == 0u) return snapshot(block, SAT_ERR_BUSY);
    write_reg(
        SAT_CD_BLOCK_HIRQ,
        static_cast<uint16_t>(~(wait_flags | SAT_CD_BLOCK_HIRQ_CMOK)));
    write_command(command);
    SAT_TRY(wait_hirq(block, SAT_CD_BLOCK_HIRQ_CMOK));
    *out_response = read_command();
    block->diagnostics.response_valid = 1u;
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
    if ((reinterpret_cast<uintptr_t>(destination) & 1u) == 0u) {
        // The SH-2 is big-endian, as are the CD Block's data words. Store
        // each word directly instead of splitting it into two byte writes.
        CdWord* output = reinterpret_cast<CdWord*>(destination);
        for (uint32_t i = 0u; i < bytes / 2u; ++i) output[i] = *dtr;
        return SAT_OK;
    }
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

/* Resets the selectors and starts the drive on `sector_count` sectors from `lba`. */
sat_result_t start_play(sat_cd_block_t* block, uint32_t lba, uint32_t sector_count) {
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
    return execute_and_wait(
        block,
        {
            static_cast<uint16_t>(0x1080u | ((fad >> 16) & 0xFFu)),
            static_cast<uint16_t>(fad),
            static_cast<uint16_t>(0x0080u | ((sector_count >> 16) & 0xFFu)),
            static_cast<uint16_t>(sector_count)
        },
        0u);
}

/* Moves `take` buffered sectors to `destination` and frees them in the drive. */
sat_result_t take_sectors(sat_cd_block_t* block, uint8_t* destination, uint32_t take) {
    SAT_TRY(execute_and_wait(
        block,
        {0x6300u, 0u, 0u, static_cast<uint16_t>(take)},
        SAT_CD_BLOCK_HIRQ_DRDY));
    SAT_TRY(transfer_words(destination, take * SAT_CD_SECTOR_BYTES));
    block->diagnostics.bytes_transferred += take * SAT_CD_SECTOR_BYTES;
    return execute_and_wait(block, {0x0600u, 0u, 0u, 0u}, 0u);
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

    SAT_TRY(start_play(block, lba, sector_count));

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
        SAT_TRY(take_sectors(block, destination, take));
        pump_progress(block);
        destination += take * SAT_CD_SECTOR_BYTES;
        remaining -= take;
        // The drive keeps filling the buffer while the observer works on this batch.
        sat_cd_notify_arrival(destination);
    }
    return SAT_OK;
}

void close_stream(sat_cd_block_t* block) {
    block->stream_active = 0u;
    block->stream_remaining = 0u;
    block->stream_destination = nullptr;
}

/* One step of an open stream: moves what has arrived, at most `limit` sectors
 * (0 = no limit); with `wait`, until something has. */
sat_result_t stream_step(sat_cd_block_t* block, uint32_t limit, bool wait) {
    uint16_t ready = 0u;
    for (;;) {
        SAT_TRY(get_ready_sectors(block, &ready));
        if (ready != 0u || !wait) break;
        SAT_TRY(wait_hirq(block, SAT_CD_BLOCK_HIRQ_CSCT));
    }
    uint32_t take = ready < block->stream_remaining ? ready : block->stream_remaining;
    if (limit != 0u && take > limit) take = limit;
    if (take == 0u) return SAT_OK;
    SAT_TRY(take_sectors(block, block->stream_destination, take));
    block->stream_destination += take * SAT_CD_SECTOR_BYTES;
    block->stream_remaining -= take;
    if (block->stream_remaining == 0u) close_stream(block);
    return SAT_OK;
}

sat_result_t read_callback(void* context, uint32_t lba, uint32_t count, void* destination) {
    return sat_cd_block_read_sectors(
        static_cast<sat_cd_block_t*>(context), lba, count, destination);
}

}  // namespace

namespace {
sat_result_t initialize(sat_cd_block_t* out_block) {
#if !defined(__sh__) && !defined(__SH2__) && !defined(SAT_CD_BLOCK_TEST_BUS)
    (void)out_block;
    return SAT_ERR_UNSUPPORTED;
#else
    begin_operation(out_block);
    const Command commands[] = {{0x7500u,0u,0u,0u},{0x0400u,0u,0u,0x040fu},
                                 {0x0600u,0u,0u,0u},{0x48fcu,0u,0u,0u}};
    for (const Command command : commands) {
        const uint16_t flags = command.cr1 == 0x7500u ? SAT_CD_BLOCK_HIRQ_EFLS :
            (command.cr1 == 0x48fcu ? SAT_CD_BLOCK_HIRQ_ESEL : 0u);
        const sat_result_t result = execute_and_wait(out_block,command,flags);
        if (result != SAT_OK) return snapshot(out_block,result);
    }
    out_block->initialized = 1u;
    return snapshot(out_block,SAT_OK);
#endif
}
}

extern "C" sat_result_t sat_cd_block_init(sat_cd_block_t* out_block, uint32_t iterations) {
    if (!out_block) return SAT_ERR_INVALID_ARG;
    *out_block = {};
    out_block->timeout_iterations = iterations ? iterations : SAT_CD_BLOCK_DEFAULT_TIMEOUT;
    return initialize(out_block);
}

extern "C" sat_result_t sat_cd_block_init_ex(sat_cd_block_t* out_block, const sat_cd_block_config_t* config) {
    if (!out_block || !config || !config->now_ms || !config->command_timeout_ms ||
        !config->operation_timeout_ms || config->command_timeout_ms > 0x7fffffffu ||
        config->operation_timeout_ms > 0x7fffffffu) return SAT_ERR_INVALID_ARG;
    const sat_cd_block_config_t saved = *config;
    *out_block = {};
    out_block->timing = saved;
    return initialize(out_block);
}

extern "C" sat_result_t sat_cd_block_get_diagnostics(
    const sat_cd_block_t* block, sat_cd_block_diagnostics_t* out) {
    if (!block || !out) return SAT_ERR_INVALID_ARG;
    *out = block->diagnostics;
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_block_command(
    sat_cd_block_t* block, const uint16_t command[4], uint16_t response[4]
) {
    if (block == nullptr || block->initialized == 0u || command == nullptr || response == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (block->read_active || block->stream_active || block->progress_active) return SAT_ERR_BUSY;
    begin_operation(block);
    remember_command(block, {command[0],command[1],command[2],command[3]});
    if ((read_reg(SAT_CD_BLOCK_HIRQ) & SAT_CD_BLOCK_HIRQ_CMOK) == 0u) return snapshot(block,SAT_ERR_BUSY);
    write_reg(SAT_CD_BLOCK_HIRQ, static_cast<uint16_t>(~SAT_CD_BLOCK_HIRQ_CMOK));
    write_command({command[0], command[1], command[2], command[3]});
    SAT_TRY(wait_hirq(block, SAT_CD_BLOCK_HIRQ_CMOK));
    const Command answer = read_command();
    response[0] = answer.cr1;
    response[1] = answer.cr2;
    response[2] = answer.cr3;
    response[3] = answer.cr4;
    block->diagnostics.response_valid = 1u;
    return snapshot(block,SAT_OK);
}

extern "C" sat_result_t sat_cd_block_set_progress_service(
    sat_cd_block_t* block, sat_cd_block_progress_fn fn, void* context) {
    if (block == nullptr) return SAT_ERR_INVALID_ARG;
    if (block->progress_active != 0u || block->read_active != 0u)
        return SAT_ERR_BUSY;
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
    if (destination == nullptr || sector_count == 0u)
        return SAT_ERR_INVALID_ARG;
    if (block == nullptr || block->initialized == 0u)
        return SAT_ERR_INVALID_ARG;
    if (block->read_active != 0u) return SAT_ERR_BUSY;
    if (block->stream_active != 0u) SAT_TRY(sat_cd_stream_finish(block));
    begin_operation(block);
    block->diagnostics.lba = lba;
    if (sector_count > 0xffffffu) return SAT_ERR_INVALID_ARG;
    block->diagnostics.bytes_requested = sector_count * SAT_CD_SECTOR_BYTES;
    block->read_active = 1u;
    const sat_result_t status = read_impl(
        block, lba, sector_count, static_cast<uint8_t*>(destination));
    block->read_active = 0u;
    if (status == SAT_ERR_TIMEOUT) return status; /* preserve first failure snapshot */
    return snapshot(block,status);
}

extern "C" sat_result_t sat_cd_stream_begin(
    sat_cd_block_t* block, uint32_t lba, uint32_t sector_count, void* destination) {
    if (block == nullptr || block->initialized == 0u || destination == nullptr ||
        sector_count == 0u || sector_count > SAT_CD_STREAM_MAX_SECTORS ||
        lba > 0xFFFFFFu - 150u || (reinterpret_cast<uintptr_t>(destination) & 1u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (block->read_active != 0u || block->stream_active != 0u) return SAT_ERR_BUSY;
    begin_operation(block);
    block->diagnostics.lba = lba;
    block->diagnostics.bytes_requested = sector_count * SAT_CD_SECTOR_BYTES;
    block->read_active = 1u;  // the setup commands are a read: no nesting
    const sat_result_t status = start_play(block, lba, sector_count);
    block->read_active = 0u;
    SAT_TRY(status);
    block->stream_active = 1u;
    block->stream_remaining = sector_count;
    block->stream_destination = static_cast<uint8_t*>(destination);
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_stream_poll(
    sat_cd_block_t* block, uint32_t max_sectors, uint32_t* out_remaining) {
    if (block == nullptr) return SAT_ERR_INVALID_ARG;
    sat_result_t status = SAT_OK;
    if (block->stream_active != 0u) {
        if (block->read_active != 0u) return SAT_ERR_BUSY;
        block->read_active = 1u;
        status = stream_step(block, max_sectors, false);
        block->read_active = 0u;
        if (status != SAT_OK) close_stream(block);
    }
    if (out_remaining != nullptr) *out_remaining = block->stream_remaining;
    return status;
}

extern "C" sat_result_t sat_cd_stream_finish(sat_cd_block_t* block) {
    if (block == nullptr) return SAT_ERR_INVALID_ARG;
    if (block->stream_active == 0u) return SAT_OK;
    if (block->read_active != 0u) return SAT_ERR_BUSY;
    block->read_active = 1u;
    sat_result_t status = SAT_OK;
    while (block->stream_active != 0u && status == SAT_OK) {
        pump_progress(block);
        status = stream_step(block, 0u, true);
    }
    block->read_active = 0u;
    if (status != SAT_OK) close_stream(block);
    return status;
}

extern "C" uint32_t sat_cd_stream_remaining(const sat_cd_block_t* block) {
    return block != nullptr && block->stream_active != 0u ? block->stream_remaining : 0u;
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

namespace saturn::hal::cd {

sat_cd_block_t* bound_block(const sat_cd_device_t* device) {
    if (device == nullptr || device->read_sectors != read_callback) return nullptr;
    return static_cast<sat_cd_block_t*>(device->context);
}

}  // namespace saturn::hal::cd
