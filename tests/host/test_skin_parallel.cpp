#include "saturn/skin3d.h"
#include <cassert>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>
static sat_parallel_process_fn worker;
static unsigned sync_calls,submissions,master_submissions;
extern "C" sat_result_t sat_parallel_register_task(sat_parallel_task_type_t type,sat_parallel_process_fn fn) {
    assert(type==SAT_PARALLEL_TASK_SKIN_DEFORM);worker=fn;return SAT_OK;
}
extern "C" sat_result_t sat_parallel_cache_sync_range(const void*,uint32_t) { ++sync_calls;return SAT_OK; }
extern "C" void* sat_parallel_uncached_address(const void* p) { return const_cast<void*>(p); }
extern "C" sat_result_t sat_parallel_submit(sat_parallel_task_type_t,const void* job,uint32_t bytes,
    void* output,uint32_t capacity,sat_parallel_handle_t* handle) {
    ++submissions;*handle=submissions;uint32_t written=0;return worker(job,bytes,output,capacity,&written);
}
extern "C" sat_result_t sat_parallel_submit_master(sat_parallel_task_type_t type,const void* job,uint32_t bytes,
    void* output,uint32_t capacity,sat_parallel_handle_t* handle) {
    ++master_submissions;return sat_parallel_submit(type,job,bytes,output,capacity,handle);
}
int main() {
    constexpr unsigned Count=2048,Iterations=50,JointCount=2;
    std::vector<sat_skin_blend_t> records(Count);
    for(unsigned v=0;v<Count;++v) {
        records[v]={{0,1,0},2,{SAT_QUAT16_ONE/2,0},{{(int16_t)(v%100),100,0},{0,100,(int16_t)(v%100)},{0,0,0}}};
    }
    sat_skin_asset_t skin={nullptr,records.data(),nullptr,0,Count,JointCount,12,1};
    sat_mat4_t matrices[JointCount];sat_mat4_identity(&matrices[0]);sat_mat4_translate(&matrices[1],SAT_FX16_ONE,0,0);
    std::vector<sat_vec3_t> expected(Count),one(Count),two(Count);
    assert(sat_skin_validate(&skin)==SAT_OK);
    assert(sat_skin_deform(&skin,matrices,JointCount,expected.data(),Count)==SAT_OK);
    assert(sat_skin_parallel_register()==SAT_OK);
    sat_skin_job_t job={&skin,matrices,one.data(),Count,0,Count,JointCount,0};sat_parallel_handle_t handle;
    assert(sat_skin_job_submit(&job,SAT_PARALLEL_MASTER,&handle)==SAT_OK);
    assert(master_submissions==1 && sync_calls);
    assert(std::memcmp(one.data(),expected.data(),Count*sizeof(sat_vec3_t))==0);
    unsigned prior=submissions;job.output_capacity=Count-1;
    assert(sat_skin_job_submit(&job,SAT_PARALLEL_SLAVE,&handle)==SAT_ERR_CAPACITY && submissions==prior);
    job.output_capacity=Count;job.first=Count;job.count=1;
    assert(sat_skin_job_submit(&job,SAT_PARALLEL_AUTO,&handle)==SAT_ERR_INVALID_ARG && submissions==prior);
    job.first=0;job.count=Count;
    auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<Iterations;++i) {
        assert(sat_skin_deform(&skin,matrices,JointCount,one.data(),Count)==SAT_OK);
        assert(sat_skin_deform(&skin,matrices,JointCount,two.data(),Count)==SAT_OK);
    }
    auto serial=std::chrono::steady_clock::now()-start;
    start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<Iterations;++i) {
        sat_skin_job_t other=job;other.output=two.data();uint32_t written=0;
        std::thread thread([&]{ assert(worker(&other,sizeof(other),two.data(),Count*sizeof(sat_vec3_t),&written)==SAT_OK); });
        assert(worker(&job,sizeof(job),one.data(),Count*sizeof(sat_vec3_t),nullptr)==SAT_OK);thread.join();
    }
    auto parallel=std::chrono::steady_clock::now()-start;
    assert(std::memcmp(one.data(),expected.data(),Count*sizeof(sat_vec3_t))==0);
    assert(std::memcmp(two.data(),expected.data(),Count*sizeof(sat_vec3_t))==0);
    std::printf("two-character host benchmark, %u iterations: serial %lld us, two host threads %lld us; bit-identical outputs (not Saturn timing)\n",
        Iterations,(long long)std::chrono::duration_cast<std::chrono::microseconds>(serial).count(),
        (long long)std::chrono::duration_cast<std::chrono::microseconds>(parallel).count());
}
