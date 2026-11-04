/* Emulator fault-injection target: the guest only launches valid DMA jobs.
 * The Ymir probe may independently assert DMA-illegal between frames. */
#include <stdint.h>
#include "saturn/saturn.h"
#include "example_util.h"

typedef struct {
    uint32_t magic, frames, irq_active, handler_calls, illegal, failures, successes;
    int32_t masked_start, recovery_start;
    uint32_t recovery_attempted;
} dma_fault_probe_t;
dma_fault_probe_t g_dma_fault_probe;
static uint32_t source[4096];

__attribute__((noinline)) void dma_fault_after_start(void) {
    __asm__ volatile("" ::: "memory");
}

__attribute__((noinline)) void dma_fault_masked_start(void) {
    __asm__ volatile("" ::: "memory");
}

static void on_illegal(void* unused) {
    (void)unused;
    ++g_dma_fault_probe.handler_calls;
}

int main(void) {
    sat_example_must(sat_app_init_default());
    g_dma_fault_probe.magic = 0x444D4146u;
    g_dma_fault_probe.irq_active = (uint32_t)sat_irq_active();
    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));
        if (g_dma_fault_probe.frames == 120u && g_dma_fault_probe.irq_active) {
            sat_example_must(sat_irq_set_handler(SAT_IRQ_DMA_ILLEGAL, on_illegal, 0));
        }
        sat_result_t result;
        if (g_dma_fault_probe.frames == 80u) {
            uint32_t sr;
            __asm__ volatile("stc sr,%0" : "=r"(sr));
            const uint32_t masked = sr | 0xF0u;
            __asm__ volatile("ldc %0,sr" :: "r"(masked) : "memory");
            dma_fault_masked_start();
            result = sat_dma_start(0u, source, (void*)0x25C70000u, sizeof source);
            g_dma_fault_probe.masked_start = result;
            __asm__ volatile("ldc %0,sr" :: "r"(sr) : "memory");
            if (result == SAT_ERR_IO) {
                g_dma_fault_probe.recovery_attempted = 1u;
                result = sat_dma_start(0u, source, (void*)0x25C70000u, sizeof source);
                g_dma_fault_probe.recovery_start = result;
            }
        } else {
            result = sat_dma_start(0u, source, (void*)0x25C70000u, sizeof source);
        }
        if (result == SAT_OK) {
            dma_fault_after_start();
            result = sat_dma_wait(0u);
        }
        if (result == SAT_OK) ++g_dma_fault_probe.successes;
        else ++g_dma_fault_probe.failures;
        sat_dma_stats_t stats;
        sat_dma_get_stats(&stats);
        g_dma_fault_probe.illegal = stats.illegal;
        ++g_dma_fault_probe.frames;
        sat_example_must(sat_app_frame_end());
    }
}
