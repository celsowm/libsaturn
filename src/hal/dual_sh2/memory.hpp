#ifndef SATURN_HAL_DUAL_SH2_MEMORY_HPP
#define SATURN_HAL_DUAL_SH2_MEMORY_HPP

#include <stdint.h>

#include "src/core/startup/memory_layout.hpp"

namespace saturn::hal::dual_sh2::memory {

constexpr uint32_t kMagic = 0x44534832u; /* "DSH2" */

enum class SharedStatus : uint32_t {
    Offline = 0u,
    Starting = 1u,
    Ready = 2u,
    Stopping = 3u,
    Fault = 4u,
};

struct MailboxSlot {
    uint32_t sequence;
    uint32_t acknowledgment;
    uint32_t command;
    uint32_t argument0;
    uint32_t argument1;
    uint32_t generation;
};

struct ControlBlock {
    uint32_t magic;
    uint32_t generation;
    uint32_t status;
    uint32_t shutdown_generation;
    uint32_t slave_entry;
    uint32_t slave_context;
    uint32_t slave_heartbeat;
    uint32_t reserved;
    MailboxSlot master_to_slave;
    MailboxSlot slave_to_master;
};

static_assert(sizeof(MailboxSlot) == 24u);
static_assert(sizeof(ControlBlock) == 80u);
static_assert(alignof(ControlBlock) >= 4u);

uint32_t cached_address(uint32_t address);
uint32_t uncached_address(uint32_t address);
bool is_supported(uint32_t address, uint32_t size = 1u);

volatile ControlBlock* control();
void reset(uint32_t generation);
void set_entry(uint32_t entry, uint32_t context);
uint32_t generation();
SharedStatus status();
void set_status(SharedStatus status_value);
void request_shutdown();
bool shutdown_requested();
void acknowledge_shutdown();
void heartbeat();
uint32_t heartbeat_value();
void write_slave_entry_vector(uint32_t entry);

}  // namespace saturn::hal::dual_sh2::memory

#endif
