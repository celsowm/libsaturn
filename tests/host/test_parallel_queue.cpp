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
    // The Slave has already written COMPLETED into the shared slot when its
    // message arrives; that completion must still count, with its ticks.
    sat_parallel_stats_t stats{};
    sat_parallel_task_slot_t done{};
    done.state = SAT_PARALLEL_COMPLETED;
    done.task_ticks = 321u;
    CHECK(apply_slave_completion(done, SAT_OK, stats));
    CHECK(stats.completed == 1u && stats.slave_task_ticks == 321u && stats.last_task_ticks == 321u);
    done.state = SAT_PARALLEL_RUNNING;
    done.task_ticks = 9u;
    CHECK(apply_slave_completion(done, SAT_ERR_IO, stats));
    CHECK(done.state == SAT_PARALLEL_FAILED && done.result == SAT_ERR_IO);
    CHECK(stats.failed == 1u && stats.slave_task_ticks == 330u);
    // A queued or cancelled slot is not the running task: nothing counts.
    done.state = SAT_PARALLEL_CANCELLED;
    CHECK(!apply_slave_completion(done, SAT_OK, stats) && stats.completed == 1u);
    std::puts("PASS: test_parallel_queue.cpp");
    return 0;
}
