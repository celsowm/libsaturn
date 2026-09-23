#ifndef SATURN_CORE_PARALLEL_TEST_FAULTS_H
#define SATURN_CORE_PARALLEL_TEST_FAULTS_H

#include <stdint.h>

/* Build-only fault IDs shared by the validation ROM and executor. This header
 * deliberately lives outside include/saturn: it is not a public API. */
#define SAT_PARALLEL_TEST_FAULT_NONE 0u
#define SAT_PARALLEL_TEST_FAULT_SUBMIT_REJECT 1u
#define SAT_PARALLEL_TEST_FAULT_WORKER_ERROR 2u
#define SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT 3u
#define SAT_PARALLEL_TEST_FAULT_ABORT_FAILURE 4u
#define SAT_PARALLEL_TEST_FAULT_RELEASE_FAILURE 5u

#ifdef __cplusplus
extern "C" {
#endif
uint32_t sat_parallel_test_fault_fired(void);
#ifdef __cplusplus
}
#endif

#endif
