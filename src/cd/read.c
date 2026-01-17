#include "saturn/cd.h"
#include "saturn/hardware.h"

#define CDBLOCK_CMD_GET_STATUS 0x00
#define CDBLOCK_CMD_GET_TOC    0x02
#define CDBLOCK_CMD_PLAY      0x04
#define CDBLOCK_CMD_SEEK      0x05
#define CDBLOCK_CMD_READ      0x06
#define CDBLOCK_CMD_PAUSE     0x0A
#define CDBLOCK_CMD_STOP      0x0B

static volatile u16* cd_regs = (volatile u16*)CDBLOCK_REGS;

void cd_init(void) {
    cd_regs[0] = CDBLOCK_CMD_GET_STATUS;
    while (!(cd_regs[0] & 0x8000));
}

CdStatus cd_read_sector(u32 lba, void* dest) {
    cd_regs[5] = (u16)(lba >> 16);
    cd_regs[6] = (u16)lba;
    cd_regs[7] = 1;
    cd_regs[0] = CDBLOCK_CMD_READ;
    
    while (!(cd_regs[0] & 0x8000));
    
    if (cd_regs[0] & 0x2000) return CD_STATUS_ERROR;
    if (cd_regs[0] & 0x4000) return CD_STATUS_NO_DISC;
    
    volatile u16* data = (volatile u16*)0x25E80000;
    u16* d = (u16*)dest;
    
    for (int i = 0; i < CD_SECTOR_SIZE / 2; i++) {
        d[i] = data[i];
    }
    
    return CD_STATUS_OK;
}

CdStatus cd_read_sectors(u32 lba, u32 count, void* dest) {
    CdStatus status;
    u8* d = (u8*)dest;
    
    for (u32 i = 0; i < count; i++) {
        status = cd_read_sector(lba + i, d + (i * CD_SECTOR_SIZE));
        if (status != CD_STATUS_OK) return status;
    }
    
    return CD_STATUS_OK;
}

CdStatus cd_seek(u32 lba) {
    cd_regs[5] = (u16)(lba >> 16);
    cd_regs[6] = (u16)lba;
    cd_regs[0] = CDBLOCK_CMD_SEEK;
    
    while (!(cd_regs[0] & 0x8000));
    
    return (cd_regs[0] & 0x2000) ? CD_STATUS_ERROR : CD_STATUS_OK;
}

CdStatus cd_pause(void) {
    cd_regs[0] = CDBLOCK_CMD_PAUSE;
    while (!(cd_regs[0] & 0x8000));
    return (cd_regs[0] & 0x2000) ? CD_STATUS_ERROR : CD_STATUS_OK;
}

CdStatus cd_play(u32 lba) {
    cd_regs[5] = (u16)(lba >> 16);
    cd_regs[6] = (u16)lba;
    cd_regs[0] = CDBLOCK_CMD_PLAY;
    while (!(cd_regs[0] & 0x8000));
    return (cd_regs[0] & 0x2000) ? CD_STATUS_ERROR : CD_STATUS_OK;
}

CdStatus cd_stop(void) {
    cd_regs[0] = CDBLOCK_CMD_STOP;
    while (!(cd_regs[0] & 0x8000));
    return (cd_regs[0] & 0x2000) ? CD_STATUS_ERROR : CD_STATUS_OK;
}

u32 cd_toc_lba(u8 track) {
    static u32 toc_lba[100] = {0};
    
    if (toc_lba[0] == 0) {
        cd_regs[0] = CDBLOCK_CMD_GET_TOC;
        while (!(cd_regs[0] & 0x8000));
        
        volatile u16* toc = (volatile u16*)0x25E80000;
        u8* toc_bytes = (u8*)toc;
        
        for (int i = 0; i < 100; i++) {
            u8 min = toc_bytes[i * 13 + 8];
            u8 sec = toc_bytes[i * 13 + 9];
            u8 frame = toc_bytes[i * 13 + 10];
            toc_lba[i] = (min * 60 + sec) * 75 + frame - 150;
        }
    }
    
    return toc_lba[track];
}
