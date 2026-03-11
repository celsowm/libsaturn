# Sega Saturn — Expanded Memory Map Reference

All addresses are for the master SH-2 address space.
The slave SH-2 has an identical view of the bus.

---

## Full Address Space Breakdown

```
Address Range              Size     Device / Region
────────────────────────────────────────────────────────────────────────────
0x00000000–0x000FFFFF      1 MB     BIOS ROM (read-only)
                                    SH-2 reset vectors here (VBR default=0)
                                    Access width: 8/16/32-bit
                                    Wait states: 0 (cached via cache)

0x00100000–0x0010FFFF      64 KB    SMPC (System Management & Peripheral
                                    Control) registers
                                    Access: 8-bit only
                                    Key registers:
                                      0x00100001  SMPC command register
                                      0x00100003  SMPC status register
                                      0x00100005  Status flag
                                      0x00100007  Input register 1
                                      0x00100009  Input register 2
                                      0x0010001F  OREG base (15 output regs)

0x00180000–0x0018FFFF      64 KB    Internal Backup RAM (battery-backed SRAM)
                                    Access: 8-bit only (even addresses)
                                    Used for save data when no cart present

0x00200000–0x003FFFFF      256 KB   Low Work RAM (LWRAM)
                                    Access: 8/16/32-bit
                                    Faster than HWRAM for SCU DMA targets
                                    Mirrored: 0x20200000–0x203FFFFF

0x01000000–0x017FFFFF      8 MB     A-Bus CS0 (cartridge slot area 1)
                                    ROM cart, RAM cart, Netlink etc.

0x01800000–0x01FFFFFF      8 MB     A-Bus CS1 (cartridge slot area 2)

0x02000000–0x023FFFFF      4 MB     A-Bus CS2 — CD-ROM block
                                    Key registers (access 16-bit):
                                      0x02000000  HIRQ (host IRQ flags)
                                      0x02000002  HIRQ mask
                                      0x02000004  CR1 (command/response)
                                      0x02000006  CR2
                                      0x02000008  CR3
                                      0x0200000A  CR4
                                    Transfer buffer: 0x02020000–0x0202FFFF

0x02400000–0x027FFFFF      4 MB     A-Bus CS3 (expansion; Netlink etc.)

0x04000000–0x047FFFFF      8 MB     Cart ROM area (mirrors CS0 in some carts)

0x05800000–0x058FFFFF      1 MB     Sound RAM (SCSP / 68EC000 shared)
                                    Access: 8/16-bit from SH-2
                                    68EC000 runs at 11.3 MHz
                                    SCSP registers: 0x05B00000–0x05BFFFFF

0x05A00000–0x05AFFFFF      1 MB     SCU (System Control Unit) registers
                                    Key registers:
                                      0x05A00000  D0R  DMA ch0 read addr
                                      0x05A00004  D0W  DMA ch0 write addr
                                      0x05A00008  D0C  DMA ch0 count
                                      0x05A0000C  D0AD DMA addr-add (src/dst)
                                      0x05A00010  D0EN DMA ch0 enable
                                      0x05A00014  D0MD DMA ch0 mode
                                      0x05A00020  D1R/D1W/D1C/D1AD/D1EN/D1MD
                                      0x05A00040  D2R/D2W/D2C/D2AD/D2EN/D2MD
                                      0x05A00060  DSTA DMA status
                                      0x05A00080  IST  interrupt status
                                      0x05A00084  AIACK IRQ ack
                                      0x05A00090  ASR0 A-bus select 0
                                      0x05A00094  ASR1 A-bus select 1
                                      0x05A00098  AREF A-bus refresh
                                      0x05A000A0  RSEL timer select
                                      0x05A000B0  T0C  timer 0 counter

0x05C00000–0x05C7FFFF      512 KB   VDP1 VRAM
                                    16-bit access. Stores:
                                      - Polygon command list
                                      - Textures (8bpp/16bpp)
                                      - Sprite data
                                    First 512 bytes: VDP1 command list

0x05C80000–0x05CFFFFF      512 KB   VDP1 Framebuffer (2 alternating)
                                    16-bit access.
                                    Each buffer: 256 KB
                                    Resolution: 512×256 at 16bpp typical

0x05D00000–0x05D0001F      32 B     VDP1 Registers
                                      0x05D00000  TVMR  TV mode
                                      0x05D00002  FBCR  framebuffer control
                                      0x05D00004  PTMR  plot trigger mode
                                      0x05D00006  EWDR  erase/write data
                                      0x05D00008  EWLR  erase/write upper-left
                                      0x05D0000A  EWRR  erase/write lower-right
                                      0x05D0000C  ENDR  draw end
                                      0x05D00010  EDSR  draw status (read)
                                      0x05D00012  LOPR  last op (read)
                                      0x05D00014  COPR  current op (read)
                                      0x05D00016  MODR  mode status (read)

0x05E00000–0x05EFFFFF      1 MB     VDP2 VRAM
                                    16-bit access. 4 banks:
                                      Bank A0: 0x05E00000 (256 KB)
                                      Bank A1: 0x05E40000 (256 KB)
                                      Bank B0: 0x05E80000 (256 KB)
                                      Bank B1: 0x05EC0000 (256 KB)
                                    Stores: BG pattern data, tilemaps,
                                    character patterns, rotation params

0x05F00000–0x05F7FFFF      512 KB   VDP2 Color RAM (CRAM)
                                    16-bit access.
                                    1024 or 2048 entries × 16-bit RGB555

0x05F80000–0x05FBFFFF      256 KB   VDP2 Registers (first 256 bytes used)
                                    Key registers (16-bit access):
                                      0x05F80000  TVMD  display mode
                                      0x05F80002  EXTEN external sync
                                      0x05F80004  TVSTAT status (read)
                                      0x05F80006  VRSIZE VRAM size
                                      0x05F80008  HCNT  H counter (read)
                                      0x05F8000A  VCNT  V counter (read)
                                      0x05F80010  RAMCTL VRAM control
                                      ... (many more cycle-pattern regs)
                                      0x05F80098  BGON  layer enable
                                      0x05F800E0  SCXIN0 scroll X NBG0 int
                                      0x05F800E2  SCYIN0 scroll Y NBG0 int

0x05FE0000–0x05FEFFFF      64 KB    SCU (mirror / additional SCU regs)

0x05FF0000–0x05FFFFFF      64 KB    Master SH-2 on-chip registers
                                    (Timers, DMAC, SCI, WDT, BSC)
                                    Key:
                                      0x05FFFF10  IPRA interrupt priority A
                                      0x05FFFF12  IPRB interrupt priority B
                                      0x05FFFF60  ICR interrupt control

0x06000000–0x060FFFFF      1 MB     High Work RAM (HWRAM)
                                    32-bit access, 0 wait states cached
                                    Boot layout (recommended):
                                      0x06000000–0x06001FFF  Slave stack
                                      0x06002000–0x06003FFF  Master stack
                                      0x06004000–0x0600FFFF  IP.BIN image
                                      0x06010000–0x060EFFFF  Game binary
                                      0x060F0000–0x060FFFFF  Exception table
```

---

## Bus Access Width Rules

| Region         | 8-bit | 16-bit | 32-bit | Notes                         |
|----------------|-------|--------|--------|-------------------------------|
| BIOS ROM       | ✓     | ✓      | ✓      | Read-only                     |
| SMPC           | ✓     | ✗      | ✗      | Even addresses only           |
| Backup SRAM    | ✓     | ✗      | ✗      | Even addresses only           |
| LWRAM          | ✓     | ✓      | ✓      | Prefer 32-bit for speed       |
| HWRAM          | ✓     | ✓      | ✓      | Fastest RAM; use for hot code |
| VDP1 VRAM      | ✗     | ✓      | ✗      | 16-bit only                   |
| VDP1 Framebuf  | ✗     | ✓      | ✗      | 16-bit only                   |
| VDP1 Registers | ✗     | ✓      | ✗      | 16-bit only                   |
| VDP2 VRAM      | ✗     | ✓      | ✗      | 16-bit only                   |
| VDP2 CRAM      | ✗     | ✓      | ✗      | 16-bit only                   |
| VDP2 Registers | ✗     | ✓      | ✗      | 16-bit only                   |
| Sound RAM      | ✓     | ✓      | ✗      | 8 or 16-bit from SH-2         |
| SCU Registers  | ✗     | ✗      | ✓      | 32-bit only                   |
| CD-ROM regs    | ✗     | ✓      | ✗      | 16-bit only                   |
| Cart slot      | ✓     | ✓      | ✓      | Depends on cart type          |

---

## Slave SH-2 Startup

The slave SH-2 is held in reset by the SCU at boot. Your IP or game
code must release it and give it a starting address:

```asm
; Release slave SH-2
; SCU register SLSFL at 0x05A000C0 (not standard — use SMPC approach):

; Method: write to SMPC to power on slave SH-2
; Then write slave entry address to slave's reset vector
; Simpler: use shared memory spinlock

; 1. Write entry address to agreed HWRAM location
mov.l   slave_entry, r0
mov.l   slave_signal_addr, r1
mov.l   r0, @r1              ; signal slave

; 2. Slave SH-2 boot stub (executed from BIOS) spins on this address:
slave_spin:
    mov.l   slave_signal_addr, r0
    mov.l   @r0, r1
    tst     r1, r1
    bt      slave_spin        ; spin until non-zero
    jmp     @r1               ; jump to signaled address
    nop
```

---

## Cache Configuration (SH-2 CCR Register)

```
CCR at 0xFFFFEC92 (SH-2 on-chip, byte access):

Bit 0: CE  Cache Enable  (1=enable; enable after BIOS handoff)
Bit 1: ID  Instruction/Data cache (0=unified 4KB)
Bit 2: OD  Operand cache bypass
Bit 3: WT  Write-through (1) vs write-back (0)
Bit 4: CP  Cache purge (write 1 to flush; auto-clears)

Boot sequence: BIOS leaves cache state undefined.
Your IP should purge and enable the cache:

  mov  #0x09, r0      ; CP=1, CE=1 → purge + enable
  mov.b r0, @(0, gbr) ; with GBR pointing to 0xFFFFEC92
```
