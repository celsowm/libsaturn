#include <cstdio>

#include "src/hal/dual_sh2/protocol.hpp"

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

struct Slot {
    uint32_t sequence = 0u;
    uint32_t acknowledgment = 0u;
    uint32_t command = 0u;
    uint32_t argument0 = 0u;
    uint32_t argument1 = 0u;
    uint32_t generation = 0u;
};

int main() {
    using saturn::hal::dual_sh2::protocol::Message;
    Slot slot;
    Message message{};
    CHECK(!saturn::hal::dual_sh2::protocol::consume_slot(slot, 7u, &message));
    CHECK(saturn::hal::dual_sh2::protocol::publish_slot(slot, 7u, 11u, 100u, 200u));
    CHECK(!saturn::hal::dual_sh2::protocol::publish_slot(slot, 7u, 12u, 0u, 0u));
    CHECK(saturn::hal::dual_sh2::protocol::consume_slot(slot, 7u, &message));
    CHECK(message.sequence == 1u && message.command == 11u &&
          message.argument0 == 100u && message.argument1 == 200u);
    CHECK(!saturn::hal::dual_sh2::protocol::consume_slot(slot, 7u, &message));

    slot.sequence = 4u;
    slot.acknowledgment = 3u;
    slot.generation = 6u;
    CHECK(!saturn::hal::dual_sh2::protocol::consume_slot(slot, 7u, &message));
    CHECK(slot.acknowledgment == 4u);

    slot.sequence = 0xFFFFFFFFu;
    slot.acknowledgment = 0xFFFFFFFFu;
    CHECK(saturn::hal::dual_sh2::protocol::publish_slot(slot, 7u, 1u, 0u, 0u));
    CHECK(slot.sequence == 1u);
    std::puts("PASS: test_dual_sh2_mailbox.cpp");
    return 0;
}
