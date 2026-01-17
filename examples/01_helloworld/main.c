#include "saturn/types.h"

void _main(void) {
    volatile u16* fb = (volatile u16*)0x25E00000;
    
    for (int i = 0; i < 320 * 224; i++) {
        fb[i] = 0x8000;
    }
    
    while (1);
}
