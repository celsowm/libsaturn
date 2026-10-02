#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/task.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

namespace {

struct Fixture {
    std::vector<sat_task_slot_t> slots;
    std::vector<uint16_t> order;
    sat_task_scheduler_t sched;
    explicit Fixture(uint16_t capacity) : slots(capacity), order(capacity) {
        OK(sat_task_scheduler_init(&sched, slots.data(), order.data(), capacity) == SAT_OK);
    }
};

std::vector<int> g_log;       /* ids of the tasks that ran, in order */
std::vector<int> g_destroyed; /* ids whose destroy function ran */

struct Probe {
    int id;
    sat_task_handle_t peer; /* a task to act on */
    int action;
};

void log_update(sat_task_scheduler_t*, sat_task_handle_t, void* data) { g_log.push_back(static_cast<Probe*>(data)->id); }
void log_destroy(void* data) { g_destroyed.push_back(static_cast<Probe*>(data)->id); }

sat_task_handle_t add(Fixture& f, Probe* p, int16_t priority, sat_task_update_fn fn = log_update,
    sat_task_destroy_fn d = log_destroy) {
    sat_task_desc_t desc = {};
    desc.update = fn;
    desc.destroy = d;
    desc.data = p;
    desc.priority = priority;
    sat_task_handle_t h = {};
    OK(sat_task_create(&f.sched, &desc, &h) == SAT_OK);
    return h;
}

void reset_logs() {
    g_log.clear();
    g_destroyed.clear();
}

void test_init_and_args() {
    sat_task_scheduler_t s;
    sat_task_slot_t slot[2];
    uint16_t order[2];
    OK(sat_task_scheduler_init(nullptr, slot, order, 2) == SAT_ERR_INVALID_ARG);
    OK(sat_task_scheduler_init(&s, nullptr, order, 2) == SAT_ERR_INVALID_ARG);
    OK(sat_task_scheduler_init(&s, slot, nullptr, 2) == SAT_ERR_INVALID_ARG);
    OK(sat_task_scheduler_init(&s, slot, order, 0) == SAT_ERR_INVALID_ARG);
    OK(sat_task_scheduler_init(&s, slot, order, 2) == SAT_OK);
    OK(sat_task_scheduler_count(&s) == 0);
    OK(sat_task_scheduler_slot_bytes(2) == 2 * sizeof(sat_task_slot_t));

    sat_task_desc_t d = {};
    sat_task_handle_t h = {};
    OK(sat_task_create(&s, &d, &h) == SAT_ERR_INVALID_ARG); /* no update function */
    OK(sat_task_create(&s, nullptr, &h) == SAT_ERR_INVALID_ARG);
    OK(sat_task_create(nullptr, &d, &h) == SAT_ERR_INVALID_ARG);
    OK(sat_task_scheduler_run(nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_task_set_update(&s, h, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_task_scheduler_run(&s) == SAT_OK); /* an empty scheduler runs */
}

void test_priority_and_creation_order() {
    Fixture f(8);
    Probe p[6] = {{0, {}, 0}, {1, {}, 0}, {2, {}, 0}, {3, {}, 0}, {4, {}, 0}, {5, {}, 0}};
    reset_logs();
    add(f, &p[0], 5);
    add(f, &p[1], 1);
    add(f, &p[2], 5);
    add(f, &p[3], -3);
    add(f, &p[4], 1);
    add(f, &p[5], 5);
    OK(sat_task_scheduler_count(&f.sched) == 6);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    const std::vector<int> expect = {3, 1, 4, 0, 2, 5};
    OK(g_log == expect);
    /* the order is the same every step */
    g_log.clear();
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log == expect);
}

void test_destroy_and_stale_handles() {
    Fixture f(4);
    Probe a = {0, {}, 0}, b = {1, {}, 0}, c = {2, {}, 0};
    reset_logs();
    const sat_task_handle_t ha = add(f, &a, 0), hb = add(f, &b, 0);
    add(f, &c, 0);
    OK(sat_task_is_alive(&f.sched, hb));
    OK(sat_task_destroy(&f.sched, hb) == SAT_OK);
    OK(g_destroyed == std::vector<int>({1}));
    OK(!sat_task_is_alive(&f.sched, hb));
    OK(sat_task_destroy(&f.sched, hb) == SAT_ERR_NOT_FOUND);
    OK(sat_task_set_update(&f.sched, hb, log_update) == SAT_ERR_NOT_FOUND);
    void* data = nullptr;
    OK(sat_task_data(&f.sched, hb, &data) == SAT_ERR_NOT_FOUND);
    OK(sat_task_data(&f.sched, ha, &data) == SAT_OK && data == &a);
    OK(sat_task_scheduler_count(&f.sched) == 2);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log == std::vector<int>({0, 2}));

    /* the slot is reused; the old handle stays stale and the new one is not confused with it */
    Probe d = {3, {}, 0};
    const sat_task_handle_t hd = add(f, &d, 0);
    OK(hd.index == hb.index);
    OK(hd.generation != hb.generation);
    OK(!sat_task_is_alive(&f.sched, hb));
    OK(sat_task_is_alive(&f.sched, hd));
    OK(sat_task_destroy(&f.sched, hb) == SAT_ERR_NOT_FOUND);
    OK(sat_task_is_alive(&f.sched, hd));

    /* a zeroed handle and an out-of-range one are stale */
    sat_task_handle_t none = {};
    OK(!sat_task_is_alive(&f.sched, none));
    sat_task_handle_t far_away = {200, 1};
    OK(!sat_task_is_alive(&f.sched, far_away));
    OK(sat_task_destroy(&f.sched, far_away) == SAT_ERR_NOT_FOUND);
}

void test_capacity() {
    Fixture f(3);
    Probe p[4] = {{0, {}, 0}, {1, {}, 0}, {2, {}, 0}, {3, {}, 0}};
    sat_task_handle_t h[3];
    for (int i = 0; i < 3; ++i) h[i] = add(f, &p[i], 0);
    sat_task_desc_t desc = {};
    desc.update = log_update;
    desc.data = &p[3];
    sat_task_handle_t extra = {};
    OK(sat_task_create(&f.sched, &desc, &extra) == SAT_ERR_CAPACITY);
    OK(sat_task_scheduler_count(&f.sched) == 3);
    OK(sat_task_destroy(&f.sched, h[1]) == SAT_OK);
    OK(sat_task_create(&f.sched, &desc, &extra) == SAT_OK);
    OK(sat_task_scheduler_stats(&f.sched).peak_live == 3);
}

/* ----- lifecycle during a run ----- */

void self_destroy(sat_task_scheduler_t* s, sat_task_handle_t self, void* data) {
    g_log.push_back(static_cast<Probe*>(data)->id);
    OK(sat_task_destroy(s, self) == SAT_OK);
    OK(!sat_task_is_alive(s, self));
    OK(sat_task_destroy(s, self) == SAT_ERR_NOT_FOUND);
}

void peer_destroy(sat_task_scheduler_t* s, sat_task_handle_t, void* data) {
    Probe* p = static_cast<Probe*>(data);
    g_log.push_back(p->id);
    OK(sat_task_destroy(s, p->peer) == SAT_OK);
}

void test_self_and_peer_destroy() {
    Fixture f(8);
    Probe p[5] = {{0, {}, 0}, {1, {}, 0}, {2, {}, 0}, {3, {}, 0}, {4, {}, 0}};
    reset_logs();
    add(f, &p[0], 0);
    add(f, &p[1], 1, self_destroy);
    const sat_task_handle_t later = add(f, &p[2], 3);
    const sat_task_handle_t earlier = add(f, &p[3], -1);
    p[4].peer = later;
    add(f, &p[4], 2, peer_destroy);

    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    /* -1: 3, 0: 0, 1: 1 (destroys itself), 2: 4 (destroys 2 before it runs), 3: skipped */
    OK(g_log == std::vector<int>({3, 0, 1, 4}));
    OK(g_destroyed == std::vector<int>({1, 2}));
    OK(sat_task_scheduler_count(&f.sched) == 3);

    g_log.clear();
    p[4].peer = earlier; /* a peer that already ran this step is just gone afterwards */
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log == std::vector<int>({3, 0, 4}));
    OK(!sat_task_is_alive(&f.sched, earlier));
}

void destroy_twice(sat_task_scheduler_t* s, sat_task_handle_t, void* data) {
    Probe* p = static_cast<Probe*>(data);
    g_log.push_back(p->id);
    OK(sat_task_destroy(s, p->peer) == SAT_OK);
    OK(sat_task_destroy(s, p->peer) == SAT_ERR_NOT_FOUND); /* stale at once, before the slot is freed */
}

void test_destroyed_slot_not_reused_during_run() {
    Fixture f(2);
    Probe a = {0, {}, 0}, b = {1, {}, 0}, spawn = {2, {}, 0};
    reset_logs();
    b.peer = add(f, &a, 5);
    add(f, &b, 0, destroy_twice);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log == std::vector<int>({1}));
    OK(sat_task_scheduler_count(&f.sched) == 1);
    /* after the run the slot is free again */
    add(f, &spawn, 0);
    OK(sat_task_scheduler_count(&f.sched) == 2);
}

void spawn_child(sat_task_scheduler_t* s, sat_task_handle_t, void* data) {
    Probe* p = static_cast<Probe*>(data);
    g_log.push_back(p->id);
    if (p->action > 0) {
        --p->action;
        static Probe child = {99, {}, 0};
        sat_task_desc_t d = {};
        d.update = log_update;
        d.data = &child;
        d.priority = -100; /* sorts before its parent, but runs next step */
        OK(sat_task_create(s, &d, nullptr) == SAT_OK);
    }
}

void test_create_during_run() {
    Fixture f(4);
    Probe parent = {1, {}, 1};
    reset_logs();
    add(f, &parent, 0, spawn_child);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log == std::vector<int>({1}));
    OK(sat_task_scheduler_count(&f.sched) == 2);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log == std::vector<int>({1, 99, 1})); /* the child, priority -100, is now first */
}

void make_kids(sat_task_scheduler_t* s, sat_task_handle_t, void* data) {
    Probe* kids = static_cast<Probe*>(data);
    for (int i = 0; i < 3; ++i) {
        sat_task_desc_t d = {};
        d.update = log_update;
        d.data = &kids[i];
        d.priority = static_cast<int16_t>(i == 1 ? -1 : 0);
        OK(sat_task_create(s, &d, nullptr) == SAT_OK);
    }
}

void test_create_during_run_keeps_creation_order() {
    Fixture f(8);
    Probe kids[3] = {{10, {}, 0}, {11, {}, 0}, {12, {}, 0}};
    sat_task_desc_t d = {};
    d.update = make_kids;
    d.data = kids;
    sat_task_handle_t h = {};
    OK(sat_task_create(&f.sched, &d, &h) == SAT_OK);
    reset_logs();
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(sat_task_destroy(&f.sched, h) == SAT_OK);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log == std::vector<int>({11, 10, 12}));
}

void nested_run(sat_task_scheduler_t* s, sat_task_handle_t, void* data) {
    int* result = static_cast<int*>(data);
    result[0] = sat_task_scheduler_run(s);
    result[1] = sat_task_scheduler_clear(s);
}

void test_run_busy() {
    Fixture f(2);
    int result[2] = {0, 0};
    sat_task_desc_t d = {};
    d.update = nested_run;
    d.data = result;
    OK(sat_task_create(&f.sched, &d, nullptr) == SAT_OK);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(result[0] == SAT_ERR_BUSY && result[1] == SAT_ERR_BUSY);
    OK(sat_task_scheduler_count(&f.sched) == 1);
}

/* ----- update replacement ----- */

void second_update(sat_task_scheduler_t*, sat_task_handle_t, void* data) { g_log.push_back(100 + static_cast<Probe*>(data)->id); }

void swap_self(sat_task_scheduler_t* s, sat_task_handle_t self, void* data) {
    g_log.push_back(static_cast<Probe*>(data)->id);
    OK(sat_task_set_update(s, self, second_update) == SAT_OK);
}

void swap_peer(sat_task_scheduler_t* s, sat_task_handle_t, void* data) {
    Probe* p = static_cast<Probe*>(data);
    g_log.push_back(p->id);
    OK(sat_task_set_update(s, p->peer, second_update) == SAT_OK);
}

void test_update_replacement() {
    Fixture f(4);
    Probe a = {0, {}, 0}, b = {1, {}, 0}, c = {2, {}, 0};
    reset_logs();
    add(f, &a, 0, swap_self);
    const sat_task_handle_t later = add(f, &c, 2);
    b.peer = later;
    add(f, &b, 1, swap_peer);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    /* a swaps itself (this call is still the old one); b swaps c before c runs, so c runs the new one now */
    OK(g_log == std::vector<int>({0, 1, 102}));
    g_log.clear();
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log == std::vector<int>({100, 1, 102}));
}

/* ----- destroy functions ----- */

void test_destructor_runs_once() {
    Fixture f(4);
    Probe a = {0, {}, 0}, b = {1, {}, 0}, c = {2, {}, 0};
    reset_logs();
    add(f, &a, 0);
    add(f, &b, 0, log_update, nullptr); /* no destroy function: fine */
    const sat_task_handle_t hc = add(f, &c, 0);
    OK(sat_task_destroy(&f.sched, hc) == SAT_OK);
    OK(sat_task_destroy(&f.sched, hc) == SAT_ERR_NOT_FOUND);
    OK(g_destroyed == std::vector<int>({2}));
    OK(sat_task_scheduler_clear(&f.sched) == SAT_OK);
    OK(g_destroyed == std::vector<int>({2, 0})); /* b has none; c was already gone */
    OK(sat_task_scheduler_count(&f.sched) == 0);
    OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
    OK(g_log.empty());
}

Fixture* g_fixture = nullptr;
sat_task_handle_t g_other;

void destroy_other(void* data) {
    g_destroyed.push_back(static_cast<Probe*>(data)->id);
    /* a destroy function may destroy a peer, but not run or clear */
    if (g_other.generation) OK(sat_task_destroy(&g_fixture->sched, g_other) == SAT_OK);
    OK(sat_task_scheduler_run(&g_fixture->sched) == SAT_ERR_BUSY);
    OK(sat_task_scheduler_clear(&g_fixture->sched) == SAT_ERR_BUSY);
}

void test_destructor_reentrancy() {
    Fixture f(4);
    g_fixture = &f;
    Probe a = {0, {}, 0}, b = {1, {}, 0};
    reset_logs();
    const sat_task_handle_t ha = add(f, &a, 0, log_update, destroy_other);
    g_other = add(f, &b, 1);
    OK(sat_task_destroy(&f.sched, ha) == SAT_OK);
    OK(g_destroyed == std::vector<int>({0, 1}));
    OK(sat_task_scheduler_count(&f.sched) == 0);
    OK(!sat_task_is_alive(&f.sched, g_other));
    g_other = {};
}

void create_in_destroy(void*) {
    sat_task_desc_t d = {};
    d.update = log_update;
    sat_task_handle_t h = {};
    g_destroyed.push_back(sat_task_create(&g_fixture->sched, &d, &h));
}

void test_clear_refuses_create_in_destructor() {
    Fixture f(4);
    g_fixture = &f;
    Probe a = {0, {}, 0};
    reset_logs();
    add(f, &a, 0, log_update, create_in_destroy);
    OK(sat_task_scheduler_clear(&f.sched) == SAT_OK);
    OK(g_destroyed == std::vector<int>({SAT_ERR_BUSY}));
    OK(sat_task_scheduler_count(&f.sched) == 0);
}

void test_clear_stales_every_handle_in_order() {
    Fixture f(6);
    Probe p[4] = {{0, {}, 0}, {1, {}, 0}, {2, {}, 0}, {3, {}, 0}};
    reset_logs();
    sat_task_handle_t h[4];
    h[0] = add(f, &p[0], 2);
    h[1] = add(f, &p[1], 0);
    h[2] = add(f, &p[2], 1);
    h[3] = add(f, &p[3], 0);
    OK(sat_task_scheduler_clear(&f.sched) == SAT_OK);
    OK(g_destroyed == std::vector<int>({1, 3, 2, 0}));
    for (int i = 0; i < 4; ++i) OK(!sat_task_is_alive(&f.sched, h[i]));
    /* usable again afterwards */
    add(f, &p[0], 0);
    OK(sat_task_scheduler_count(&f.sched) == 1);
}

void test_generation_wraps_past_zero() {
    Fixture f(1);
    Probe a = {0, {}, 0};
    uint16_t last = 0;
    for (uint32_t i = 0; i < 70000u; ++i) {
        const sat_task_handle_t h = add(f, &a, 0, log_update, nullptr);
        OK(h.generation != 0);
        OK(h.generation != last);
        last = h.generation;
        OK(sat_task_destroy(&f.sched, h) == SAT_OK);
    }
}

/* ----- randomized against a reference model ----- */

struct Model {
    struct Entry {
        int id;
        int priority;
        uint32_t seq;
        bool alive;
    };
    std::vector<Entry> entries;
    uint32_t seq = 0;
    std::vector<int> order() const {
        std::vector<const Entry*> v;
        for (const Entry& e : entries) {
            if (e.alive) v.push_back(&e);
        }
        std::stable_sort(v.begin(), v.end(), [](const Entry* a, const Entry* b) {
            if (a->priority != b->priority) return a->priority < b->priority;
            return a->seq < b->seq;
        });
        std::vector<int> ids;
        for (const Entry* e : v) ids.push_back(e->id);
        return ids;
    }
};

void test_random_against_model() {
    Fixture f(24);
    Model m;
    std::vector<Probe> probes(4000);
    std::vector<sat_task_handle_t> handles(4000);
    uint32_t s = 12345u;
    auto rnd = [&]() { s = s * 1664525u + 1013904223u; return s >> 8; };
    int next_id = 0;
    for (int step = 0; step < 3000; ++step) {
        const uint32_t op = rnd() % 4u;
        if (op < 2u && sat_task_scheduler_count(&f.sched) < 24u) {
            const int id = next_id++;
            probes[id].id = id;
            const int pr = static_cast<int>(rnd() % 7u) - 3;
            handles[id] = add(f, &probes[id], static_cast<int16_t>(pr), log_update, nullptr);
            m.entries.push_back({id, pr, m.seq++, true});
        } else if (op == 2u && !m.entries.empty()) {
            Model::Entry& e = m.entries[rnd() % m.entries.size()];
            if (e.alive) {
                OK(sat_task_destroy(&f.sched, handles[e.id]) == SAT_OK);
                e.alive = false;
            } else {
                OK(sat_task_destroy(&f.sched, handles[e.id]) == SAT_ERR_NOT_FOUND);
            }
        } else {
            g_log.clear();
            OK(sat_task_scheduler_run(&f.sched) == SAT_OK);
            OK(g_log == m.order());
        }
        uint32_t live = 0;
        for (const Model::Entry& e : m.entries) live += e.alive ? 1u : 0u;
        OK(sat_task_scheduler_count(&f.sched) == live);
    }
}

}  // namespace

int main() {
    test_init_and_args();
    test_priority_and_creation_order();
    test_destroy_and_stale_handles();
    test_capacity();
    test_self_and_peer_destroy();
    test_destroyed_slot_not_reused_during_run();
    test_create_during_run();
    test_create_during_run_keeps_creation_order();
    test_run_busy();
    test_update_replacement();
    test_destructor_runs_once();
    test_destructor_reentrancy();
    test_clear_refuses_create_in_destructor();
    test_clear_stales_every_handle_in_order();
    test_generation_wraps_past_zero();
    test_random_against_model();
    std::puts("PASS: test_task.cpp (16 tests)");
    return 0;
}
