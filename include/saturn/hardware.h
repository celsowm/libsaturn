#ifndef SATURN_HARDWARE_H
#define SATURN_HARDWARE_H

#define VDP1_REGS      0x25D00000
#define VDP1_TVMR      (*(volatile u16*)(VDP1_REGS + 0x0000))
#define VDP1_FBCR      (*(volatile u16*)(VDP1_REGS + 0x0002))
#define VDP1_PTMR      (*(volatile u16*)(VDP1_REGS + 0x0004))
#define VDP1_EWDR      (*(volatile u16*)(VDP1_REGS + 0x0006))
#define VDP1_EWLR      (*(volatile u16*)(VDP1_REGS + 0x0008))
#define VDP1_EWRR      (*(volatile u16*)(VDP1_REGS + 0x000A))
#define VDP1_ENDR      (*(volatile u16*)(VDP1_REGS + 0x000C))
#define VDP1_EDSR      (*(volatile u16*)(VDP1_REGS + 0x0010))
#define VDP1_LOPR      (*(volatile u16*)(VDP1_REGS + 0x0012))
#define VDP1_COPR      (*(volatile u16*)(VDP1_REGS + 0x0014))

#define VDP2_REGS      0x25F80000
#define VDP2_TVMD      (*(volatile u16*)(VDP2_REGS + 0x0000))
#define VDP2_EXTEN     (*(volatile u16*)(VDP2_REGS + 0x0002))
#define VDP2_TVSTAT    (*(volatile u16*)(VDP2_REGS + 0x0004))
#define VDP2_RAMCTL    (*(volatile u16*)(VDP2_REGS + 0x000E))
#define VDP2_CYCA0L    (*(volatile u16*)(VDP2_REGS + 0x0010))
#define VDP2_CYCA0U    (*(volatile u16*)(VDP2_REGS + 0x0012))
#define VDP2_CYCA1L    (*(volatile u16*)(VDP2_REGS + 0x0014))
#define VDP2_CYCA1U    (*(volatile u16*)(VDP2_REGS + 0x0016))
#define VDP2_CYCB0L    (*(volatile u16*)(VDP2_REGS + 0x0018))
#define VDP2_CYCB0U    (*(volatile u16*)(VDP2_REGS + 0x001A))
#define VDP2_CYCB1L    (*(volatile u16*)(VDP2_REGS + 0x001C))
#define VDP2_CYCB1U    (*(volatile u16*)(VDP2_REGS + 0x001E))
#define VDP2_BGON      (*(volatile u16*)(VDP2_REGS + 0x0020))
#define VDP2_CHCTLA    (*(volatile u16*)(VDP2_REGS + 0x0028))
#define VDP2_BMPNA     (*(volatile u16*)(VDP2_REGS + 0x002C))
#define VDP2_MPOFN     (*(volatile u16*)(VDP2_REGS + 0x003C))
#define VDP2_PRISA     (*(volatile u16*)(VDP2_REGS + 0x00F0))
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
