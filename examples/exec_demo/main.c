/* exec_demo - sat_exec round trip. The program execs itself (0.BIN) kRounds
 * times, handing the round number to the next run, each time with the Slave
 * SH-2 running and VBlank interrupts live, so every run starts from what the
 * previous one left. The backdrop changes colour per round; the last round
 * stays on green. harness/run-exec.ps1 reads g_exec_demo. */
#include <stdint.h>

#include "saturn/saturn.h"
#include "example_util.h"

enum {
    kRounds = 4u,
    kFramesPerRound = 40u,
    kSlaveTimeoutTicks = 60000u,
    kMagic = 0x45584543u, /* 'EXEC' */
};

typedef struct exec_demo {
    uint32_t magic;
    uint32_t round;          /* 0 after the BIOS boot, +1 per sat_exec */
    int32_t arg_status;      /* sat_exec_arg: SAT_ERR_NOT_FOUND on round 0 */
    int32_t missing_status;  /* sat_exec of a missing file: SAT_ERR_NOT_FOUND, nothing stopped */
    int32_t exec_status;     /* a sat_exec of 0.BIN that came back (never, when it works) */
    int32_t slave_status;    /* sat_dual_sh2_start */
    uint32_t slave_beats;    /* the Slave's heartbeats this round */
    uint32_t frames;         /* frames this round */
    uint32_t done;           /* 1 once the last round is reached */
} exec_demo_t;

exec_demo_t g_exec_demo;

static sat_cd_block_t g_block;
static sat_cd_device_t g_device;
static sat_cdfs_volume_t g_volume;

static void slave_program(void* context) {
    (void)context;
    for (;;) {
        sat_dual_sh2_slave_heartbeat();
        if (sat_dual_sh2_slave_stop_requested() != 0u) break;
    }
}

static const uint16_t kRoundColors[] = {
    SAT_COLOR_RED, SAT_COLOR_YELLOW, SAT_COLOR_MAGENTA, SAT_COLOR_BLUE, SAT_COLOR_GREEN,
};

int main(void) {
    uint32_t round = 0u;
    uint32_t bytes = 0u;
    g_exec_demo.arg_status = sat_exec_arg(&round, sizeof(round), &bytes);
    round = (g_exec_demo.arg_status == SAT_OK && bytes == sizeof(round)) ? round + 1u : 0u;
    g_exec_demo.round = round;
    g_exec_demo.exec_status = SAT_OK;
    g_exec_demo.magic = kMagic;

    sat_example_must(sat_app_init_default());
    sat_example_must(sat_cd_block_init(&g_block, SAT_CD_BLOCK_DEFAULT_TIMEOUT));
    sat_example_must(sat_cd_block_bind_device(&g_block, &g_device, 0u));
    sat_example_must(sat_cdfs_mount(&g_volume, &g_device));
    g_exec_demo.missing_status = sat_exec(&g_volume, "NOPE.BIN", 0, 0u);

    g_exec_demo.slave_status = sat_dual_sh2_configure_slave(slave_program, 0);
    if (g_exec_demo.slave_status == SAT_OK) {
        g_exec_demo.slave_status = sat_dual_sh2_start(kSlaveTimeoutTicks);
    }

    const uint16_t color = kRoundColors[round < kRounds ? round : kRounds];
    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(color, color, &pad));
        sat_example_must(sat_app_frame_end());
        ++g_exec_demo.frames;
        g_exec_demo.slave_beats = sat_dual_sh2_slave_heartbeat_count();
        if (g_exec_demo.frames == kFramesPerRound) {
            if (round < kRounds) {
                g_exec_demo.exec_status = sat_exec(&g_volume, "0.BIN", &round, sizeof(round));
            } else {
                g_exec_demo.done = 1u;
            }
        }
    }
}
