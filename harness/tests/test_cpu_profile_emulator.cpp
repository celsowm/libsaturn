// SPDX-License-Identifier: GPL-3.0-only
#include "cpu_profile.hpp"
#include <ymir/sys/saturn.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
static void check(bool ok) { if(!ok) {std::cerr<<"CPU trace integration failed\n";std::exit(1);} }
struct Trace final:ymir::debug::ISH2Tracer {
    harness::CpuProfile stats;
    uint64_t returned_cycles=0;
    unsigned executions=0,completions=0,entries=0;
    uint32_t last_pc=0,call_target=0,return_target=0;
    bool last_instruction=true;
    Trace(){stats.active=true;}
    void ExecuteInstruction(uint32_t pc,uint16_t,bool) override {++executions;last_pc=pc;}
    void ExecutionStepCompleted(uint32_t pc,uint64_t cycles,bool instruction) override {
        ++completions;returned_cycles=cycles;last_instruction=instruction;
        if(instruction)check(pc==last_pc);
        stats.complete(pc,cycles,instruction);
    }
    void Call(uint32_t target) override{call_target=target;}
    void Return(uint32_t target) override{return_target=target;}
    void Interrupt(uint8_t,uint8_t,ymir::sh2::InterruptSource,uint32_t) override{++entries;}
};
template<bool cache>void run() {
    auto machine=std::make_unique<ymir::Saturn>();
    machine->EnableSH2CacheEmulation(cache);
    machine->EnableDebugTracing(true);
    Trace master,slave;
    machine->masterSH2.UseTracer(&master);machine->slaveSH2.UseTracer(&slave);
    const uint32_t base=0x06004000;
    auto& p=machine->masterSH2.GetProbe();
    p.PC()=base;p.R(0)=0;p.R(15)=0x060ff000;p.SR().u32=0xf0;
    p.GetCache().WriteCCR<true>(cache?1:0);
    for(auto [offset,opcode]:{std::pair{0,0xe003}, {2,0xb004},{4,0x0009},
                            {6,0x0009},{14,0x7002},{16,0x000b},{18,0x0009}})
        p.MemWriteWord(base+offset,static_cast<uint16_t>(opcode),false);
    p.RefillPipeline();
    uint64_t sum=0;
    for(unsigned i=0;i<6;++i) {
        const auto cost=machine->masterSH2.Step<true,cache>();sum+=cost;
        check(master.completions==i+1 && master.executions==i+1 && master.returned_cycles==cost);
    }
    check(p.R(0)==5 && p.PC()==base+6 && p.PR()==base+6);
    check(master.call_target==base+14 && master.return_target==base+6);
    check(master.stats.cycles==sum && master.stats.instructions==6 && slave.completions==0);
    // A separately stepped Slave must not be charged to Master.
    auto& s=machine->slaveSH2.GetProbe();s.PC()=base+6;s.SR().u32=0xf0;s.RefillPipeline();
    const auto slave_cost=machine->slaveSH2.Step<true,cache>();
    check(slave.stats.instructions==1 && slave.stats.cycles==slave_cost && master.stats.cycles==sum);
    // NMI entry has a modeled cost but must not manufacture an instruction.
    p.VBR()=0x06001000;p.MemWriteLong(p.VBR()+11*4,base+6,false);
    machine->masterSH2.SetNMI();
    for(unsigned i=0;i<3 && master.entries==0;++i)machine->masterSH2.Step<true,cache>();
    check(master.entries==1 && !master.last_instruction && master.stats.interrupt_cycles>0);
    machine->masterSH2.UseTracer(nullptr);machine->slaveSH2.UseTracer(nullptr);
}
int main(){run<false>();run<true>();std::cout<<"SH-2 profile integration passed (cache off/on)\n";}
