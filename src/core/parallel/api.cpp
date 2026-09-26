#include "saturn/parallel.h"

#include "src/core/parallel/executor.hpp"

extern "C" sat_result_t sat_parallel_init(const sat_parallel_config_t* config) {
    return saturn::core::parallel::executor::init(config);
}

extern "C" sat_result_t sat_parallel_shutdown(uint32_t timeout_ticks) {
    return saturn::core::parallel::executor::shutdown(timeout_ticks);
}

extern "C" uint8_t sat_parallel_initialized(void) {
    return saturn::core::parallel::executor::initialized();
}

extern "C" sat_parallel_mode_t sat_parallel_mode(void) {
    return saturn::core::parallel::executor::mode();
}

extern "C" sat_parallel_mode_t sat_parallel_backend(void) {
    return saturn::core::parallel::executor::backend();
}

extern "C" uint8_t sat_parallel_slave_available(void) {
    return saturn::core::parallel::executor::slave_available();
}

extern "C" sat_result_t sat_parallel_register_task(
    sat_parallel_task_type_t type, sat_parallel_process_fn process) {
    return saturn::core::parallel::executor::register_task(type, process);
}

extern "C" sat_result_t sat_parallel_cache_sync_range(
    const void* address, uint32_t size) {
    return saturn::core::parallel::executor::cache_sync_range(address, size);
}

extern "C" void* sat_parallel_uncached_address(const void* address) {
    return saturn::core::parallel::executor::uncached_address(address);
}

extern "C" sat_result_t sat_parallel_submit(
    sat_parallel_task_type_t type, const void* input, uint32_t input_size,
    void* output, uint32_t output_capacity, sat_parallel_handle_t* out_handle) {
    return saturn::core::parallel::executor::submit(
        type, input, input_size, output, output_capacity, out_handle);
}

extern "C" sat_result_t sat_parallel_submit_ex(
    const sat_parallel_submit_desc_t* desc, sat_parallel_handle_t* out_handle) {
    return saturn::core::parallel::executor::submit_ex(desc, out_handle);
}

extern "C" sat_result_t sat_parallel_for(
    uint32_t begin, uint32_t end, uint32_t min_grain,
    sat_parallel_range_fn fn, void* context, void* output, uint32_t output_bytes) {
    return saturn::core::parallel::executor::parallel_for(
        begin, end, min_grain, fn, context, output, output_bytes);
}

extern "C" sat_result_t sat_parallel_submit_master(
    sat_parallel_task_type_t type, const void* input, uint32_t input_size,
    void* output, uint32_t output_capacity, sat_parallel_handle_t* out_handle) {
    return saturn::core::parallel::executor::submit_master(
        type, input, input_size, output, output_capacity, out_handle);
}

extern "C" sat_result_t sat_parallel_service(void) {
    return saturn::core::parallel::executor::service();
}

extern "C" uint8_t sat_parallel_done(sat_parallel_handle_t handle) {
    return saturn::core::parallel::executor::done(handle);
}

extern "C" sat_parallel_task_state_t sat_parallel_state(sat_parallel_handle_t handle) {
    return saturn::core::parallel::executor::state(handle);
}

extern "C" sat_result_t sat_parallel_result(sat_parallel_handle_t handle, uint32_t* out_size) {
    return saturn::core::parallel::executor::result(handle, out_size);
}

extern "C" sat_result_t sat_parallel_wait(sat_parallel_handle_t handle, uint32_t timeout_ticks) {
    return saturn::core::parallel::executor::wait(handle, timeout_ticks);
}

extern "C" sat_result_t sat_parallel_abort(
    sat_parallel_handle_t handle, uint32_t timeout_ticks) {
    return saturn::core::parallel::executor::abort_task(handle, timeout_ticks);
}

extern "C" sat_result_t sat_parallel_cancel(sat_parallel_handle_t handle) {
    return saturn::core::parallel::executor::cancel(handle);
}

extern "C" sat_result_t sat_parallel_release(sat_parallel_handle_t handle) {
    return saturn::core::parallel::executor::release(handle);
}

extern "C" sat_result_t sat_parallel_stats(sat_parallel_stats_t* out_stats) {
    return saturn::core::parallel::executor::stats(out_stats);
}
