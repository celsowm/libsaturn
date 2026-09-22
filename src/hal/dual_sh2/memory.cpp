#include "src/hal/dual_sh2/memory.hpp"

#include "src/hal/dual_sh2/memory_logic.hpp"
#include "src/hal/sh2/cache.hpp"
#include "src/hal/sh2/cpu.hpp"

namespace saturn::hal::dual_sh2::memory {

namespace {

volatile ControlBlock* control_uncached() {
    return reinterpret_cast<volatile ControlBlock*>(
        sh2::cache::cache_through(saturn::core::startup::kDualSh2ControlBase));
}

uint32_t read32(volatile const uint32_t* value) {
    return *value;
}

void write32(volatile uint32_t* value, uint32_t data) {
    *value = data;
}

}  // namespace

uint32_t cached_address(uint32_t address) {
    return memory_logic::cached(address);
}

uint32_t uncached_address(uint32_t address) {
    return memory_logic::uncached(address);
}

bool is_supported(uint32_t address, uint32_t size) {
    if (size == 0u) return false;
    const uint32_t end = address + size;
    if (end < address) return false;
    return address >= saturn::core::startup::kDualSh2ControlBase &&
           end <= saturn::core::startup::kDualSh2ControlEnd;
}

volatile ControlBlock* control() {
    return control_uncached();
}

void reset(uint32_t new_generation) {
    volatile ControlBlock* const block = control_uncached();
    write32(&block->magic, kMagic);
    write32(&block->generation, new_generation);
    write32(&block->status, static_cast<uint32_t>(SharedStatus::Offline));
    write32(&block->shutdown_generation, 0u);
    write32(&block->slave_entry, 0u);
    write32(&block->slave_context, 0u);
    write32(&block->slave_heartbeat, 0u);
    write32(&block->reserved, 0u);
    volatile MailboxSlot* const slots[] = {
        &block->master_to_slave,
        &block->slave_to_master,
    };
    for (volatile MailboxSlot* const target : slots) {
        write32(&target->sequence, 0u);
        write32(&target->acknowledgment, 0u);
        write32(&target->command, 0u);
        write32(&target->argument0, 0u);
        write32(&target->argument1, 0u);
        write32(&target->generation, 0u);
    }
    sh2::compiler_barrier();
}

void set_entry(uint32_t entry, uint32_t context) {
    volatile ControlBlock* const block = control_uncached();
    write32(&block->slave_entry, entry);
    write32(&block->slave_context, context);
    sh2::compiler_barrier();
}

uint32_t generation() {
    return read32(&control_uncached()->generation);
}

SharedStatus status() {
    return static_cast<SharedStatus>(read32(&control_uncached()->status));
}

void set_status(SharedStatus status_value) {
    write32(&control_uncached()->status, static_cast<uint32_t>(status_value));
    sh2::compiler_barrier();
}

void request_shutdown() {
    volatile ControlBlock* const block = control_uncached();
    write32(&block->shutdown_generation, generation());
    sh2::compiler_barrier();
}

bool shutdown_requested() {
    const volatile ControlBlock* const block = control_uncached();
    return read32(&block->shutdown_generation) == read32(&block->generation) &&
           read32(&block->generation) != 0u;
}

void acknowledge_shutdown() {
    write32(&control_uncached()->shutdown_generation, 0u);
    sh2::compiler_barrier();
}

void heartbeat() {
    volatile ControlBlock* const block = control_uncached();
    write32(&block->slave_heartbeat, read32(&block->slave_heartbeat) + 1u);
}

uint32_t heartbeat_value() {
    return read32(&control_uncached()->slave_heartbeat);
}

void write_slave_entry_vector(uint32_t entry) {
    const uint32_t address = sh2::cache::cache_through(
        saturn::core::startup::kSlaveEntryVectorAddress);
    if (address == 0u) return;
    *reinterpret_cast<volatile uint32_t*>(address) = entry;
    sh2::compiler_barrier();
}

}  // namespace saturn::hal::dual_sh2::memory
