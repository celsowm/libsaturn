#include <cstdio>
#include <cstdlib>

#include "src/core/startup/memory_layout.hpp"
#include "src/hal/dual_sh2/memory_logic.hpp"
#include "src/hal/dual_sh2/protocol.hpp"

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main() {
    using namespace saturn;
    using namespace saturn::hal::dual_sh2;

    CHECK(memory_logic::uncached(0x06002000u) == 0x26002000u);
    CHECK(memory_logic::cached(0x26002000u) == 0x06002000u);
    CHECK(memory_logic::uncached(0x00200000u) == 0x20200000u);
    CHECK(memory_logic::cached(0x20200000u) == 0x00200000u);
    CHECK(memory_logic::uncached(0x05F80000u) == 0u);
    CHECK(!memory_logic::is_work_ram(0x060FFFFFu, 2u));
    CHECK(memory_logic::is_work_ram(saturn::core::startup::kDualSh2ControlBase, 0x2000u));

    CHECK(protocol::next_sequence(0u) == 1u);
    CHECK(protocol::next_sequence(0xFFFFFFFFu) == 1u);
    CHECK(protocol::has_message(7u, 6u));
    CHECK(!protocol::has_message(7u, 7u));
    CHECK(!protocol::has_message(0u, 0u));

    std::puts("PASS: test_dual_sh2_protocol.cpp");
    return 0;
}
