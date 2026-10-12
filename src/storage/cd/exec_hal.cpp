#include "src/storage/cd/exec_hal.hpp"

#include "src/hal/cd/block.hpp"
#include "src/hal/scu/dsp.hpp"
#include "src/hal/scu/irq.hpp"
#include "src/hal/sh2/cpu.hpp"
#include "src/hal/smpc/smpc.hpp"

/* The copy routine sat_exec runs from the top of Work RAM Low once the old
 * image may be overwritten: it can call nothing, so it is position-independent
 * assembly that is copied there and entered through the cache-through mirror
 * (the manual wants CCR written from there).
 *   r4 = staged image (cache-through), r5 = SAT_EXEC_LOAD_ADDRESS (cache-
 *   through), r6 = longwords, r7 = entry
 * It copies, purges the cache (CP first, then the mode it had, as
 * sh2::cache::purge_all), sets the stack crt0 also sets and jumps. */
asm(R"(
    .section .text.saturn_exec_trampoline,"ax",@progbits
    .align 2
    .global _saturn_exec_trampoline
    .global _saturn_exec_trampoline_end
_saturn_exec_trampoline:
0:  mov.l   @r4+, r0
    dt      r6
    mov.l   r0, @r5
    bf/s    0b
    add     #4, r5
    mov.l   1f, r1
    mov.b   @r1, r2
    mov     #0x10, r0
    mov.b   r0, @r1
    mov     r2, r0
    and     #0x09, r0
    mov.b   r0, @r1
    mov.l   2f, r15
    jmp     @r7
    nop
    .align 2
1:  .long   0xFFFFFE92
2:  .long   0x060FFFFC
_saturn_exec_trampoline_end:
    .previous
)");

extern "C" const uint8_t saturn_exec_trampoline[];
extern "C" const uint8_t saturn_exec_trampoline_end[];

namespace saturn::storage::exec {

namespace {

template <typename T>
volatile T& reg(uint32_t address) {
    return *reinterpret_cast<volatile T*>(static_cast<uintptr_t>(address));
}

/* SCU (SCU manual 6.1): interrupt mask/status, DMA enables and forced stop. */
constexpr uint32_t kScuDmaEnable[3] = {0x25FE0010u, 0x25FE0030u, 0x25FE0050u};
constexpr uint32_t kScuDmaForcedStop = 0x25FE0060u;
constexpr uint32_t kScuIms = 0x25FE00A0u;
constexpr uint32_t kScuIst = 0x25FE00A4u;
/* SCSP: 32 slots of 0x20 bytes; word 0 holds KYONEX (bit 12) and KYONB (bit 11).
 * The common control word keeps MEM4MB (bit 9) and takes MVOL 0. */
constexpr uint32_t kScspSlots = 0x25B00000u;
constexpr uint32_t kScspCommon = 0x25B00400u;
constexpr uint16_t kScspKeyOnExecute = 0x1000u;
constexpr uint16_t kScspMem4Mb = 0x0200u;
/* VDP1 PTMR = 0: no more plotting. VDP2 0x00E..0x11E are the writable
 * registers after TVMD, EXTEN, TVSTAT, VRSIZE and the counters. */
constexpr uint32_t kVdp1Ptmr = 0x25D00004u;
constexpr uint32_t kVdp2Base = 0x25F80000u;
constexpr uint32_t kVdp2FirstCleared = 0x00Eu;
constexpr uint32_t kVdp2End = 0x120u;

}  // namespace

void* staging() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(logic::kWorkRamLowBase | logic::kCacheThroughBit));
}

logic::Handoff* handoff() {
    return reinterpret_cast<logic::Handoff*>(
        static_cast<uintptr_t>(logic::kHandoffBase | logic::kCacheThroughBit));
}

sat_cd_block_t* bound_block(const sat_cd_device_t* device) {
    return saturn::hal::cd::bound_block(device);
}

void stop_machine() {
    namespace sh2 = saturn::hal::sh2;
    (void)sh2::save_and_mask_interrupts();
    /* The BIOS vectors and on-chip priorities come back; then mask again. */
    if (saturn::hal::scu::irq::live()) saturn::hal::scu::irq::stop();
    sh2::set_interrupt_mask(15u);
    reg<uint32_t>(kScuIms) = 0x0000BFFFu;
    reg<uint32_t>(kScuIst) = 0u;

    (void)saturn::hal::smpc::slave_off();

    reg<uint32_t>(kScuDmaForcedStop) = 1u;
    for (uint32_t address : kScuDmaEnable) reg<uint32_t>(address) = 0u;
    reg<uint32_t>(kScuDmaForcedStop) = 0u;
    saturn::hal::scu::dsp::stop();

    for (uint32_t slot = 0; slot < 32u; ++slot) reg<uint16_t>(kScspSlots + slot * 0x20u) = 0u;
    reg<uint16_t>(kScspSlots) = kScspKeyOnExecute;
    reg<uint16_t>(kScspCommon) = kScspMem4Mb;
    (void)saturn::hal::smpc::sound_off();

    reg<uint16_t>(kVdp1Ptmr) = 0u;
    for (uint32_t offset = kVdp2FirstCleared; offset < kVdp2End; offset += 2u) {
        reg<uint16_t>(kVdp2Base + offset) = 0u;
    }
    reg<uint16_t>(kVdp2Base) = 0u;  /* TVMD: display off */
}

void start_image(uint32_t bytes) {
    const uint32_t routine = static_cast<uint32_t>(saturn_exec_trampoline_end - saturn_exec_trampoline);
    uint8_t* const at = reinterpret_cast<uint8_t*>(
        static_cast<uintptr_t>(logic::kTrampolineBase | logic::kCacheThroughBit));
    for (uint32_t i = 0; i < routine && i < logic::kTrampolineMax; ++i) at[i] = saturn_exec_trampoline[i];
    using Routine = void (*)(const void*, void*, uint32_t, uint32_t);
    const Routine run = reinterpret_cast<Routine>(at);
    run(staging(),
        reinterpret_cast<void*>(static_cast<uintptr_t>(SAT_EXEC_LOAD_ADDRESS | logic::kCacheThroughBit)),
        (bytes + 3u) / 4u, SAT_EXEC_LOAD_ADDRESS);
    for (;;) {
    }
}

void reset_machine() {
    saturn::hal::smpc::system_reset();
}

}  // namespace saturn::storage::exec
