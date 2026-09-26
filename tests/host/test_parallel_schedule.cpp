#include <cstdio>

#include "src/core/parallel/queue_logic.hpp"

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

using namespace saturn::core::parallel::queue_logic;

static Candidate task(uint16_t slot, uint8_t priority, uint32_t order, bool master = false,
                      Dep dep = Dep::None, int32_t dep_slot = -1) {
    return Candidate{slot, priority, order, master, dep, dep_slot};
}

int main() {
    uint16_t out[kBatchMax] = {};

    // Higher priority first, FIFO inside a priority.
    {
        const Candidate c[] = {task(0, 0, 1), task(1, 2, 2), task(2, 1, 3), task(3, 3, 4), task(4, 2, 5)};
        const uint32_t n = pick_batch(c, 5u, kBatchMax, out);
        CHECK(n == 4u);
        CHECK(out[0] == 3u && out[1] == 1u && out[2] == 4u && out[3] == 2u);   /* 3, 2, 2, 1; slot 0 waits */
        const uint32_t two = pick_batch(c, 5u, 2u, out);
        CHECK(two == 2u && out[0] == 3u && out[1] == 1u);
    }

    // Master-only tasks never go to the Slave; the Master takes the best of them.
    {
        const Candidate c[] = {task(0, 3, 1, true), task(1, 1, 2), task(2, 2, 3, true), task(3, 2, 4)};
        const uint32_t n = pick_batch(c, 4u, kBatchMax, out);
        CHECK(n == 2u && out[0] == 3u && out[1] == 1u);
        const int32_t m = pick_master(c, 4u);
        CHECK(m == 0);                                   /* slot 0: priority 3 */
        const Candidate only_slave[] = {task(0, 1, 1), task(1, 1, 2)};
        CHECK(pick_master(only_slave, 2u) == -1);
    }

    // A dependency that is not finished keeps a high-priority task back...
    {
        const Candidate c[] = {task(0, 0, 1), task(1, 3, 2, false, Dep::Pending, 5)};
        CHECK(!satisfied(c[1].dep));
        const uint32_t n = pick_batch(c, 2u, kBatchMax, out);
        CHECK(n == 1u && out[0] == 0u);                  /* dependency (slot 5) is not queued here */
    }
    // ...but one queued in the same batch runs first, even at lower priority.
    {
        const Candidate c[] = {task(0, 0, 1), task(1, 3, 2, false, Dep::Pending, 0), task(2, 1, 3)};
        const uint32_t n = pick_batch(c, 3u, kBatchMax, out);
        CHECK(n == 3u);
        CHECK(out[0] == 2u);                             /* best ready: priority 1 */
        CHECK(out[1] == 0u && out[2] == 1u);             /* then the dependency, then its dependent */
        // The dependency must precede the dependent whatever the priorities.
        uint32_t pos_dep = 9u, pos_task = 9u;
        for (uint32_t i = 0u; i < n; ++i) { if (out[i] == 0u) pos_dep = i; if (out[i] == 1u) pos_task = i; }
        CHECK(pos_dep < pos_task);
    }
    // A finished dependency is no obstacle; a failed one never becomes eligible.
    {
        const Candidate done[] = {task(0, 1, 1, false, Dep::Done, -1)};
        CHECK(pick_batch(done, 1u, kBatchMax, out) == 1u);
        const Candidate failed[] = {task(0, 1, 1, false, Dep::Failed, -1)};
        CHECK(pick_batch(failed, 1u, kBatchMax, out) == 0u);
        CHECK(pick_master(failed, 1u) == -1);
    }
    // A chain fits in one batch in order; the batch cap cuts it.
    {
        const Candidate c[] = {task(0, 1, 1), task(1, 1, 2, false, Dep::Pending, 0),
                               task(2, 1, 3, false, Dep::Pending, 1), task(3, 1, 4, false, Dep::Pending, 2),
                               task(4, 1, 5, false, Dep::Pending, 3)};
        const uint32_t n = pick_batch(c, 5u, kBatchMax, out);
        CHECK(n == 4u && out[0] == 0u && out[1] == 1u && out[2] == 2u && out[3] == 3u);
    }
    // A dependency on a Master-only task cannot join a Slave batch.
    {
        const Candidate c[] = {task(0, 1, 1, true), task(1, 1, 2, false, Dep::Pending, 0)};
        CHECK(pick_batch(c, 2u, kBatchMax, out) == 0u);
        CHECK(pick_master(c, 2u) == 0);
    }
    // Master picks respect priority and FIFO too.
    {
        const Candidate c[] = {task(0, 1, 5, true), task(1, 1, 3, true), task(2, 2, 9, true)};
        CHECK(pick_master(c, 3u) == 2);
        const Candidate d[] = {task(0, 1, 5, true), task(1, 1, 3, true)};
        CHECK(pick_master(d, 2u) == 1);
    }
    CHECK(kPriorityDefault <= kPriorityMax && kBatchMax == 4u);

    std::puts("PASS: test_parallel_schedule.cpp");
    return 0;
}
