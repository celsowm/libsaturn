#ifndef SATURN_DMA_H
#define SATURN_DMA_H

#include "saturn/types.h"

typedef enum {
    DMA_CH0 = 0,
    DMA_CH1,
    DMA_CH2,
    DMA_CH_COUNT
} DmaChannel;

typedef enum {
    DMA_SRC_CD = 0,
    DMA_SRC_MAIN,
    DMA_SRC_SUB,
    DMA_SRC_VDP1,
    DMA_SRC_VDP2,
    DMA_SRC_SOUND
} DmaSource;

typedef enum {
    DMA_DEST_MAIN,
    DMA_DEST_SUB,
    DMA_DEST_VDP1,
    DMA_DEST_VDP2,
    DMA_DEST_SOUND,
    DMA_DEST_VDP1_SCR,
    DMA_DEST_VDP2_SCR,
    DMA_DEST_VDP2_COL
} DmaDest;

typedef enum {
    DMA_MODE_QUAD,
    DMA_MODE_INDIRECT,
    DMA_MODE_SCATTER_GATHER
} DmaMode;

typedef struct {
    u32 src_addr;
    u32 dest_addr;
    u32 size;
    u32 mode;
} DmaTransfer;

void dma_init(void);
u32 dma_transfer(DmaChannel ch, const DmaTransfer* t);
void dma_wait(DmaChannel ch);
void dma_wait_all(void);

#endif
