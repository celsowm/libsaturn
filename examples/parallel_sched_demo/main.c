/* Slave SH-2 scheduling: task priorities, dependencies, batched dispatch and
 * a data-parallel loop, on the two real CPUs.
 *
 *   A  a long task keeps the Slave busy while four more are queued in a mixed
 *      order; they then travel in ONE signal and run by priority
 *   B  a high-priority task that depends on a low-priority one runs after it; a
 *      Master-only task can depend on a Slave task; a task whose dependency
 *      failed is cancelled
 *   C  sat_parallel_for splits a hashing loop between the CPUs
 * g_sched_demo holds what harness/tests/test_parallel_sched.py checks. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/parallel.h"

#define SCHED_DEMO_MAGIC 0x53434831u /* "SCH1" */
#define TASK_ORDER 0x7E01u
#define TASK_SPIN 0x7E02u
#define TASK_FAIL 0x7E03u
#define STARTUP_TIMEOUT 60000u
#define WAIT_TIMEOUT 60000u
#define FOR_COUNT 8192u
#define FOR_ROUNDS 16u
#define SLAVE_STACK_LIMIT 0x06004000u   /* below the program image: the Slave's stack */

typedef struct order_input {
    uint32_t spin;
} order_input_t;

typedef struct order_output {
    uint32_t ordinal;     /* 1, 2, 3 ... in the order the tasks really ran */
    uint32_t on_slave;
} order_output_t;

typedef struct hash_entry {
    uint32_t value;
    uint32_t on_slave;
} hash_entry_t;

typedef struct sched_demo_results {
    uint32_t magic;
    uint32_t init_status;
    uint32_t slave_ready;
    /* A: ordinals of the tasks submitted as LOW, HIGH, NORMAL, URGENT */
    uint32_t prio_ordinal[4];
    uint32_t prio_on_slave;          /* how many of the four ran on the Slave */
    uint32_t prio_signals;           /* execute messages for the blocker + the four */
    uint32_t prio_batched;           /* tasks that travelled in a batch */
    /* B */
    uint32_t dep_low_ordinal;
    uint32_t dep_urgent_ordinal;     /* depends on the low one, runs after it */
    uint32_t dep_other_ordinal;
    uint32_t master_dep_slave_ordinal;
    uint32_t master_dep_master_ordinal;
    uint32_t master_dep_on_slave;    /* must be 0: it ran on the Master */
    uint32_t failed_dep_wait_result; /* wait() of the dependent, as -result */
    uint32_t failed_dep_state;       /* SAT_PARALLEL_CANCELLED */
    uint32_t dependency_cancels;
    /* C */
    uint32_t for_single_ticks;
    uint32_t for_parallel_ticks;
    uint32_t for_match;              /* parallel result identical to the serial one */
    uint32_t for_slave_elements;     /* elements the Slave computed */
    uint32_t for_small_slave_elements; /* a range below the grain stays on the Master: 0 */
    uint32_t for_status;
    uint32_t errors;
    uint32_t frames;
} sched_demo_results_t;

volatile sched_demo_results_t g_sched_demo;

static sat_ascii_font_t font;
static volatile uint32_t g_order_counter;
static hash_entry_t g_hash[FOR_COUNT];
static uint32_t g_reference[FOR_COUNT];
static order_input_t g_input[16];
static order_output_t g_output[16];
static sat_parallel_handle_t g_handle[16];

static uint16_t frt_counter(void) {
    volatile uint8_t* const high = (volatile uint8_t*)0xFFFFFE12u;
    volatile uint8_t* const low = (volatile uint8_t*)0xFFFFFE13u;
    const uint16_t h = *high;
    return (uint16_t)((h << 8u) | *low);
}

/* The Slave's stack lives below the program image; the Master's does not. */
static uint32_t on_slave_cpu(void) {
    uint32_t sp;
    __asm__ volatile("mov r15,%0" : "=r"(sp));
    return sp < SLAVE_STACK_LIMIT ? 1u : 0u;
}

static sat_result_t order_task(const void* input, uint32_t input_size, void* output,
                               uint32_t output_capacity, uint32_t* output_size) {
    const order_input_t* in = (const order_input_t*)input;
    order_output_t* out = (order_output_t*)output;
    volatile uint32_t* counter = (volatile uint32_t*)sat_parallel_uncached_address((const void*)&g_order_counter);
    if (input_size != sizeof(*in) || output_capacity < sizeof(*out)) return SAT_ERR_INVALID_ARG;
    for (volatile uint32_t i = 0u; i < in->spin; ++i) {
    }
    out->ordinal = ++*counter;
    out->on_slave = on_slave_cpu();
    *output_size = sizeof(*out);
    return SAT_OK;
}

static sat_result_t fail_task(const void* input, uint32_t input_size, void* output,
                              uint32_t output_capacity, uint32_t* output_size) {
    (void)input;
    (void)input_size;
    (void)output;
    (void)output_capacity;
    *output_size = 0u;
    return SAT_ERR_IO;
}

static void hash_range(void* context, uint32_t begin, uint32_t end) {
    hash_entry_t* entries = (hash_entry_t*)context;
    const uint32_t slave = on_slave_cpu();
    for (uint32_t i = begin; i < end; ++i) {
        uint32_t h = i * 2654435761u;
        for (uint32_t k = 0u; k < FOR_ROUNDS; ++k) h = (h * 1664525u + 1013904223u) ^ (h >> 13u);
        entries[i].value = h;
        entries[i].on_slave = slave;
    }
}

static sat_parallel_handle_t submit_order(uint8_t priority, uint8_t force_master, uint32_t slot,
                                          uint32_t spin, sat_parallel_handle_t depends_on) {
    sat_parallel_submit_desc_t desc = {0};
    sat_parallel_handle_t handle = 0u;
    g_input[slot].spin = spin;
    g_output[slot].ordinal = 0u;
    g_output[slot].on_slave = 0u;
    desc.type = TASK_ORDER;
    desc.priority = priority;
    desc.force_master = force_master;
    desc.input = &g_input[slot];
    desc.input_size = sizeof(order_input_t);
    desc.output = &g_output[slot];
    desc.output_capacity = sizeof(order_output_t);
    desc.depends_on = depends_on;
    if (sat_parallel_submit_ex(&desc, &handle) != SAT_OK) ++g_sched_demo.errors;
    g_handle[slot] = handle;
    return handle;
}

static void wait_for(sat_parallel_handle_t handle) {
    if (sat_parallel_wait(handle, WAIT_TIMEOUT) != SAT_OK) ++g_sched_demo.errors;
}

static void scenario_priority_and_batch(void) {
    sat_parallel_stats_t before = {0}, after = {0};
    sat_parallel_stats(&before);
    /* The blocker takes the idle Slave at once; the four below queue behind it. */
    (void)submit_order(SAT_PARALLEL_PRIORITY_NORMAL, 0u, 0u, 40000u, 0u);
    (void)submit_order(SAT_PARALLEL_PRIORITY_LOW, 0u, 1u, 0u, 0u);
    (void)submit_order(SAT_PARALLEL_PRIORITY_HIGH, 0u, 2u, 0u, 0u);
    (void)submit_order(SAT_PARALLEL_PRIORITY_NORMAL, 0u, 3u, 0u, 0u);
    (void)submit_order(SAT_PARALLEL_PRIORITY_URGENT, 0u, 4u, 0u, 0u);
    for (uint32_t i = 0u; i < 5u; ++i) wait_for(g_handle[i]);
    sat_parallel_stats(&after);
    for (uint32_t i = 0u; i < 4u; ++i) {
        g_sched_demo.prio_ordinal[i] = g_output[1u + i].ordinal;
        g_sched_demo.prio_on_slave += g_output[1u + i].on_slave;
    }
    g_sched_demo.prio_signals = after.slave_signals - before.slave_signals;
    g_sched_demo.prio_batched = after.batched_tasks - before.batched_tasks;
    for (uint32_t i = 0u; i < 5u; ++i) (void)sat_parallel_release(g_handle[i]);
}

static void scenario_dependencies(void) {
    sat_parallel_stats_t before = {0}, after = {0};
    sat_parallel_handle_t blocker, low, urgent, other, on_slave, on_master, bad, dependent;
    sat_parallel_submit_desc_t desc = {0};
    sat_parallel_stats(&before);

    /* An urgent task that needs a low-priority one waits for it. */
    blocker = submit_order(SAT_PARALLEL_PRIORITY_NORMAL, 0u, 5u, 40000u, 0u);
    low = submit_order(SAT_PARALLEL_PRIORITY_LOW, 0u, 6u, 0u, 0u);
    urgent = submit_order(SAT_PARALLEL_PRIORITY_URGENT, 0u, 7u, 0u, low);
    other = submit_order(SAT_PARALLEL_PRIORITY_HIGH, 0u, 8u, 0u, 0u);
    wait_for(blocker);
    wait_for(low);
    wait_for(urgent);
    wait_for(other);
    g_sched_demo.dep_low_ordinal = g_output[6].ordinal;
    g_sched_demo.dep_urgent_ordinal = g_output[7].ordinal;
    g_sched_demo.dep_other_ordinal = g_output[8].ordinal;

    /* A task pinned to the Master can wait for one running on the Slave. */
    on_slave = submit_order(SAT_PARALLEL_PRIORITY_NORMAL, 0u, 9u, 20000u, 0u);
    on_master = submit_order(SAT_PARALLEL_PRIORITY_NORMAL, 1u, 10u, 0u, on_slave);
    wait_for(on_master);
    wait_for(on_slave);
    g_sched_demo.master_dep_slave_ordinal = g_output[9].ordinal;
    g_sched_demo.master_dep_master_ordinal = g_output[10].ordinal;
    g_sched_demo.master_dep_on_slave = g_output[10].on_slave;

    /* A failed dependency cancels what waited for it. */
    desc.type = TASK_FAIL;
    desc.priority = SAT_PARALLEL_PRIORITY_NORMAL;
    if (sat_parallel_submit_ex(&desc, &bad) != SAT_OK) ++g_sched_demo.errors;
    dependent = submit_order(SAT_PARALLEL_PRIORITY_NORMAL, 0u, 11u, 0u, bad);
    g_sched_demo.failed_dep_wait_result = (uint32_t)(-sat_parallel_wait(dependent, WAIT_TIMEOUT));
    g_sched_demo.failed_dep_state = (uint32_t)sat_parallel_state(dependent);
    sat_parallel_stats(&after);
    g_sched_demo.dependency_cancels = after.dependency_cancels - before.dependency_cancels;

    /* Released in dependency order: nothing may depend on a released task. */
    (void)sat_parallel_release(dependent);
    (void)sat_parallel_release(bad);
    (void)sat_parallel_release(on_master);
    (void)sat_parallel_release(on_slave);
    (void)sat_parallel_release(other);
    (void)sat_parallel_release(urgent);
    (void)sat_parallel_release(low);
    (void)sat_parallel_release(blocker);
}

static void scenario_parallel_for(void) {
    uint16_t start;
    uint32_t match = 1u;
    start = frt_counter();
    hash_range(g_hash, 0u, FOR_COUNT);
    g_sched_demo.for_single_ticks = (uint16_t)(frt_counter() - start);
    for (uint32_t i = 0u; i < FOR_COUNT; ++i) {
        g_reference[i] = g_hash[i].value;
        g_hash[i].value = 0u;
        g_hash[i].on_slave = 0u;
    }
    start = frt_counter();
    g_sched_demo.for_status = (uint32_t)(-sat_parallel_for(0u, FOR_COUNT, 64u, hash_range, g_hash,
                                                            g_hash, sizeof(g_hash)));
    g_sched_demo.for_parallel_ticks = (uint16_t)(frt_counter() - start);
    for (uint32_t i = 0u; i < FOR_COUNT; ++i) {
        if (g_hash[i].value != g_reference[i]) match = 0u;
        g_sched_demo.for_slave_elements += g_hash[i].on_slave;
    }
    g_sched_demo.for_match = match;

    /* A range below twice the grain runs entirely on the Master. */
    for (uint32_t i = 0u; i < 64u; ++i) g_hash[i].on_slave = 0u;
    (void)sat_parallel_for(0u, 64u, 64u, hash_range, g_hash, g_hash, 64u * sizeof(hash_entry_t));
    for (uint32_t i = 0u; i < 64u; ++i) g_sched_demo.for_small_slave_elements += g_hash[i].on_slave;
}

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    sat_parallel_config_t config = {SAT_PARALLEL_SLAVE, 0, 0u, 0u, STARTUP_TIMEOUT};
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_sched_demo.magic = SCHED_DEMO_MAGIC;

    g_sched_demo.init_status = (uint32_t)(-sat_parallel_init(&config));
    g_sched_demo.slave_ready = sat_parallel_slave_available();
    if (sat_parallel_register_task(TASK_ORDER, order_task) != SAT_OK ||
        sat_parallel_register_task(TASK_FAIL, fail_task) != SAT_OK) {
        ++g_sched_demo.errors;
    }
    (void)TASK_SPIN;
    if (g_sched_demo.slave_ready != 0u) {
        scenario_priority_and_batch();
        scenario_dependencies();
        scenario_parallel_for();
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_GREEN, &pad) != SAT_OK) break;
        ++g_sched_demo.frames;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "SLAVE SCHEDULING", 8, 8, 8, 0u, 0u);
        line("SLAVE ", g_sched_demo.slave_ready, 24);
        line("SIGNALS ", g_sched_demo.prio_signals, 40);
        line("BATCHED ", g_sched_demo.prio_batched, 56);
        line("ORDER U ", g_sched_demo.prio_ordinal[3], 72);
        line("FOR SINGLE ", g_sched_demo.for_single_ticks, 96);
        line("FOR PARALLEL ", g_sched_demo.for_parallel_ticks, 112);
        line("FOR MATCH ", g_sched_demo.for_match, 128);
        line("ERRORS ", g_sched_demo.errors, 152);
        (void)sat_app_frame_end();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_parallel_shutdown(STARTUP_TIMEOUT);
    (void)sat_shutdown();
    return 0;
}
