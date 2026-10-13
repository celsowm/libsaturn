#include <cassert>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include "saturn/dual_sh2.h"
#include "saturn/skin3d.h"
#include "src/core/parallel/executor.hpp"
#include "src/core/parallel/queue_logic.hpp"
#include "src/hal/sh2/cache.hpp"
#include "src/hal/sh2/frt.hpp"
#include "src/hal/dual_sh2/memory.hpp"

namespace executor=saturn::core::parallel::executor;
constexpr uint32_t kWorkRamBase=0x06000000u,kWorkRamBytes=65536u;
constexpr uint32_t kExecute=0x50524c31u,kComplete=0x50524c32u;
constexpr uint32_t kBatch=0x50524c33u,kBatchComplete=0x50524c34u;
static sat_parallel_task_slot_t* slots;
static sat_dual_sh2_message_t completion;
static bool pending,allow_response;
static sat_result_t worker_result=SAT_OK;
static unsigned master_runs,invalidations;
static sat_dual_sh2_entry_t real_entry;
static bool use_real_worker,worker_request;
static sat_dual_sh2_message_t request;
static unsigned worker_invalidations;
namespace saturn::hal::sh2 {
void compiler_barrier() {}
namespace frt { uint16_t counter(){static uint16_t tick;return ++tick;} }
namespace cache {
uint8_t control_register(){return 0;}
void purge_all(Mode) {}
bool is_supported_work_ram(uint32_t a,uint32_t n){return a>=kWorkRamBase &&
    a+n>=a && a+n<=kWorkRamBase+kWorkRamBytes;}
bool invalidate_range(uint32_t,uint32_t){++invalidations;return true;}
bool invalidate_line(uint32_t){return true;}
}
}
namespace saturn::hal::dual_sh2::memory {
uint32_t cached_address(uint32_t a){return a;}
uint32_t uncached_address(uint32_t a){return a;}
}
extern "C" {
sat_result_t sat_dual_sh2_configure_slave(sat_dual_sh2_entry_t entry,void*){real_entry=entry;return SAT_OK;}
sat_result_t sat_dual_sh2_start(uint32_t){return SAT_OK;}
sat_dual_sh2_state_t sat_dual_sh2_state(){return SAT_DUAL_SH2_OFFLINE;}
uint8_t sat_dual_sh2_available(){return 1;}
sat_result_t sat_dual_sh2_stop(uint32_t){return SAT_OK;}
sat_result_t sat_dual_sh2_send(uint32_t command,uint32_t argument,uint32_t token){
    assert(!pending && (command==kExecute || command==kBatch));
    if(use_real_worker){
        assert(command==kExecute && real_entry);
        request={1,command,argument,token};worker_request=true;
        real_entry(nullptr);return SAT_OK;
    }
    for(unsigned i=0;i<4;++i)if(slots[i].state==SAT_PARALLEL_RUNNING){
        // Real worker writes terminal slots before publishing its mailbox.
        slots[i].state=worker_result==SAT_OK?SAT_PARALLEL_COMPLETED:SAT_PARALLEL_FAILED;
        slots[i].result=worker_result;
        slots[i].output_size=sizeof(uint32_t);
    }
    completion={1,command==kBatch?kBatchComplete:kComplete,token,static_cast<uint32_t>(worker_result)};
    pending=true;return SAT_OK;
}
sat_result_t sat_dual_sh2_receive(sat_dual_sh2_message_t* message){
    if(!pending || !allow_response)return SAT_ERR_NOT_FOUND;
    *message=completion;pending=false;return SAT_OK;
}
sat_result_t sat_dual_sh2_wait_response(sat_dual_sh2_message_t* message,uint32_t){
    assert(pending);*message=completion;pending=false;return SAT_OK;
}
sat_result_t sat_dual_sh2_slave_receive(sat_dual_sh2_message_t* message){
    if(!worker_request)return SAT_ERR_NOT_FOUND;
    *message=request;worker_request=false;worker_invalidations=invalidations;return SAT_OK;
}
sat_result_t sat_dual_sh2_slave_send(uint32_t command,uint32_t token,uint32_t result){
    // No input/output cache calls occur in the callback. Require dispatch-table
    // and count consumption on this CPU between receive and task execution.
    assert(invalidations>=worker_invalidations+2);
    completion={1,command,token,result};pending=true;return SAT_OK;
}
uint8_t sat_dual_sh2_slave_stop_requested(){return !use_real_worker || pending;}
void sat_dual_sh2_slave_heartbeat(){}
sat_result_t sat_dual_sh2_slave_signal_master(){return SAT_OK;}
}
static sat_result_t process(const void*,uint32_t,void*,uint32_t,uint32_t* bytes){
    ++master_runs;*bytes=sizeof(uint32_t);return SAT_OK;
}
static void assert_owned(sat_parallel_handle_t handle){
    uint32_t bytes=0;
    assert(executor::state(handle)==SAT_PARALLEL_RUNNING);
    assert(!executor::done(handle));
    assert(executor::result(handle,&bytes)==SAT_ERR_BUSY);
    assert(executor::release(handle)==SAT_ERR_BUSY);
}
int main(){
#ifdef _WIN32
    void* work=VirtualAlloc(reinterpret_cast<void*>(kWorkRamBase),kWorkRamBytes,
        MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    void* work=mmap(reinterpret_cast<void*>(kWorkRamBase),kWorkRamBytes,
        PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#endif
    assert(work==reinterpret_cast<void*>(kWorkRamBase));
    slots=static_cast<sat_parallel_task_slot_t*>(work);
    auto* input=reinterpret_cast<uint32_t*>(kWorkRamBase+4096u);
    auto* output=reinterpret_cast<uint32_t*>(kWorkRamBase+8192u);
    sat_parallel_config_t config={SAT_PARALLEL_AUTO,slots,4,0,60000};
    assert(executor::init(&config)==SAT_OK);
    assert(executor::register_task(SAT_PARALLEL_TASK_SCENE_GEOMETRY,process)==SAT_OK);
    sat_parallel_handle_t first=0,dependent=0;
    assert(executor::submit(SAT_PARALLEL_TASK_SCENE_GEOMETRY,input,4,output,4,&first)==SAT_OK);
    assert_owned(first);
    // A dependent Master task must also wait for mailbox acknowledgment.
    sat_parallel_submit_desc_t desc={};desc.type=SAT_PARALLEL_TASK_SCENE_GEOMETRY;
    desc.force_master=1;desc.input=input;desc.input_size=4;
    desc.output=output+1;desc.output_capacity=4;desc.depends_on=first;
    assert(executor::submit_ex(&desc,&dependent)==SAT_OK);
    assert(executor::service()==SAT_OK && master_runs==0);
    assert(executor::wait(first,60000)==SAT_OK && !pending);
    uint32_t bytes=0;assert(executor::result(first,&bytes)==SAT_OK && bytes==4);
    assert(invalidations>0 && executor::wait(dependent,60000)==SAT_OK && master_runs==1);
    assert(executor::release(first)==SAT_OK && executor::release(dependent)==SAT_OK);
    // Retain every slot in a batch until the batch completion is consumed.
    allow_response=false;
    sat_parallel_handle_t blocker=0,a=0,b=0;
    assert(executor::submit(SAT_PARALLEL_TASK_SCENE_GEOMETRY,input,4,output,4,&blocker)==SAT_OK);
    assert(executor::submit(SAT_PARALLEL_TASK_SCENE_GEOMETRY,input,4,output+1,4,&a)==SAT_OK);
    assert(executor::submit(SAT_PARALLEL_TASK_SCENE_GEOMETRY,input,4,output+2,4,&b)==SAT_OK);
    assert(executor::wait(blocker,60000)==SAT_OK);
    allow_response=false;
    assert(executor::service()==SAT_OK);assert_owned(a);assert_owned(b);
    assert(executor::wait(a,60000)==SAT_OK && executor::wait(b,60000)==SAT_OK);
    assert(executor::release(blocker)==SAT_OK && executor::release(a)==SAT_OK && executor::release(b)==SAT_OK);
    // An early failure also retains ownership until its message is consumed.
    worker_result=SAT_ERR_IO;allow_response=false;
    sat_parallel_handle_t failed=0;
    assert(executor::submit(SAT_PARALLEL_TASK_SCENE_GEOMETRY,input,4,output,4,&failed)==SAT_OK);
    assert_owned(failed);
    assert(executor::wait(failed,60000)==SAT_ERR_IO);
    assert(executor::result(failed,&bytes)==SAT_ERR_IO && executor::release(failed)==SAT_OK);
    // Abort can safely stop an active task even after the early terminal write.
    worker_result=SAT_OK;
    sat_parallel_handle_t aborted=0;
    assert(executor::submit(SAT_PARALLEL_TASK_SCENE_GEOMETRY,input,4,output,4,&aborted)==SAT_OK);
    assert_owned(aborted);
    assert(executor::abort_task(aborted,60000)==SAT_OK);
    assert(executor::result(aborted,&bytes)==SAT_ERR_TIMEOUT && executor::release(aborted)==SAT_OK);
    assert(executor::shutdown(60000)==SAT_OK);
    // Exercise the real Slave lookup after startup and after a late adapter
    // registration, rather than only the terminal-slot mailbox stub above.
    pending=false;use_real_worker=true;allow_response=false;
    assert(executor::init(&config)==SAT_OK);
    assert(executor::register_task(SAT_PARALLEL_TASK_SKIN_DEFORM,process)==SAT_OK);
    sat_parallel_handle_t late=0;
    assert(executor::submit(SAT_PARALLEL_TASK_SKIN_DEFORM,input,4,output,4,&late)==SAT_OK);
    assert(executor::wait(late,60000)==SAT_OK && executor::release(late)==SAT_OK);
    assert(executor::register_task(SAT_PARALLEL_TASK_SCENE_GEOMETRY,process)==SAT_OK);
    assert(executor::submit(SAT_PARALLEL_TASK_SCENE_GEOMETRY,input,4,output,4,&late)==SAT_OK);
    assert(executor::wait(late,60000)==SAT_OK && executor::release(late)==SAT_OK);
    assert(executor::shutdown(60000)==SAT_OK);
#ifdef _WIN32
    VirtualFree(work,0,MEM_RELEASE);
#else
    munmap(work,kWorkRamBytes);
#endif
    std::puts("parallel completion ownership: OK");
}
