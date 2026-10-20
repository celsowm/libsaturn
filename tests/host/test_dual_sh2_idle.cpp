#include <cassert>
#include <cstdio>
#include "src/hal/dual_sh2/idle_wait.hpp"

using namespace saturn::hal::dual_sh2;

struct Transport {
    uint16_t tick=0;
    unsigned polls=0, shared_checks=0, heartbeats=0, scans=0;
    unsigned arrival=700, stop_at=0;
    bool flag=false, queued=false, suppress_signal=false, publish_on_clear=false;
    uint16_t now() {
        ++polls; ++tick;
        if(polls==arrival) {queued=true;if(!suppress_signal)flag=true;}
        return tick;
    }
    bool pending() {return flag;}
    void acknowledge() {
        flag=false;
        if(publish_on_clear)queued=true;
    }
    void heartbeat() {++heartbeats;}
    bool stopping() {++shared_checks;return stop_at && polls>=stop_at;}
    bool receive(protocol::Message* m) {
        ++scans;
        if(!queued)return false;
        queued=false;flag=false;*m={1,42,17,23};return true;
    }
};

int main() {
    protocol::Message message={};
    Transport normal;
    assert(wait_for_request(normal,&message));
    assert(message.command==42 && normal.polls>=700 && normal.polls<=702);
    assert(normal.shared_checks<6 && normal.heartbeats<5);
    // A lost/coalesced signal cannot strand an already published sequence.
    Transport lost;lost.suppress_signal=true;
    assert(wait_for_request(lost,&message));
    assert(lost.polls>=700 && lost.polls<=700+kIdleMaintenanceTicks);
    // A request published before the captured flag is cleared is consumed.
    Transport clear_race;clear_race.flag=true;clear_race.publish_on_clear=true;
    assert(wait_for_request(clear_race,&message) && clear_race.polls==2);
    // A spurious flag is cleared, then the normal request still arrives.
    Transport spurious;spurious.flag=true;
    assert(wait_for_request(spurious,&message) && spurious.shared_checks<7);
    // Shutdown is bounded even without a signal and across FRT rollover.
    Transport stop;stop.arrival=100000;stop.stop_at=550;stop.tick=65500;
    assert(!wait_for_request(stop,&message));
    assert(stop.polls>=550 && stop.polls<=550+kIdleMaintenanceTicks);
    // A second published request with no new edge is checked on entry.
    normal.queued=true;normal.flag=false;
    assert(wait_for_request(normal,&message));
    std::puts("PASS: local FRT idle wait, signal races, maintenance and shutdown");
}
