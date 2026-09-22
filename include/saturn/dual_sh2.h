#ifndef SATURN_DUAL_SH2_H
#define SATURN_DUAL_SH2_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sat_dual_sh2_state {
    SAT_DUAL_SH2_OFFLINE = 0,
    SAT_DUAL_SH2_STARTING = 1,
    SAT_DUAL_SH2_READY = 2,
    SAT_DUAL_SH2_STOPPING = 3,
    SAT_DUAL_SH2_FAULT = 4,
} sat_dual_sh2_state_t;

typedef void (*sat_dual_sh2_entry_t)(void* context);

typedef struct sat_dual_sh2_message {
    uint32_t sequence;
    uint32_t command;
    uint32_t argument0;
    uint32_t argument1;
} sat_dual_sh2_message_t;

/* timeout is measured in local FRT ticks when the FRT is running. */
sat_result_t sat_dual_sh2_configure_slave(sat_dual_sh2_entry_t entry, void* context);
sat_result_t sat_dual_sh2_start(uint32_t timeout_ticks);
sat_dual_sh2_state_t sat_dual_sh2_state(void);
uint8_t sat_dual_sh2_available(void);
sat_result_t sat_dual_sh2_stop(uint32_t timeout_ticks);
sat_result_t sat_dual_sh2_reset(uint32_t timeout_ticks);

sat_result_t sat_dual_sh2_send(uint32_t command, uint32_t argument0, uint32_t argument1);
sat_result_t sat_dual_sh2_receive(sat_dual_sh2_message_t* out_message);
sat_result_t sat_dual_sh2_wait_response(
    sat_dual_sh2_message_t* out_message, uint32_t timeout_ticks);
sat_result_t sat_dual_sh2_signal_slave(void);

/* These functions are intended for code running from the configured Slave
 * entry callback.  They do not start or stop the CPU. */
sat_result_t sat_dual_sh2_slave_receive(sat_dual_sh2_message_t* out_message);
sat_result_t sat_dual_sh2_slave_send(uint32_t command, uint32_t argument0, uint32_t argument1);
uint8_t sat_dual_sh2_slave_stop_requested(void);
void sat_dual_sh2_slave_heartbeat(void);
uint32_t sat_dual_sh2_slave_heartbeat_count(void);
sat_result_t sat_dual_sh2_slave_signal_master(void);

#ifdef __cplusplus
}
#endif

#endif
