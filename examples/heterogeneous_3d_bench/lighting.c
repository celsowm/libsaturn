#include "lighting.h"

#include "saturn/irq.h"
#include "saturn/parallel.h"
#include "saturn/scu_dsp.h"
#include "saturn/time.h"

#include "heterogeneous_lighting_ports.h"
#include "heterogeneous_lighting_stream.h"

enum { SLAVE_LIGHT = 128, STREAM_PC = 32 };
static const int32_t direction[3] = {383, -619, 728};
static const uint32_t direction_words[] = {383, (uint32_t)-619, 728};

_Alignas(16) int32_t normals[MAX_VERTICES][3];
_Alignas(16) int32_t intensity[MAX_VERTICES];

typedef struct { uint32_t first, last, use_c; } light_job_t;
static light_job_t slave_job;
static volatile uint16_t dsp_irq_frc;
static volatile uint32_t dsp_irq_serial;
static uint8_t dsp_available, slave_available;

static uint32_t elapsed(uint16_t start) {
    return (uint16_t)(sat_time_frc() - start);
}

static void dsp_end(void* user) {
    (void)user;
    dsp_irq_frc = sat_time_frc();
    ++dsp_irq_serial;
}

__attribute__((noinline))
static void light_c(unsigned first, unsigned last, int32_t* output) {
    for (unsigned i = first; i < last; ++i) {
        const int32_t* v = normals[i];
        *output++ = direction[0]*v[0] + direction[1]*v[1] + direction[2]*v[2];
    }
}

__attribute__((noinline))
static void light_mac(unsigned first, unsigned last, int32_t* output) {
    const int32_t* v = normals[first];
    for (unsigned i = first; i < last; ++i) {
        const int32_t* d = direction;
        int32_t sum;
        __asm__ volatile(
            "clrmac\n\t"
            "mac.l @%[v]+,@%[d]+\n\t"
            "mac.l @%[v]+,@%[d]+\n\t"
            "mac.l @%[v]+,@%[d]+\n\t"
            "sts macl,%[sum]"
            : [v] "+&r"(v), [d] "+&r"(d), [sum] "=r"(sum)
            :
            : "mach", "macl", "memory");
        *output++ = sum;
    }
}

static sat_result_t slave_light(const void* input, uint32_t input_bytes,
                                void* output, uint32_t capacity, uint32_t* produced) {
    if (input_bytes != sizeof(light_job_t)) return SAT_ERR_INVALID_ARG;
    const light_job_t job = *(const light_job_t*)input;
    const uint32_t bytes = (job.last - job.first) * sizeof(int32_t);
    if (bytes != capacity || job.first >= job.last || job.last > MAX_VERTICES)
        return SAT_ERR_INVALID_ARG;

    /* Normals are produced by the Master through its write-through cache. */
    sat_parallel_purge_cache();
    if (job.use_c) light_c(job.first, job.last, output);
    else light_mac(job.first, job.last, output);
    *produced = bytes;
    return SAT_OK;
}

sat_result_t lighting_init(void) {
    const sat_parallel_config_t config = {SAT_PARALLEL_SLAVE, 0, 0, 0, TIMEOUT};
    SAT_TRY(sat_parallel_register_task(SLAVE_LIGHT, slave_light));
    SAT_TRY(sat_parallel_init(&config));
    slave_available = sat_parallel_slave_available();
    SAT_TRY(sat_dsp_load_program(kLightingPorts, kLightingPortsCount, 0));
    SAT_TRY(sat_dsp_load_program(kLightingStream, kLightingStreamCount, STREAM_PC));
    SAT_TRY(sat_dsp_write_data(0, 0, direction_words, 3));
    (void)sat_irq_set_handler(SAT_IRQ_DSP_END, dsp_end, 0);
    dsp_available = 1;
    return SAT_OK;
}

static sat_result_t finish_dsp(void) {
    const sat_result_t result = sat_dsp_wait(TIMEOUT);
    if (result != SAT_OK) {
        dsp_available = 0;
        (void)sat_dsp_stop();
        /* Clearing EX alone does not cancel a transfer already on the bus. */
        SAT_TRY(sat_dsp_wait(TIMEOUT));
    }
    return result;
}

static sat_result_t port_tiles(unsigned count, lighting_ticks_t* ticks) {
    for (unsigned first = 0; first < count; first += TILE) {
        uint32_t words[3 * TILE];
        uint16_t start = sat_time_frc();
        for (unsigned i = 0; i < TILE; ++i)
            for (unsigned axis = 0; axis < 3; ++axis)
                words[3*i + axis] = (uint32_t)normals[first+i][axis];
        SAT_TRY(sat_dsp_write_data(1, 0, words, 3 * TILE));
        ticks->ports += elapsed(start);

        start = sat_time_frc();
        SAT_TRY(sat_dsp_start(0));
        SAT_TRY(finish_dsp());
        ticks->dsp_elapsed += elapsed(start);

        start = sat_time_frc();
        SAT_TRY(sat_dsp_read_data(2, 0, words, TILE));
        for (unsigned i = 0; i < TILE; ++i) intensity[first+i] = (int32_t)words[i];
        ticks->ports += elapsed(start);
    }
    ticks->dsp_vertices = count;
    return SAT_OK;
}

static sat_result_t start_stream(unsigned first, unsigned count) {
    const uintptr_t src = (uintptr_t)normals[first] & 0x0fffffffu;
    const uintptr_t dst = (uintptr_t)&intensity[first] & 0x0fffffffu;
    if (src < 0x06000000u || src + 12*count > 0x06100000u ||
        dst < 0x06000000u || dst + 4*count > 0x06100000u)
        return SAT_ERR_INVALID_ARG;
    const uint32_t params[] = {src >> 2, dst >> 2, count / TILE, 1};
    SAT_TRY(sat_dsp_write_data(3, 0, params, 4));
    return sat_dsp_start(STREAM_PC);
}

sat_result_t lighting_run(unsigned mode, unsigned count, lighting_ticks_t* ticks,
                          void (*independent_work)(void)) {
    *ticks = (lighting_ticks_t){0};
    if (count < TILE || count > MAX_VERTICES || count % TILE || mode > TRIPLE_SLAVE_WEIGHTED)
        return SAT_ERR_INVALID_ARG;

    const uint16_t begin = sat_time_frc();
    if (mode == DSP_PORTS) {
        if (!dsp_available) return SAT_ERR_UNSUPPORTED;
        SAT_TRY(port_tiles(count, ticks));
        ticks->total = elapsed(begin);
        return SAT_OK;
    }

    const int use_c = mode == MASTER_C || mode == DUAL_C;
    const int dual = mode == DUAL_C || mode == DUAL_MAC || mode >= TRIPLE_EIGHTH;
    unsigned dsp_count = 0, slave_count = 0;
    if (mode == DSP_STREAM) dsp_count = count;
    if (mode == MASTER_DSP) dsp_count = count / 4;
    if (mode >= TRIPLE_EIGHTH && mode <= TRIPLE_THREE_EIGHTHS)
        dsp_count = count * (mode - TRIPLE_EIGHTH + 1) / 8;
    if (mode == TRIPLE_SLAVE_WEIGHTED) dsp_count = count * 3 / 8;
    if (mode >= MASTER_DSP && mode <= TRIPLE_THREE_EIGHTHS &&
        count >= 2*TILE && dsp_count < TILE) dsp_count = TILE;
    if (mode >= TRIPLE_EIGHTH && count < 3*TILE) dsp_count = 0;
    dsp_count -= dsp_count % TILE;
    if (dual) slave_count = (count - dsp_count) / 2;
    if (mode >= DUAL_SLAVE_WEIGHTED) slave_count = count - dsp_count;
    /* Keep output cache lines private to each writer. */
    const unsigned master_end = count - dsp_count - slave_count;
    const unsigned slave_end = count - dsp_count;
    if ((dsp_count && !dsp_available) || (slave_count && !slave_available))
        return SAT_ERR_UNSUPPORTED;

    sat_parallel_handle_t handle = 0;
    sat_parallel_stats_t stats;
    sat_result_t result = SAT_OK;
    if (slave_count) {
        slave_job = (light_job_t){master_end, slave_end, use_c};
        result = sat_parallel_submit(SLAVE_LIGHT, &slave_job, sizeof(slave_job),
                                     intensity + master_end, 4*slave_count, &handle);
    }

    const uint32_t irq_before = dsp_irq_serial;
    const uint16_t dsp_start = sat_time_frc();
    if (result == SAT_OK && dsp_count) result = start_stream(slave_end, dsp_count);
    const int dsp_started = dsp_count && result == SAT_OK;
    ticks->dispatch = elapsed(begin);

    uint16_t phase = sat_time_frc();
    if (independent_work) independent_work();
    if (master_end) {
        if (use_c) light_c(0, master_end, intensity);
        else light_mac(0, master_end, intensity);
    }
    ticks->master = elapsed(phase);

    phase = sat_time_frc();
    if (handle) {
        const sat_result_t waited = sat_parallel_wait(handle, TIMEOUT);
        if (waited == SAT_OK) {
            (void)sat_parallel_stats(&stats);
            ticks->slave = stats.last_task_ticks;
        } else if (sat_parallel_state(handle) == SAT_PARALLEL_RUNNING) {
            if (sat_parallel_abort(handle, TIMEOUT) != SAT_OK) slave_available = 0;
        }
        const sat_result_t released = sat_parallel_release(handle);
        if (result == SAT_OK) result = waited != SAT_OK ? waited : released;
    }
    if (dsp_started) {
        const sat_result_t finished = finish_dsp();
        const uint16_t observed = dsp_irq_serial != irq_before ? dsp_irq_frc : sat_time_frc();
        ticks->dsp_elapsed = (uint16_t)(observed - dsp_start);
        if (finished == SAT_OK)
            (void)sat_parallel_cache_sync_range(intensity + slave_end, 4*dsp_count);
        if (result == SAT_OK) result = finished;
    }
    ticks->join = elapsed(phase);
    ticks->total = elapsed(begin);
    ticks->dsp_vertices = dsp_count;
    ticks->slave_vertices = slave_count;
    return result;
}

unsigned lighting_mismatches(unsigned count) {
    unsigned errors = 0;
    for (unsigned i = 0; i < count; ++i) {
        int64_t sum = 0;
        for (unsigned axis = 0; axis < 3; ++axis)
            sum += (int64_t)normals[i][axis] * direction[axis];
        if (sum != intensity[i]) ++errors;
    }
    return errors;
}
