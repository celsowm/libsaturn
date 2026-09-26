#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "saturn/mpeg.h"
#include "src/hal/cd/mpeg_logic.hpp"

// A stub CD Block that records the commands it was sent and answers like the Ymir core.
namespace {
struct FakeBlock {
    uint8_t mpeg_version = 0u;
    uint8_t auth_status = 0u;
    uint32_t polls_until_authenticated = 0u;   // Is Device Authenticated answers 0 this many times
    bool never_authenticates = false;
    bool init_rejects = false;
    bool fail_next = false;
    bool authenticate_requested = false;
    uint16_t log[256][4] = {};
    uint32_t commands = 0u;
} g_fake;
}  // namespace

extern "C" sat_result_t sat_cd_block_command(sat_cd_block_t*, const uint16_t command[4], uint16_t response[4]) {
    if (g_fake.fail_next) {
        g_fake.fail_next = false;
        return SAT_ERR_BUSY;
    }
    std::memcpy(g_fake.log[g_fake.commands++], command, 8u);
    std::memset(response, 0, 8u);
    switch (command[0] >> 8u) {
    case 0x01:  // hardware info: flags 0, hardware version 2, MPEG version, drive 6.0
        response[0] = 0x0100u;
        response[1] = 0x0002u;
        response[2] = g_fake.mpeg_version;
        response[3] = 0x0600u;
        break;
    case 0xE0:
        if (command[1] == 1u) g_fake.authenticate_requested = true;
        break;
    case 0xE1:
        if (g_fake.authenticate_requested && !g_fake.never_authenticates) {
            if (g_fake.polls_until_authenticated != 0u) --g_fake.polls_until_authenticated;
            else g_fake.auth_status = 2u;
        }
        response[1] = g_fake.auth_status;
        break;
    case 0x93:
        response[0] = g_fake.init_rejects ? 0xFF00u : 0x0100u;
        break;
    }
    return SAT_OK;
}

static void reset() {
    g_fake = FakeBlock{};
}

int main() {
    sat_cd_block_t block = {};
    block.initialized = 1u;
    sat_mpeg_info_t info = {};

    // decoding the answer
    using namespace saturn::hal::cd::mpeg_logic;
    const uint16_t sample[4] = {0x0100u, 0x0A02u, 0x0007u, 0x0600u};
    const HardwareInfo decoded = decode_hardware_info(sample);
    assert(decoded.hardware_flags == 0x0Au && decoded.hardware_version == 2u && decoded.mpeg_version == 7u);
    assert(decoded.drive_version == 6u && decoded.drive_revision == 0u && card_present(decoded));
    const std::array<uint16_t, 4> none = {};
    assert(!card_present(decode_hardware_info(none.data())));

    // no card (both emulators): the probe says so and start sends nothing beyond the two queries
    reset();
    assert(sat_mpeg_probe(&block, &info) == SAT_OK);
    assert(info.present == 0u && info.mpeg_version == 0u && info.authenticated == 0u);
    assert(info.hardware_version == 2u && info.drive_version == 6u && info.drive_revision == 0u);
    assert(g_fake.commands == 2u && (g_fake.log[0][0] >> 8u) == 0x01u && (g_fake.log[1][0] >> 8u) == 0xE1u);
    assert(g_fake.log[1][1] == 1u);     // asks about the MPEG device, not the disc
    const uint32_t before = g_fake.commands;
    assert(sat_mpeg_start(&block) == SAT_ERR_NOT_CONNECTED);
    assert(g_fake.commands == before + 2u);   // only the probe

    // a card that is not authenticated yet: authenticate (MPEG), poll, init
    reset();
    g_fake.mpeg_version = 5u;
    g_fake.polls_until_authenticated = 2u;
    assert(sat_mpeg_start(&block) == SAT_OK);
    bool saw_auth = false, saw_init = false;
    for (uint32_t i = 0u; i < g_fake.commands; ++i) {
        if ((g_fake.log[i][0] >> 8u) == 0xE0u) {
            saw_auth = true;
            assert(g_fake.log[i][1] == 1u && !saw_init);
        }
        if ((g_fake.log[i][0] >> 8u) == 0x93u) {
            saw_init = true;
            assert(saw_auth);       // never initialised before it was authenticated
        }
    }
    assert(saw_auth && saw_init);

    // already authenticated: no second authentication
    reset();
    g_fake.mpeg_version = 5u;
    g_fake.auth_status = 2u;
    assert(sat_mpeg_probe(&block, &info) == SAT_OK && info.present == 1u && info.authenticated == 1u);
    assert(info.mpeg_version == 5u && info.authentication_status == 2u);
    g_fake.commands = 0u;
    assert(sat_mpeg_start(&block) == SAT_OK);
    for (uint32_t i = 0u; i < g_fake.commands; ++i) assert((g_fake.log[i][0] >> 8u) != 0xE0u);

    // authentication that never completes times out; init that is refused is reported
    reset();
    g_fake.mpeg_version = 5u;
    g_fake.never_authenticates = true;
    assert(sat_mpeg_start(&block) == SAT_ERR_TIMEOUT);
    reset();
    g_fake.mpeg_version = 5u;
    g_fake.auth_status = 2u;
    g_fake.init_rejects = true;
    assert(sat_mpeg_start(&block) == SAT_ERR_VERIFY_FAILED);

    // transport errors and bad arguments pass through
    reset();
    g_fake.fail_next = true;
    assert(sat_mpeg_probe(&block, &info) == SAT_ERR_BUSY);
    assert(sat_mpeg_probe(nullptr, &info) == SAT_ERR_INVALID_ARG);
    assert(sat_mpeg_probe(&block, nullptr) == SAT_ERR_INVALID_ARG);

    std::puts("PASS: test_mpeg_api.cpp");
    return 0;
}
