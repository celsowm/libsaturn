#include "saturn/skin3d.h"

namespace {
constexpr uint32_t kAutoSlaveMinimumVertices=128u;
sat_result_t sync(const void* p, uint32_t n) {
    return n ? sat_parallel_cache_sync_range(p, n) : SAT_OK;
}
sat_result_t process(const void* input, uint32_t bytes, void* output,
                     uint32_t capacity, uint32_t* written) {
    if (!input || bytes != sizeof(sat_skin_job_t)) return SAT_ERR_INVALID_ARG;
    const auto& job = *static_cast<const sat_skin_job_t*>(input);
    if (capacity < job.output_capacity * sizeof(sat_vec3_t)) return SAT_ERR_CAPACITY;
    sat_skin_asset_t skin = *static_cast<const sat_skin_asset_t*>(sat_parallel_uncached_address(job.skin));
    skin.rigid = static_cast<const sat_skin_rigid_t*>(sat_parallel_uncached_address(skin.rigid));
    skin.blend = static_cast<const sat_skin_blend_t*>(sat_parallel_uncached_address(skin.blend));
    const auto* matrices = static_cast<const sat_mat4_t*>(sat_parallel_uncached_address(job.joint_world));
    SAT_TRY(sat_skin_deform_range(&skin, matrices, job.joint_world_count,
        job.first, job.count, static_cast<sat_vec3_t*>(output), job.output_capacity));
    if (written) *written = job.output_capacity * sizeof(sat_vec3_t);
    return SAT_OK;
}
}
extern "C" sat_result_t sat_skin_parallel_register(void) {
    return sat_parallel_register_task(SAT_PARALLEL_TASK_SKIN_DEFORM, process);
}
extern "C" sat_result_t sat_skin_job_submit(const sat_skin_job_t* job,
    sat_parallel_mode_t policy, sat_parallel_handle_t* handle) {
    if (!job || !job->skin || !job->joint_world || !job->output || !handle ||
        policy < SAT_PARALLEL_MASTER || policy > SAT_PARALLEL_AUTO ||
        job->joint_world_count < job->skin->joint_count) return SAT_ERR_INVALID_ARG;
    const uint32_t vertices = sat_skin_vertex_count(job->skin);
    if (job->output_capacity < vertices) return SAT_ERR_CAPACITY;
    if (job->output_capacity > UINT32_MAX / sizeof(sat_vec3_t) ||
        job->first > vertices || job->count > vertices - job->first) return SAT_ERR_INVALID_ARG;
    if (job->skin->max_joint >= job->joint_world_count || job->skin->pos_shift > 15)
        return SAT_ERR_INVALID_ARG;
    SAT_TRY(sync(job->skin, sizeof(*job->skin)));
    SAT_TRY(sync(job->skin->rigid, job->skin->rigid_count * sizeof(sat_skin_rigid_t)));
    SAT_TRY(sync(job->skin->blend, job->skin->blend_count * sizeof(sat_skin_blend_t)));
    SAT_TRY(sync(job->joint_world, job->joint_world_count * sizeof(sat_mat4_t)));
    // AUTO avoids paying worker publication/join overhead for tiny skins.
    if (policy == SAT_PARALLEL_MASTER || (policy == SAT_PARALLEL_AUTO && job->count < kAutoSlaveMinimumVertices))
        return sat_parallel_submit_master(SAT_PARALLEL_TASK_SKIN_DEFORM,
            job, sizeof(*job), job->output, job->output_capacity * sizeof(sat_vec3_t), handle);
    return sat_parallel_submit(SAT_PARALLEL_TASK_SKIN_DEFORM,
        job, sizeof(*job), job->output, job->output_capacity * sizeof(sat_vec3_t), handle);
}
