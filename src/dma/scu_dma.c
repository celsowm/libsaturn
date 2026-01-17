#include "saturn/dma.h"
#include "saturn/hardware.h"

void dma_init(void) {
    SCU_D0EN = 0;
}

u32 dma_transfer(DmaChannel ch, const DmaTransfer* t) {
    volatile u32* dmad = &SCU_D0AD + (ch * 4);
    
    dmad[0] = t->src_addr;
    dmad[1] = t->dest_addr;
    dmad[2] = t->size;
    dmad[3] = 0x100 | (t->mode << 4);
    
    return 0;
}

void dma_wait(DmaChannel ch) {
    volatile u32* dmad = &SCU_D0EN + (ch * 4);
    while (*dmad & 0x100);
}

void dma_wait_all(void) {
    for (int i = 0; i < DMA_CH_COUNT; i++) {
        dma_wait(i);
    }
}
