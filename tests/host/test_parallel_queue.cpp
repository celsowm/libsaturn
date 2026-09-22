#include <cstdio>

#include "src/core/parallel/queue_logic.hpp"

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

int main() {
    using namespace saturn::core::parallel::queue_logic;
    sat_parallel_task_slot_t slot{};
    slot.generation = 1u;
    slot.state = SAT_PARALLEL_QUEUED;
    slot.token = make_handle(2u, slot.generation);

    uint16_t index = 0u;
    uint16_t generation = 0u;
    CHECK(decode_handle(slot.token, 4u, &index, &generation));
    CHECK(index == 2u && generation == 1u);
    CHECK(handle_matches(slot, slot.token, 2u, 4u));
    CHECK(!handle_matches(slot, make_handle(2u, 2u), 2u, 4u));
    CHECK(!handle_matches(slot, make_handle(1u, 1u), 2u, 4u));
    CHECK(next_generation(0xFFFFu) == 1u);
    CHECK(!decode_handle(0u, 4u, &index, &generation));
    CHECK(terminal(SAT_PARALLEL_COMPLETED));
    CHECK(terminal(SAT_PARALLEL_FAILED));
    CHECK(!terminal(SAT_PARALLEL_RUNNING));
    std::puts("PASS: test_parallel_queue.cpp");
    return 0;
}
