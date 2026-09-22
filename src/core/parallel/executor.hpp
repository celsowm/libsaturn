#ifndef SATURN_CORE_PARALLEL_EXECUTOR_HPP
#define SATURN_CORE_PARALLEL_EXECUTOR_HPP

#include <stdint.h>

#include "saturn/parallel.h"

namespace saturn::core::parallel::executor {

sat_result_t init(const sat_parallel_config_t* config);
sat_result_t shutdown(uint32_t timeout_ticks);
sat_result_t service();
void slave_entry(void* context);
sat_result_t register_task(sat_parallel_task_type_t type,
                           sat_parallel_process_fn process);
sat_result_t cache_sync_range(const void* address, uint32_t size);
void* uncached_address(const void* address);
sat_result_t submit(sat_parallel_task_type_t type, const void* input,
                    uint32_t input_size, void* output, uint32_t output_capacity,
                    sat_parallel_handle_t* out_handle);
sat_result_t submit_master(sat_parallel_task_type_t type, const void* input,
                           uint32_t input_size, void* output,
                           uint32_t output_capacity,
                           sat_parallel_handle_t* out_handle);
uint8_t initialized();
sat_parallel_mode_t mode();
sat_parallel_mode_t backend();
uint8_t slave_available();
uint8_t done(sat_parallel_handle_t handle);
sat_parallel_task_state_t state(sat_parallel_handle_t handle);
sat_result_t result(sat_parallel_handle_t handle, uint32_t* out_size);
sat_result_t wait(sat_parallel_handle_t handle, uint32_t timeout_ticks);
sat_result_t abort_task(sat_parallel_handle_t handle, uint32_t timeout_ticks);
sat_result_t cancel(sat_parallel_handle_t handle);
sat_result_t release(sat_parallel_handle_t handle);
sat_result_t stats(sat_parallel_stats_t* out_stats);

}  // namespace saturn::core::parallel::executor

#endif
