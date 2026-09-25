/* Skybridge 3D: per-frame metrics and parallel-task accounting. */
#include "skybridge.h"

sb_frame_metrics_t g_metrics;
static uint32_t g_last_parallel_wait;
static uint32_t g_last_parallel_submitted;
static uint32_t g_last_parallel_failed;
static sat_parallel_stats_t g_last_parallel_stats;
/* A failed abort/release is an ownership failure, not a normal task error.
 * Keep all associated storage pinned and stop dispatching until reset. */
uint8_t g_parallel_recovery_blocked;

void parallel_recovery_failed(void) {
    g_parallel_recovery_blocked=1u;
    ++g_metrics.failures;
}

uint32_t parallel_slave_task_count(void) {
    sat_parallel_stats_t stats={0};
    return sat_parallel_stats(&stats)==SAT_OK?stats.slave_tasks:0u;
}

uint32_t parallel_master_task_count(void) {
    sat_parallel_stats_t stats={0};
    return sat_parallel_stats(&stats)==SAT_OK?stats.master_tasks:0u;
}

void record_task_dispatch(uint32_t slave_before,uint32_t master_before,
                                 uint16_t* slave_count,uint16_t* master_count) {
    const uint32_t slave_after=parallel_slave_task_count();
    const uint32_t master_after=parallel_master_task_count();
    if(slave_after!=slave_before)++*slave_count;
    else if(master_after!=master_before)++*master_count;
}

uint32_t hash_word(uint32_t hash,uint32_t value) {
    for(uint8_t byte=0u;byte<4u;++byte) {
        hash^=(value>>(24u-(uint32_t)byte*8u))&0xFFu;
        hash*=16777619u;
    }
    return hash;
}

static uint32_t hash_scene_face(uint32_t hash,const sat_scene3d_face_t* face,
                                uint32_t key) {
    hash=hash_word(hash,key);
    hash=hash_word(hash,face->projected_safe);
    hash=hash_word(hash,face->gouraud_valid);
    hash=hash_word(hash,face->material.kind);
    hash=hash_word(hash,face->material.rgb555);
    hash=hash_word(hash,face->material.color_calc_slot);
    for(uint8_t vertex=0u;vertex<4u;++vertex) {
        /* world is stored only for the clipping fallback. */
        if(!face->projected_safe) {
            hash=hash_word(hash,(uint32_t)face->world.v[vertex].x);
            hash=hash_word(hash,(uint32_t)face->world.v[vertex].y);
            hash=hash_word(hash,(uint32_t)face->world.v[vertex].z);
        }
        hash=hash_word(hash,(uint16_t)face->projected.x[vertex]);
        hash=hash_word(hash,(uint16_t)face->projected.y[vertex]);
        if(face->gouraud_valid)hash=hash_word(hash,face->gouraud[vertex]);
    }
    if(face->material.texture!=0) {
        const sat_vdp1_texture_t* texture=face->material.texture;
        hash=hash_word(hash,texture->srca);
        hash=hash_word(hash,texture->width);
        hash=hash_word(hash,texture->height);
        hash=hash_word(hash,texture->palette);
        hash=hash_word(hash,texture->valid);
    }
    return hash;
}

void record_gem_batch_merge(const sat_scene3d_prepare_batch_t* batch) {
    uint16_t i;
    if(g_metrics.gem_merge_hash==0u)g_metrics.gem_merge_hash=2166136261u;
    for(i=0u;i<batch->metrics.prepared_faces;++i) {
        g_metrics.gem_merge_hash=hash_scene_face(g_metrics.gem_merge_hash,
                                                  &batch->faces[i],batch->keys[i]);
    }
    ++g_metrics.gem_merge_batches;
    g_metrics.gem_merged_faces=(uint16_t)(g_metrics.gem_merged_faces+
                                           batch->metrics.prepared_faces);
}

void record_parallel_snapshot(void) {
    sat_parallel_stats_t stats={0};
    const uint32_t local_failures=g_metrics.failures;
    if(sat_parallel_stats(&stats)!=SAT_OK)return;
    g_metrics.task_count=(uint16_t)((stats.submitted-g_last_parallel_submitted)>0xFFFFu
        ?0xFFFFu:stats.submitted-g_last_parallel_submitted);
    {
        const uint32_t failures=local_failures+
            (stats.failed-g_last_parallel_failed);
        g_metrics.failures=(uint16_t)(failures>0xFFFFu?0xFFFFu:failures);
    }
    /* master_wait_ticks is a raw SH-2 FRT delta. Keep it separate from the
     * millisecond samples collected around the actual wait calls below. */
    g_metrics.master_wait_frt_ticks=stats.master_wait_ticks-g_last_parallel_wait;
    g_metrics.task_wait_frt_ticks=stats.master_wait_ticks-
        g_last_parallel_stats.master_wait_ticks;
    g_metrics.task_completion_frt_ticks=stats.completion_ticks-
        g_last_parallel_stats.completion_ticks;
    g_metrics.task_master_frt_ticks=stats.master_task_ticks-
        g_last_parallel_stats.master_task_ticks;
    g_metrics.task_slave_frt_ticks=stats.slave_task_ticks-
        g_last_parallel_stats.slave_task_ticks;
    g_last_parallel_stats=stats;
    g_last_parallel_wait=stats.master_wait_ticks;
    g_last_parallel_submitted=stats.submitted;
    g_last_parallel_failed=stats.failed;
}
