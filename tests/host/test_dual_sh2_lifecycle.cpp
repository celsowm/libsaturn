#include <cstdio>

#include "src/hal/dual_sh2/lifecycle_logic.hpp"

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

int main() {
    using namespace saturn::hal::dual_sh2::lifecycle_logic;
    CHECK(transition(State::Offline, Event::StartIssued) == State::Starting);
    CHECK(transition(State::Starting, Event::StartAcknowledged) == State::Ready);
    CHECK(transition(State::Ready, Event::StopRequested) == State::Stopping);
    CHECK(transition(State::Stopping, Event::StopAcknowledged) == State::Offline);
    CHECK(transition(State::Starting, Event::Timeout) == State::Fault);
    CHECK(transition(State::Fault, Event::Reset) == State::Offline);
    CHECK(transition(State::Ready, Event::StartIssued) == State::Fault);
    CHECK(timeout_elapsed(0xFFF0u, 0x0005u, 0x0015u));
    CHECK(!timeout_elapsed(0x1000u, 0x1005u, 0x0010u));
    std::puts("PASS: test_dual_sh2_lifecycle.cpp");
    return 0;
}
