/*
 * libsaturn Hello World Example
 *
 * This example demonstrates basic libsaturn usage for Sega Saturn development.
 * It initializes Saturn hardware and shows how to use the library.
 */

#include <saturn/system.h>
#include <saturn/peripheral.h>
#include <saturn/vdp1.h>
#include <saturn/vdp2.h>

void wait_for_button(void) {
    PadState state;

    peripheral_init();

    while (1) {
        peripheral_read(&state);

        if (state.buttons & BTN_START) {
            break;
        }

        interrupt_wait_vblank();
    }
}

void draw_frame(void) {
    vdp1_start_frame();
    vdp1_clear_screen(0x0000);
    vdp1_end_frame();
}

int main(void) {
    system_init();
    vdp1_init();
    vdp2_init();

    while (1) {
        draw_frame();
        wait_for_button();
        break;
    }

    system_halt();
    return 0;
}
