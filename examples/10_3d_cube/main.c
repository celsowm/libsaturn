#include "saturn/shared.h"
#include "saturn/system.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/peripheral.h"

SharedData shared;

extern void _slave_main(void);

void _main(void) {
    system_init();
    vdp1_init();
    vdp2_init();
    peripheral_init();
    
    dualcpu_init();
    dualcpu_start_slave();
    
    shared.state = SHARED_STATE_SLAVE_WORKING;
    PadState pad;
    
    while (1) {
        peripheral_read(&pad);
        shared.input.x = pad.axis_x;
        shared.input.y = pad.axis_y;
        shared.input.buttons = pad.buttons;
        
        dualcpu_signal_slave();
        dualcpu_wait_for_slave();
        
        vdp1_start_frame();
        vdp1_wait_for_vblank();
        vdp1_end_frame();
    }
}

void _slave_main(void) {
    extern void slave_main(void);
    slave_main();
}
