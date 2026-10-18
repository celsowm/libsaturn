# Section 9 Direct Memory Access Controller (DMAC)

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=247)

Physical PDF pages 247–302. Text extraction; consult the PDF for figures.

## Bookmarks

- [9.1 Overview](#pdf-page-247)
- [9.1.1 Features](#pdf-page-247)
- [9.1.2 Block Diagram](#pdf-page-249)
- [9.1.3 Pin Configuration](#pdf-page-250)
- [9.1.4 Register Configuration](#pdf-page-250)
- [9.2 Register Descriptions](#pdf-page-251)
- [9.2.1 DMA Source Address Registers 0 and 1 (SAR0 and SAR1)](#pdf-page-251)
- [9.2.2 DMA Destination Address Registers 0 and 1 (DAR0 and DAR1)](#pdf-page-252)
- [9.2.3 DMA Transfer Count Registers 0 and 1 (TCR0 and TCR1)](#pdf-page-252)
- [9.2.4 DMA Channel Control Registers 0 and 1 (CHCR0 and CHCR1)](#pdf-page-253)
- [9.2.5 DMA Vector Number Registers 0 and 1 (VCRDMA0, VCRDMA1)](#pdf-page-257)
- [9.2.6 DMA Request/Response Selection Control Registers 0 and 1 (DRCR0, DRCR1)](#pdf-page-258)
- [9.2.7 DMA Operation Register (DMAOR)](#pdf-page-259)
- [9.3 Operation](#pdf-page-261)
- [9.3.1 DMA Transfer Flow](#pdf-page-261)
- [9.3.2 DMA Transfer Requests](#pdf-page-263)
- [9.3.3 Channel Priorities](#pdf-page-265)
- [9.3.4 DMA Transfer Types](#pdf-page-267)
- [9.3.5 Number of Bus Cycles](#pdf-page-274)
- [9.3.6 DMA Transfer Request Acknowledge Signal Output Timing](#pdf-page-274)
- [9.3.7 DREQ Pin Input Detection Timing](#pdf-page-284)
- [9.3.8 DMA Transfer End](#pdf-page-299)
- [9.4 Examples of Use](#pdf-page-300)
- [9.4.1 DMA Transfer Between On-Chip SCI and External Memory](#pdf-page-300)
- [9.5 Usage Notes](#pdf-page-301)

## PDF page 247

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=247)

```text
      Section 9   Direct Memory Access Controller (DMAC)


9.1        Overview

The SH7604 includes a two-channel direct memory access controller (DMAC). The DMAC can
be used in place of the CPU to perform high-speed data transfers between external devices
equipped with DACK (transfer request acknowledge signal), external memories, memory-mapped
external devices, and on-chip peripheral modules (except for the DMAC, BSC and UBC). Using
the DMAC reduces the burden on the CPU and increases the operating efficiency of the chip as a
whole.


9.1.1        Features

The DMAC has the following features:

•    Number of channels: 2
•    Address space: 4 Gbytes in the architecture
•    Selectable data transfer unit: Byte, word (2 bytes), longword (4 bytes) or 16-byte unit (16-byte
    transfers first perform four longword reads and then four longword writes)
•    Maximum transfer count: 16,777,216 (16M) transfers
•    With cache hits, CPU instruction processing and DMA operation can proceed in parallel
•    The maximum transfer rate for synchronous DRAM burst transfers is 38 Mbytes/sec (f = 28.7
    MHz)
•    Single address mode transfers: Either the transfer source or transfer destination (peripheral
    device) is accessed by a DACK signal (selectable) while the other is accessed by address. One
    transfer unit of data is transferred in each bus cycle.
    Devices that can be used in DMA transfer:
    ¾    External devices with DACK and memory-mapped external devices (including external
        memories)
•    Dual address mode transfers: Both the transfer source and transfer destination are accessed by
    address. One transfer unit of data is transferred in two bus cycles.
    Device combinations capable of transfer:
    ¾    Two external memories
    ¾    External memory and memory-mapped external devices
    ¾    Two memory-mapped external devices
    ¾    External memory and on-chip peripheral module (excluding the DMAC, BSC and UBC).
    ¾    Memory-mapped external devices and on-chip peripheral modules (excluding the DMAC,
        BSC and UBC)



                                                                                                                231
```

## PDF page 248

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=248)

```text
    ¾    Two on-chip peripheral modules (excluding the DMAC, BSC and UBC)
        (access size permitted by a register of the peripheral module that is the transfer source or
        destination)
•    Transfer requests
    ¾    External requests (from DREQ pins. DREQ can be detected either by edge or by level, and
        either active-low or active-high can be selected)
    ¾    On-chip peripheral module requests (serial communication interface (SCI))
    ¾    Auto-request (the transfer request is generated automatically within the DMAC)
•    Selectable bus modes: Cycle-steal mode or burst mode
•    Selectable channel priority levels: Fixed or round-robin mode
•    An interrupt request can be sent to the CPU when data transfer ends













































232
```

## PDF page 249

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=249)

```text
9.1.2        Block Diagram

Figure 9.1 shows a block diagram of the DMAC.




          On-chip                                           SARn
         peripheral
           module  Peripheral busDARn
                                Internal busIteration       TCRn

          DREQn                              control

              RXI                           Register
              TXI                            control        CHCRn


                                            Start-up                   DMAC module bus
           DACKn                             control       DMAOR
             DEIn
    External                                Request
      ROM                                    priority
    External                                 control
      RAM                                   Interrupt    VCRDMAn
                                             control
    External I/O
     (memory
     mapped)
                    External bus
    External I/O                      Bus interface
       (with
   acknowledge)

                     Bus controller                                 DMAC


    DMAOR:      DMA operation register
      SARn:      DMA source address register
     DARn:      DMA destination address register
      TCRn:      DMA transfer counter register
    CHCRn:     DMA channel control register
  VCRDMAn:     DMA vector register
      DEIn:      DMA transfer end interrupt request to CPU
       RXI:      On-chip SCI receive-data-full interrupt transfer request
       TXI:      On-chip SCI transmit-data-full interrupt transfer request
         n:      0 to 1
                       Figure 9.1   DMAC Block Diagram

                                                                         233
```

## PDF page 250

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=250)

```text
9.1.3        Pin Configuration

Table 9.1 shows the DMAC pins.

Table 9.1      DMAC Pin Configuration

Channel     Name                              Symbol     I/O    Function
0                  DMA transfer request     DREQ0      I        DMA transfer request input from external
                                                                   device to channel 0
               DMA transfer request           DACK0      O      DMA transfer request acknowledge output
               acknowledge                                         from channel 0 to external device
1                  DMA transfer request     DREQ1      I        DMA transfer request input from external
                                                                   device to channel 1
               DMA transfer request           DACK1      O      DMA transfer request acknowledge output
               acknowledge                                         from channel 1 to external device


9.1.4        Register Configuration

Table 9.2 summarizes the DMAC registers. The DMAC has a total of 13 registers. Each channel
has six control registers. One control register is shared by both channels.

































234
```

## PDF page 251

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=251)

```text
Table 9.2      DMAC Registers

                                                                                 Initial                            Access
Channel Name                                           Abbr.         R/W         Value           Address            Size*3
0              DMA source address register 0    SAR0         R/W       Undefined    H'FFFFFF80   32
            DMA destination address                    DAR0        R/W       Undefined    H'FFFFFF84   32
            register 0
            DMA transfer count register 0      TCR0         R/W       Undefined    H'FFFFFF88   32
            DMA channel control register 0    CHCR0      R/(W)*1  H'00000000 H'FFFFFF8C   32
            DMA vector number register N0   VCRDMA0R/(W)*1  Undefined    H'FFFFFFA0   32
            DMA request/response selection             DRCR0      R/(W)*1  H'00             H'FFFFFE71   8*3
            control register 0
1              DMA source address register 1    SAR1         R/W       Undefined    H'FFFFFF90    32
            DMA destination address                    DAR1        R/W       Undefined    H'FFFFFF94   32
            register 1
            DMA transfer count register 1      TCR1         R/W       Undefined    H'FFFFFF98   32
            DMA channel control register 1    CHCR1      R/(W)*1  H'00000000 H'FFFFFF9C   32
            DMA vector number register N1   VCRDMA1R/(W)*1  Undefined    H'FFFFFFA8   32
            DMA request/response selection             DRCR1      R/(W)*1  H'00             H'FFFFFE72   8*3
            control register 1
Shared    DMA operation register                DMAOR     R/(W)*2  H'00000000 H'FFFFFFB0   32
Notes:  1.   Only 0 can be written to bit 1 of CHCR0 and CHCR1, after reading 1, to clear the flags.
          2.   Only 0 can be written to bits 1 and 2 of the DMAOR, after reading 1, to clear the flags.
          3.   Access DRCR0 and DRCR1 in byte units. Access all other registers in longword units.


9.2        Register Descriptions


9.2.1        DMA Source Address Registers 0 and 1 (SAR0 and SAR1)

                    Bit:        31            30            29            …             3              2              1              0
           Bit name:                                                        …
        Initial value:        —            —            —            …            —            —            —            —
                 R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


DMA source address registers 0 and 1 (SAR0 and SAR1) are 32-bit read/write registers that
specify the source address of a DMA transfer. During a DMA transfer, these registers indicate the
next source address. (In single-address mode, SAR is ignored in transfers from external devices
with DACK to memory-mapped external devices or external memory). In 16-byte unit transfers,
always set the value of the source address to a 16-byte boundars (16n address). Operation results
                                                                                                                           235
```

## PDF page 252

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=252)

```text
cannot be guaranteed if other values are used. The value after a reset is undefined. Values are
retained in standby mode and during module standbys.


9.2.2        DMA Destination Address Registers 0 and 1 (DAR0 and DAR1)

                     Bit:        31            30            29            …             3              2              1              0
            Bit name:                                                        …
         Initial value:        —            —            —            …            —            —            —            —
                   R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


DMA destination address registers 0 and 1 (DAR0 and DAR1) are 32-bit read/write registers that
specify the destination address of a DMA transfer. During a DMA transfer, these registers indicate
the next destination address. (In single-address mode, DAR is ignored in transfers from memory-
mapped external devices or external memory to external devices with DACK). The value after a
reset is undefined. Values are retained in standby mode and during module standbys.


9.2.3        DMA Transfer Count Registers 0 and 1 (TCR0 and TCR1)

                     Bit:        31            30            29            28            27            26            25            24
            Bit name:        —            —            —            —            —            —            —            —
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R             R             R             R             R             R             R             R


                     Bit:        23            22            21            …             3              2              1              0
            Bit name:                                                        …
         Initial value:        —            —            —            …            —            —            —            —
                   R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


DMA transfer count registers 0 and 1 (TCR0 and TCR1) are 32-bit read/write registers that
specify the DMA transfer count. The lower 24 of the 32 bits are valid. The value is written as 32
bits, including the upper eight bits. The number of transfers is 1 when the setting is H'00000001,
16,777,215 when the setting is H'00FFFFFF and 16, 777,216 (the maximum) when H'00000000 is
set. During a DMA transfer, these registers indicate the remaining transfer count.

Set the initial value as the write value in the upper eight bits. These bits always read 0. The initial
value after a reset is undefined. Values are retained in standby mode and during module standbys.
For 16-byte transfers, set the count to 4 times the number of transfers.





236
```

## PDF page 253

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=253)

```text
9.2.4        DMA Channel Control Registers 0 and 1 (CHCR0 and CHCR1)

                      Bit:        31            30            29            …            19            18            17            16
             Bit name:        —            —            —            …            —            —            —            —
         Initial value:         0              0              0             …             0              0              0              0
                   R/W:         R             R             R             …             R             R             R             R


                      Bit:        15            14            13            12            11            10             9              8
             Bit name:      DM1        DM0        SM1         SM0         TS1         TS0          AR           AM
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


                      Bit:         7              6              5              4              3              2              1              0
             Bit name:        AL           DS           DL           TB           TA            IE            TE           DE
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W       R/(W)*       R/W
Note:   Only 0 can be written, to clear the flag.

DMA channel control registers 0 and 1 (CHCR0 and CHCR1) are 32-bit read/write registers that
control the DMA transfer mode. They also indicate the DMA transfer status. Only the lower 16 of
the 32 bits are valid. They are written as 32-bit values, including the upper 16 bits. Write the initial
values to the upper 16 bits. These bits always read 0. The registers are initialized to H'00000000
by a reset and in standby mode. Values are retained during a module standby.

•    Bits 15 and 14—Destination Address Mode Bits 1, 0 (DM1, DM0): Select whether the DMA
     destination address is incremented, decremented or left fixed (in single address mode, DM1
     and DM0 are ignored when transfers are made from a memory-mapped external device, on-
     chip peripheral module, or external memory to an external device with DACK). DM1 and
     DM0 are initialized to 00 by a reset and in standby mode. Values are retained during a module
     standby.
















                                                                                                                                        237
```

## PDF page 254

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=254)

```text
Bit 15:  DM1          Bit 14:  DM0        Description
0                            0                           Fixed destination address                                      (Initial value)
                          1                           Destination address is incremented (+1 for byte transfer size,
                                                  +2 for word transfer size, +4 for longword transfer size, +16
                                                  for 16-byte transfer size)
1                            0                           Destination address is decremented (–1 for byte transfer size,
                                                  –2 for word transfer size, –4 for longword transfer size, –16
                                                  for 16-byte transfer size)
                          1                           Reserved (setting prohibited)


•    Bits 13 and 12—Source Address Mode Bits 1, 0 (SM1, SM0): Select whether the DMA source
    address is incremented, decremented or left fixed. In single address mode, SM1 and SM0 are
    ignored when transfers are made from an external device with DACK to a memory-mapped
    external device, on-chip peripheral module, or external memory. For a 16-byte transfer, the
    address is incremented by +16 regardless of the SM1 and SM0 values. SM1 and SM0 are
    initialized to 00 by a reset and in standby mode. Values are retained during a module standby.

Bit 13:  SM1          Bit 12:  SM0        Description
0                            0                           Fixed source address (+16 for 16-byte transfer size)
                                                                                                                      (Initial value)
                          1                           Source address is incremented (+1 for byte transfer size, +2
                                                  for word transfer size, +4 for longword transfer size, +16 for
                                                  16-byte transfer size)
1                            0                           Source address is decremented (–1 for byte transfer size, –2
                                                  for word transfer size, –4 for longword transfer size, +16 for
                                                  16-byte transfer size)
                          1                           Reserved (setting prohibited)


•    Bits 11 and 10—Transfer Size Bits (TS1, TS0): Select the DMA transfer size. When the
    transfer source or destination is an on-chip peripheral module register for which an access size
    has been specified, that size must be selected. During 16-byte transfers, set the transfer address
    mode bit for dual address mode. TS1 and TS0 are initialized to 00 by a reset and in standby
    mode. Values are retained during a module standby.

Bit 11:  TS1           Bit 10:  TS0         Description
0                            0                           Byte unit                                                                 (initial value)
                          1                           Word (2-byte) unit
1                            0                           Longword (4-byte) unit
                          1                           16-byte unit (4 longword transfers)




238
```

## PDF page 255

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=255)

```text
•    Bit 9—Auto Request Mode Bit (AR): Selects whether auto-request (generated within the
    DMAC) or module request (an external request or from the on-chip SCI module) is used for
    the transfer request. The AR bit is initialized to 0 by a reset and in standby mode. Its value is
    retained during a module standby.

Bit 9:  AR                             Description
0                                           Module request mode                                                          (Initial value)
1                                           Auto-request mode


•    Bit 8—Acknowledge/Transfer Mode Bit (AM): In dual address mode, this bit selects whether
    the DACK signal is output during the data read cycle or write cycle. In single-address mode, it
    selects whether to transfer data from memory to device or from device to memory. The AM bit
    is initialized to 0 by a reset and in standby mode. Its value is retained during a module standby.

Bit 8:  AM                           Description
0                                         DACK output in read cycle/transfer from memory to device  (Initial value)
1                                         DACK output in write cycle/transfer from device to memory


•    Bit 7—Acknowledge Level Bit (AL): Selects whether the DACK signal is an active-high
    signal or an active-low signal. The AL bit is initialized to 0 by a reset and in standby mode. Its
    value is retained during a module standby.

Bit 7:  AL                             Description
0                                           DACK is an active-low signal                                               (Initial value)
1                                           DACK is an active-high signal


•    Bit 6—DREQ Select Bit (DS): Selects the DREQ input detection method used. The DS bit is
    initialized to 0 by a reset and in standby mode. Its value is retained during a module standby.

Bit 6:  DS                             Description
0                                           Detected by level                                                                  (Initial value)
1                                           Detected by edge


•    Bit 5—DREQ Level Bit (DL): Selects active-high or active-low for the DREQ signal. The DL
    bit is initialized to 0 by a reset and in standby mode. Its value is retained during a module
    standby.







                                                                                                                                  239
```

## PDF page 256

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=256)

```text
Bit 5:  DL                             Description
0                                           When DS is 0, DREQ is detected by low level; when DS is 1, DREQ is
                                       detected by fall                                                                     (Initial value)
1                                           When DS is 0, DREQ is detected by high level; when DS is 1, DREQ is
                                       detected by rise


•    Bit 4—Transfer Bus Mode Bit (TB): Selects the bus mode for DMA transfers. The TB bit is
    initialized to 0 by a reset and in standby mode. Its value is retained during a module standby.

Bit 4:  TB                             Description
0                                           Cycle-steal mode                                                                 (Initial value)
1                                           Burst mode


•    Bit 3—Transfer Address Mode Bit (TA): Selects the DMA transfer address mode. The TA bit
    is initialized to 0 by a reset and in standby mode. Its value is retained during a module standby.

Bit 3:  TA                             Description
0                                           Dual address mode                                                              (Initial value)
1                                           Single address mode


•    Bit 2—Interrupt Enable Bit (IE): Determines whether or not to request a CPU interrupt at the
    end of a DMA transfer. When the IE bit is set to 1, an interrupt (DEI) request is setnt to the
    CPU when the TE bit is set. The IE bit is initialized to 0 by a reset and in standby mode. Its
    value is retained during a module standby.

Bit 2:  IE                              Description
0                                           Interrupt disabled                                                                 (Initial value)
1                                           Interrupt enabled


•    Bit 1—Transfer-End Flag Bit (TE): Indicates that the transfer has ended. When the value in the
    DMA transfer count register (TCR) becomes 0, the DMA transfer ends normally and the TE
    bit is set to 1. This flag is not set if the transfer ends because of an NMI interrupt or DMA
    address error, or because the DME bit of the DMA operation register (DMAOR) or the DE bit
    was cleared. To clear the TE bit, read 1 from it and then write 0. When the TE bit is set, setting
    the DE bit to 1 will not enable a transfer. The TE bit is initialized to 0 by a reset and in standby
    mode. Its value is retained during a module standby.







240
```

## PDF page 257

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=257)

```text
Bit 1:  TE                             Description
0                                           DMA has not ended or was aborted                                     (Initial value)
                                       Cleared by reading 1 from the TE bit and then writing 0
1                                           DMA has ended normally (by TCR = 0)


•    Bit 0—DMA Enable Bit (DE): Enables or disables DMA transfers. In auto-request mode, the
    transfer starts when this bit or the DME bit in DMAOR is set to 1. The NMIF and AE bits in
    DMAOR and the TE bit must be all set to 0. In external request mode or on-chip peripheral
    module request mode, the transfer begins when the DMA transfer request is received from the
    relevant device or on-chip peripheral module, provided this bit and the DME bit are set to 1.
    As with the auto-request mode, the TE bit and the NMIF and AE bits in DMAOR must be all
    set to 0. The transfer can be stopped by clearing this bit to 0. The DE bit is initialized to 0 by a
    reset and in standby mode. Its value is retained during a module standby.

Bit 0:  DE                             Description
0                                           DMA transfer disabled                                                          (Initial value)
1                                           DMA transfer enabled


9.2.5        DMA Vector Number Registers 0 and 1 (VCRDMA0, VCRDMA1)

                     Bit:        31            30            29            …            11            10             9              8
            Bit name:        —            —            —            …            —            —            —            —
         Initial value:         0              0              0             …             0              0              0              0
                   R/W:         R             R             R             …             R             R             R             R


                     Bit:         7              6              5              4              3              2              1              0
            Bit name:      VC7         VC6         VC5         VC4         VC3         VC2         VC1         VC0
         Initial value:        —            —            —            —            —            —            —            —
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


DMA vector number registers 0 and 1 (VCRDMA0, VCRDMA1) are 32-bit read/write registers
that set the DMAC transfer-end interrupt vector number. Only the lower eight bits of the 32 are
effective. They are written as 32-bit values, including the upper 24 bits. Write the initial values to
the upper 24 bits. These bits are initialized to H'000000XX (last eight bits are undefined) by a
reset and in standby mode. Values are retained during a module standby.

•    Bits 31 to 8—Reserved: These bits always read 0. The write value should always be 0.




                                                                                                                                     241
```

## PDF page 258

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=258)

```text
•    Bits 7 to 0—Vector Number Bits 7–0 (VC7–VC0): Set the interrupt vector numbers at the end
    of a DMAC transfer. Interrupt vector numbers of 0–127 can be set. When a transfer-end
    interrupt occurs, exception handling and interrupt control fetch the vector number and control
    is transferred to the specified interrupt handling routine. The VC7–VC0 bits are undefined
    upon reset and in standby mode. Always write 0 to VC7.


9.2.6        DMA Request/Response Selection Control Registers 0 and 1 (DRCR0, DRCR1)

                     Bit:         7              6              5              4              3              2              1              0
            Bit name:        —            —            —            —            —            —           RS1         RS0
         Initial value:         0              0              0              0              0              0              0              0
                  R/W:         R             R             R             R             R             R           R/W         R/W


DMA request/response selection control registers 0 and 1 (DRCR0, DRCR1) are 8-bit read/write
registers that set the vector address of the DMAC transfer request source. They are written as 8-bit
values. They are initialized to H'00 by a reset, but retain their values in a module standby.

•    Bits 7 to 2—Reserved

•    Bits 1 and 0—Resource Select Bits 1 and 0 (RS1, RS0): Specify which transfer request to input
    to the DMAC. Changing the transfer request source must be done when the DMA enable bit
    (DE) is 0. The RS1 and RS0 bits are initialized to 00 by a reset.

Bit 1:  RS1            Bit 0:  RS0           Description
0                            0                           DREQ (external request)                                       (Initial value)
0                            1                           RXI (on-chip SCI receive-data-full interrupt transfer request)*
1                            0                           TXI (on-chip SCI transmit-data-empty interrupt transfer
                                                   request)*
1                            1                           Reserved (setting prohibited)
Note:   For RX2 and TX1, set for dual transfer mode.
          The DREQ settings in CHCR are DS = 1 and DL = 0.
















242
```

## PDF page 259

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=259)

```text
9.2.7        DMA Operation Register (DMAOR)

                      Bit:        31            30            29            …            11            10             9              8
             Bit name:        —            —            —            …            —            —            —            —
         Initial value:         0              0              0             …             0              0              0              0
                   R/W:         R             R             R             …             R             R             R             R


                      Bit:         7              6              5              4              3              2              1              0
             Bit name:        —            —            —            —           PR           AE         NMIF       DMIE
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R             R             R             R           R/W       R/(W)*     R/(W)*       R/W
Note:   Only 0 can be written, to clear the flag.

The DMA operation register (DMAOR) is a 32-bit read/write register that controls the DMA
transfer mode. It also indicates the DMA transfer status. Only the lower four of the 32 bits are
valid. DMAOR is written as a 32-bit value, including the upper 28 bits. Write the initial values to
the upper 28 bits. These bits always read 0. DMAOR is initialized to H'00000000 by a reset and in
standby mode.

•    Bits 31 to 4—Reserved: These bits always read 0. The write value should always be 0.

•    Bit 3—Priority Mode Bit (PR): Selects the priority level between channels when there are
     transfer requests for multiple channels. It is initialized to 0 by a reset and in standby mode. Its
     value is retained during a module standby.

Bit 3:  PR                             Description
0                                           Fixed priority (channel 0 > channel 1)                                  (Initial value)
1                                           Round-robin (Top priority shifts to bottom after each transfer. The
                                        priority for the first DMA transfer after a reset is channel 1 > channel 0)


•    Bit 2—Address Error Flag Bit (AE): This flag indicates that an address error has occurred in
     the DMAC. When the AE bit is set to 1, DMA transfer cannot be enabled even if the DE bit in
     the DMA channel control register (CHCR) is set to 1. To clear the AE bit, read 1 from it and
     then write 0. Operation is performed up to the DMAC transfer being executed when the
     address error occurred. AE is initialized to 0 by a reset and in standby mode.

Bit 2:  AE                             Description
0                                           No DMAC address error                                                       (Initial value)
                                        To clear the AE bit, read 1 from it and then write 0
1                                           Address error by DMAC


                                                                                                                                        243
```

## PDF page 260

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=260)

```text
•    Bit 1—NMI Flag Bit (NMIF): This flag indicates that an NMI interrupt has occurred. When
    the NMIF bit is set to 1, DMA transfer cannot be enabled even if the DE bit in CHCR and the
    DME bit are set to 1. To clear the NMIF bit, read 1 from it and then write 0. Ends after the
    DMAC operation executing when the NMI comes in (operation goes to destination). When the
    NMI interrupt is input while the DMAC is not operating, the NMIF bit is set to 1. The NMIF
    bit is initialized to 0 by a reset or in the standby mode. Values are held during a module
    standby.

Bit 1:  NMIF                         Description
0                                           No NMIF interrupt (initial value)
                                    To clear the NMIF bit, read 1 from it and then write 0.
1                                           NMIF has occurred


•    Bit 0—DMA Master Enable Bit (DME): Enables or disables DMA transfers on all channels. A
    DMA transfer becomes enabled when the DE bit in the CHCR and the DME bit are set to 1.
    For this to be effective, however, the TE bit in CHCR and the NMIF and AE bits must all be 0.
    When the DME bit is cleared, all channel DMA transfers are aborted. DME is initialized to 0
    by a reset and in standby mode. Its value is retained during a module standby.

Bit 0:  DME                          Description
0                                           DMA transfers disabled on all channels                               (Initial value)
1                                           DMA transfers enabled on all channels






























244
```

## PDF page 261

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=261)

```text
9.3        Operation

When there is a DMA transfer request, the DMAC starts the transfer according to the
predetermined channel priority; when the transfer-end conditions are satisfied, it ends the transfer.
Transfers can be requested in three modes: auto-request, external request, and on-chip module
request. A transfer can be in either single address mode or dual address mode. The bus mode can
be either burst or cycle-steal.


9.3.1        DMA Transfer Flow

After the DMA source address registers (SAR), DMA destination address registers (DAR), DMA
transfer count registers (TCR), DMA channel control registers (CHCR), DMA vector number
registers (VCRDMA), DMA request/response selection control registers (DRCR), and DMA
operation register (DMAOR) are initialized (initializing sets each register so that ultimately the
condition (DE = 1, DME = 1, TE = 0, NMIF = 0, AE = 0) is satisfied), the DMAC transfers data
according to the following procedure:

1.   Checks to see if transfer is enabled (DE = 1, DME = 1, TE = 0, NMIF = 0, AE = 0)
2.   When a transfer request comes and transfer is enabled, the DMAC transfers 1 transfer unit of
    data. (In auto-request mode, the transfer begins automatically when the DE bit and DME bit
    are set to 1. The TCR value will be decremented by 1.) The actual transfer flows vary
    depending on the address mode and bus mode.
3.   When the specified number of transfers have been completed (when TCR reaches 0), the
    transfer ends normally. If the IE bit in CHCR is set to 1 at this time, a DEI interrupt is sent to
    the CPU.
4.   When an address error occurs in the DMAC or an NMI interrupt is generated, the transfer is
    aborted. Transfers are also aborted when the DE bit in CHCR or the DME bit in DMAOR is
    changed to 0.

Figure 9.2 shows a flowchart illustrating this procedure.



















                                                                                                                    245
```

## PDF page 262

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=262)

```text
                             Start

                        Initial settings
                 (SAR, DAR, TCR, CHCR,
                VCRDMA, DRCR, DMAOR)



                             DE,               No
                    DME = 1 and NMIF,
                         AE, TE = 0?

                          Yes

                             Has a              No                                             *2
                    transfer request been
                         generated?*1                                                       Bus
                          Yes                                              *3         mode, transfer
                                                                              request mode, DREQ detec-
                                                                                       tion method?
                           Transfer
                       TCR-1 → TCR,
                  SAR, and DAR updated
                                                                   *4

                                               No        16-byte transfer         *5
                          TCR = 0?                         in progress?


                          Yes                                                   NMIF = 1,           No
                    DEI interrupt request                                or AE = 1, or  DE = 0,
                        (when IE = 1)                                            or DME
                                                                                   = 0?

                         NMIF = 1 or            No                                        Yes
                    AE = 1, or DE = 0, or                                  Transfer aborted
                          DME = 0?

                          Yes
                            TE = 1                            TE = 1


                         End transfer                      End normally


Notes:  1.   In auto-request mode, the transfer will start when the NMIF, AE, and TE bits are all 0
               and the DE and DME bits are then set to 1.
          2.   In burst mode, DREQ = level detection (external request), or cycle-steal mode.
          3.   In burst mode, DREQ = edge detection (external request), or auto-request mode in
               burst mode.
          4.   16-byte transfer cycle in progress.
          5.   End of a 16-byte transfer cycle.

                                          Figure 9.2   DMA Transfer Flow


246
```

## PDF page 263

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=263)

```text
9.3.2        DMA Transfer Requests

DMA transfer requests are usually generated in either the data transfer source or destination, but
they can also be generated by devices that are neither the source nor the destination. Transfers can
be requested in three modes: auto-request, external request, and on-chip peripheral module
request. The request mode is selected with the AR bit in DMA channel control registers 0 and 1
(CHCR0, CHCR1) and the RS0 and RS1 bits in DMA request/response selection control registers
0 and 1 (DRCR0, DRCR1).

Table 9.3      Selecting the DMA Transfer Request Using the AR and RS Bits

 CHCR          DRCR
AR           RS1    RS0    Request Mode                Resource Select
0              0         0         Module request mode      DREQ external request (external request mode)
            0         1                                                  RXI (SCI receive) request
            1         0                                                  TXI (SCI transmit) request
1              X        X        Auto-request mode


Auto-Request: When there is no transfer request signal from an external source (as in a memory-
to-memory transfer or a transfer between memory and an on-chip peripheral module unable to
request a transfer), the auto-request mode allows the DMAC to automatically generate a transfer
request signal internally. When the DE bits in CHCR0 and CHCR1 and the DME bit in the DMA
operation register (DMAOR) are set to 1, the transfer begins (so long as the TE bits in CHCR0 and
CHCR1 and the NMIF and AE bits in DMAOR are all 0).

External Request: In this mode a transfer is started by a transfer request signal (DREQ) from an
external device. Choose one of the modes shown in table 9.4 according to the application system.
When DMA transfer is enabled (DE = 1, DME = 1, TE = 0, NMIF = 0, AE = 0), a transfer is
performed upon input of a DREQ signal.



















                                                                                                                       247
```

## PDF page 264

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=264)

```text
Table 9.4      Selecting External Request Modes with the TA and AM Bits

   CHCR    Transfer
TA    AM     Address Mode      Acknowledge Mode        Source                      Destination
0       0        Dual address          DACK output in read              Any*1                         Any*1
              mode                     cycle
      1        Dual address            DACK output in write             Any*1                         Any*1
              mode                     cycle
1       0        Single address        Data transferred from            External memory*2         External device
              mode                     memory to device                 or memory-mapped          with DACK
                                                                        external device
      1        Single address          Data transferred from            External device           External memory*2
              mode                     device to memory                 with DACK                 or memory-mapped
                                                                                                  external device
Notes:  1.   External memory, memory-mapped external device, on-chip peripheral module
              (excluding DMAC, BSC, and UBC)
         2.   Except synchronous DRAM


Choose to detect DREQ either by the falling edge or by level using the DS and DL bits in CHCR0
and CHCR1 (DS = 0 is level detection, DS = 1 is edge detection; for edge detection, DL = 0 is
rising edge, DL = 1 is falling edge; for level detection, DL = 0 is active-low, DL = 1 is active-
high). The source of the transfer request does not have to be the data transfer source or destination.

Table 9.5      Selecting the External Request Signal with the DS and DL Bits

       DRCR
DS           DL          External Request
0              0            Level (active-low)
            1            Level (active-high)
1              0            Edge (falling)
            1            Edge (rising)


On-Chip Module Request: In this mode, transfers are started by the transfer request signal
(interrupt request signal) of an on-chip peripheral module in the SH7064. The transfer request
signals are the receive-data-full interrupt (RXI) and transmit-data-empty interrupt (TXI) of the
serial communication interface (SCI) (table 9.6). If DMA transfer is enabled (DE = 1, DME = 1,
TE = 0, NMIF = 0, AE = 0), DMA transfer starts upon the input of a transfer request signal.







248
```

## PDF page 265

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=265)

```text
When RXI (transfer request when the SCI’s receive data buffer is full) is set as the transfer
request, however, the transfer source must be the SCI’s receive data register (RDR). Likewise,
when TXI (transfer request when the SCI’s transmit data buffer is empty) is set as the transfer
request, the transfer destination must be the SCI’s transmit data register (TDR).

Table 9.6      Selecting On-Chip Peripheral Module Request Mode with the AR and RS bits

                      DMA Transfer
                      Request            DMA Transfer                                            Bus        DREQ
AR     RS1   RS0      Source             Request Signal         Source    Destination            Mode       Setting
001SCI                                   RXI (SCI receive-          RDR        Any*               Cycle-    Edge,
                      receiver           data-full transfer                                      steal      active-low
                                         request)
010SCI                                   TXI (SCI transmit-         Any*        TDR               Cycle-    Edge,
                      transmitter        data-empty transfer                                     steal      active-low
                                         request)
Note:   External memory, memory-mapped external device, on-chip peripheral module (excluding
         DMAC, BSC, and UBC)


When outputting transfer requests from the SCI, its interrupt enable bits (TIE and RIE in SCR)
must be set to output the interrupt signals. Note that transfer request signals from on-chip
peripheral modules (interrupt request signals) are sent not just to the DMAC but to the CPU as
well. When an on-chip peripheral module is specified as the transfer request source, set the
priority level values in the interrupt priority level registers (IPRC–IPRE) of the interrupt controller
(INTC) at or below the levels set in the I3–I0 bits of the CPU’s status register so that the CPU
does not accept the interrupt request signal.

The DMA transfer request signals shown in table 9.6 are automatically fetched when the
corresponding DMA transfer is performed. If cycle-steal mode is used, a DMA transfer request
(interrupt request) from any module will be cleared at the first transfer; if burst mode is used, it
will be cleared at the last transfer.


9.3.3        Channel Priorities

When the DMAC receives simultaneous transfer requests on two channels, it selects a channel
according to a predetermined priority order. There are two priority modes, fixed and round-robin.
The channel priority is selected by the priority bit, PR, in the DMA operation register (DMAOR).

Fixed Priority Mode: In this mode, the relative channel priority levels are fixed. When PR is set
to 0, the priority, high to low, is channel 0 > channel 1. Figure 9.3 shows an example of a transfer
in burst mode.





                                                                                                                   249
```

## PDF page 266

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=266)

```text
    DREQ0

    DREQ1
                                                           Channel 0             Channel 0             Channel 1
         Bus                                               destination          destination           destination
       cycle       CPU        CPU       CPU
                                                 Channel 0            Channel 0            Channel 1
                                                   source               source                source

   Figure 9.3   Fixed Mode Burst DMA Transfer (Dual Address, Active-Low DREQ Level)

In cycle-steal mode, once a channel 0 request is accepted, channel 1 requests are also accepted
until the next request is made, which makes more effective use of the bus cycle. When requests
come simultaneously for channel 0 and channel 1 when DMA operation is starting, the first is
transmitted multiplexed with channel 0 and thereafter channel 1 and channel 0 transfers are
performed alternately.


  DREQ0

  DREQ1
                                            Channel 0                      Channel 1                    Channel 0
        Bus                                    source                        source                       source
      cycle     CPU       CPU     CPU                                                                          CPUCPU
                                                      Channel 0                      Channel 1
                                                      destination                    destination

                          Figure 9.4   Fixed Mode Cycle-Steal DMA Transfer
                                 (Dual Address, Active-Low DREQ Level)

Round-Robin Mode: Switches the priority of channel 0 and channel 1, shifting their ability to
receive transfer requests. Each time one transfer ends on one channel, the priority shifts to the
other channel. The channel on which the transfer just finished is assigned low priority. After reset,
channel 0 has higher priority than channel 1.

Figure 9.5 shows how the priority changes when channel 0 and channel 1 transfers are requested
simultaneously and another channel 0 transfer is requested after the first two transfers end. The
DMAC operates as follows:

1.   Transfer requests are generated simultaneously to channels 1 and 0.
2.   Channel 1 has the higher priority, so the channel 1 transfer begins first (channel 0 waits for
    transfer).
3.   When the channel 1 transfer ends, channel 1 becomes the lower-priority channel.


250
```

## PDF page 267

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=267)

```text
4.   The channel 0 transfer begins.
5.   When the channel 0 transfer ends, channel 0 becomes the lower-priority channel.
6.   A channel 0 transfer is requested.
7.   The channel 0 transfer begins.
8.   When the channel 0 transfer ends, channel 0 is already the lower-priority channel, so the order
    remains the same.



     Transfer requests         Waiting channel          DMAC operation                   Channel priority order

     1. Requests occur                                 2. Channel 1
             in channels                                    transfer starts                          1 > 0
                   0 and 1
                                          0                                        Priority
                                                       3. Channel 1               changes            0 > 1
                                                           transfer ends


                                                       4. Channel 0
                                                           transfer starts

                                          None                                     Priority
                                                       5. Channel 0               changes            1 > 0
                                                           transfer ends

                                                                                              Waiting for
     6.  Request occurs                                7. Channel 0                       transfer request
             in channel 0                                  transfer starts

                                          None                                     Priority
                                                       8. Channel 0          does not change         1 > 0
                                                           transfer ends

                          Figure 9.5   Channel Priority in Round-Robin Mode


9.3.4        DMA Transfer Types

The DMAC supports all the transfers shown in table 9.7. It can operate in single address mode or
dual address mode, as defined by how many bus cycles the DMAC takes to access the transfer
source and transfer destination. The actual transfer operation timing varies with the DMAC bus
mode used: cycle-steal mode or burst mode.






                                                                                                                     251
```

## PDF page 268

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=268)

```text
Table 9.7      Supported DMA Transfers

                                                                            Destination
                                           External                                                       On-Chip
                                           Device with          External          Memory-Mapped           Peripheral
Source                                     DACK                 Memory            External Device         Module
External device with DACK             Not available     Single             Single                    Not available
External memory                            Single                Dual                Dual                       Dual
Memory-mapped external device   Single                Dual                Dual                       Dual
On-chip peripheral module             Not available     Dual                Dual                       Dual*
Single:   Single address mode
Dual:      Dual address mode
Note:     Access size enabled by the register of the on-chip peripheral module that is the source or
           destination (excludes DMAC, BSC, and UBC).


Address Modes:

•    Single Address Mode
    In single address mode, both the transfer source and destination are external; one (selectable) is
    accessed by a DACK signal while the other is accessed by address. In this mode, the DMAC
    performs the DMA transfer in one bus cycle by simultaneously outputting a transfer request
    acknowledge DACK signal to one external device to access it while outputting an address to
    the other end of the transfer. Figure 9.6 shows an example of a transfer between external
    memory and external device with DACK. The external device outputs data to the data bus
    while that data is written in external memory in the same bus cycle.


                                    External address bus         External data bus
                      SH7604
                                                                                      External
                                     DMAC                                             memory




                                                                                  External device
                                                                                     with DACK


                                                       DACK
                                                      DREQ

                                                     :  Data flow

                              Figure 9.6   Data Flow in Single Address Mode
252
```

## PDF page 269

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=269)

```text
Two types of transfers are possible in single address mode: 1) transfers between external
devices with DACK and memory-mapped external devices; and 2) transfers between external
devices with DACK and external memory. Transfer requests for both of these must be by
means of the external request signal (DREQ). Figure 9.7 shows the DMA transfer timing for
single address mode.



            CK
       A26–A0                                              Address output to external
                                                           memory space

            CS
            WE                                             Write strobe signal to external
                                                           memory space
      D30–D0                                               Data output from external
                                                           device with DACK
         DACK                                              DACK signal (active low) to external
                                                           device with DACK
             BS


                     a. External device with DACK to external memory space


          CK

    A26–A0                                              Address output to external
                                                        memory space
          CS
          RD                                            Read strobe signal to external
                                                        memory space
    D30–D0                                              Data output from external
                                                        memory space
      DACK

          BS                                            DACK signal (active low) to external
                                                        device with DACK

                     b. External memory space to external device with DACK

                Figure 9.7   DMA Transfer Timing in Single Address Mode








                                                                                                          253
```

## PDF page 270

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=270)

```text
•    Dual Address Mode
    In dual address mode, both the transfer source and destination are accessed (selectable) by
    address. The source and destination can be located externally or internally. The DMAC
    accesses the source in the read cycle and the destination in the write cycle, so the transfer is
    performed in two separate bus cycles. The transfer data is temporarily stored in the DMAC.
    Figure 9.8 shows an example of a transfer between two external memories in which data is
    read from one memory in the read cycle and written to the other memory in the following write
    cycle.


                                                    External data bus
                     SH7604                                           2
                                                                                  External
                                  DMAC                                            memory




                                                                                  External
                                                                      1           memory



                                                    :  Data flow
                                                  1:  Read cycle
                                                  2:  Write cycle

                              Figure 9.8   Data Flow in Dual Address Mode

    In dual address mode transfers, external memory, memory-mapped external devices and on-
    chip peripheral modules can be mixed without restriction. Specifically, this enables transfers
    between the following:

    1.   External memory and external memory.
    2.   External memory and memory-mapped external devices.
    3.   Memory-mapped external devices and memory-mapped external devices.
    4.   External memory and on-chip peripheral modules (excluding the DMAC, BSC, and UBC).
    5.   Memory-mapped external devices and on-chip peripheral modules (excluding the DMAC,
        BSC, and UBC). The access size is that is enabled by the register of the on-chip peripheral
        module that is the source or destination (excludes the DMAC, BSC, and UBC).
    6.   On-chip peripheral modules (excluding the DMAC, BSC, and UBC) and on-chip
        peripheral modules (excluding the DMAC, BSC, and UBC).






254
```

## PDF page 271

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=271)

```text
    Transfer requests can be auto-requests, external requests, or on-chip peripheral module
    requests. When the transfer request source is the SCI, however, either the data destination or
    source must be the SCI (see table 9.6). Dual address mode outputs DACK in either the read
    cycle or write cycle. CHCR controls the cycle in which DACK is output.

    Figure 9.9 shows the DMA transfer timing in dual address mode.



             CK
       A26–A0                                                                 Address output to
                                                                              external memory space

             CS
                                                                              Read strobe signal to
             RD                                                               external memory space
                                                                              Write strobe signal to
             WE                                                               external memory space

       D31–D0                                                                 I/O data of external
                                                                              memory space
                                                                              DMAC acknowledge signal
         DACK                                                                 (active-low)


             BS

                      Figure 9.9   DMA Transfer Timing in Dual Address Mode
     (External Memory Space ﬁ   External Memory Space, DACK Output in Read Cycle)

Bus Modes: There are two bus modes: cycle-steal and burst. Select the mode with the TB bits in
CHCR0 and CHCR1.

•    Cycle-Steal Mode
    In cycle-steal mode, the bus right is given to another bus master after the DMAC transfers one
    transfer unit (byte, word, longword, or 16 bytes). When another transfer request occurs, the bus
    right is retrieved from the other bus master and another transfer is performed for one transfer
    unit. When that transfer ends, the bus right is passed to the other bus master. This is repeated
    until the transfer end conditions are satisfied.

    Cycle-steal mode can be used with all categories of transfer destination, transfer source, and
    transfer request source. The CPU may take the bus twice when an acknowledge signal is
    output during the write cycle or in single address mode. Figure 9.10 shows an example of
    DMA transfer timing in cycle-steal mode (dual address mode, DREQ level detection).





                                                                                                                  255
```

## PDF page 272

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=272)

```text
       DREQ

                                                         Bus right returned to CPU
         Bus       CPU       CPU       CPU     DMAC       DMAC     CPU        DMAC    DMAC         CPU
        cycle
                                                 Read     Write                  Read     Write

                      Figure 9.10   DMA Transfer Timing in Cycle-Steal Mode
                             (Dual Address Mode, DREQ Level Detection)

•    Burst Mode
    In burst mode, once the DMAC gets the bus, the transfer continues until the transfer end
    condition is satisfied. When external request mode is used with level detection of the DREQ
    pin, however, negating DREQ will pass the bus to the other bus master after completion of the
    bus cycle of the DMAC that currently has an acknowledged request, even if the transfer end
    conditions have not been satisfied. Burst mode cannot be used when the transfer request
    originates from the serial communication interface (SCI).

    Figure 9.11 shows an example of DMA transfer timing in burst mode (single address mode,
    DREQ level detection).


    DREQ


      Bus
     cycle      CPU      CPU       CPU    DMAC        DMAC   DMAC        DMAC  DMAC         DMAC       CPU

   Figure 9.11   DMA Transfer Timing in Burst Mode (Single Address Mode, DREQ Level
                                                   Detection)

Refreshes cannot be performed during a burst transfer, so ensure that the number of transfers
satisfies the refresh request period when a memory requiring refreshing is used.

Relationship of Request Modes and Bus Modes by DMA Transfer Category: Table 9.8 shows
the relationship between request modes, bus modes, etc., by DMA transfer category.













256
```

## PDF page 273

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=273)

```text
Table 9.8      Relationship of Request Modes and Bus Modes by DMA Transfer Category

                                                                                                               Transfer
Address                                                                         Request            Bus         Size
Mode         Transfer Category                                                  Mode               Mode        (Bytes)
Single         External device with DACK and external memory     External          B/C        1/2/4
              External device with DACK and memory-mapped                       External          B/C        1/2/4
              external device
Dual           External memory and external memory                     All*1                 B/C        1/2/4/16
              External memory and memory-mapped external                        All*1                 B/C        1/2/4/16
              device
              Memory-mapped external device and memory-                         All*1                 B/C        1/2/4/16
              mapped external device
              External memory and on-chip peripheral module      All*2                 B/C*3      1/2/4/16*4
              Memory-mapped external device and on-chip                         All*2                 B/C*3      1/2/4/16*4
              peripheral module
              On-chip peripheral module and on-chip peripheral                  All*2                 B/C*3      1/2/4/16*4
              module
B: Burst, C: Cycle-steal
Notes:  1.   External requests and auto-requests are both available. The SCI cannot be specified as
              the transfer request source, however, except for on-chip peripheral module requests.
          2.   External requests, auto-requests and on-chip peripheral module requests are all
              available. When the SCI is the transfer request source, however, the transfer
              destination or transfer source must be the SCI.
          3.   If the transfer request source is the SCI, cycle-steal (C) only (DREQ by edge detection,
              active low).
          4.   The access size is that permitted by the register of the on-chip peripheral module that is
              the transfer destination or source.


Bus Mode and Channel Priority: When a given channel (1) is transferring in burst mode and
there is a transfer request to a channel (0) with a higher priority, the transfer of the channel with
higher priority (0) will begin immediately. When channel 0 is also operating in the burst mode, the
channel 1 transfer will continue as soon as the channel 0 transfer has completely finished. When
channel 0 is in cycle-steal mode, channel 1 will begin operating again after channel 0 completes
the transfer of one transfer unit, but the bus will then switch between the two in the order channel
1, channel 0, channel 1, channel 0. Since channel 1 is in burst mode, it will not give the bus to the
CPU. This example is illustrated in Figure 9.12.









                                                                                                                         257
```

## PDF page 274

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=274)

```text
   Bus        CPU                  DMAC        DMAC       DMAC       DMAC        DMAC       DMAC         CPU
  state                     DMACch1  ch1        ch0         ch1        ch0         ch1        ch1

                                                ch0          ch1          ch0

                           DMAC ch1               DMAC ch1 and ch0                  DMAC ch1
              CPU         Burst mode                   Cycle-steal                 Burst mode          CPU


    Note:   Priority is ch0 > ch1, ch1 is in burst mode, ch0 is in cycle-steal mode
                  Figure 9.12   Bus Status when Multiple Channels are Operating


9.3.5        Number of Bus Cycles

The number of states in the bus cycle when the DMAC is the bus master is controlled by the bus
control register (BCR1) and wait state control register (WCR) of the bus state controller just as it
is when the CPU is the bus master.


9.3.6        DMA Transfer Request Acknowledge Signal Output Timing

DMA transfer request acknowledge signal DACKn is output synchronous to the DMAC address
output specified by the channel control register AM bit of the address bus. The timing is normally
to have the acknowledge signal become valid when the DMA address output begins and become
invalid 0.5 cycles before the address output ends. (See figure 9.11.) The output timing of the
acknowledge signal varies with the settings of the connected memory space. The output timing of
acknowledge signals in the memory spaces is shown in figure 9.13.


                                  Clock


                                 DACK
                                                                               0.5 cycles


                          Address bus              CPU             DMAC


                          Figure 9.13         Example of DACK Output Timing









258
```

## PDF page 275

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=275)

```text
Acknowledge Signal Output when External Memory Is Set as Ordinary Memory Space: The
timing at which the acknowledge signal is output is the same in the DMA read and write cycles
specified by the AM bit (figures 9.14 and 9.15). When DMA address output begins, the
acknowledge signal becomes valid; 0.5 cycles before address output ends, it becomes invalid. If a
wait is inserted in this period and address output is extended, the acknowledge signal is also
extended.


                                                                                             T1      TW     T2
     Clock


    DACK
                                               0.5 cycles
                                  DMAC        Invalid    DMAC
  Address                          read        write      write
       bus            CPU                                                   CPU               DMAC read

                               Basic timing                                                       1 wait inserted

                 Figure 9.14   DACK Output in Ordinary Space Accesses (AM = 0)



   Clock


  DACK

                 DMAC       Invalid   DMAC                                                Invalid
Address           read       write     write                                               write
     bus                                                 CPU              DMAC read                   DMAC write

                      Basic timing                                             1 wait inserted

                 Figure 9.15   DACK Output in Ordinary Space Accesses (AM = 1)

In a longword access of a 16-bit external device (figure 9.16) or an 8-bit external device (figure
9.17), or a word access of an 8-bit external device (figure 9.18), the lower and upper addresses are
output 2 and 4 times in each DMAC access in order to align the data. For all of these addresses,
the acknowledge signal becomes valid simultaneous with the start of output and becomes invalid
0.5 cycles before the address output ends. When multiple addresses are output in a single access to
align data for synchronous DRAM, DRAM, pseudo-SRAM, or burst ROM, an acknowledge
signal is output to those addresses as well.



                                                                                                                    259
```

## PDF page 276

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=276)

```text
          Clock


         DACK

                                                                                Invalid
                                                      *1                               *2write
       Address                                                                             DMAC write
            bus                CPU H         DMAC read H    DMAC read L

                                                     Basic timing
    Notes: 1.     L: LSB side
            2.     H: MSB side
                      Figure 9.16   DACK Output in Ordinary Space Accesses
                        (AM = 0, Longword Access to 16-Bit External Device)



          Clock


          DACK

                                           DMAC            DMAC             DMAC             DMAC
       Address                            read HH          read HL         read LH          read LL
             bus          CPU H

                                                         Basic timing

                      Figure 9.17   DACK Output in Ordinary Space Accesses
                         (AM = 0, Longword Access to 8-Bit External Device)



          Clock


         DACK

                                                                                Invalid
       Address                                                                   write
            bus                CPU H         DMAC read H    DMAC read L                    DMAC write

                                                     Basic timing

                      Figure 9.18   DACK Output in Ordinary Space Accesses
                           (AM = 0, Word Access to 8-Bit External Device)



260
```

## PDF page 277

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=277)

```text
Acknowledge Signal Output when External Memory Is Set as Synchronous DRAM: When
external memory is set as synchronous DRAM auto-precharge and AM = 0, the acknowledge
signal is output across the row address, read command, wait and read address of the DMAC read
(figure 9.19). Since the synchronous DRAM read has only burst mode, during a single read an
invalid address is output; the acknowledge signal, however, is output on the same timing (figure
9.20). At this time, the acknowledge signal is extended until the write address is output after the
invalid read. When AM = 1, the acknowledge signal is output across the row address and column
address of the DMAC write (figure 9.21).




      Clock




     DACK
                                                      Read
                                                   command
                                            Row
                                          address
   Address                                                       Read 1     Read 2    Read 3    Read 4
         bus             CPU


                                                            DMAC read (basic timing)

                 Figure 9.19   DACK Output in Synchronous DRAM Burst Read
                                         (Auto-Precharge, AM = 0)



       Clock



      DACK                                  Read

                                     Rowcommand                                       Row      Column

    Address                        address                                          address   address
         bus           CPU                                                   Invalid readRead

                                                     DMAC read                             DMAC write
                                                    (basic timing)                         (basic timing)

                 Figure 9.20   DACK Output in Synchronous DRAM Single Read
                                         (Auto-Precharge, AM = 0)




                                                                                                              261
```

## PDF page 278

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=278)

```text
                               Clock



                              DACK



                                                              Row     Column
                                                            address   address
                            Address
                                 bus

                                                       DMAC write (basic timing)

                    Figure 9.21   DACK Output in Synchronous DRAM Write
                                        (Auto-Precharge, AM = 1)

When external memory is set as bank active synchronous DRAM, during a burst read the
acknowledge signal is output across the read command, wait and read address when the row
address is the same as the previous address output (figure 9.22). When the row address is different
from the previous address, the acknowledge signal is output across the precharge, row address,
read command, wait and read address (figure 9.23).



            Clock




           DACK


                                                 Read
                                              command
         Address               CPU                        Read 1     Read 2    Read 3    Read 4
              bus

                                                             DMAC read (basic timing)

                 Figure 9.22   DACK Output in Synchronous DRAM Burst Read
                              (Bank Active, Same Row Address, AM = 0)








262
```

## PDF page 279

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=279)

```text
       Clock




      DACK

                                       Pre-      Row         Read
                                     charge    address    command
    Address            CPU                                           Read 1  Read 2   Read 3   Read 4
         bus

                                                    DMAC read
                                                   (basic timing)

                  Figure 9.23   DACK Output in Synchronous DRAM Burst Read
                            (Bank Active, Different Row Address, AM = 0)

When external memory is set as bank active synchronous DRAM, during a single read the
acknowledge signal is output across the read command, wait and read address when the row
address is the same as the previous address output (figure 9.24). When the row address is different
from the previous address, the acknowledge signal is output across the precharge, row address,
read command, wait and read address (figure 9.25). Since the synchronous DRAM read has only
burst mode, during a single read an invalid address is output; the acknowledge signal is output on
the same timing. At this time, the acknowledge signal is extended until the write address is output
after the invalid read.



       Clock



      DACK

                                       Read                                       Row      Column
    Address                         command                                     address    address
         bus           CPU                       Read            Invalid read

                                           DMAC read (basic timing)         DMAC write (basic timing)

                 Figure 9.24   DACK Output in Synchronous DRAM Single Read
                               (Bank Active, Same Row Address, AM = 0)








                                                                                                                263
```

## PDF page 280

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=280)

```text
       Clock


      DACK                                  Row
                                         address
                                     Pre-         Read                                        Row      Column
    Address                        charge                        Readcommand                address   address
         bus          CPU                                                              Invalid read

                                                        DMAC read                             DMAC write
                                                       (basic timing)                        (basic timing)

                 Figure 9.25   DACK Output in Synchronous DRAM Single Read
                             (Bank Active, Different Row Address, AM = 0)

When external memory is set as bank active synchronous DRAM, during a write the acknowledge
signal is output across the wait and column address when the row address is the same as the
previous address output (figure 9.26). When the row address is different from the previous
address, the acknowledge signal is output across the precharge, row address, wait and column
address (figure 9.27).




                                       Clock



                                      DACK

                                                                    Column
                                    Address                         address
                                         bus

                                                                  DMAC write
                                                                 (basic timing)

                     Figure 9.26   DACK Output in Synchronous DRAM Write
                               (Bank Active, Same Row Address, AM = 1)











264
```

## PDF page 281

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=281)

```text
                        Clock



                       DACK

                                                                   Row      Column
                                                     Precharge   address    address
                     Address
                          bus

                                                               DMAC write
                                                              (basic timing)

                    Figure 9.27   DACK Output in Synchronous DRAM Write
                           (Bank Active, Different Row Address, AM = 1)

Acknowledge Signal Output when External Memory Is Set as DRAM: When external memory
is set as DRAM and a row address is output during a read or write, the acknowledge signal is
output across the row address and column address (figures 9.28–9.30).



                Clock




               DACK
                                                            Row
                                             Precharge    address
             Address                                                Column address
                  bus

                                                       DMAC read or write
                                                           (basic timing)

             Figure 9.28   DACK Output in Normal DRAM Accesses (AM = 1 or 0)














                                                                                                         265
```

## PDF page 282

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=282)

```text
                               Clock


                               DACK
                                                             Column
                            Address                          address

                                  bus

                                                      DMAC read or write
                                                          (basic timing)

                      Figure 9.29   DACK Output in DRAM Burst Accesses
                                  (Same Row Address, AM = 1 or 0)



                Clock



               DACK
                                                  Pre-     Row
                                                charge   address
             Address                                                Column address
                  bus

                                                         DMAC read or write
                                                             (basic timing)

                      Figure 9.30   DACK Output in DRAM Burst Accesses
                                (Different Row Address, AM = 1 or 0)

Acknowledge Signal Output when External Memory Is Set as Pseudo-SRAM: When external
memory is set as pseudo-SRAM , the acknowledge signal is output synchronous to the DMAC
address for both reads and writes (figures 9.31–9.33).
















266
```

## PDF page 283

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=283)

```text
       Clock




      DACK

                                        Pre-
                                       charge
    Address                                          DMAC address
         bus

                                              DMAC read or write
                                                  (basic timing)

Figure 9.31   DACK Output in Normal Pseudo-SRAM Accesses (AM = 1 or 0)



       Clock



      DACK


    Address                            DMAC address
         bus

                                   DMAC read or write
                                      (basic timing)

         Figure 9.32   DACK Output in Pseudo-SRAM Burst Accesses
                         (Same Row Address, AM = 1 or 0)



       Clock


      DACK
                                         Pre-
    Address                             charge
         bus                                          DMAC address

                                               DMAC read or write
                                                  (basic timing)

         Figure 9.33   DACK Output in Pseudo-SRAM Burst Accesses
                      (Different Row Address, AM = 1 or 0)

                                                                                             267
```

## PDF page 284

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=284)

```text
Acknowledge Signal Output When External Memory Is Set as Burst ROM: When external
memory is set as burst ROM, the acknowledge signal is output synchronous to the DMAC address
(no dual writes allowed) (figure 9.34).



       Clock



      DACK

   Address                                            DMAC cycle                      DMAC cycle
         bus
                                                                 DMAC (1 wait state)

                   Figure 9.34   DACK Output in Nibble Accesses of Burst ROM


9.3.7        DREQ Pin Input Detection Timing

In external request mode, DREQ pin signals are usually detected at the rising edge of the clock
pulse (CKIO). When a request is detected, a DMAC bus cycle is produced three cycles later at the
earliest and a DMA transfer performed. After the request is detected, the timing of the next input
detection varies with the bus mode, address mode, method of DREQ input detection, and the
memory connected.

DREQ Pin Input Detection Timing in Cycle-Steal Mode:

In cycle-steal mode, once a request is detected from the DREQ pin, request detection for the next
DMA transfer cannot be performed for a certain period of time. After request detection has again
become possible, detectable cycles continue until a request is detected.

Figure 9.35 illustrates the timing from the detection of a request till the next time requests are
detectable.

•    Cycle-Steal Mode Edge Detection
    Requests can be detected 2 cycles after DACK output. After that point, the request is input to
    DREQ. (If input prior to that point, a request may or may not be detected, depending on the
    internal state.)










268
```

## PDF page 285

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=285)

```text
    Transfer width: Byte, word, longword
    Transfer bus mode: Cycle-steal mode
    Transfer address modes: Dual and single modes
    DREQ detection method: Edge detection
    DACK output timing: Read, write (dual), DMAC cycle (single)
    Bus cycle: Basic bus cycle

      Clock

                           *1                                                                          *1
     DREQ
              1st acceptance                                                       2nd acceptance
                                                                         2 cycles
      DACK



         Bus           CPU
       cycle                                  CPU                *2                DMAC


Notes:  1.   Request detection
          2.   When DACK is output in a write (dual), the cycle is a DMAC read. Otherwise, the cycle
              is a CPU cycle.

        Figure 9.35   DREQ Pin Input Detection Timing in Cycle-Steal Mode with Edge
                                                     Detection (1)

    Figures 9.36 and 9.37 show examples of how to change the bus width of an external device.


























                                                                                                                        269
```

## PDF page 286

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=286)

```text
      Clock

                            *1                                                                          *1
     DREQ
               1st acceptance                                                         2nd acceptance
                                                                            2 cycles
      DACK


         Bus            CPU                                                                  DMAC L
       cycle                                   CPU                *2              DMAC H


Notes:  1.   Request detection
          2.   When DACK is output in a write (dual), the cycle is a DMAC read. Otherwise, the cycle
               is a CPU cycle.

                    Figure 9.36   Changing the Bus Size of a 16-Bit External Device



        Clock

                           *1                                                            *1
      DREQ
                1st acceptance                                             2nd acceptance
                                                                 2 cycles
       DACK


          Bus           CPU           CPU            *2           DMAC HH   DMAC HL           DMAC LH       DMAC LL
        cycle


Notes:  1.   Request detection
          2.   When DACK is output in a write (dual), the cycle is a DMAC read. Otherwise, the cycle
               is a CPU cycle.

                    Figure 9.37   Changing the Bus Size of an 8-Bit External Device













270
```

## PDF page 287

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=287)

```text
    Transfer width: 16-byte
    Transfer bus mode: Cycle-steal mode
    Transfer address mode: Dual mode
    DREQ detection method: Edge detection
    DACK output timing: DMAC read and write cycles
    Bus cycle: Basic bus cycle



      Clock

                      *1                             *1
     DREQ
              1st acceptance              2nd acceptance
                                    2 cycles
     DACK

        Bus                                 DMAC 2             DMAC 4
      cycle       CPU         *2       *3         *3         *3         *3
                                    DMAC 1            DMAC 3

Notes         1.    Request detection
              2.    When a write (dual) occurs at DACK output, the cycle is a DMAC read.
                   Otherwise, the cycle is a CPU cycle.
              3.    When DACK is output in a write (dual), the cycle is a DMAC write; when in a read
                   (dual), the cycle is a DMAC read.

        Figure 9.38   DREQ Pin Input Detection Timing in Cycle-Steal Mode with Edge
                                                    Detection (2)

Requests can be detected 2 cycles after DACK output. After that point, the request is input to
DREQ. (If input prior to that point, a request may or may not be detected, depending on the
internal state.) DACK is output synchronous to all 4 transfers (figure 9.38).



















                                                                                                                    271
```

## PDF page 288

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=288)

```text
    Transfer width: Byte, word, longword
    Transfer bus mode: Cycle-steal mode
    Transfer address modes: Dual and single modes
    DREQ detection method: Level detection
    DACK output timing: Read (dual), DMAC cycle (single)
    Bus cycle: Basic bus cycle



     Clock

                       *1                                                *2
    DREQ                                 3 cycles                                       Area where 2nd
              1st acceptance                                                        acceptance is possible
                                                                      2 cycles
     DACK


        Bus            CPU                    CPU                     DMAC read
      cycle


    Notes: 1.   Request detection
            2.   Request detection not established.
             Figure 9.39   Timing of DREQ Pin Input Detection in Cycle-Steal Mode
                                            with Level Detection (1)

•    Cycle-Steal Mode Level Detection

    Requests can be detected for the first time 3 cycles after the bus cycle prior to the DMAC read
    cycle and detection starts sometime between then and 2 cycles after DACK output (figure 9.40,
    41). This varies with variations in waits and the like. This means that if request output is
    stopped within 3 cycles from the bus cycle prior to the DMAC read cycle, the next DMA
    transfer is not performed; if request output is stopped within 2 cycles of DACK output, the
    next DMA transfer may sometimes be performed. See Examples of Handling of Request
    Signal Acceptance later in this section (9.3.7).















272
```

## PDF page 289

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=289)

```text
    Clock

                    *1                                              *2
   DREQ                               3 cycles                                     Area where 2nd
           1st acceptance                                                      acceptance is possible
                                                                  2 cycles
    DACK


Bus cycle           CPU           CPU H          CPU L          DMAC H           DMAC L

Notes: 1.   Request detection
         2.   Request detection not established.
               Figure 9.40   Changing the Bus Size of a 16-Bit External Device



  Clock

                 *1                                                *2
 DREQ                                                                           Area where 2nd
        1st acceptance       3 cycles                                        acceptance is possible
                                                               2 cycles
 DACK

    Bus                             CPU HL         CPU LL          DMAC HL         DMAC LL
  cycle
                           CPU HH          CPU LH          DMAC HH         DMAC LH

Notes: 1.   Request detection
         2.   Request detection not established.
               Figure 9.41   Changing the Bus Size of an 8-Bit External Device


















                                                                                                            273
```

## PDF page 290

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=290)

```text
   Transfer width: Byte, word, longword
   Transfer bus mode: Cycle-steal mode
   Transfer address mode: Dual mode
   DREQ detection method: Level detection
   DACK output timing: DMAC write cycle
   Bus cycle: Basic bus cycle



  Clock

                  **
  DREQ
            1st acceptance                                                                                  2nd acceptance
                                                                              2 cycles
  DACK


    Bus           CPU           CPU                  CPU             DMAC read                     DMAC write             CPU
   cycle                                                              Invalid
                                                                       write

   Note:   Request detection
            Figure 9.42   Timing of DREQ Pin Input Detection in Cycle Steal Mode
                                        with Level Detection (2)

The next request can be detected 2 cycles after DACK output (figure 9.42).



























274
```

## PDF page 291

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=291)

```text
    Transfer width: 16-byte
    Transfer bus mode: Cycle-steal mode
    Transfer address mode: Dual mode
    DREQ detection method: Level detection
    DACK output timing: DMAC write cycle
    Bus cycle: Basic bus cycle



       Clock

                  *1                           *2
       DREQ               3 cycles
              1st acceptance                2nd acceptance

                                          2 cycles
       DACK

                                                   DMAC             DMAC         DMAC            DMAC
         Bus                  CPU                  read 2           read 4       write 1         write 3

        cycle                             DMAC              DMAC          Invalid         DMAC            DMAC
                  CPU                     read 1            read 3         write          write 2         write 4


    Notes: 1.   Request detection
             2.   Request detection not established.


              Figure 9.43   Timing of DREQ Pin Input Detection in Cycle Steal Mode
                                             with Level Detection (3)

Requests can be detected for the first time 3 cycles after the bus cycle prior to the DMAC read
cycle and starts sometime between then and 2 cycles after DACK output (figure 9.43). This varies
with variations in waits and the like. This means that if request output is stopped within 3 cycles
from the bus cycle prior to the DMAC read cycle, the next DMA transfer is not performed; if
request output is stopped within 2 cycles of DACK output, the next DMA transfer may sometimes
be performed.


















                                                                                                                    275
```

## PDF page 292

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=292)

```text
    Transfer width: 16-byte
    Transfer bus mode: Cycle-steal mode
    Transfer address mode: Dual mode
    DREQ detection method: Level detection
    DACK output timing: DMAC write cycle
    Bus cycle: Basic bus cycle



    Clock


               *** *
   DREQ
            1st acceptance             2nd acceptance  3rd acceptance                                 4th acceptance

                                          2 cycles   2 cycles                                            2 cycles

   DACK                                                    A1     A2     A3     A4                                                    B1     B2     B3     B4

                                   1st 16-byte transfer                                2nd 16-byte transfer


              CPU       DMAC       Invalid     DMAC        DMAC                              DMAC        DMAC
                         read 1     write      write 2     write 4     CPU            DMACread 4write 1  write 3     CPU
      Bus
     cycle
                   CPU        DMAC       DMAC        DMAC                   DMAC                   DMAC       DMAC
                              read 4     write 1     write 3             CPUread 1            Invalidwritewrite 2write 4

    Note:   Request detection
              Figure 9.44   Timing of DREQ Pin Input Detection in Cycle Steal Mode
                                              with Level Detection (4)

For 16-byte transfers, DACK signals are output at all consecutive writes (figure 9.44). The
acknowledge signals are A1, A2, A3, A4, B1, B2, B3, B4, ….

The second transfer request can be detected 2 cycles after output of acknowledge signal A1. The
third transfer request is detected at A3, that is, 2 cycles after output of the third acknowledge
signal of the first transfer. The fourth transfer request is detected 2 cycles after output of B3.
Requests thereafter are detected 2 cycles after the third acknowledge signal of each transfer, as
with the fourth transfer.

Note:    When transferring alternately on channels 0 and 1 by round robin or the like, the next
          request signal is detected only 2 cycles after the first acknowledge signal of each transfer
          (figure 9.45).









276
```

## PDF page 293

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=293)

```text
                 B4DMACwrite 4
                 B3 DMACwrite 3
  12 3 45 6   *3rd acceptanceB2DMACwrite 2DMACwrite 1
                2 cyclesB12nd 1 ch 16-byte transferInvalidwriteDMACread 4
      B4  CPUDMACwrite 4DMACread 1
      B3  DMACwrite 3CPU
      B2 DMACwrite 2DMACwrite 1C4DMACwrite 4
      B12nd 0 ch 16-byte transferInvalidwriteDMACread 4C3DMACwrite 3
      1st acceptanceDMACread 1*C2DMACwrite 2DMACwrite 1
             4th acceptance2 cyclesC13rd 0 ch 16-byte transferInvalidwriteDMACread 4
   **A4  CPUDMACwrite 4DMACread 1
    3rd acceptance2 cyclesA3DMACwrite 3CPU
DREQ detection method: Level detectionDACK output timing: DMAC write cycleBus cycle: Basic bus cycleA2DMACwrite 2DMACwrite 1A4DMACwrite 4
    2nd acceptance2 cyclesA11st 0 ch 16-byte transferInvalidwriteDMACread 4A3DMACwrite 3
         DMACread 1*2nd acceptanceA2DMACwrite 2DMACwrite 1
   **CPUCPU     2 cyclesA11st 1 ch 16-byte transferInvalidwriteDMACread 4
Transfer width: 16-byteTransfer bus mode: Cycle-steal modeTransfer address mode: Dual modePriority mode: Round robin modeCKDREQ01st acceptanceDREQ1DACK0DACK1Buscycle12 345DMACread 16Note:  Request detection timing



   Figure 9.45   Example of Simultaneous Operation of 2 Channels


                     277
```

## PDF page 294

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=294)

```text
DREQ Pin Input Detection Timing in Burst Mode: In burst mode, the request detection timing
differs when DREQ input is detected by edge and when detected by level.

When DREQ input is detected by edge, once a request is detected, DMA transfers continue until
the conditions for ending the transfers are met, regardless of the state of the DREQ pin thereafter.
During this period, requests cannot be detected. When the transfer start conditions are met after a
transfer ends, requests can be detected again for each cycle.

When DREQ input is detected by level, whenever a request is detected for the same channel as in
the next request detection cycle, that channel is executed continuously. When no request is input,
however, the bus cycles of other channels and other bus masters are executed.

•    Burst Mode, Single Mode, Level Detection

    Acknowledge signals for request signals are output 3 cycles later at the earliest. Even when the
    request signal is dropped within 2 cycles of the output of this acknowledge signal, the fourth or
    fifth requests in figure 9.46 are accepted. This means that 4 or 5 DMA transfers are executed
    even when the request for the 1st acknowledge signal drops out.

    Transfer width: Byte, word, longword
    Transfer bus mode: Burst mode
    Transfer address mode: Single mode
    DREQ detection method: Level detection
    DACK output timing: DMAC cycle
    Bus cycle: Basic bus cycle


    Clock

                   *     ** *   **
   DREQ
              1st    2nd     3rd         4th         5th           6th
              accept-accept- accept-     accept-     accept-       accept-
              ance   ance    ance        ance        ance          ance

      Bus                               DMAC read 1  DMAC read 2   DMAC read 3   DMAC read 4   DMAC read 5   DMAC read 6
     cycle

   DACK


    Note:   Request detection (The points when the acceptances occur vary with the type of wait.)
 Figure 9.46   Timing of DREQ Pin Input Detection in Burst Mode with Level Detection (1)
                              (Data Transfer from Normal Space to Device)








278
```

## PDF page 295

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=295)

```text
Acknowledge signals for request signals are output 4 cycles later, at the soonest. Even when the
request signal is dropped within 0.5 cycle of the output of this acknowledge signal, the third
request in figure 9.46 is accepted. This means that the 3rd DMA transfer is executed even when
the request for the 1st acknowledge signal drops out. The detection timing for the 4th and
subsequent requests is as shown in figure 9.46.


    Transfer width: Byte, word, longword
    Transfer bus mode: Burst mode
    Transfer address mode: Single mode
    DREQ detection method: Level detection
    DACK output timing: DMAC cycle
    Bus cycle: Basic bus cycle


 Clock

                 **  ****
 DREQ
            1st    2nd    3rd         4th          5th          6th
            accept-accept-accept-     accept-      accept-      accept-
            ance   ance   ance        ance         ance         ance

   Bus                                InvalidDMAC write 1   DMAC write 2  DMAC write 3  DMAC write 4   DMAC write 5  DMAC write 6
  cycle                                write

 DACK


    Note:   Request detection (The points when the acceptances occur vary with the type of wait.)
 Figure 9.47   Timing of DREQ Pin Input Detection in Burst Mode with Level Detection (2)
                              (Data Transfer from Device to Normal Space)

Acknowledge signals for request signals are output 4 cycles later at the earliest. Even when the
request signal is dropped within 0.5 cycle of the output of this acknowledge signal, the third
request in figure 9.47 is accepted. This means that the 3rd DMA transfer is executed even when
the request for the first acknowledge signal drops out. The detection timing for the 4th and
subsequent requests is a shown in figure 9.47.

Note on Evaluation Chip: The request signal detection timing in the evaluation chip differs from
that in the user chip. The evaluation chip detection timing corresponding to figures 9.46 and 9.47
is shown in figures 9.48 and 9.49.












                                                                                                                    279
```

## PDF page 296

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=296)

```text
    Transfer width: Byte, word, longword
    Transfer bus mode: Burst mode
    Transfer address mode: Single mode
    DREQ detection method: Level detection
    DACK output timing: DMAC cycle
    Bus cycle: Basic bus cycle


    Clock

   DREQ            *            *****


                1st        2nd      3rd        4th          5th         6th
                accept-    accept-  accept-    accept-      accept-     accept-
                ance       ance     ance       ance         ance        ance

     cycleBus                                  DMAC read 1   DMAC read 2  DMAC read 3  DMAC read 4   DMAC read 5  DMAC read 6

    DACK


    Note:   Request detection (The points when the acceptances occur vary with the type of wait.)
 Figure 9.48   Timing of DREQ Pin Input Detection in Burst Mode with Level Detection (3)
               (Data Transfer from Normal Space to Device, Using Evaluation Chip)


    Transfer width: Byte, word, longword
    Transfer bus mode: Burst mode
    Transfer address mode: Single mode
    DREQ detection method: Level detection
    DACK output timing: DMAC cycle
    Bus cycle: Basic bus cycle


  Clock

                *** ** *
 DREQ
               1st     2nd     3rd             4th             5th              6th
               accept- accept- accept-         accept-         accept-          accept-
               ance    ance    ance            ance            ance             ance

    Bus                                  Invalid DMAC                      Invalid
  cycle                                  write                             Invalidwrite 1                     DMACwritewrite 2                     DMACwrite                             Invalidwrite 3                     DMACwrite                             Invalidwrite 4                      DMACwritewrite 5

 DACK


    Note:    Request detection (The points when the acceptances occur vary with the type of wait.)
 Figure 9.49   Timing of DREQ Pin Input Detection in Burst Mode with Level Detection (4)
             (Data Transfer from Device to Normal Memory, Using Evaluation Chip)




280
```

## PDF page 297

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=297)

```text
•    Burst Mode, Dual Mode, Level Detection


    Transfer width: Byte, word, longword
    Transfer bus mode: Burst mode
    Transfer address mode: Single mode
    DREQ detection method: Level detection
    DACK output timing: DMAC read
    Bus cycle: Basic bus cycle



   Clock


                        *                       *                                                          *
  DREQ

                   1st                   2nd                                               3rd
                   acceptance            acceptance                                        acceptance

  DACK


     Bus                                        CPU                     DMAC read                               DMAC read
   cycle
                                                                                        DMAC
                                                                                     invalid write

    Note:   Request detection (The points when the 1st and 2nd acceptances occur vary with the
             type of wait.)
 Figure 9.50   Timing of DREQ Pin Input Detection in Burst Mode with Level Detection (5)

    Acknowledge signals for request signals are output 4 cycles later, at the soonest. Even when
    the request signal is dropped within 0.5 cycle of the output of the acknowledge signal, the 2nd
    request in figure 9.50 is accepted. This means that two DMA transfers are executed even when
    the request for the 1st acknowledge signal drops out.




















                                                                                                                        281
```

## PDF page 298

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=298)

```text
    Transfer width: Byte, word, longword
    Transfer bus mode: Burst mode
    Transfer address mode: Dual mode
    DREQ detection method: Level detection
    DACK output timing: DMAC write
    Bus cycle: Basic bus cycle



  Clock


                       *                    **
 DREQ

                   1st                2nd                                          3rd
                   acceptance         acceptance                                   acceptance

 DACK


    Bus                                     CPU                      DMAC read                              DMAC write
   cycle
                                                                                 DMAC
                                                                               invalid write

    Note:   Request detection (The points when the 1st and 2nd acceptances occur vary with the
            type of wait.)
 Figure 9.51   Timing of DREQ Pin Input Detection in Burst Mode with Level Detection (6)

    Acknowledge signals for request signals are output 6 cycles later, at the soonest. Even when
    the request signal is dropped within 0.5 cycle of the output of the acknowledge signal, the 2nd
    request in figure 9.51 is accepted. This means that two DMA transfers are executed even when
    the request for the 1st acknowledge signal drops out.

Examples of Handling of Request Signal Acceptance: When DREQ level acceptance is used in
the cycle-steal mode, the following methods can be used when the request signal is received:

1.   Control the number of transfers by TCR
2.   Use edge for request acceptance
3.   Perform acknowledge signal output at the DMAC write timing

Additional Cautions when Emulators Are Used: When DREQ level acceptance is by an
emulator in cycle-steal mode, the timing of request signal acceptance is 2 cycles after the output of
the acknowledge signal, so it differs from ordinary specifications. This means that when DMAC
operation is emulated, the timing is somewhat different, which may have other ramifications.





282
```

## PDF page 299

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=299)

```text
9.3.8        DMA Transfer End

The DMA transfer ending conditions vary when channels end individually and when both
channels end together.

Conditions for Channels Ending Individually: When either of the following conditions are met,
the transfer will end in the relevant channel only:

•    The value of the channel’s DMA transfer count register (TCR) becomes 0.
    When the TCR value becomes 0, the DMA transfer for that channel ends and the transfer-end
    flag bit (TE) is set in CHCR. If the IE (interrupt enable) bit has already been set, a DMAC
    interrupt (DEI) request is sent to the CPU. In 16-byte transfer, when the TCR is 3,2,1 during
    the final transfer, the source address will be output four times, but the destination address will
    only be output the number of times found in TCR before transfer ends.

•    The DE bit of the DMA channel control register (CHCR) is cleared to 0.
    When the DMA enable bit (DE) in CHCR is cleared, DMA transfers in the affected channel
    are halted. The TE bit is not set when this happens.


       Source          Source          Source          Source        Destination     Destination
       address        address         address          address         address         address
      first time    second time      third time      fourth time      first time    second time

                                                                                                        CPU


                                                                                       TCR = 0
                                                                             (transfer ends normally)

                              Figure 9.52   16-Byte Transfer when TCR = 2

Conditions for Both Channels Ending Simultaneously: Transfers on both channels end when
either of the following conditions is met:

•    The NMIF (NMI flag) bit or AE (address error flag) bit is set to 1 in DMAOR.
    When an NMI interrupt or DMAC address error occurs and the NMIF or AE bit is set to 1 in
    DMAOR, all channels stop their transfers. The DMA source address register (SAR),
    designation address register (DAR), and transfer count register (TCR) are all updated by the
    transfer immediately preceding the halt. When this transfer is the final transfer, TE = 1 and the
    transfer ends. To resume transfer after NMI interrupt exception handling or address error
    exception handling, clear the appropriate flag bit. When the DE bit is then set to 1, the transfer
    on that channel will restart. To avoid this, keep its DE bit at 0. In dual address mode, DMA
    transfer will be halted after the completion of the following write cycle even when the address
    error occurs in the initial read cycle. SAR, DAR and TCR are updated by the final transfer.


                                                                                                                    283
```

## PDF page 300

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=300)

```text
•    The DMA master enable (DME) bit in DMAOR is cleared to 0.
    Clearing the DME bit in DMAOR forcibly aborts the transfers on both channels at the end of
    the current bus cycle. When the transfer is the final transfer, TE = 1 and the transfer ends.

9.4        Examples of Use


9.4.1        DMA Transfer Between On-Chip SCI and External Memory

In the following example, data received on the on-chip serial communication interface (SCI) is
transferred to external memory using DMAC channel 1. Table 9.9 shows the transfer conditions
and register settings.

Table 9.9      Register Settings for Transfers between On-Chip SCI and External Memory

Transfer Conditions                                                                   Register       Setting
Transfer source: RDR of on-chip SCI                                          SAR1            H'FFFFFE05
Transfer destination: external memory (word space)                  DAR1            Destination address
Number of transfers: 64                                                               TCR1            H'0040
Transfer destination address: incremented                                 CHCR1         H'4045
Transfer source address: fixed
Bus mode: cycle-steal
Transfer unit: byte
DEI interrupt request generated at end of transfer (DE = 1)
Channel priority: Fixed (0 > 1) (DME = 1)                                    DMAOR        H'0001
Transfer request source (transfer request signal): SCI (RXI)      DRCR1         H'01
Note:   Check the CPU interrupt level when interrupts are enabled in the SCI.





















284
```

## PDF page 301

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=301)

```text
9.5        Usage Notes

1.   DMA request/response selection control registers 0 and 1 (DRCR0 and DRCR1) should be
    accessed in bytes. All other registers should be accessed in longword units.
2.   Before rewriting CHCR0, CHCR1, DRCR0, and DRCR1, first clear the DE bit for the
    specified channel to 0 or clear the DME bit in DMAOR to 0.
3.   When the DMAC is not operating, the NMIF bit in DMAOR is set even when an NMI
    interrupt is input.
4.   When the cache is used as on-chip RAM, the DMAC cannot access this RAM.
5.   Set to standby mode after the DME bit in DMAOR is set to 0.
6.   Do not access the DMAC, BSC, and UBC on-chip peripheral modules.
7.   Do not access the cache (address array, data array, associative purge area).
8.   To detect the DREQ pin signal in single address mode, use edge detection.










































                                                                                                                  285
```

## PDF page 302

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=302)

```text
286
```
