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
    cfg.now_ms=nullptr;assert(sat_cd_block_init_ex(&block,&cfg)==SAT_ERR_INVALID_ARG);
    std::puts("CD deadline bus tests passed");
}
