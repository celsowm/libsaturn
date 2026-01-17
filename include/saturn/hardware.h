#ifndef SATURN_HARDWARE_H
#define SATURN_HARDWARE_H

#define VDP1_REGS      0x25F00000
#define VDP1_FBCR      (*(volatile u16*)0x25F80000)
#define VDP1_PTMR      (*(volatile u16*)0x25F80008)
#define VDP1_EWDR      (*(volatile u16*)0x25F8000C)
#define VDP1_EWLR      (*(volatile u16*)0x25F80010)
#define VDP1_EWRR      (*(volatile u16*)0x25F80012)
#define VDP1_ENDR      (*(volatile u16*)0x25F80014)
#define VDP1_COAR      (*(volatile u16*)0x25F80018)
#define VDP1_COAG      (*(volatile u16*)0x25F8001A)
#define VDP1_COPR      (*(volatile u16*)0x25F8001C)

#define VDP2_TVMD      (*(volatile u16*)0x25F80000)
#define VDP2_TVSTAT    (*(volatile u16*)0x25F80004)
#define VDP2_EXTEN     (*(volatile u16*)0x25F80010)
#define VDP2_VRAM_SIZE 0x80000

#define SCU_REGS       0x25FE0000
#define SCU_D0AD       (*(volatile u32*)0x25FE0080)
#define SCU_D0EN       (*(volatile u32*)0x25FE0084)
#define SCU_D0MD       (*(volatile u32*)0x25FE0088)

#define SMPC_REGS      0x26000000
#define SMPC_COMREG    (*(volatile u8*)0x20100060)
#define SMPC_SF        (*(volatile u8*)0x20100061)

#define SCSP_REGS      0x25B00000

#define CDBLOCK_REGS  0x25F90000
#define HIRQ           (*(volatile u16*)0x25F90900)
#define CR1            (*(volatile u16*)0x25F90908)
#define CR2            (*(volatile u16*)0x25F9090A)
#define CR3            (*(volatile u16*)0x25F9090C)
#define CR4            (*(volatile u16*)0x25F9090E)
#define FA             (*(volatile u16*)0x25F90910)
#define FB             (*(volatile u16*)0x25F90912)
#define FC             (*(volatile u16*)0x25F90914)
#define FD             (*(volatile u16*)0x25F90916)

#endif
