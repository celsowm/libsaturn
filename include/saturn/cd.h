#ifndef SATURN_CD_H
#define SATURN_CD_H

#include "saturn/types.h"

#define CD_SECTOR_SIZE 2048
#define CD_MAX_LBA 0xFFFFFF

typedef enum {
    CD_STATUS_OK = 0,
    CD_STATUS_BUSY,
    CD_STATUS_ERROR,
    CD_STATUS_NO_DISC
} CdStatus;

typedef enum {
    CD_MODE_READ,
    CD_MODE_PAUSE,
    CD_MODE_STOP,
    CD_MODE_SEEK
} CdMode;

typedef struct {
    u8 track;
    u8 index;
    u8 min;
    u8 sec;
    u8 frame;
    u8 zero;
    u8 amin;
    u8 asec;
    u8 aframe;
} __attribute__((packed)) CdTime;

void cd_init(void);
CdStatus cd_read_sector(u32 lba, void* dest);
CdStatus cd_read_sectors(u32 lba, u32 count, void* dest);
CdStatus cd_seek(u32 lba);
CdStatus cd_pause(void);
CdStatus cd_play(u32 lba);
CdStatus cd_stop(void);
u32 cd_toc_lba(u8 track);

#endif
