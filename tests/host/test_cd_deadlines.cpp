/* Exercise the actual HAL against a deterministic fake CD register bus. */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "saturn/cd_block.h"

static uint16_t words[4], hirq;
static uint32_t clock_value, due, latency, ready_sectors;
static bool pending, never_complete;
static uint16_t sat_cd_test_read(uint32_t address) {
    if(address==SAT_CD_BLOCK_HIRQ) {
        if(pending && !never_complete && int32_t(clock_value-due)>=0) {
            pending=false;hirq=0xffffu;words[0]=0;words[3]=uint16_t(ready_sectors);
        }
        return hirq;
    }
    const uint32_t regs[]={SAT_CD_BLOCK_CR1,SAT_CD_BLOCK_CR2,SAT_CD_BLOCK_CR3,SAT_CD_BLOCK_CR4};
    for(unsigned i=0;i<4;++i)if(address==regs[i])return words[i];
    return 0;
}
static void sat_cd_test_write(uint32_t address,uint16_t value) {
    if(address==SAT_CD_BLOCK_HIRQ){hirq&=value;return;}
    const uint32_t regs[]={SAT_CD_BLOCK_CR1,SAT_CD_BLOCK_CR2,SAT_CD_BLOCK_CR3,SAT_CD_BLOCK_CR4};
    for(unsigned i=0;i<4;++i)if(address==regs[i])words[i]=value;
    if(address==SAT_CD_BLOCK_CR4){pending=true;due=clock_value+latency;}
}
#define SAT_CD_BLOCK_TEST_BUS 1
#include "src/hal/cd/block.cpp"
extern "C" void sat_cd_notify_arrival(const void*) {}
extern "C" sat_result_t sat_cd_device_init(sat_cd_device_t* out,sat_cd_read_sectors_fn fn,void* context,uint32_t count) {
    *out={fn,context,count};return SAT_OK;
}
static uint32_t fake_clock(void*) {return clock_value++;}
static void reset(uint32_t start=0) {
    clock_value=start;hirq=0xffff;latency=2;ready_sectors=0;
    pending=never_complete=false;
    for(auto& word:words)word=0;
}
int main() {
    sat_cd_block_t block{};
    sat_cd_block_config_t cfg{fake_clock,nullptr,40,200};
    reset();assert(sat_cd_block_init_ex(&block,&cfg)==SAT_OK);
    assert(block.initialized && block.diagnostics.response_valid);
    const uint16_t cmd[4]={0,0,0,0};uint16_t response[4]={0xbeef,0xbeef,0xbeef,0xbeef};
    never_complete=true;
    assert(sat_cd_block_command(&block,cmd,response)==SAT_ERR_TIMEOUT);
    assert(!block.diagnostics.response_valid && block.diagnostics.timeout_count==1);
    assert(response[0]==0xbeef && block.diagnostics.elapsed_ms>=40);
    assert(!block.read_active);
    reset(0xfffffff0u);assert(sat_cd_block_init_ex(&block,&cfg)==SAT_OK);
    assert(block.diagnostics.elapsed_ms<cfg.operation_timeout_ms);
    block.read_active=1;assert(sat_cd_block_command(&block,cmd,response)==SAT_ERR_BUSY);
    block.read_active=0;
    reset();assert(sat_cd_block_init_ex(&block,&cfg)==SAT_OK);
    uint8_t sector[2048];
    // Commands complete promptly, CSCT stays high, but no sector ever arrives.
    // The aggregate deadline must still expire across repeated successful waits.
    assert(sat_cd_block_read_sectors(&block,21,1,sector)==SAT_ERR_TIMEOUT);
    assert(!block.read_active && block.diagnostics.bytes_transferred==0);
    assert(block.diagnostics.timeout_count==1 && block.diagnostics.elapsed_ms>=200);
    reset();latency=15;cfg.operation_timeout_ms=50;
    assert(sat_cd_block_init_ex(&block,&cfg)==SAT_ERR_TIMEOUT);
    assert(!block.initialized && block.diagnostics.timeout_count==1);
    reset();cfg.operation_timeout_ms=200;
    assert(sat_cd_block_init_ex(&block,&cfg)==SAT_OK);
    ready_sectors=1;
    assert(sat_cd_block_read_sectors(&block,21,1,sector)==SAT_OK);
    assert(block.diagnostics.bytes_transferred==2048 && block.diagnostics.lba==21);
    assert(sat_cd_stream_begin(&block,21,1,sector)==SAT_OK);
    never_complete=true;
    assert(sat_cd_stream_finish(&block)==SAT_ERR_TIMEOUT);
    assert(!block.read_active && !block.stream_active);
    // A ring stream: one play over more sectors than the ring holds, the caller's room as the limit.
    reset();cfg.operation_timeout_ms=200;assert(sat_cd_block_init_ex(&block,&cfg)==SAT_OK);
    static uint8_t ring[4*2048+2048];
    assert(sat_cd_stream_begin_ring(nullptr,21,10,ring,4)==SAT_ERR_INVALID_ARG);
    assert(sat_cd_stream_begin_ring(&block,21,0,ring,4)==SAT_ERR_INVALID_ARG);
    assert(sat_cd_stream_begin_ring(&block,21,0x1000000u,ring,4)==SAT_ERR_INVALID_ARG);
    assert(sat_cd_stream_begin_ring(&block,21,10,ring,0)==SAT_ERR_INVALID_ARG);
    assert(sat_cd_stream_begin_ring(&block,21,10,ring+1,4)==SAT_ERR_INVALID_ARG);
    assert(sat_cd_stream_begin_ring(&block,21,10,nullptr,4)==SAT_ERR_INVALID_ARG);
    for(unsigned i=0;i<sizeof ring;++i)ring[i]=0xAA;
    ready_sectors=3;
    assert(sat_cd_stream_begin_ring(&block,21,10,ring,4)==SAT_OK);
    assert(sat_cd_stream_begin(&block,21,1,sector)==SAT_ERR_BUSY);
    uint32_t remaining=0;
    auto zeros=[&](unsigned first,unsigned count) {   // the fake bus stores zero words: they mark where sectors landed
        for(unsigned s=first;s<first+count;++s)for(unsigned i=0;i<2048;++i)if(ring[s*2048+i]!=0)return false;
        return true;
    };
    assert(sat_cd_stream_poll(&block,2,&remaining)==SAT_OK && remaining==8);     // the room allows 2 of the 3 that are ready
    assert(zeros(0,2) && ring[2*2048]==0xAA);
    assert(sat_cd_stream_poll(&block,0,&remaining)==SAT_OK && remaining==6);     // 3 are ready, 2 fit before the end
    assert(zeros(2,2));
    for(unsigned i=0;i<sizeof ring;++i)ring[i]=0xAA;                              // the next sectors must start at the ring's base
    assert(sat_cd_stream_poll(&block,0,&remaining)==SAT_OK && remaining==3);
    assert(zeros(0,3) && ring[3*2048]==0xAA && ring[4*2048]==0xAA);
    assert(sat_cd_stream_poll(&block,0,&remaining)==SAT_OK && remaining==2);     // 1 fits before the end
    assert(sat_cd_stream_poll(&block,0,&remaining)==SAT_OK && remaining==0);     // the last 2: the stream closes
    assert(!block.stream_active && block.stream_ring_end==nullptr);
    // Abort: closes at once, and a plain stream may begin again.
    assert(sat_cd_stream_begin_ring(&block,21,10,ring,4)==SAT_OK);
    assert(sat_cd_stream_abort(&block)==SAT_OK && !block.stream_active && sat_cd_stream_remaining(&block)==0);
    assert(sat_cd_stream_abort(&block)==SAT_OK && sat_cd_stream_abort(nullptr)==SAT_ERR_INVALID_ARG);
    assert(sat_cd_stream_begin(&block,21,1,sector)==SAT_OK);
    never_complete=false;
    cfg.now_ms=nullptr;assert(sat_cd_block_init_ex(&block,&cfg)==SAT_ERR_INVALID_ARG);
    std::puts("CD deadline bus tests passed");
}
