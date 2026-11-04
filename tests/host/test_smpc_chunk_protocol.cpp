#include <cassert>
#include <cstdint>
#include <cstring>

/* Model the registers around the real SMPC HAL. This controls delayed chunks
 * and ignored CONTINUE requests; it is not a model of physical timings. */
namespace fake {
uint8_t input[7], output[32], sr, sf, iosel;
uint32_t ist;
uint8_t stream[96];
unsigned bytes, offset, countdown, requests, writes, conflicting_breaks;
bool active, ignore_continue, suppress_interrupt, stale_on_continue, immediate_continue;

void complete_chunk() {
    const unsigned n = bytes - offset > 32u ? 32u : bytes - offset;
    std::memset(output, 0xFF, sizeof output);
    std::memcpy(output, stream + offset, n);
    offset += n;
    /* PDL is set on the first chunk, NPE while more data remains. */
    sr = static_cast<uint8_t>(0x80u | (offset == n ? 0x40u : 0u) |
                              (offset < bytes ? 0x20u : 0u));
    sf = 0u;
    if (!suppress_interrupt) ist |= 0x80u;
}
struct Reg8 {
    enum Kind { Input, Output, Sr, Sf, Command, Iosel } kind;
    unsigned index = 0;
    operator uint8_t() const {
        if (kind == Sf && countdown && --countdown == 0u) complete_chunk();
        switch (kind) {
            case Input: return input[index];
            case Output: return output[index];
            case Sr: return sr;
            case Sf: return sf;
            case Iosel: return iosel;
            default: return 0u;
        }
    }
    Reg8& operator=(uint8_t v) {
        if (kind == Sf) sf = v;
        if (kind == Iosel) iosel = v;
        if (kind == Command && v == 0x10u) {
            active = true;
            offset = 0u;
            countdown = 3u;
        }
        if (kind == Input) {
            const uint8_t old = input[index];
            input[index] = v;
            if (index == 0u && active && (v & 0x40u)) {
                if (((old ^ v) & 0x80u) != 0u) ++conflicting_breaks;
                active = false;
            }
            else if (index == 0u && active) {
                ++requests;
                if (!ignore_continue) {
                    sf = 1u;
                    if (immediate_continue) complete_chunk();
                    else countdown = 3u;
                }
                else if (stale_on_continue) ist |= 0x80u;
            }
        }
        return *this;
    }
};
struct Ist {
    operator uint32_t() const { return ist; }
    Ist& operator=(uint32_t v) { ++writes; ist &= v; return *this; }
};
Reg8 in(unsigned i) { return {Reg8::Input, i}; }
Reg8 out(unsigned i) { return {Reg8::Output, i}; }
void reset(bool ignore) {
    std::memset(input, 0, sizeof input);
    std::memset(output, 0, sizeof output);
    sr = sf = iosel = 0u;
    ist = 1u << 4u; /* unrelated pending event */
    bytes = offset = countdown = requests = writes = conflicting_breaks = 0u;
    active = false;
    ignore_continue = ignore;
    suppress_interrupt = false;
    stale_on_continue = false;
    immediate_continue = false;
    for (unsigned port = 0; port < 2; ++port) {
        stream[bytes++] = 0x16u;
        for (unsigned tap = 0; tap < 6; ++tap) {
            stream[bytes++] = 0x16u;
            stream[bytes++] = 0xFFu;
            stream[bytes++] = 0xFFu;
            stream[bytes++] = static_cast<uint8_t>(0x40u + port * 6u + tap);
            stream[bytes++] = 0x80u;
            stream[bytes++] = 0u;
            stream[bytes++] = 0u;
        }
    }
}
}

#define SMPC_IREG(n) fake::in(n)
#define SMPC_OREG(n) fake::out(n)
#define SMPC_SR fake::Reg8{fake::Reg8::Sr}
#define SMPC_SF fake::Reg8{fake::Reg8::Sf}
#define SMPC_COMREG fake::Reg8{fake::Reg8::Command}
#define SMPC_IOSEL1 fake::Reg8{fake::Reg8::Iosel}
#define SMPC_OREG31 fake::out(31)
#define SCU_IST fake::Ist{}
#include "src/hal/smpc/smpc.cpp"

int main() {
    using namespace saturn::hal::smpc;
    fake::reset(false);
    PeripheralSnapshot snapshot{};
    assert(read_peripherals(&snapshot));
    assert(fake::requests == 2u);
    assert(snapshot.ports[0].tap_count == 6u && snapshot.ports[1].tap_count == 6u);
    assert(snapshot.ports[1].devices[5].data[2] == 0x4Bu);
    assert((fake::ist & (1u << 4u)) != 0u);

    /* The busy interval may finish before the first poll. A replacement
     * must not depend on observing the intermediate SF=1 state. */
    fake::reset(false);
    fake::immediate_continue = true;
    assert(read_peripherals(&snapshot));
    assert(fake::requests == 2u);
    assert(snapshot.ports[1].devices[5].data[2] == 0x4Bu);

    fake::reset(true);
    fake::stale_on_continue = true; /* event arrives after IST was cleared */
    assert(!read_peripherals(&snapshot));
    assert(fake::requests == 1u);
    assert(fake::conflicting_breaks == 0u);

    fake::reset(true);
    fake::ist |= 0x80u; /* An old SMPC event must not validate CONTINUE. */
    assert(!read_peripherals(&snapshot));
    assert(fake::requests == 1u);
    assert((fake::ist & (1u << 4u)) != 0u);

    fake::reset(false);
    fake::suppress_interrupt = true;
    assert(!read_peripherals(&snapshot));
    assert(fake::requests == 1u);

    fake::reset(false);
    assert(read_peripherals(&snapshot)); /* recovery after a timed-out request */
    assert(fake::requests == 2u);

}
