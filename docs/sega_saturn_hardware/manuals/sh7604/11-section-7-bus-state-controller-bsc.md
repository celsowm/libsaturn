# Section 7 Bus State Controller (BSC)

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=145)

Physical PDF pages 145–228. Text extraction; consult the PDF for figures.

## Bookmarks

- [7.1 Overview](#pdf-page-145)
- [7.1.1 Features](#pdf-page-145)
- [7.1.2 Block Diagram](#pdf-page-146)
- [7.1.3 Pin Configuration](#pdf-page-148)
- [7.1.4 Register Configuration](#pdf-page-150)
- [7.1.5 Address Map](#pdf-page-150)
- [7.2 Description of Registers](#pdf-page-152)
- [7.2.1 Bus Control Register 1 (BCR1)](#pdf-page-152)
- [7.2.2 Bus Control Register 2 (BCR2)](#pdf-page-154)
- [7.2.3 Wait Control Register (WCR)](#pdf-page-156)
- [7.2.4 Individual Memory Control Register (MCR)](#pdf-page-158)
- [7.2.5 Refresh Timer Control/Status Register (RTCSR)](#pdf-page-162)
- [7.2.6 Refresh Timer Counter (RTCNT)](#pdf-page-163)
- [7.2.7 Refresh Time Constant Register (RTCOR)](#pdf-page-164)
- [7.3 Access Size and Data Alignment](#pdf-page-164)
- [7.3.1 Connection to Ordinary Devices](#pdf-page-164)
- [7.3.2 Connection to Little-Endian Devices](#pdf-page-166)
- [7.4 Accessing Ordinary Space](#pdf-page-167)
- [7.4.1 Basic Timing](#pdf-page-167)
- [7.4.2 Wait State Control](#pdf-page-171)
- [7.5 Synchronous DRAM Interface](#pdf-page-173)
- [7.5.1 Synchronous DRAM Direct Connection](#pdf-page-173)
- [7.5.2 Address Multiplexing](#pdf-page-175)
- [7.5.3 Burst Reads](#pdf-page-176)
- [7.5.4 Single Reads](#pdf-page-180)
- [7.5.5 Writes](#pdf-page-181)
- [7.5.6 Bank Active Function](#pdf-page-182)
- [7.5.7 Refreshes](#pdf-page-190)
- [7.5.8 Power-On Sequence](#pdf-page-193)
- [7.5.9 Phase Shift by PLL](#pdf-page-195)
- [7.6 DRAM Interface](#pdf-page-197)
- [7.6.1 DRAM Direct Connection](#pdf-page-197)
- [7.6.2 Address Multiplexing](#pdf-page-199)
- [7.6.3 Basic Timing](#pdf-page-200)
- [7.6.4 Wait State Control](#pdf-page-201)
- [7.6.5 Burst Access](#pdf-page-202)
- [7.6.6 Refresh Timing](#pdf-page-204)
- [7.6.7 Power-On Sequence](#pdf-page-205)
- [7.7 Pseudo-SRAM Interface](#pdf-page-205)
- [7.7.1 Pseudo-SRAM Direct Connection](#pdf-page-205)
- [7.7.2 Basic Timing](#pdf-page-208)
- [7.7.3 Wait State Control](#pdf-page-209)
- [7.7.4 Burst Access](#pdf-page-210)
- [7.7.5 Refreshing](#pdf-page-211)
- [7.7.6 Power-On Sequence](#pdf-page-213)
- [7.8 Burst ROM Interface](#pdf-page-213)
- [7.9 Waits between Access Cycles](#pdf-page-216)
- [7.10 Bus Arbitration](#pdf-page-217)
- [7.10.1 Master Mode](#pdf-page-219)
- [7.10.2 Slave Mode](#pdf-page-221)
- [7.10.3 Partial-Share Master Mode](#pdf-page-222)
- [7.10.4 External Bus Address Monitor](#pdf-page-225)
- [7.10.5 Master/Slave Coordination](#pdf-page-225)
- [7.11 Other Topics](#pdf-page-226)
- [7.11.1 Resets](#pdf-page-226)
- [7.11.2 Access as Seen from the CPU or DMAC](#pdf-page-226)
- [7.11.3 Emulator](#pdf-page-228)

## PDF page 145

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=145)

```text
                      Section 7   Bus State Controller (BSC)


7.1        Overview

The bus state controller (BSC) manages the address spaces and outputs control signals so that
optimum memory accesses can be made in the four spaces. This enables memories like DRAM,
synchronous DRAM and pseudo-SRAM, and peripheral chips, to be linked directly.


7.1.1        Features

The BSC has the following features:

•    Address space is divided into four spaces
    ¾    A maximum linear 32 Mbytes for each of the address spaces CS0–CS3
    ¾    The type of memory connected can be specified for each space (DRAM, synchronous
        DRAM, pseudo-SRAM, burst ROM, etc.).
    ¾    Bus width can be selected for each space (8, 16, or 32 bits).
    ¾    Wait state insertion can be controlled for each space.
    ¾    Outputs control signals for each space.
•    Cache
    ¾    Cache areas and cache-through areas can be selected by access address.
    ¾    When a cache access misses, 16 bytes are read consecutively in 4-byte units (because of
        cache fill); writes use the write-through system.
    ¾    Cache-through accesses are accessed according to access size.
•    Refresh
    ¾    Supports CAS-before-RAS refresh (auto-refresh) and self-refresh.
    ¾    Refresh interval can be set using the refresh counter and clock selection.
•    Direct interface to DRAM
    ¾    Multiplexes row/column address output.
    ¾    Burst transfer during reads, high-speed page mode for consecutive accesses.
    ¾    Generates a Tp cycle to ensure RAS precharge time.
•    Direct interface to synchronous DRAM
    ¾    Multiplexes row/column address output.
    ¾    Burst read, single write
    ¾    Bank active mode






                                                                                                                   129
```

## PDF page 146

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=146)

```text
•    Master and slave modes (bus arbitration)
    ¾    Total master and partial-share master modes. In total master mode, all resources are shared
         with other CPUs. Bus permission is shared when an external bus release request is
         received. In partial-share master mode, only the CS2 space is shared with other CPUs; all
         other spaces can be accessed at any time.
    ¾    In slave mode, the external bus is accessed when a bus use request is output and bus use
         permission is received.
•    Refresh counter can be used as an interval timer
    ¾    Interrupt request generated upon compare match (CMI interrupt request signal).


7.1.2        Block Diagram

Figure 7.1 shows the BSC block diagram.











































130
```

## PDF page 147

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=147)

```text
                                                      Bus         Internal bus
                                                    interface




                            Wait
        WAIT                control        WCR
                             unit



                                          BCR1
                            Area
     CS3–CS0                control
                             unit
                                          BCR2


          BS                                             Module bus
          RD                               MCR
         CAS
         RAS                              RTCSR
       RD/WR               Memory
    WE3–WE0                 control       RTCNT
         CKE                 unit
       IVECF                            Comparator
    CMI interrupt request

                                          RTCOR



   Interrupt
   controller  Peripheral bus
                                                           BSC


WCR: Wait control register                               RTCNT: Refresh timer counter
BCR: Bus control register                                 RTCOR: Refresh time constant register
MCR: Individual memory control register         RTCSR: Refresh timer control/status register
                     Figure 7.1   BSC Block Diagram



                                                                     131
```

## PDF page 148

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=148)

```text
7.1.3        Pin Configuration

Table 7.1 lists the bus state controller pin configuration.

Table 7.1      Pin Configuration

                      With Bus
Signal        I/O     Released     Description
A26–A0      I/O     I                    Address bus. 27 bits are available to specify a total 128 Mbytes of
                                      memory space. The most significant 2 bits are used to specify the
                                      CS space, so the size of the spaces is 32 Mbytes. When the bus is
                                      released, these become inputs for the external bus cycle address
                                      monitor.
D31–D0      I/O     Hi-Z              32-bit data bus. When reading or writing a 16-bit width area, use
                                      D15–D0; when reading or writing a 8-bit width area, use D7–D0.
                                      With 8-bit accesses that read or write a 32-bit width area, input and
                                      output the data via the byte position determined by the lower
                                      address bits of the 32-bit bus.
BS              I/O     I                    Indicates start of bus cycle or monitor. With the basic interface
                                      (device interfaces except for DRAM, synchronous DRAM, pseudo-
                                      SRAM), signal is asserted for a single clock cycle simultaneous
                                      with address output. The start of the bus cycle can be determined
                                      by this signal. This signal is asserted for 1 cycle synchronous with
                                      column address output in DRAM, synchronous DRAM and pseudo-
                                      SRAM accesses. When the bus is released, BS becomes an input
                                      for address monitoring of external bus cycles.
CS0–          O       Hi-Z              Chip select. Signals that select area; specified by A26 and A25.
CS3
RD/WR,        I/O     I                    Read/write signal. Signal that indicates access cycle direction
WE                                    (read/write). Connected to WE pin when DRAM/synchronous
                                      DRAM is connected. When the bus is released, becomes an input
                                      for address monitoring of external bus cycles.
RAS, CE    O       Hi-Z              RAS pin for DRAM/synchronous DRAM. CE pin for pseudo-SRAM.
CAS, OE    O       Hi-Z              Open when using DRAM. CAS pin for synchronous DRAM. OE pin
                                      for pseudo-SRAM.
CASHH,        O       Hi-Z              When DRAM is used, connected to CAS pin for the most
DQMUU,                                significant byte (D31–D24). When synchronous DRAM is used,
WE3                                   connected to DQM pin for the most significant byte. When pseudo-
                                      SRAM is used, connected to WE pin for the most significant byte.
                                      For basic interface, indicates writing to the most significant byte.
CASHL,        O       Hi-Z              When DRAM is used, connected to CAS pin for the second byte
DQMUL,                                (D23–D16). When synchronous DRAM is used, connected to DQM
WE2                                   pin for the second byte. When pseudo-SRAM is used, connected
                                      to WE pin for the second byte. For basic interface, indicates writing
                                      to the second byte.


132
```

## PDF page 149

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=149)

```text
Table 7.1      Pin Configuration (cont)

                      With Bus
Signal        I/O     Released     Description
CASLH,         O       Hi-Z              When DRAM is used, connected to CAS pin for the third byte
DQMLU,                                  (D15–D8). When synchronous DRAM is used, connected to DQM
WE1                                     pin for the third byte. When pseudo-SRAM is used, connected to
                                        WE pin for the third byte. For basic interface, indicates writing to
                                        the third byte.
CASLL,         O       Hi-Z              When DRAM is used, connected to CAS pin for the least significant
DQMLL,                                  byte (D7–D0). When synchronous DRAM is used, connected to
WE0                                     DQM pin for the least significant byte. When pseudo-SRAM is
                                        used, connected to WE pin for the least significant byte. For basic
                                        interface, indicates writing to the least significant byte.
RD              O       Hi-Z              Read pulse signal (read data output enable signal). Normally,
                                        connected to the device’s /OE pin; when there is an external data
                                        buffer, the read cycle data can only be output when this signal is
                                        low.
WAIT          I         Ignore           Hardware wait input.
BACK,          I         I                    Bus use enable input in partial-share master or slave mode: BACK.
BRLS                                    Bus release request input in total master: BRLS.
BREQ,          O       O                  Bus request output in partial-share master or slave mode: BREQ.
BGR                                     Bus grant output in total master: BGR.
CKE           O       O                  Synchronous DRAM clock enable control. Signal for supporting
                                        synchronous DRAM self-refresh.
IVECF        O       Hi-Z              Interrupt vector fetch.
DREQ0      I         I                    DMA request 0.
DACK0       O       O                  DMA acknowledge 0.
DREQ1      I         I                    DMA request 1.
DACK1       O       O                  DMA acknowledge 1.
Note:   Hi-Z: High impedance

















                                                                                                                           133
```

## PDF page 150

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=150)

```text
7.1.4        Register Configuration

The BSC has seven registers. These registers are used to control wait states, bus width, interfaces
with memories like DRAM, synchronous DRAM, pseudo-SRAM, and burst ROM, and DRAM,
synchronous DRAM, and pseudo-SRAM refreshing. The register configurations are shown in
table 7.2.

The size of the registers themselves is 16 bits. If read as 32 bits, the upper 16 bits are 0. In order to
prevent writing mistakes, 32-bit writes are accepted only when the value of the upper 16 bits of the
write data is H'A55A; no other writes are performed. Initialize the reserved bits.

Initialization Procedure: Do not access a space other than CS0 until the settings for the interface
to memory are completed.

Table 7.2      Register Configuration

Name                                                  Abbr.         R/W    Initial Value   Address*1      Access Size
Bus control register 1                          BCR1        R/W    H'03F0            H'FFFFFFE0  16*2, 32
Bus control register 2                          BCR2        R/W    H'00FC           H'FFFFFFE4  16*2, 32
Wait control register                            WCR         R/W    H'AAFF           H'FFFFFFE8  16*2, 32
Individual memory control register      MCR          R/W    H'0000            H'FFFFFFEC  16*2, 32
Refresh timer control/status register   RTCSR      R/W    H'0000            H'FFFFFFF0   16*2, 32
Refresh timer counter                         RTCNT      R/W    H'0000            H'FFFFFFF4   16*2, 32
Refresh time constant register            RTCOR     R/W    H'0000            H'FFFFFFF8   16*2, 32
Notes:  1.   This address is for 32-bit accesses; for 16-bit accesses add 2.
          2.   16-bit access is for read only.


7.1.5        Address Map

The SH7604 address map, which has a memory space of 256 Mbytes, is divided into four spaces.
The types and data width of devices that can be connected are specified for each space. The
overall space address map is shown in table 7.3. Since the spaces of the cache area and the cache-
through area are the same, the maximum memory space that can be connected is 128 Mbytes. This
means that when address H'20000000 is accessed in a program, the data accessed is actually in
H'00000000.

There are several spaces for cache control. These include the associative purge space for cache
purges, address array read/write space for reading and writing addresses (address tags), and data
array read/write space for forced reads and writes of data arrays.




134
```

## PDF page 151

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=151)

```text
Table 7.3      Address Map

Address                                  Space                           Memory                                       Size
H'00000000 to H'01FFFFFF   CS0 space, cache                    Ordinary space or burst ROM      32 Mbytes
                                   area
H'02000000 to H'03FFFFFF   CS1 space, cache                    Ordinary space                            32 Mbytes
                                   area
H'04000000 to H'05FFFFFF   CS2 space, cache                    Ordinary space or synchronous           32 Mbytes
                                   area                        DRAM
H'06000000 to H'07FFFFFF   CS3 space, cache                    Ordinary space, synchronous             32 Mbytes
                                   area                        DRAM, DRAM or pseudo-DRAM
H'08000000 to H'1FFFFFFF   Reserved
H'20000000 to H'21FFFFFF   CS0 space, cache-                   Ordinary space or burst ROM      (32 Mbytes)
                                   through area
H'22000000 to H'23FFFFFF   CS1 space, cache-                   Ordinary space                            (32 Mbytes)
                                   through area
H'24000000 to H'25FFFFFF   CS2 space, cache-                   Ordinary space or synchronous           (32 Mbytes)
                                   through area                DRAM
H'26000000 to H'27FFFFFF   CS3 space, cache-                   Ordinary space, synchronous             (32 Mbytes)
                                   through area                DRAM, DRAM or pseudo-DRAM
H'28000000 to H'3FFFFFFF   Reserved
H'40000000 to H'47FFFFFF   Associative purge                                                           128 Mbytes
                                   space
H'48000000 to H'5FFFFFFF   Reserved
H'60000000 to H'7FFFFFFF   Address array,                                                              512 Mbytes
                                   read/write space
H'80000000 to H'BFFFFFFF   Reserved
H'C0000000 to H'C0000FFF   Data array, read/write                                                      4 kbytes
                                   space
H'C0001000 to H'DFFFFFFF  Reserved
H'E0001000 to H'FFFF7FFF   Reserved
H'FFFF8000 to H'FFFFBFFF  For setting                                                                  16 kbytes
                                   synchronous DRAM
                                   mode
H'FFFFC000 to H'FFFFFDFF Reserved                                                                           15.5 kbytes
H'FFFFFE00 to H'FFFFFFFF On-chip peripheral                                                            512 bytes
                                   modules
Note:      Do not access reserved spaces, as this will cause operating errors.




                                                                                                                 135
```

## PDF page 152

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=152)

```text
7.2        Description of Registers


7.2.1        Bus Control Register 1 (BCR1)

                     Bit:        15            14            13            12            11            10             9              8
            Bit name:  MASTER       —            —       ENDIAN  BSTROM   PSHR     AHLW1   AHLW0
         Initial value:        —             0              0              0              0              0              1              1
                   R/W:         R             R             R           R/W         R/W         R/W         R/W         R/W


                     Bit:         7              6              5              4              3              2              1              0
            Bit name:    A1LW1    A1LW0    A0LW1    A0LW0        —        DRAM2   DRAM1   DRAM0
         Initial value:         1              1              1              1              0              0              0              0
                   R/W:      R/W         R/W         R/W         R/W           R           R/W         R/W         R/W


Initialize ENDIAN, BSTROM, PSHR and DRAM2–DRAM0 bits after a power-on reset and do
not write to them thereafter. To change other bits by writing to them, write the same value as they
are initialized to. Do not access any space other than CS0 until the register initialization ends.

•    Bit 15—Bus Arbitration (MASTER): The MASTER bit is used to check the settings of the bus
    arbitration function set by the mode settings with the external input pin. It is a read-only bit.

Bit 15 (MASTER)                Description
0                                           Master mode
1                                           Slave mode


•    Bits 14, 13, and 3—Reserved bits: These bits always read 0. The write value should always be
    0.

•    Bit 12—Endian Specification for Area 2 (ENDIAN): In big-endian format, the MSB of byte
    data is the lowest byte address and byte data goes in order toward the LSB. For little-endian
    format, the LSB of byte data is the lowest byte address and byte data goes in order toward the
    MSB. When this bit is 1, the data is rearranged into little-endian format before transfer when
    the CS2 space is read or written to. It is used when handling data with little-endian processors
    or running programs written with little-endian format in mind.

Bit 12:  ENDIAN                  Description
0                                           Big-endian, as in other areas                                               (Initial value)
1                                           Little-endian




136
```

## PDF page 153

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=153)

```text
•    Bit 11—Area 0 Burst ROM Enable (BSTROM)

Bit 11:  BSTROM                Description
0                                           Area 0 is accessed normally                                                (Initial value)
1                                           Area 0 is accessed as burst ROM


•    Bit 10—Partial Space Share Specification (PSHR): When bus arbitration is in master mode and
    the PSHR bit is 1, only area 2 is handled as a shared space. When areas other than area 2 are
    accessed, bus ownership is not requested. When this bit is 1, address monitor specification is
    disabled. This mode is called partial-share master mode. The initial value is 0.

•    Bits 9 and 8—Long Wait Specification for Areas 2 and 3 (AHLW1, AHLW0): When the basic
    memory interface setting is made for area 2 and area 3, the wait specification of this field is
    effective when the bits that specify the respective area waits in the wait control register
    (W21/W20 or W31/W30) specify long waits (i.e., 11).

Bit 9:  AHLW1       Bit 8:  AHLW0       Description
0                            0                            3 waits
                          1                            4 waits
1                            0                            5 waits
                          1                            6 waits                                                                  (Initial value)


•    Bits 7 and 6—Long Wait Specification for Area 1 (A1LW1, A1LW0): When the basic
    memory interface setting is made for area 1, the wait specification of this field is effective
    when the bits that specify the wait in the wait control register specify long wait (i.e., 11).

Bit 7:  A1LW1       Bit 6:  A1LW0       Description
0                            0                            3 waits
                          1                            4 waits
1                            0                            5 waits
                          1                            6 waits                                                                  (Initial value)


•    Bits 5 and 4—Long Wait Specification for Area 0 (A0LW1, A0LW0): When the basic
    memory interface setting is made for area 0, the wait specification of this field is effective
    when the bits that specify the wait in the wait control register specify long wait (i.e., 11).








                                                                                                                                    137
```

## PDF page 154

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=154)

```text
Bit 5:  A0LW1       Bit 4:  A0LW0       Description
0                            0                            3 waits                                                                  (Initial value)
                            1                            4 waits
1                            0                            5 waits
                            1                            6 waits


•    Bits 2 to 0—Enable for DRAM and Other Memory (DRAM2–DRAM0)

DRAM2    DRAM1    DRAM0    Description
0               0               0               Areas 2 and 3 are ordinary spaces                                 (Initial value)
                               1               Area 2 is ordinary space; area 3 is synchronous DRAM space
               1               0               Area 2 is ordinary space; area 3 is DRAM space
                               1               Area 2 is ordinary space; area 3 is pseudo-SRAM space
1               0               0               Area 2 is synchronous DRAM space, area 3 is ordinary space
                               1               Areas 2 and 3 are synchronous DRAM spaces
               1               0               Reserved (do not set)
                               1               Reserved (do not set)


7.2.2        Bus Control Register 2 (BCR2)

                       Bit:        15            14            13            12            11            10             9              8
             Bit name:        —            —            —            —            —            —            —            —
          Initial value:         0              0              0              0              0              0              0              0
                    R/W:         R             R             R             R             R             R             R             R


                       Bit:         7              6              5              4              3              2              1              0
             Bit name:    A3SZ1     A3SZ0     A2SZ1     A2SZ0     A1SZ1     A1SZ0         —            —
          Initial value:         1              1              1              1              1              1              0              0
                    R/W:      R/W         R/W         R/W         R/W         R/W         R/W           R             R


Initialize BCR2 after a power-on reset and do not write to it thereafter. When writing to it, write
the same values as those the bits are initialized to. Do not access any space other than CS0 until
the register initialization ends.







138
```

## PDF page 155

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=155)

```text
•    Bits 15 to 8—Reserved: These bits always read 0. The write value should always be 0.

•    Bits 7 and 6—Bus Size Specification for Area 3 (A3SZ1–A3SZ0). Effective only when
     ordinary space is set.

Bit 7:  A3SZ1        Bit 6:  A3SZ0        Description
0                            0                            Reserved (do not set)
                           1                            Byte (8-bit) size
1                            0                            Word (16-bit) size
                           1                            Longword (32-bit) size                                            (Initial value)


•    Bits 5 and 4—Bus Size Specification for Area 2 (A2SZ1–A2SZ0): Effective only when
     ordinary space is set.

Bit 5:  A2SZ1        Bit 4:  A2SZ0        Description
0                            0                            Reserved (do not set)
                           1                            Byte (8-bit) size
1                            0                            Word (16-bit) size
                           1                            Longword (32-bit) size                                            (Initial value)


•    Bits 3 and 2—Bus Size Specification for Area 1 (A1SZ1–A1SZ0)

Bit 3:  A1SZ1        Bit 2:  A1SZ0        Description
0                            0                            Reserved (do not set)
                           1                            Byte (8-bit) size
1                            0                            Word (16-bit) size
                           1                            Longword (32-bit) size                                            (Initial value)


•    Bits 1 and 0—Reserved: These bits always read 0. The write value should always be 0.

Note:    The bus size of area 0 is specified by the mode input pins.














                                                                                                                                           139
```

## PDF page 156

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=156)

```text
7.2.3        Wait Control Register (WCR)

                       Bit:        15            14            13            12            11            10             9              8
             Bit name:      IW31        IW30       IW21        IW20       IW11        IW10        IW01       IW00
          Initial value:         1              0              1              0              1              0              1              0
                    R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


                       Bit:         7              6              5              4              3              2              1              0
             Bit name:      W31         W30        W21         W20        W11         W10         W01        W00
          Initial value:         1              1              1              1              1              1              1              1
                    R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


Do not access a space other than CS0 until the settings for register initialization are completed.

•    Bits 15 to 8—Idles between Cycles for Areas 3 to 0 (IW31–IW00): These bits specify idle
     cycles inserted between consecutive accesses to different areas. Idles are used to prevent data
     conflict between ROM or the like, which is slow to turn the read buffer off, and fast memories
     and I/O interfaces. Even when access is to the same area, idle cycles must be inserted when a
     read access is followed immediately by a write access. The idle cycles to be inserted comply
     with the specification for the previously accessed area.

IW31, IW21, IW11, IW01      IW30, IW20, IW10, IW00      Description
0                                            0                                            No idle cycle
                                          1                                            One idle cycle inserted
1                                            0                                            Two idle cycles inserted      (Initial value)
                                          1                                            Reserved (do not set)


•    Bits 7 to 0—Wait Control for Areas 3 to 0 (W31–W00)

     During the basic cycle:

W31, W21, W11, W01        W30, W20, W10, W00        Description
0                                         0                                         External wait input disabled without wait
0                                         1                                         External wait input enabled with one wait
1                                         0                                         External wait input enabled with two waits
1                                         1                                         Complies with the long wait specification of
                                                                                bus control register 1 (BCR1). External wait
                                                                                input is enabled                        (Initial value)




140
```

## PDF page 157

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=157)

```text
     When area 3 is DRAM, the number of CAS assert cycles is specified by wait control bits W31
     and W30:

Bit 7:  W31                         Bit 6:  W30                         Description
0                                         0                                         1 cycle
                                        1                                         2 cycles
1                                         0                                         3 cycles
                                        1                                         Reserved (do not set)


     When the setting is for 2 or more cycles, external wait input is enabled.

     When area 2 or 3 is synchronous DRAM, CAS latency is specified by wait control bits W31
     and W30, and W21 and W20, respectively:

W31, W21                          W30, W20                          Description
0                                         0                                         1 cycle
                                        1                                         2 cycles
1                                         0                                         3 cycles
                                        1                                         4 cycles


     With synchronous DRAM, external wait input is ignored regardless of any setting.

     When area 3 is pseudo-SRAM, the number of cycles from BS signal assertion to the end of the
     cycle is specified by wait control bits W31 and W30:

Bit 7:  W31                         Bit 6:  W30                         Description
0                                         0                                         2 cycles
                                        1                                         3 cycles
1                                         0                                         4 cycles
                                        1                                         Reserved (do not set)


     When the setting is for 3 or more cycles, external wait input is enabled.












                                                                                                                                               141
```

## PDF page 158

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=158)

```text
7.2.4        Individual Memory Control Register (MCR)

                     Bit:        15            14            13            12            11            10             9              8
            Bit name:      TRP        RCD       TRWL     TRAS1    TRAS0        BE        RASD         —
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W           R


                     Bit:         7              6              5              4              3              2              1              0
            Bit name:     AMX2         SZ        AMX1      AMX0      RFSH       RMD          —            —
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W           R             R


The TRP, RCD, TRWL, TRAS1–TRAS0, BE, RASD, AMX2–AMX0 and SZ bits are initialized
after a power-on reset. Do not write to them thereafter. When writing to them, write the same
values as they are initialized to. Do not access any space other than CS2 and CS3 until the register
initialization ends.

•    Bit 15—RAS Precharge Time (TRP): When DRAM is connected, specifies the minimum
    number of cycles after RAS is negated before the next assert. When pseudo-SRAM is
    connected, specifies the minimum number of cycles after CE is negated before the next assert.
    When synchronous DRAM is connected, specifies the minimum number of cycles after
    precharge until a bank active command is output. See section 7.5, Synchronous DRAM
    Interface, for details.

Bit 15:  TRP                         Description
0                                           1 cycle                                                                                  (Initial value)
1                                           2 cycles


•    Bit 14—RAS-CAS Delay (RCD): When DRAM is connected, specifies the number of cycles
    after RAS is asserted before CAS is asserted. When pseudo-SRAM is connected, specifies the
    number of cycles after CE is asserted before BS is asserted. When synchronous DRAM is
    connected, specifies the number of cycles after a bank active (ACTV) command is issued until
    a read or write command (READ, READA, WRIT, WRITA) is issued.

Bit 14:  RCD                        Description
0                                           1 cycle                                                                                  (Initial value)
1                                           2 cycles






142
```

## PDF page 159

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=159)

```text
•    Bit 13—Write-Precharge Delay (TRWL): When the synchronous DRAM is not in the bank
    active mode, this bit specifies the number of cycles between the write cycle and the start-up of
    the auto-precharge. The timing from this point to the point at which the next command can be
    issued is calculated within the bus state controller. In bank active mode, this bit specifies the
    period for which the precharge command is disabled after the write command (WRIT) is
    issued. This bit is ignored when memory other than synchronous DRAM is connected.

Bit 13:  TRWL                     Description
0                                           1 cycle                                                                                  (Initial value)
1                                           2 cycles


•    Bits 12 and 11—CAS-Before-RAS Refresh RAS Assert Time (TRAS1–TRAS0): The RAS
    assertion width for DRAM is TRAS; the OE width for pseudo-SRAM is TRAS + 1 cycle.
    After an auto-refresh command is issued, the synchronous DRAM does not issue a bank active
    command for TRAS + 2 cycles, regardless of the TRP bit setting. For synchronous DRAMs,
    there is no RAS assertion period, but there is a limit for the time from the issue of a refresh
    command until the next access. This value is set to observe this limit. Commands are not
    issued for TRAS + 1 cycle when self-refresh is cleared.

Bit 12:  TRAS1      Bit 11:  TRAS0      Description
0                            0                            2 cycles                                                                (Initial value)
                         1                            3 cycles
1                            0                            4 cycles
                         1                            Reserved (do not set)


•    Bit 10—Burst Enable (BE)

Bit 10:  BE                           Description
0                                           Burst disabled                                                                      (Initial value)
1                                           High-speed page mode during DRAM interfacing is enabled. Data is
                                     continuously transferred in static column mode during pseudo-SRAM
                                     interfacing. During synchronous DRAM access, burst operation is
                                     always enabled regardless of this bit.













                                                                                                                                143
```

## PDF page 160

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=160)

```text
•    Bit 9—RAS Down Mode (RASD)

Bit 9:  RASD                        Description
0                                           For DRAM, RAS is negated after access ends (normal operation).
                                      For synchronous DRAM, a read or write is performed using auto-
                                      precharge mode. The next access always starts with a bank active
                                      command.
1                                           For DRAM, after access ends RAS down mode is entered in which RAS
                                      is left asserted. When using this mode with an external device
                                      connected which performs writes other than to DRAM, see section
                                      7.6.5, Burst Access.
                                      For synchronous DRAM, access ends in the bank active state. This is
                                      only valid for area 3. When area 2 is synchronous DRAM, the mode is
                                      always auto-precharge.


•    Bits 7, 5, and 4—Address Multiplex (AMX2–AMX0)

    For DRAM interface:

Bit 7:                   Bit 5:                   Bit 4:
AMX2                     AMX1                     AMX0                   Description
0                            0                           0                           8-bit column address DRAM
                                                  1                           9-bit column address DRAM
                         1                           0                           10-bit column address DRAM
                                                  1                           11-bit column address DRAM
1                            0                           0                           Reserved (do not set)
                                                  1                           Reserved (do not set)
                         1                           0                           Reserved (do not set)
                                                  1                           Reserved (do not set)



















144
```

## PDF page 161

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=161)

```text
    For synchronous DRAM interface:

Bit 7:                    Bit 5:                   Bit 4:
AMX2                      AMX1                     AMX0                   Description
0                            0                           0                           16-Mbit DRAM (1M · 16 bits)
                                                   1                           16-Mbit DRAM (2M · 8 bits)*
                          1                           0                           16-Mbit DRAM (4M · 4 bits)*
                                                   1                           4-Mbit DRAM (256k · 16 bits)
1                            0                           0                           Reserved (do not set)
                                                   1                           Reserved (do not set)
                          1                           0                           Reserved (do not set)
                                                   1                           2-Mbit DRAM (128k · 16 bits)
Note:   Reserved. Do not set when SZ bit in MCR is 0 (16-bit bus width).


•    Bit 6—Memory Data Size (SZ): For synchronous DRAM, DRAM, and pseudo-SRAM space,
    the data bus width of BCR2 is ignored in favor of the specification of this bit.

Bit 6:  SZ                             Description
0                                           Word                                                                                     (Initial value)
1                                           Longword


•    Bit 3—Refresh Control (RFSH): This bit determines whether or not the refresh operation of
    DRAM/synchronous DRAM/pseudo-SRAM is performed. This bit is not valid in the slave
    mode and is always handled as 0.

Bit 3:  RFSH                        Description
0                                           No refresh                                                                            (Initial value)
1                                           Refresh


•    Bit 2—Refresh Mode (RMODE): When the RFSH bit is 1, this bit selects normal refresh or
    self-refresh. When the RFSH bit is 0, do not set this bit to 1. When the RFSH bit is 1, self-
    refresh mode is entered immediately after the RMD bit is set to 1. When the RFSH bit is 1 and
    this bit is 0, a CAS-before-RAS refresh or auto-refresh is performed at the interval set in the 8-
    bit interval timer. When a refresh request occurs during an external area access, the refresh is
    performed after the access cycle is completed. When set for self-refresh, self-refresh mode is
    entered immediately unless the SH7604 is in the middle of an synchronous DRAM or pseudo-
    SRAM area access. If it is, self-refresh mode is entered when the access ends. Refresh requests
    from the interval timer are ignored during self-refresh. Self-refresh is not supported for
    DRAM, so always set RMD to 0 when using DRAM.


                                                                                                                                   145
```

## PDF page 162

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=162)

```text
Bit 2:  RMODE                    Description
0                                           Normal refresh                                                                     (Initial value)
1                                           Self-refresh


•    Bits 8, 1, and 0—Reserved: These bits always read 0.


7.2.5        Refresh Timer Control/Status Register (RTCSR)

                       Bit:        15            14            13            12            11            10             9              8
             Bit name:        —            —            —            —            —            —            —            —
          Initial value:         0              0              0              0              0              0              0              0
                    R/W:         R             R             R             R             R             R             R             R


                       Bit:         7              6              5              4              3              2              1              0
             Bit name:      CMF        CMIE      CKS2       CKS1      CKS0         —            —            —
          Initial value:         0              0              0              0              0              0              0              0
                    R/W:      R/W         R/W         R/W         R/W         R/W           R             R             R


•    Bits 15 to 8—Reserved: These bits always read 0.

•    Bit 7—Compare Match Flag (CMF): This status flag, which indicates that the values of
     RTCNT and RTCOR match, is set/cleared under the following conditions:

Bit 7:  CMF                          Description
0                                           RTCNT and RTCOR match
                                         Clear condition: After RTCSR is read when CMF is 1, 0 is written in
                                         CMF
1                                           RTCNT and RTCOR do not match
                                         Set condition: RTCNT = RTCOR


•    Bit 6—Compare Match Interrupt Enable (CMIE): Enables or disables an interrupt request
     caused by the CMF bit of RTSCR when CMF is set to 1.

Bit 6:  CMIE                         Description
0                                           Interrupt request caused by CMF is disabled                       (Initial value)
1                                           Interrupt request caused by CMF is enabled






146
```

## PDF page 163

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=163)

```text
•    Bits 5 to 3—Clock Select Bits (CKS2–CKS0)

Bit 5:  CKS2          Bit 4:  CKS1        Bit 3:  CKS0        Description
0                            0                           0                           Count-up disabled                     (Initial value)
                                                        1                           CLK/4
                            1                           0                           CLK/16
                                                        1                           CLK/64
1                            0                           0                           CLK/256
                                                        1                           CLK/1024
                            1                           0                           CLK/2048
                                                        1                           CLK/4096


•    Bits 2 to 0—Reserved: These bits always read 0. The write value should always be 0.


7.2.6        Refresh Timer Counter (RTCNT)

                       Bit:        15            14            13            12            11            10             9              8
              Bit name:        —            —            —            —            —            —            —            —
          Initial value:         0              0              0              0              0              0              0              0
                    R/W:         R             R             R             R             R             R             R             R


                       Bit:         7              6              5              4              3              2              1              0
              Bit name:
          Initial value:         0              0              0              0              0              0              0              0
                    R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


The 8-bit counter RTCNT counts up with input clocks. The clock select bit of RTCSR selects an
input clock. RTCNT values can always be read/written by the CPU. When RTCNT matches
RTCOR, RTCNT is cleared. Returns to 0 after it counts up to 255.

•    Bits 15 to 8—Reserved: These bits always read 0. The write value should always be 0.












                                                                                                                                                 147
```

## PDF page 164

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=164)

```text
7.2.7        Refresh Time Constant Register (RTCOR)

                     Bit:        15            14            13            12            11            10             9              8
            Bit name:        —            —            —            —            —            —            —            —
         Initial value:         0              0              0              0              0              0              0              0
                  R/W:         R             R             R             R             R             R             R             R


                     Bit:         7              6              5              4              3              2              1              0
            Bit name:
         Initial value:         0              0              0              0              0              0              0              0
                  R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


RTCOR is an 8-bit read/write register. The values of RTCOR and RTCNT are constantly
compared. When the values correspond, the compare match flag in RTCSR is set and RTCNT is
cleared to 0. When the refresh bit (RFSH) in the individual memory control register is set to 1, a
refresh request signal occurs. The refresh request signal is held until refresh operation is
performed. If the refresh request is not processed before the next match, the previous request
becomes ineffective.

When the CMIE bit in RTSCR is set to 1, an interrupt request is sent to the controller by this
match signal. The interrupt request is output continuously until the CMF bit in RTSCR is cleared.
When the CMF bit clears, it only affects the interrupt; the refresh request is not cleared by this
operation. When a refresh is performed and refresh requests are counted using interrupts, a refresh
can be set simultaneously with the interval timer interrupt.

•    Bits 15 to 8—Reserved: These bits always read 0. The write value should always be 0.

7.3        Access Size and Data Alignment


7.3.1        Connection to Ordinary Devices

Byte, word, and longword are supported as access units. Data is aligned based on the data width of
the device. Therefore, reading longword data from a byte-width device requires four read
operations. The bus state controller automatically converts data alignment and data length between
interfaces. The data width for external devices can be connected to either 8 bits, 16 bits or 32 bits
by setting BCR2 (for the CS1–CS3 spaces) or using the mode pins (for the CS0 space). Since the
data width of devices connected to the respective spaces is specified statically, however, the data
width cannot be changed for each access cycle.





148
```

## PDF page 165

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=165)

```text
Instruction fetches are always performed in 32-bit units. When branching to an odd word
boundary (4n + 2 address), instruction fetches are performed in longword units from a 4n address.
Figures 7.2 to 7.4 show the relationship between device data widths and access units.


   A26–A0    D31          D23         D15         D7       D0   32-bit device data input/output pin
   000000     70                                                Byte read/write of address 0
   000001                  70                                   Byte read/write of address 1
   000002                              70                       Byte read/write of address 2
   000003                                          70Byte read/write of address 3
   000000     15         8 70                                   Word read/write of address 0
   000002                              15         870Word read/write of address 2
   000000     31       24  23       16 15         8   7           0Longword read/write of address 0

            Figure 7.2   32-Bit External Devices and Their Access Units (Ordinary)


               A26–A0     D15         D7      D0    16-bit device data input/output pin
               000000     70                        Byte read/write of address 0
               000001                  70Byte read/write of address 1
               000002     70                        Byte read/write of address 2
               000003                  70Byte read/write of address 3
               000000     15                   0    Word read/write of address 0
               000002     15                   0    Word read/write of address 2
               000000     31                  16
               000002     15                   0    Longword read/write of address 0

            Figure 7.3   16-Bit External Devices and Their Access Units (Ordinary)


                     A26–A0      D7      D0   8-bit device data input/output pin
                      000000     70Byte read/write of address 0
                      000001     70Byte read/write of address 1
                      000002     70Byte read/write of address 2
                      000003     70Byte read/write of address 3
                      000000     15         8
                      000001     70Word read/write of address 0
                      000002     15         8
                      000003     70Word read/write of address 2
                      000000     31       24
                      000001     23       26
                      000002     15         8
                      000003     70Longword read/write of address 0

            Figure 7.4   8-Bit External Devices and Their Access Units (Ordinary)


                                                                                                   149
```

## PDF page 166

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=166)

```text
7.3.2        Connection to Little-Endian Devices

The SH7604 provides a conversion function in CS2 space for connection to and to maintain
program compatibility with devices that use little-endian format (in which the LSB is the 0
position in the byte data lineup). When the endian specification bit of BCR1 is set to 1, CS2 space
is little-endian. The relationship between device data width and access unit for little-endian format
is shown in figures 7.5 and 7.6. When sharing memory or the like with a little-endian bus master,
the SH7604 connects D31–D24 to the least significant byte of the other bus master and D7–D0 to
the most significant byte, when the bus width is 32 bits. When the width is 16 bits, the SH7604
connects D15–D8 to the least significant byte of the other bus master and D7–D0 to the most
significant byte.

When support software like the compiler or linker does not support switching, the instruction code
and constants in the program do not become little-endian. For this reason, be careful not to place
program code or constants in the CS2 space. When instructions or data in other CS spaces are used
by transferring them to CS2 space with the SH7604, there is no problem because the SH7604
converts the endian format. Programs that are designed for use with little-endian format assume
that the LSB is stored in the lowest address. Even when a program written in a high-level language
like C is recompiled as is, it may not execute properly. The sign bit of signed 16-bit data at address
0 is stored at address 1 in little-endian format and at address 0 in big-endian format. It is possible
to correctly execute a program written for little-endian format by allocating the program and
constants to an area other than CS2 space and the data area to CS2 space. Note that the SH7604
does not support little-endian mode for devices with an 8-bit data bus width.


   A26–A0       D31          D23           D15           D7        D0    32-bit device data input/output pin
    000000      70                                                       Byte read/write of address 0
    000001                     70                                        Byte read/write of address 1
    000002                                  70                           Byte read/write of address 2
    000003                                                70Byte read/write of address 3
    000000      7015         8                                           Word read/write of address 0
    000002                                  7015         8               Word read/write of address 2
    000000      7015         8              23       16   31       24    Longword read/write of address 0

      Figure 7.5   32-Bit External Devices and Their Access Units (Little-Endian Format)














150
```

## PDF page 167

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=167)

```text
                 A26–A0      D15           D7        D0    16-bit device data input/output pin
                  000000      70                           Byte read/write of address 0
                  000001                    70Byte read/write of address 1
                  000002      70                           Byte read/write of address 2
                  000003                    70             Byte read/write of address 3
                  000000      7         015           8    Word read/write of address 0
                  000002      7         015           8    Word read/write of address 2
                  000000       7        015           8
                  000002      23       16    31      24    Longword read/write of address 0

      Figure 7.6   16-Bit External Devices and Their Access Units (Little-Endian Format)

Using the Little-Endian Function: The SH7604 normally uses big-endian alignment for data
input and output, but an endian conversion function is provided for the CS2 space to enable
connection to little-endian devices. The following two points should be noted when using this
function:

•    Little endian alignment should be used in the CS2 through-area.
•    When data is shared with another little-endian device using this function, the same access size
    must be used by both. For example, to read data written in longword size by another little-
    endian device, the SH7604 must use longword read access.

7.4        Accessing Ordinary Space


7.4.1        Basic Timing

A strobe signal is output by ordinary space accesses of CS0–CS3 spaces to provide primarily for
SRAM direct connections. Figure 7.7 shows the basic timing of ordinary space accesses. Ordinary
accesses without waits end in 2 cycles. The BS signal is asserted for 1 cycle to indicate the start of
the bus cycle. The CSn signal is negated by the fall of clock T2 to ensure the negate period. The
negate period is thus half a cycle when accessed at the minimum pitch.

The access size is not specified during a read. The correct access start address will be output to the
LSB of the address, but since no access size is specified, the read will always be 32 bits for 32-bit
devices and 16 bits for 16-bit devices. For writes, only the WE signal of the byte that will be
written is asserted. For 32-bit devices, WE3 specifies writing to a 4n address and WE0 specifies
writing to a 4n+3 address. For 16-bit devices, WE1 specifies writing to a 2n address and WE0
specifies writing to a 2n+1 address. For 8-bit devices, only WE0 is used.

The RD signal must be used to control data output of external devices so that conflicts do not
occur between trace information for emulators or the like output from the SH7604 and external
device read data. In other words, when data buses are provided with buffers, the RD signal must
be used for data output in the read direction. When RD/WR signals do not perform accesses, the


                                                                                                                 151
```

## PDF page 168

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=168)

```text
chip stays in read status, so there is a danger of conflicts occurring with output when this is used to
control the external data buffer.


                                                         T1                  T2



                                        CKIO


                                     A26–A0


                                         CSn


                                     RD/WR


                                           RD
                         Read
                                     D31–D0


                                         WEn
                         Write
                                     D31–D0


                                           BS



                          Figure 7.7   Basic Timing of Ordinary Space Access

Figure 7.8 shows an example of a 32-bit data width SRAM connection, figure 7.9 a 16-bit data
width SRAM connection, and figure 7.10 an 8-bit data width SRAM connection.















152
```

## PDF page 169

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=169)

```text
    SH7604                                                                                          128 k × 8-bitSRAM

          A18                          A16
           ……                        …
          A2                           A0
         CSn/CR                        CS/CR
          RD/CR                        OE/CR
         D31                           I/O7
           …      ……                 ……
         D24                           I/O0
CASHH/DQMUU/WE3/CR                     WE
         D23
           ……
         D16                           A16
CASHL/DQMUL/WE2/CR                   ……
         D15                           A0
           ……                          CS/CR
          D8                           OE/CR
CASLH/DQMLU/WE1/CR                     I/O7
          D7                         ……
           ……                          I/O0
          D0                           WE
CASLL/DQMLL/WE0


                                       A16
                                     ……
                                       A0
                                       CS/CR
                                       OE/CR
                                       I/O7
                                     ……
                                       I/O0
                                       WE


                                       A16
                                     ……
                                       A0
                                       CS/CR
                                       OE/CR
                                       I/O7
                                     ……
                                       I/O0
                                       WE


    Figure 7.8   Example of 32-Bit Data Width SRAM Connection



                                               153
```

## PDF page 170

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=170)

```text
           SH7604                                                                                     128 k × 8-bitSRAM

                  A17                               A16
                    ……                           …
                   A1                               A0
                  CSn/CR                            CS/CR
                   RD/CR                            OE/CR
                  D15                               I/O7
                    …     ……                     ……
                   D8                               I/O0
      CASLH/DQMLU/WE1/CR                            WE
                   D7
                    ……
                   D0                               A16
       CASLL/DQMLL/WE0                           ……
                                                    A0
                                                    CS/CR
                                                    OE/CR
                                                    I/O7
                                                 ……
                                                    I/O0
                                                    WE


           Figure 7.9   Example of 16-Bit Data Width SRAM Connection



          SH7604                                                                                        128 k × 8 bitSRAM

                  A16                                A16
                   ……                             …
                  A0                                 A0
                 CSn/CR                              CS/CR
                  RD/CR                              OE/CR
                  D7                                 I/O7
                   …     ……                       ……
                  D0                                 I/O0
      CASLL/DQMLL/WE0                                WE






           Figure 7.10   Example of 8-Bit Data Width SRAM Connection






154
```

## PDF page 171

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=171)

```text
7.4.2        Wait State Control

The number of wait states inserted into ordinary space access states can be controlled using the
WCR and BCR1 register settings. When the Wn1 and Wn0 wait specification bits in WCR for the
given CS space are 01 or 10, software waits are inserted according to the wait specification. When
Wn1 and Wn0 are 11, wait cycles are inserted according to the long wait specification bit AnLW
in BCR1. The long wait specification in BCR1 can be made independently for CS0 and CS1
spaces, but the same value must be specified for CS2 and CS3 spaces. All WCR specifications are
independent. A Tw cycle as long as the number of specified cycles is inserted as a wait cycle at the
wait timing for ordinary access space shown in figure 7.11.


                                                 T1                 Tw            T2



                                CKIO


                             A26–A0


                                 CSn


                             RD/WR


                                   RD
                 Read
                             D31–D0


                                 WEn
                 Write
                             D31–D0


                                   BS



           Figure 7.11   Wait Timing of Ordinary Space Access (Software Wait Only)







                                                                                                                 155
```

## PDF page 172

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=172)

```text
When the wait is specified by software using WCR, the wait input WAIT signal from outside is
sampled. Figure 7.12 shows WAIT signal sampling. A 2-cycle wait is specified as a software wait.
The sampling is performed when the Tw state shifts to the T2 state, so there is no effect even when
the WAIT signal is asserted in the T1 cycle or the first Tw cycle. The WAIT signal is sampled at
the clock rise. External waits should not be inserted, however, into word accesses of devices (such
as ordinary space and burst ROM) that have an 8-bit bus width (byte-size devices). Control waits
in such cases with software only.


                                                                             Wait states
                                                                             from WAIT
                                                                            signal input
                                     T1             Tw             Tw            Twx              T2


                   Clock


                 A26–A0


                     CSn


                 RD/WR


                      RD
      Read
                D31–D0


                    WEn
      Write
                D31–D0

                   WAIT


                       BS


                      Figure 7.12   Wait State Timing of Ordinary Space Access
                                      (Wait States from WAIT Signal)








156
```

## PDF page 173

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=173)

```text
7.5        Synchronous DRAM Interface


7.5.1        Synchronous DRAM Direct Connection

2-Mbit (128k · 16), 4-Mbit (256k · 16), and 16-Mbit (1M · 16, 2M · 8, and 4M · 4)
synchronous DRAMs can be connected directly to the SH7604. All of these are internally divided
into two banks. Since synchronous DRAM can be selected by the CS signal, areas CS2 and CS3
can be connected using a common RAS or other control signal. When the enable bits for DRAM
and other memory (DRAM2–DRAM0) in BCR1 are set to 001, CS2 is ordinary space and CS3 is
synchronous DRAM space. When set to 100, CS2 is synchronous DRAM space and CS3 is
ordinary space. When set to 101, both CS2 and CS3 are synchronous DRAM spaces.

The supported synchronous DRAM operating mode is for burst read and single write. The burst
length depends on the data bus width, comprising 4 bursts for a 32-bit width, and 8 bursts for a 16-
bit width. The data bus width is specified by the SZ bit in MCR. Burst operation is always
performed, so the burst enable (BE) bit in MCR is ignored.

Control signals for directly connecting synchronous DRAM are the RAS/CE, CAS/OE, RD/WR,
CS2 or CS3, DQMUU, DQMUL, DQMLU, DQMLL, and CKE signals. Signals other than CS2
and CS3 are common to every area, and signals other than CKE are valid and fetched only when
CS2 or CS3 is true. Therefore, synchronous DRAM of multiple areas can be connected in parallel.
CKE is negated (to the low level); only when a self-refresh is performed otherwise it is asserted
(to the high level).

Commands can be specified for synchronous DRAM using the RAS/CE, CAS/OE, RD/WR, and
certain address signals. These commands are NOP, auto-refresh (REF), self-refresh (SELF), all-
bank precharge (PALL), specific bank precharge (PRE), row address strobe/bank active (ACTV),
read (READ), read with precharge (READA), write (WRIT), write with precharge (WRITA), and
mode register write (MRS).

Bytes are specified using DQMUU, DQMUL, DQMLU, and DQMLL. The read/write is
performed on the byte whose DQM is low. For 32-bit data, DQMUU specifies 4n address access
and DQMLL specifies 4n + 3 address access. For 16-bit data, only DQMLU and DQMLL are
used. Figure 7.13 shows an example in which a 32-bit connection uses a 256k · 16 bit
synchronous DRAM. Figure 7.14 shows an example with a 16-bit connection.













                                                                                                              157
```

## PDF page 174

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=174)

```text
                                                  256 k × 16-bit
         SH7604                                   synchronous
                                                    DRAM
                A11                               A9
                  ……                           ……
                 A2                               A0
               CKIO                               CLK
                CKE                               CKE
                CSn/CR                            CS/CR
             RAS/CE/CR                            RAS/CR
             CAS/OE/CR                            CAS/CR
              RD/WR/CR                            WE/CR
                D31                               I/O15
                  ……                           ……
                D16                               I/O0
    CASHH/DQMUU/WE3/CR                            DQMU
     CASHL/DQMUL/WE2/CR                           DQML
                D15
                  ……                              A9
                 D0                            ……
     CASLH/DQMLU/WE1/CR                           A0
     CASLL/DQMLL/WE0/CR                           CLK
                                                  CKE
                                                  CS/CR
                                                  RAS/CR
                                                  CAS/CR
                                                  WE/CR
                                                  I/O15
                                               ……
                                                  I/O0
                                                  DQMU
                                                  DQML

           Figure 7.13   Synchronous DRAM 32-bit Device Connection


















158
```

## PDF page 175

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=175)

```text
                                                                                  256 k × 16-bit
                                                                                  synchronous
                 SH7604                                                               DRAM

                            A10                                                   A9
                               ……                                            ……
                              A1                                                  A0
                           CKIO                                                   CLK
                            CKE                                                   CKE
                            CSn                                                   CS/CR
                        RAS/CE/CR                                                 RAS/CR
                        CAS/OE/CR                                                 CAS/CR
                         RD/WR/CR                                                 WE/CR
                            D15                                                   I/O15
                               ……                                            ……
                              D0                                                  I/O0
         CASLH/DQMLU/WE1/CR                                                       DQMU
          CASLL/DQMLL/WE0/CR                                                      DQML






                  Figure 7.14   Synchronous DRAM 16-bit Device Connection


7.5.2        Address Multiplexing

Addresses are multiplexed according to the MCR’s address multiplex specification bits AMX2–
AMX0 and size specification bit SZ so that synchronous DRAMs can be connected directly
without an external multiplex circuit. Table 7.4 shows the relationship between the multiplex
specification bits and bit output to the address pins.

A26–A14 and A0 always output the original value regardless of multiplexing.

When SZ = 0, the data width on the synchronous DRAM side is 16 bits and the LSB of the
device’s address pins (A0) specifies word address. The A0 pin of the synchronous DRAM is thus
connected to the A1 pin of the SH7604, the rest of the connection proceeding in the same order,
beginning with the A1 pin to the A2 pin.

When SZ = 1, the data width on the synchronous DRAM side is 32 bits and the LSB of the
device’s address pins (A0) specifies longword address. The A0 pin of the synchronous DRAM is
thus connected to the A2 pin of the SH7604, the rest of the connection proceeding in the same
order, beginning with the A1 pin to the A3 pin.





                                                                                                     159
```

## PDF page 176

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=176)

```text
Table 7.4      SZ and AMX Bits and Address Multiplex Output

              Setting                                                                      External Address Pins
SZ   AMX2    AMX1    AMX0    Output Timing      A1–A8        A9       A10      A11      A12      A13
1      0            0            0            Column address     A1–A8        A9       A10      A11      L/H*1     A21*2
                                         Row address          A9–A16      A17     A18      A19      A20      A21*2
1      0            0            1            Column address     A1–A8        A9       A10      A11      L/H*1     A22*2
                                         Row address          A10–A17    A18     A19      A20      A21      A22*2
1      0            1            0            Column address     A1–A8        A9       A10      A11      L/H*1     A23*2
                                         Row address          A11–A18    A19     A20      A21      A22      A23*2
1      0            1            1            Column address     A1–A8        A9       L/H*1    A19*2   A12      A13
                                         Row address          A9–A16      A17     A18      A19*2   A20      A21
1      1            1            1            Column address     A1–A8        A9       L/H*1    A18*2   A12      A13
                                         Row address          A9–A16      A17     A17      A18*2   A20      A21
0      0            0            0            Column address     A1–A8        A9       A10      L/H*1    A20*2    A13
                                         Row address          A9–A16      A17     A18      A19      A20*2    A21
0      0            1            1            Column address     A1–A8        L/H*1   A18*2   A11      A12      A13
                                         Row address          A9–A16      A17     A18*2   A19      A20      A21
0      1            1            1            Column address     A1–A8        L/H*1   A17*2   A11      A12      A13
                                         Row address          A9–A16      A16     A17*2   A19      A20      A21
AMX2–AMX0 settings of 100, 101 and 110 are reserved, so do not use them. When SZ = 0, the
settings 001 and 010 are reserved as well, so do not use them either.
Notes:  1.   L/H is a bit used to specify commands. It is fixed at L or H according to the access
               mode.
          2.   Specifies bank address.


7.5.3        Burst Reads

Figure 7.15 shows the timing chart for burst reads. In the following example, 2 synchronous
DRAMs of 256k · 16 bits are connected, the data width is 32 bits and the burst length is 4. After a
Tr cycle that performs ACTV command output, a READA command is called in the Tc cycle and
read data is accepted at internal clock falls from Td1 to Td4. Tap is a cycle for waiting for the
completion of the auto-precharge based on the READA command within the synchronous DRAM.
During this period, no new access commands are issued to the same bank. Accesses of the other
bank of the synchronous DRAM by another CS space are possible. Depending on the TRP
specification in MCR, the SH7604 determines the number of Tap cycles and does not issue a
command to the same bank during that period.



160
```

## PDF page 177

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=177)

```text
Figure 7.15 shows an example of the basic cycle. Because a slower synchronous DRAM is
connected, setting WCR and MCR bits can extend the cycle. The number of cycles from the
ACTV command output cycle Tr to the READA command output cycle Tc can be specified by the
RCD bit in MCR. 0 specifies 1 cycle; 1 specifies 2 cycles. For 2 cycles, a NOP command issue
cycle Trw for the synchronous DRAM is inserted between the Tr cycle and the Tc cycle. The
number of cycles between the READA command output cycle Tc and the initial read data fetch
cycle Td1 can be specified independently for areas CS2 and CS3 between 1 cycle and 4 cycles
using the W21/W20 and W31/W30 bits in WCR. The CAS latency when using bus arbitration in
the partial-share master mode can be set differently for CS2 and CS3 spaces. The number of
cycles at this time corresponds to the number of CAS latency cycles of the synchronous DRAM.
When 2 cycles or more, a NOP command issue cycle Tw is inserted between the Tc cycle and the
Td1 cycle. The number of cycles in the precharge completion waiting cycle Tap is specified by the
TRP bit in MCR. When the CAS latency is 1, a Tap cycle of 1 or 2 cycles is generated. When the
CAS latency is 2 or more, a Tap cycle equal to the TRP specification – 1 is generated. During the
Tap cycle, no commands other than NOP are issued to the same bank. Figure 7.16 shows an
example of burst read timing when RCD is 1, W31/W30 is 01, and TRP is 1.

With the synchronous DRAM cycle, when the bus cycle starts in ordinary space access, the BS
signal asserted for 1 cycle is asserted in each of cycles Td1–Td4 for the purpose of the external
address monitoring described in the section on bus arbitration. When another CS space is accessed
after an synchronous DRAM read with a wait-between-buses specification of 0, the BS signal may
be continuously asserted. The address is updated every time data is fetched while burst reads are
being performed. The burst transfer unit is 16 bytes, so address updating affects A3–A1. The
access order follows the address order in 16-byte data transfers by the DMAC, but reading starts
from the address + 4 so that the last missed data in the fill operation after a cache miss can be read.

When the data width is 16 bits, 8 burst cycles are required for a 16-byte data transfer. The data
fetch cycle goes from Td1 to Td8. From Td1 to Td8, the BS signal is asserted in every cycle.

Synchronous DRAM CAS latency is up to 3 cycles, but the CAS latency of the bus state controller
can be specified up to 4. This is so that circuits containing latches can be installed between
synchronous DRAMs and the SH7604.

















                                                                                                                   161
```

## PDF page 178

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=178)

```text
                    Tr         Tc        Td1       Td2        Td3        Td4       Tap


       CKIO

   A26–A11

        A10


     A9–A1

     CS2 or
        CS3

       RAS


       CAS

        WE

     DQMxx


    D31–D0

         BS

                   Figure 7.15   Basic Burst Read Timing (Auto-Precharge)


























162
```

## PDF page 179

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=179)

```text
                   Tr         TrW         Tc    Tw        Td1        Td2      Td3       Td4        Tap


    CKIO

A26–A11

      A10


  A9–A1

  CS2 or
     CS3

     RAS


     CAS

      WE

  DQMxx


 D31–D0

       BS


          Figure 7.16   Burst Read Wait Specification Timing (Auto-Precharge)


























                                                                                                          163
```

## PDF page 180

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=180)

```text
7.5.4        Single Reads

When a cache area is accessed and there is a cache miss, the cache fill cycle is performed in 16-
byte units. This means that all the data read in the burst read is valid. Since the required data when
a cache-through area is accessed has a maximum length of 32 bits, however, the remaining 12
bytes are wasted. The same kind of wasted data access is produced when synchronous DRAM is
specified as the source in a DMA transfer by the DMAC and the transfer unit is other than 16
bytes. Figure 7.17 shows the timing of a single address read. Because the synchronous DRAM is
set to the burst read/single write mode, the read data output continues after the required data is
received. To avoid data conflict, an empty read cycle is performed from Td2 to Td4 after the
required data is read in Td1 and the device waits for the end of synchronous DRAM operation. In
this case, data is only fetched in Td1, so the BS signal is asserted for Td1 only.

When the data width is 16 bits, the number of burst transfers during a read is 8. BS is asserted and
data fetched in cache-through and other DMA read cycles only in the Td1 and Td2 cycles (of the 8
cycles from Td1 to Td8) for longword accesses, and only in the Td1 cycle for word or byte
accesses.

Empty cycles tend to increase the memory access time, lower the program execution speed, and
lower the DMA transfer speed, so it is important to avoid accessing unnecessary cache-through
areas and to use data structures that enable 16-byte unit transfers by placing data on 16-byte
boundaries when performing DMA transfers that specify synchronous DRAM as the source.
































164
```

## PDF page 181

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=181)

```text
                         Tr          Tc          Td1         Td2         Td3         Td4          Tap


         CKIO

     A26–A11

           A10


        A9–A1

       CS2 or
          CS3

          RAS


          CAS

           WE

       DQMxx


      D31–D0

            BS


                          Figure 7.17   Single Read Timing (Auto-Precharge)


7.5.5        Writes

Unlike synchronous DRAM reads, synchronous DRAM writes are single writes. Figure 7.18
shows the basic timing chart for write accesses. After the ACTV command Tr, a WRITA
command is issued in Tc to perform an auto-precharge. In the write cycle, the write data is output
simultaneously with the write command. When writing with an auto-precharge, the bank is
precharged after the completion of the write command within the synchronous DRAM, so no
command can be issued to that bank until the precharge is completed. For that reason, besides a
cycle Tap to wait for the precharge during read accesses, the issuing of any new commands to the
same bank during this period is delayed by adding a cycle Trw1 to wait until the precharge is
started. The number of cycles in the Trw1 cycle can be specified using the TRWL bit in MCR.








                                                                                                                165
```

## PDF page 182

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=182)

```text
                                  Tr              Tc            Trwl            Tap


                 CKIO


             A26–A11


                   A10


                A9–A1


                CS2 or
                   CS3


                  RAS


                  CAS


                    WE


               DQMxx


              D31–D0


                    BS


                     Figure 7.18   Basic Write Cycle Timing (Auto-Precharge)


7.5.6        Bank Active Function

A synchronous DRAM bank function is used to support high-speed accesses of the same row
address. When the RASD bit in MCR is set to 1, read/write accesses are performed using
commands without auto-precharge (READ, WRIT). In this case, even when the access is
completed, no precharge is performed. When accessing the same row address in the same bank, a
READ or WRIT command can be called immediately without calling an ACTV command, just
like the RAS down mode of the DRAM’s high-speed page mode. Synchronous DRAM is divided
into two banks, so one row address in each can stay active. When the next access is to a different
row address, a PRE command is called first to precharge the bank, and access is performed by an

166
```

## PDF page 183

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=183)

```text
ACTV command and READ or WRIT command, in that order, after the precharge is completed.
With successive accesses to different row addresses, the precharge is performed after the access
request occurs, so the access time is longer. When writing, performing an auto-precharge means
that no command can be called for tRWL + tAP cycles after a WRITA command is called. When
the bank active mode is used, READ or WRIT commands can be issued consecutively if the row
address is the same. This shortens the number of cycles by tRWL + tAP for each write. The number
of cycles between the issue of the precharge command and the row address strobe command is
determined by the TRP bit in MCR.

Whether execution is faster when the bank active mode is used or when basic access is used is
determined by the proportion of accesses to the same row address (P1) and the average number of
cycles from the end of one access to the next access (tA). When tA is longer than tAP, the delay
waiting for the precharge during a read becomes invisible. If tA is longer than tRWL + tAP, the
delay waiting for the precharge also becomes invisible during writes. The difference between the
bank active mode and basic access speeds in these cases is the number of cycles between the start
of access and the issue of the read/write command: (tRP + tRCD) · (1 – P1) and tRCD,
respectively.

The time that a bank can be kept active, tRAS, is limited. When it is not assured that this period
will be provided by program execution and that another row address will be accessed without a hit
to the cache, the synchronous DRAM must be set to auto-refresh and the refresh cycle must be set
to the maximum value tRAS or less. This enables the limit on the maximum active period for each
bank to be ensured. When auto-refresh is not being used, some measure must be taken in the
program to ensure that the bank does not stay active for longer than the prescribed period.

Figure 7.19 shows a burst read cycle that is not an auto-precharge cycle, figure 7.20 shows a burst
read cycle to a same row address, figure 7.21 shows a burst read cycle to different row addresses,
figure 7.22 shows a write cycle without auto-precharge, figure 7.23 shows a write cycle to a same
row address, and figure 7.24 shows a write cycle to different row addresses.

In figure 7.20, a cycle that does nothing, Tnop, is inserted before the Tc cycle that issues the
READ command. Synchronous DRAMs, however, have a 2 cycle latency during reads for the
DQMxx signals that specify bytes. If the Tc cycle is performed immediately without inserting a
Tnop cycle, the DQMxx signal for the Td1 cycle data output cannot be specified. This is why the
Tnop cycle is inserted. When the CAS latency is 2 or more, the Tnop cycle is not inserted so that
timing requirements will be met even when a DQMxx signal is set after the Tc cycle.

When the SH7604 is set to the bank active mode, the access will start with figure 7.19 or figure
7.22 and repeat figure 7.20 or figure 7.23 for as long as the same row address continues to be
accessed when only accesses to the respective banks of the CS3 space are considered. Accesses to
other CS spaces during this period have no effect. When an access occurs to a different row
address while the bank is active, figure 7.21 or figure 7.24 will be substituted for figures 7.20 and
7.23 after this is detected. Both banks will become inactive even in the bank active mode after the
refresh cycle ends or after the bus is released by bus arbitration.
                                                                                                                     167
```

## PDF page 184

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=184)

```text
                     Tr         Tc          Td1         Td2         Td3         Td4


      CKIO


   A26–A11


        A10


     A9–A1


       CS3


       RAS


       CAS

        WE


    DQMxx


    D31–D0


         BS


                       Figure 7.19   Burst Read Timing (No Precharge)





















168
```

## PDF page 185

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=185)

```text
                Tnop         Tc          Td1         Td2         Td3         Td4


   CKIO


A26–A11


     A10


  A9–A1


    CS3


    RAS


    CAS

     WE


 DQMxx


D31–D0


      BS


          Figure 7.20   Burst Read Timing (Bank Active, Same Row Address)





















                                                                                           169
```

## PDF page 186

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=186)

```text
                     Tp         Tr        Tc       Td1        Td2       Td3        Td4


        CKIO

    A26–A11


         A10

      A9–A1

         CS3

        RAS


        CAS

         WE

      DQMxx

     D31–D0

          BS


          Figure 7.21   Burst Read Timing (Bank Active, Different Row Addresses)


























170
```

## PDF page 187

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=187)

```text
                   Tr           Tc


   CKIO


A26–A11


     A10


  A9–A1


    CS3


    RAS


    CAS


     WE


 DQMxx

D31–D0



      BS

   Figure 7.22   Write Timing (No Precharge)


















                                                                      171
```

## PDF page 188

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=188)

```text
                                               Tc


                                CKIO


                             A26–A11


                                  A10


                               A9–A1


                                 CS3


                                 RAS


                                 CAS


                                  WE


                              DQMxx


                              D31–D0


                                   BS

                Figure 7.23   Write Timing (Bank Active, Same Row Address)


















172
```

## PDF page 189

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=189)

```text
                        Tp            Tr           Tc


         CKIO


     A26–A11


           A10


        A9–A1


          CS3


          RAS


          CAS


           WE


       DQMxx

      D31–D0



            BS


Figure 7.24   Write Timing (Bank Active, Different Row Addresses)

















                                                                                  173
```

## PDF page 190

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=190)

```text
7.5.7        Refreshes

The bus state controller is equipped with a function to control refreshes of synchronous DRAM.
Auto-refreshes can be performed by setting the MCR’s RMD bit to 0 and the RFSH bit to 1. When
the synchronous DRAM is not accessed for a long period of time, set the RFSH bit and RMODE
bit both to 1 to initiate self-refresh mode, which uses low power consumption to retain data.

Auto-Refresh: Refreshes are performed at the interval determined by the input clock selected by
the CKS2–CKS0 bits in RTCSR and the value set in RTCOR. Set the CKS2–CKS0 bits and
RTCOR so that the refresh interval specifications of the synchronous DRAM being used are
satisfied. First, set RTCOR, RTCNT and the RMODE and RFSH bits in MCR, then set the CKS2–
CKS0 bits. When a clock is selected with the CKS2–CKS0 bits, RTCNT starts counting up from
the value at that time. The RTCNT value is constantly compared to the RTCOR value and a
request for a refresh is made when the two match, starting an auto-refresh. RTCNT is cleared to 0
at that time and the count up starts again. Figure 7.25 shows the timing for the auto-refresh cycle.

First, a PALL command is issued during the Tp cycle to change all the banks from active to
precharge states. A REF command is then issued in the Trr cycle. After the Trr cycle, no new
commands are output for the number of cycles specified in the TRAS bit in MCR + 2 cycles. The
TRAS bit must be set to satisfy the refresh cycle time specifications (active/active command delay
time) of the synchronous DRAM. When the MCR’s TRP bit is 1, an NOP cycle is inserted
between the Tp cycle and Trr cycle.

During a manual reset, no refresh request is issued, since there is no RTCNT count-up. To perform
a refresh properly, make the manual reset period shorter than the refresh cycle interval and set
RTCNT to (RTCOR – 1) so that the refresh is performed immediately after the manual reset is
cleared.
























174
```

## PDF page 191

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=191)

```text
                        Tp            Trr            Trc            Trc            Trc            Tre


       CKIO


         A10


     CS2 or
        CS3

        RAS


        CAS


         WE


    DQMxx


          BS

                                     Figure 7.25   Auto-Refresh Timing

Self-Refreshes: The self-refresh mode is a type of standby mode that produces refresh timing and
refresh addresses within the synchronous DRAM. It is started up by setting the RMODE and
RFSH bits to 1. The synchronous DRAM is in self-refresh mode when the CKE signal level is
low. During the self-refresh, the synchronous DRAM cannot be accessed. To clear the self-refresh,
set the RMODE bit to 0. After self-refresh mode is cleared, issuing of commands is prohibited for
the number of cycles specified in the MCR’s TRAS bit + 1 cycle. Figure 7.26 shows the self-
refresh timing. Immediately set the synchronous DRAM so that the auto-refresh is performed in
the correct interval. This ensures a correct self-refresh clear and data holding. When self-refresh
mode is entered while the synchronous DRAM is set for auto-refresh or when leaving the standby
mode with a manual reset or NMI, auto-refresh can be re-started if RFSH is 1 and RMODE is 0
when the self-refresh mode is cleared. When time is required between clearing the self-refresh
mode and starting the auto-refresh mode, this time must be reflected in the initial RTCNT setting.
When the RTCNT value is set to RTCOR – 1, the refresh can be started immediately.

If the standby function of the SH7604 is used after the self-refresh is set to enter the standby
mode, the self-refresh state continues; the self-refresh state will also be maintained after returning
from a standby using an NMI.

A manual reset cannot be used to exit the self-refresh state either. During a power-on reset, the bus
state controller register is initialized, so the self-refresh state is ended.


                                                                                                                    175
```

## PDF page 192

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=192)

```text
Refresh Requests and Bus Cycle Requests: When a refresh request occurs while a bus cycle is
executing, the refresh will not be executed until the bus cycle is completed. When a refresh request
occurs while the bus is released using the bus arbitration function, the refresh will not be executed
until the bus is recaptured. If RTCNT and RTCOR match and a new refresh request occurs while
waiting for the refresh to execute, the previous refresh request is erased. To make sure the refresh
executes properly, be sure that the bus cycle and bus capture do not exceed the refresh interval.

If a bus arbitration request occurs during a self-refresh, the bus is not released until the self-refresh
is cleared. During a self-refresh, the slave chips halt if there is a master-slave structure.


                      Tp        Trr       Trc                 Trc        Trc       Trc       Trc       Tre
       CKIO


        CKE

         A10

     CS2 or
         CS3

        RAS

        CAS

         WE

     DQMxx

          BS

                                       Figure 7.26   Self-Refresh Timing



















176
```

## PDF page 193

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=193)

```text
7.5.8        Power-On Sequence

To use synchronous DRAM, the mode must first be set after the power is turned on. To properly
initialize the synchronous DRAM, the synchronous DRAM mode register must be written to after
the registers of the bus state controller have first been set. The synchronous DRAM mode register
is set using a combination of the RAS/CE, CAS/OE and RD/WR signals. They fetch the value of
the address signal at that time. If the value to be set is X, the bus state controller operates by
writing to address X + H'FFFF8000 from the CPU, which allows the value X to be written to the
synchronous DRAM mode register. Data is ignored at this time, but the mode is written using
word as the size. Write any data in word size to the following addresses to select the burst read
single write supported by the SH7604, a CAS latency of 1 to 3, a sequential wrap type, and a burst
length of 8 or 4 (depending on whether the width is 16 bits or 32 bits).

For 16 bits:            CAS latency 1        H'FFFF8426
                      CAS latency 2          H'FFFF8446
                      CAS latency 3          H'FFFF8466

For 32 bits:            CAS latency 1        H'FFFF8848
                      CAS latency 2          H'FFFF8888
                      CAS latency 3          H'FFFF88C8

Figure 7.27 shows the mode register setting timing.

Writing to address X + H'FFFF8000 first issues an all-bank precharge command (PALL) in the Tp
cycle, then issues a mode register write command in the Tmw cycle. When the TRP bit in MCR is
set to 1, a single idle cycle is inserted between the Tp cycle and the Tmw cycle.

Before setting the mode register, an idle time of 100 ms (differs by memory manufacturer) must be
assured after the power required by the synchronous DRAM is turned on. When the pulse width of
the reset signal is longer than the idle time, the mode register may be set immediately without
problem. At least the number of dummy auto-refresh cycles specified by the manufacturer (usually
8 must be executed). After setting auto-refresh, it is usual for this to occur naturally during the
various initializations, but to make sure, the interval at which refresh requests are generated can be
shortened only while the dummy cycles are executing. Because the address counter within the
synchronous DRAM is not initialized when auto-refresh is used during single read or write
accesses, an auto-refresh cycle must always be used.












                                                                                                                    177
```

## PDF page 194

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=194)

```text
                                       Tp          Tmw


                         CKIO


                     A26–A12



                          A11


                          A10


                       A9–A1


                       CS2 or
                          CS3


                          RAS


                          CAS


                          WE


                       DQMxx


                           BS

                   Figure 7.27   Synchronous DRAM Mode Write Timing


















178
```

## PDF page 195

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=195)

```text
7.5.9        Phase Shift by PLL

The signals for synchronous DRAM interfaces change in the SH7604 at the rising edge of the
internal clock. Read data is fetched on the falling edge of an internal clock. Sampling of the
signals input by the synchronous DRAM and output of the read data, however, starts at the rising
edge of the external clock (figure 7.28).

When the internal clock of the SH7604 and external clock are synchronized, signal transmission
from the SH7604 to the synchronous DRAM has a 1 cycle margin. The transmission of read data
from the synchronous DRAM to the SH7604, however, is much tighter: only 1/2 cycle, including
the synchronous DRAM access time. When a clock system is connected without a means of
synchronization such as an on-chip PLL, transmission from the SH7604 to the synchronous
DRAM takes 1 cycle less the delay time of the clock system and transmission from the
synchronous DRAM to the SH7604 takes 1/2 cycle plus the clock system delay time. The clock
system delay time depends on the power supply voltage, temperature, and manufacturing variance,
so it has a fairly wide range. When the phase of the internal clock of the SH7604 is delayed using
a PLL that delays the phase 90 degrees relative to external clocks, transmission from the SH7604
to the synchronous DRAM and transmission from the SH7604 to the synchronous DRAM each
take 3/4 cycle.

Given this, using a clock whose phase is shifted 90 degrees from the external clock using a PLL as
the internal clock can ensure a margin of safety.

When using a PLL, it is important to note that synchronous DRAM does not contain an on-chip
PLL. When using the external clock input clock mode, instability in the clock supplied from
outside can cause shifts in phase, so a synchronization settling time in the SH7604’s on-chip PLL
is needed to equalize the SH7604’s internal clock and the external clock. During this
synchronization settling time, the internal clock of the synchronous DRAM and the internal clock
of the SH7604 will not always operate in perfect synchronization. To ensure the synchronous
DRAM and SH7604 operate properly, be sure that the external clock supplied is not unstable.




















                                                                                                                   179
```

## PDF page 196

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=196)

```text
    a. Phase Shifted 90° by PLL

               External
          CLK (CKIO)
                                         1/4 cycle (90°)

               Internal
                   CLK
                                               3/4 cycle


                                    Signal           SDRAM input latch
                                    output                            3/4 cycle


                                                            SDRAM                     Input
                                                          data output              data latch
    b. Phase Shift Using PLL is 0

                   External
              CLK (CKIO)



                    Internal
                       CLK
                                              1 cycle


                                Signal                   SDRAM input latch
                                output                                 1/2 cycle


                                                                SDRAM             Input
                                                              data output      data latch

                                     Figure 7.28   Phase Shift by PLL

















180
```

## PDF page 197

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=197)

```text
          c. No PLL Used

           External
      CLK (CKIO)
                                     Clock circut delay α (fluctuates with temperature and voltage)

            Internal
               CLK
                                        1 – α cycle


                                Signal           SDRAM input latch
                                output                         1/2 + α cycle


                                                       SDRAM                     Input
                                                     data output               data latch

                                 Figure 7.28   Phase Shift by PLL (cont)

7.6        DRAM Interface


7.6.1        DRAM Direct Connection

When the DRAM and other memory enable bits (DRAM2–DRAM0) in BCR1 are set to 010, the
CS3 space becomes DRAM space, and a DRAM interface function can be used to directly connect
the SH7604 to DRAM.

The data width of an interface can be 16 or 32 bits (figures 7.29 and 7.30). Two-CAS 16-bit
DRAMs can be connected, since CAS is used to control byte access. The RAS, CASHH, CASHL,
CASLH, CASLL, and RD/WR signals are used to connect the DRAM. When the data width is 16
bits, CASHH, and CASHL are not used. In addition to ordinary read and write access, burst access
using high-speed page mode is also supported.


















                                                                                                             181
```

## PDF page 198

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=198)

```text
                    256k × 16 bit
     SH7604          DRAM

        A10          A8
         ……         ……
         A2          A0



       RAS/CE        RAS
                     OE
       RD/WR         WE
        D31          I/O15
        D16……        I/O0…

   CASHH/DQMUU/WE3   UCAS
   CASHL/DQMUL/WE2   LCAS
        D15
         ……
         D0          A8
   CASLH/DQMLU/WE1  ………
   CASLL/DQMLL/WE0   A0



                     RAS
                     OE
                     WE
                     I/O15
                    …I/O0…

                     UCAS
                     LCAS


    Figure 7.29   Example of DRAM Connection (32-Bit Data Width)


















182
```

## PDF page 199

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=199)

```text
                                                                                  256k × 16 bit
                      SH7604                                                          DRAM

                                    A9                                             A8
                                     ……                                         ……
                                    A1                                             A0



                             RAS/CE                                                RAS
                                                                                   OE
                              RD/WR                                                WE
                                  D15                                              I/O15
                                    D0……                                        …I/O0…

              CASLH/DQMLU/WE1                                                      UCAS
               CASLL/DQMLL/WE0                                                     LCAS







                Figure 7.30   Example of DRAM Connection (16-Bit Data Width)


7.6.2        Address Multiplexing

When the CS3 space is set to DRAM, addresses are always multiplexed. This allows DRAMs that
require multiplexing of row and column addresses to be connected directly to SH7604
microprocessors without additional multiplexing circuits. There are four ways of multiplexing,
which can be selected using the MCR’s AMX1–AMX0 bits. Table 7.5 illustrates the relationship
between the AMX1–AMX0 bits and address multiplexing. Address multiplexing is performed on
address output pins A13–A1. The original addresses are output to pins A26–A14. During DRAM
accesses, AMX2 is reserved, so set it to 0.

Table 7.5      Relationship between AMX1–AMX0 and Address Multiplexing

                               No. of Column            Row                       Column
AMX1             AMX0          Address Bits             Address Output            Address Output
0                     0                     8 bits                           A21–A9                       A13–A1
               1                     9 bits                           A22–A10                     A13–A1
1                     0                     10 bits                         A23–A11                     A13–A1
               1                     11 bits                         A24–A12                     A13–A1





                                                                                                          183
```

## PDF page 200

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=200)

```text
7.6.3        Basic Timing

The basic timing of a DRAM access is 3 cycles. Figure 7.31 shows the basic DRAM access
timing. Tp is the precharge cycle, Tr is the RAS assert cycle, Tc1 is the CAS assert cycle, and Tc2
is the read data fetch cycle. When accesses are consecutive, the Tp cycle of the next access
overlaps the Tc2 cycle of the previous access, so accesses can be performed in a minimum of 3
cycles each.


                                             Tp                  Tr                   Tc1                Tc2


                            CKIO


                        A26–A14


                         A13–A1


                             RAS


                            CASn


                         RD/WR


            Read               RD


                         D31–D0


                         RD/WR


            Write              RD


                         D31–D0



                             CS3


                               BS

                                      Figure 7.31   Basic Access Timing
184
```

## PDF page 201

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=201)

```text
7.6.4        Wait State Control

When the clock frequency is raised, 1 cycle may not always be sufficient for all states to end, as in
basic access. Setting bits in WCR and MCR enables the state to be lengthened. Figure 7.32 shows
an example of lengthening a state using settings. The Tp cycle (which ensures a sufficient RAS
precharge time) can be extended to 2 cycles by insertion of a Tpw cycle by means of the TRP bit
in MCR. The number of cycles between RAS assert and CAS assert can be extended to 2 cycles
by inserting a Trw cycle by means of the RCD bit in MCR. The number of cycles from CAS assert
to the end of access can be extended from 1 cycle to 3 cycles by setting the W31/W30 bits in
WCR. When a value other than 00 is set in W31 and W30, the external wait pin WAIT is also
sampled, so the number of cycles is further increased. Figure 7.33 shows the timing of wait state
control using the WAIT pin. In either case, when consecutive accesses occur, the Tp cycle of one
access overlaps the Tc2 cycle of the previous access.


                                  Tp          Tpw            Tr          Trw    Tc1          Tw         Tc2

                    CKIO

               A26–A14

                 A13–A1

                     RAS

                   CASn

                 RD/WR

     Read             RD

                D31–D0

                 RD/WR

     Write            RD

                D31–D0

                     CS3

                       BS

                                       Figure 7.32   Wait State Timing



                                                                                                                   185
```

## PDF page 202

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=202)

```text
                                  Tp              Tr              Tc1           Tw   Twx           Tc2


                   CKIO

              A26–A14

                A13–A1

                    RAS


                  CASn

                RD/WR

    Read             RD


               D31–D0


                RD/WR

    Write            RD


               D31–D0


                    CS3


                      BS

                  WAIT


                                 Figure 7.33   External Wait State Timing


7.6.5        Burst Access

In addition to the ordinary mode of DRAM access, in which row addresses are output at every
access and data is then accessed, DRAM also has a high-speed page mode for use when
continuously accessing the same row that enables fast access of data by changing only the column
address after the row address is output. Select ordinary access or high-speed page mode by setting
the burst enable bit (BE) in MCR. Figure 7.34 shows the timing of burst operation in high-speed
page mode. When performing burst access, cycles can be inserted using the wait state control
function.


186
```

## PDF page 203

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=203)

```text
The SH7604 has an address comparator to detect matches of row addresses in burst mode. When
this function is used and the BE bit in MCR is set to 1, setting the MCR’s RASD bit (which
specifies RAS down mode) to 1 places the SH7604 in RAS down mode, which leaves the RAS
signal asserted. Since the CASHH, CASHL, CASLH and CASLL signals are shared with WE3,
WE2, WE1 and WE0 of ordinary space, however, write cycles to ordinary space during RAS
down mode will simultaneously initiate an erroneous write access to the DRAM. This means that
when no external devices that write to other than DRAM are connected, a DRAM can be directly
interfaced using RAS down mode. When RAS down mode is used, the refresh cycle must be less
than the maximum DRAM RAS assert time tRAS when the refresh cycle is longer than the tRAS
maximum.

When an external circuit is added to keep the CASHH, CASHL, CASLH, and CASLL signals
connected to the DRAM asserted only when the CS3 level is low, there are no restrictions on the
use of RAS down mode.


                                Tp       Tr       Tc1      Tc2                         Tc1       Tc2

                 CKIO

             A26–A14

              A13–A0

                  RAS

                 CASn

               RD/WR

   Read             RD

              D31–D0


               RD/WR

   Write            RD

              D31–D0


                  CS3


                    BS

                                   Figure 7.34   Burst Access Timing

                                                                                                            187
```

## PDF page 204

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=204)

```text
7.6.6        Refresh Timing

The BSC has a function for controlling DRAM refreshes. By setting the MCR’s RMODE bit to 0
and RFSH bit to 1, distributed refreshing using the CAS-before-RAS refresh cycle can be
performed.

Refreshes are performed at the interval determined by the input clock selected with CKS2–CKS0
in RTCSR and the value set in RTCOR. Set the values of RTCOR and CKS2–CKS0 so they
satisfy the refresh interval specifications of the DRAM being used. First, set RTCOR, RTCNT and
the RMODE and RFSH bits in MCR, then set the CKS2–CKS0 bits. When a clock is selected with
the CKS2–CKS0 bits, RTCNT starts counting up from the value at that time. The RTCNT value is
constantly compared to the RTCOR value and a request for a refresh is made when the two match,
starting a CAS-before-RAS refresh. RTCNT is cleared to 0 at that time and the count up starts
again. Figure 7.35 shows the timing for the CAS-before-RAS refresh cycle.

The number of RAS assert cycles in the refresh cycle is specified by the TRAS bit in MCR. As
with ordinary accesses, the specification of the RAS precharge time in refresh cycles follows the
setting of the TRP bit in MCR.


                          Tp                 Trr                 Trc1             Trc2    Tre


         CKIO


          RAS


        CASn


      RD/WR


           RD


          CS3


            BS



                                    Figure 7.35   Refresh Cycle Timing



188
```

## PDF page 205

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=205)

```text
7.6.7        Power-On Sequence

When DRAM is used after the power is turned on, there is a requirement for a waiting period
during which accesses cannot be performed (100 ms or 200 ms minimum) followed by the
prescribed number of dummy CAS-before-RAS refresh cycles (usually 8). The bus state controller
does not perform any special operations for the power-on reset, so the required power-on sequence
must be implemented by the initialization program executed after a power-on reset.

7.7        Pseudo-SRAM Interface


7.7.1        Pseudo-SRAM Direct Connection

When the DRAM and other memory enable bits (DRAM2–DRAM0) in BCR1 are set to 011, the
CS3 space becomes pseudo-SRAM space, and the pseudo-SRAM interface function can be used to
directly connect the SH7604 to pseudo-SRAM. The interface data width is 16 or 32 bits.

The refresh and output enable signals of the connected pseudo-SRAM are multiplexed. The
signals used for connecting pseudo-SRAM are the CE, OE, WE3, WE2, WE1, and WE0 signals.
The WE3 and WE2 signals are not used when the data width is 16 bits. When non-multiplexed
pseudo-SRAM is connected, the RD signal is also used.

In addition to ordinary read and write access, burst access using the static column access function
is also supported. Figure 7.36 shows an example of connections to 1-M pseudo-SRAM with
separate OE and RFSH pins; figure 7.37 shows an example of connections to 4-M pseudo-SRAM
with multiplexed OE/RFSH pins. 256-k pseudo-SRAM is multiplexed in the same way as 4-M
pseudo-SRAM. All data widths are 32 bits.

























                                                                                                               189
```

## PDF page 206

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=206)

```text
                     128k × 8 bit
  SH7604            pseudo-SRAM

     A18             A16
      A2……          …A0…

    RAS/CE           CE
     RD              OE
    CAS/OE           RFSH
     D31             I/O7
      ……            ……
     D24             I/O0
 CASHH/DQMUU/WE3     WE
     D23
     D16……           A16
 CASHL/DQMUL/WE2    …A0…
     D15             CS
      D8             OE
 CASLH/DQMLU/WE1     RFSH
      D7             I/O7
      ………………
      D0             I/O0
 CASLL/DQMLL/WE0     WE


                     A16
                    …A0…

                     CS
                     OE
                     RFSH
                     I/O7
                    ……
                     I/O0
                     WE


                     A16
                    …A0…

                     CS
                     OE
                     RFSH
                     I/O7
                    …I/O0…

                     WE


  Figure 7.36   Example of Pseudo-SRAM Connection (1-Mbit Pseudo-SRAM)


190
```

## PDF page 207

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=207)

```text
                   512k × 8 bit
 SH7604            pseudo-SRAM

    A20            A18
     A2……         …A0…

   RAS/CE          /CRCE
   CAS/OE          OE/RFSH
    D31            I/O7
     ……           ……
    D24            I/O0
CASHH/DQMUU/WE3    WE
    D23
    D16……          A18
CASHL/DQMUL/WE2   …A0…
    D15            CS
     D8            OE/RFSH
CASLH/DQMLU/WE1    I/O7
     D7           ……
                   I/O0
     D0…………WE

CASLL/DQMLL/WE0
                   A18
                  …A0…

                   CS
                   OE/RFSH
                   I/O7
                  ……
                   I/O0
                   WE


                   A18
                  …A0…

                   CS
                   OE/RFSH
                   I/O7
                  ……
                   I/O0
                   WE


Figure 7.37   Example of Pseudo-SRAM Connection (4-Mbit Pseudo-SRAM)







                       191
```

## PDF page 208

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=208)

```text
7.7.2        Basic Timing

Figure 7.38 shows the basic pseudo-SRAM access timing. Tp is the precharge cycle, Tr is the CE
assert cycle, Tc1 is the write data output and BS assert cycle, and Tc2 is the read data fetch cycle.
When accesses are consecutive, precharge cycle Tp overlaps the Tc2 cycle of the previous access,
so reads or writes can be performed in a minimum of 3 cycles each.


                                         Tp              Tr             Tc1             Tc2

                       CKIO


                    A26–A1


                        CS3


                          BS


                         CE


                         OE


                         RD
       Read

                       WEn


                   D31–D0


                         OE


                         RD
        Write

                       WEn


                   D31–D0



                                     Figure 7.38   Basic Access Timing

192
```

## PDF page 209

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=209)

```text
7.7.3        Wait State Control

When the clock frequency is raised, 1 cycle may not always be sufficient for all states to end, as in
basic access. Setting bits in WCR and MCR enables the state to be lengthened. Figure 7.39 shows
an example of lengthening a state using settings. The Tp cycle that ensures a sufficient CE
precharge time can be extended to 2 cycles by insertion of a Tpw cycle by means of the TRP bit in
MCR. The number of cycles between BS assert and the end of access can be extended from 2 to 4
cycles by setting the W31/W30 bits in WCR. When a value other than 00 is set in W31 and W30,
the external wait pin WAIT is also sampled, so the number of cycles can be further increased.
Figure 7.40 shows the timing of wait state control using the WAIT pin. In either case, when
consecutive accesses occur, the Tp cycle of one access overlaps the Tc2 cycle of the previous
access. The RCD bit in MCR is set to 0 for a pseudo-SRAM interface, but when set to 1, the
number of cycles from the CE assert to the BS assert or write data output becomes 2.


                                   Tp          Tpw          Tr          Tc1          Tw          Tc2

                    CKIO

                 A26–A1


                     CS3

                       BS

                       CE

                      OE

                      RD
     Read
                    WEn

                D31–D0

                      OE

                      RD
     Write
                    WEn

                D31–D0


                                       Figure 7.39   Wait State Timing

                                                                                                                  193
```

## PDF page 210

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=210)

```text
                                 Tp          Tr          Tc1          Tw         TwX          Tc2

                 CKIO

              A26–A1

                   CS3

                    BS

                    CE

                    OE


                    RD
   Read
                  WEn

              D31–D0

                    OE

   Write            RD

                  WEn

              D31–D0

                 WAIT


                                Figure 7.40   External Wait State Timing


7.7.4        Burst Access

In addition to normal access, in which CE is alternatively asserted and negated at every access,
when consecutive accesses are to the same row address the pseudo-SRAM can access data at high
speed by changing only the column address and leaving CE asserted. This function is called the
static column mode. Select between ordinary access and burst mode using static column mode by
setting the burst enable bit (BE) in MCR. Figure 7.41 shows the timing of burst operation using
static column mode. When performing burst access, cycles can be inserted using the wait state
control function.




194
```

## PDF page 211

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=211)

```text
                               Tp            Tr          Tc1         Tc2            Tc1            Tc2

                CKIO

             A26–A1
                                                                                    Only column
                 CS3                                                              address changes


                   BS

                   CE

                   OE


                   RD
  Read
                 WEn


             D31–D0

                   OE

                   RD
  Write

                 WEn

             D31–D0


                                     Figure 7.41   Static Column Mode


7.7.5        Refreshing

The BSC has a function for controlling pseudo-SRAM refreshing. By setting the MCR’s RMODE
bit to 0 and RFSH bit to 1, distributed refreshing using the auto-refresh cycle can be performed.

Refreshes are performed at the interval determined by the input clock selected with CKS2–CKS0
in RTCSR and the value set in RTCOR. Set the values of RTCOR and CKS2–CKS0 so they
satisfy the refresh interval specifications of the pseudo-SRAM being used. First, set RTCOR,
RTCNT and the RMODE and RFSH bits in MCR, then set the CKS2–CKS0 bits. When a clock is
selected with the CKS2–CKS0 bits, RTCNT starts counting up from the value at that time. The
RTCNT value is constantly compared to the RTCOR value and a request for a refresh is made
when the two match, starting an auto-refresh. RTCNT is cleared to 0 at that time and the count up
starts again. Figure 7.42 shows the timing for the auto-refresh cycle.

                                                                                                               195
```

## PDF page 212

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=212)

```text
The number of OE assert cycles for auto-refresh is specified by the TRAS bit in MCR. As with
ordinary accesses, the specification of the precharge time from OE negation to the next CE assert
follows the setting of the TRP bit in MCR.


                            Tp               Trr              Trc              Trc                 Tre

          CKIO


           CS3


             BS


            CE


    OE/RFSH


            RD


          WEn



                                           Figure 7.42   Auto-Refresh

The self-refresh mode is initiated in the pseudo-SRAM when the RFSH signal stays low for a
prescribed period of time. A self-refresh is started by setting the RMODE and RFSH bits to 1.
During the self-refresh, the pseudo-SRAM cannot be accessed. To clear self-refreshing, set either
the RMODE or RFSH bit to 0. After self-refresh mode is cleared, issuing of commands is
inhibited for 1 auto-refresh cycle. If more time than this is required to return from self-refresh,
write the program so that there are no accesses to the pseudo-SRAM, including auto-refreshes,
during this period. Figure 7.43 shows the self-refresh timing. After self-refreshing is cleared,
immediately set the pseudo-SRAM so that auto-refresh is performed in the correct interval. This
ensures correct self-refresh clearing and data retention. When time is required between clearing
the self-refreshing and initiating the auto-refresh mode, this time must be reflected in the initial
RTCNT setting.








196
```

## PDF page 213

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=213)

```text
                            Tp           Trr          Trc                         Trc          Tre

            CKIO


             CS3

               BS


               CE


       OE/RFSH

               RD

             WEn


                                           Figure 7.43   Self-Refresh


7.7.6        Power-On Sequence

When pseudo-SRAM is used after the power is turned on, there is a requirement for a waiting
period during which accesses cannot be performed (100 ms minimum) followed by the prescribed
number of dummy auto-refresh cycles (usually 8). The bus state controller does not perform any
special operations for the power-on reset, so the required power-on sequence must be implemented
by the initialization program executed after a power-on reset.

7.8        Burst ROM Interface

Set the BSTROM bit in BCR1 to set the CS0 space for connection to burst ROM. The burst ROM
interface is used to permit fast access to ROMs that have the nibble access function. Figure 7.44
shows the timing of nibble accesses to burst ROM. Set for two wait cycles. The access is basically
the same as an ordinary access, but when the first cycle ends, only the address is changed. The
CS0 signal is not negated, enabling the next access to be conducted without the T1 cycle required
for ordinary space access. From the second time on, the T1 cycle is omitted, so access is 1 cycle
faster than ordinary accesses. Currently, the nibble access can only be used on 4-address ROM.
This function can only be utilized for word or longword reads to 8-bit ROM and longword reads to
16-bit ROM. Mask ROMs have slow access speeds and require 4 instruction fetches for 8-bit
widths and 16 accesses for cache fills. Limited support of nibble access was thus added to alleviate
this problem. When connecting to an 8-bit width ROM, a maximum of 4 consecutive accesses are
performed; when connecting to a 16-bit width ROM, a maximum of 2 consecutive accesses are
performed. Figure 7.45 shows the relationship between data width and access size. For cache fills
and DMAC 16-byte transfers, longword accesses are repeated 4 times.

                                                                                                                  197
```

## PDF page 214

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=214)

```text
When one or more wait states are set for a burst ROM access, the WAIT pin is sampled. When the
burst ROM is set and 0 specified for waits, there are 2 access cycles from the second time on.
Figure 7.46 shows the timing.


                         T1        Tw1        Tw2         T2        Tw1         Tw2         T2



          CKIO


       A26–A0


           CS0

       RD/WR


            RD

       D31–D0


             BS

                       Figure 7.44   Burst ROM Nibble Access (2 Wait States)




























198
```

## PDF page 215

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=215)

```text
    T1          Tw          T2          Tw          T2          Tw          T2          Tw          T2
8-bit bus-width longword access
    T1          Tw          T2          Tw          T2
8-bit bus-width access
    T1          Tw          T2
8-bit bus-width byte access
    T1          Tw          T2          Tw          T2
16-bit bus-width longword access
    T1          Tw          T2
16-bit bus-width word access
    T1          Tw          T2
16-bit bus-width byte access
    T1          Tw          T2
32-bit bus-width longword access
    T1          Tw          T2
32-bit bus-width word access
    T1          Tw          T2
32-bit bus-width byte access


        Figure 7.45   Data Width and Burst ROM Access (1 Wait State)

























                                                                                                                      199
```

## PDF page 216

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=216)

```text
                                          T1           T2            T1           T2



                         CKIO


                      A26–A0


                          CS0


                      RD/WR


                            RD


                      D31–D0


                            BS

                       Figure 7.46   Burst ROM Nibble Access (No Wait States)

7.9        Waits between Access Cycles

Because operating frequencies have become high, when a read from a slow device is completed,
data buffers may not go off in time to prevent data conflicts with the next access. This lowers
device reliability and causes errors. To prevent this, a function has been added to avoid data
conflicts that memorizes the space and read/write state of the preceding access and inserts a wait
in the access cycle for those cases in which problems are found to occur when the next access
starts up. Checks are performed in two cases: if a read cycle is followed immediately by a read
access to a different CS space, and if a read access is followed immediately by a write from the
SH7604. When the SH7604 is writing continuously, if the format is always to have the direction
of the data from the SH7604 to other memory, there are no particular problems. Neither is there
any particular problem if the following read access is to the same CS space, since data is output
from the same data buffer. The number of idle cycles to be inserted into the access cycle when
reading from another CS space, or performing a write, after a read from the CS3 space, is specified
by the IW31 and IW30 bits in WCR. Likewise, IW21 and IW20 specify the number of idle cycles
after CS2 reads, IW11 and IW10 specify the number after CS1 reads, and IW01 and IW00 specify
the number after CS0 reads. From 0 to 2 cycles can be specified. When there is already a gap
between accesses, the number of empty cycles is subtracted from the number of idle cycles before
insertion. When a write cycle is performed immediately after a read access, 1 wait cycle is inserted
even when 0 is specified for waits between access cycles.

200
```

## PDF page 217

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=217)

```text
When the SH7604 shifts to a read cycle immediately after a write, the write data becomes high
impedance when the clock rises, but the RD signal, which indicates read cycle data output enable,
is not asserted until the clock falls. The result is that no waits are inserted into the access cycle.

When bus arbitration is being performed, an empty cycle is inserted for arbitration, so no wait is
inserted between cycles.


                       T1          T2      Twait        T1         T2        Twait       T1          T2
         CKIO


      A26–A0

         CSm

          CSn


            BS

      RD/WR


            RD

     D31–D0

                    CSm space read                   CSn space read                CSn space write
                                  Specification of waits           Specification of waits
                                between CSm accesses              between CSn accesses
                               (reading different spaces)         (read followed by write)

                                 Figure 7.47   Waits between Access Cycles

7.10      Bus Arbitration

The SH7604 has a bus arbitration function that, when a bus release request is received from an
external device, releases the bus to that device after the bus cycle being executed is completed. In
addition, it also has a bus arbitration function for supporting the connection of two processors.
These are connected to each other as master and slave through bus arbitration, which enables a
multiprocessor system to be implemented with a minimum of hardware.

There are three modes for bus arbitration: master mode, partial-share master mode, and slave
mode. Master mode keeps the bus under normal conditions and permits other devices to use the
bus by releasing it when they request its use. The slave mode normally does not have the bus. The
bus is requested when an external bus access cycle comes up and then releases the bus when the

                                                                                                                     201
```

## PDF page 218

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=218)

```text
access is completed. The partial-share master mode only shares CS2 space with external devices.
For the CS2 space, the mode is slave mode; for other spaces, the bus is held constantly without
any bus arbitration. Which CS space of the chip in master mode the CS2 space of the chip in
partial-share master mode is allocated to, is determined by external circuitry.

Master or slave mode can be specified using external mode pins. Partial-share master mode is
reached from master mode by a software setting. See Section 3, Oscillator Circuits and Operating
Modes, for the external mode pin settings. When a device in master or slave mode does not have
the bus, the bus goes to high impedance, so the master mode chip and slave mode chips can be
connected directly. In the partial-share master mode, the bus is always driven, so an external
buffer is needed to connect to a master bus. In master mode, a connection to an external device
requesting the bus can be substituted for the slave mode connection. In the following explanation,
external devices requesting the bus are also called slaves.

The SH7604 has two internal bus masters, the CPU and the DMAC. When synchronous DRAM,
DRAM or pseudo-SRAM is connected and refresh control is performed, the refresh request
becomes a third master. In addition to these, there are also bus requests from external devices
while in the master mode. The priority for bus requests when they occur simultaneously is, highest
to lowest, refresh requests, bus requests from external devices, DMAC and CPU.

When the bus is being passed between slave and master, all bus control signals are negated before
the bus is released to prevent erroneous operation of the connected devices. Once the bus is
received, the bus control signals change from negated to bus driven. The master and slave passing
the bus between them drive the same signal values, so output buffer conflict is avoided. Turning
the output buffer off for the bus control signals on the side that releases the bus and on at the side
acquiring the bus can eliminate the high impedance period of the signals. It is usually not
necessary to insert a pull-up resistance into these control signals to prevent malfunction caused by
external noise while they are at high impedance.

Bus permission is granted at the end of the bus cycle. When the bus is requested, the bus is
released immediately if there is no ongoing bus cycle. If there is a current bus cycle, the bus is not
released until the bus cycle ends. Even when there does not appear to be an ongoing bus cycle
when seen from outside the SH7604, it cannot be determined whether or not the bus will be
released immediately when a bus control signal such as a CSn signal is seen, since an internal bus
cycle, such as inserting a wait between access cycles, may have been started. The bus cannot be
released during burst transfers for cache fills or 16-byte DMAC block transfers. Likewise, the bus
cannot be released between the read and write cycles of a TAS instruction. Arbitration is also not
performed between multiple bus cycles produced by a data width smaller than the access size,
such as a longword access to an 8-bit data width memory. Bus arbitration is performed between
external vector fetch, PC save, and SR save cycles during interrupt handling, which are all
independent accesses.

Because the CPU in the SH7604 is connected to cache memory by a dedicated internal bus, cache
memory can be read even when the bus is being used by another bus master on the chip or

202
```

## PDF page 219

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=219)

```text
externally. Writing from the CPU always produces a write cycle externally since the write-through
system is used by the SH7604 for the cache. When an external bus address monitor is not specified
by the user break controller, the internal bus that connects the CPU, DMAC and on-chip
peripheral modules can operate in parallel to the external bus. This means that both read and write
accesses from CPU to on-chip peripheral modules and from DMAC to on-chip peripheral module
are possible. If an external bus address monitor is specified, the internal bus will be used for
address monitoring when the bus is passed to the external bus master, so accesses to on-chip
peripheral modules by the CPU and DMAC must wait for the bus to be returned.


7.10.1      Master Mode

Master mode processors keep the bus unless they receive a bus request. When a bus release
request (BRLS) assertion (low level) is received from an external device, buses are released and a
bus grant (BGR) is asserted (low level) as soon as the bus cycle being executed is completed.
When it receives a negated (high level) BRLS signal, indicating that the slave has released the bus,
it negates the BGR (to high level) and begins using the bus. When the bus is released, all output
and I/O signals related to the bus interface are changed to high impedance, except for the CKE
signal for the synchronous DRAM interface, the BGR signal for bus arbitration, and DMA transfer
control signals DACK0 and DACK1.

When the DRAM or pseudo-SRAM has finished precharging, the bus is released. The
synchronous DRAM also issues a precharge command to the active bank or banks. After this is
completed, the bus is released.

The specific bus release sequence is as follows. First, the bus use enable signal is asserted
synchronously with the fall of the clock. Half a cycle later, the address bus and data bus become
high impedance synchronous with the rise of the clock. Thereafter the bus control signals (BS,
CSn, RAS, CAS, WEn, RD, RD/WR, IVECF) become high impedance with the fall of the clock.
All of these signals are negated at least 1.5 cycles before they become high impedance. Sampling
for bus request signals occurs at the clock fall.

The sequence when the bus is taken back from the slave is as follows. When the negation of BRLS
is detected at a clock fall, BGR is immediately negated and the master simultaneously starts to
drive the bus control signals. The address bus and data bus are driven starting at the next clock
rise. The bus control signals are asserted and the bus cycle actually starts from the same clock rise
at which the address and data signals are driven, at the earliest. Figure 7.48 shows the timing of
bus arbitration in master mode.










                                                                                                                     203
```

## PDF page 220

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=220)

```text
                    CKIO

                 BRLS

                  BGR

   Master       Address
    mode             data
      side         CSn/CR
               RD/WR
                    RD/CR
                  WEn

                     BS

                 BREQ

                 BACK
    Slave       Address
    mode             data
      side         CSn/CR
               RD/WR
                    RD/CR
                  WEn

                     BS

                                        Figure 7.48   Bus Arbitration

When a refresh request is generated in the SH7604 while BGR is asserted and the bus released, the
BGR may or may not be negated.

Case in Which BGR is Negated: If access processing for an external device has not been started
before the refresh request is generated, the BGR signal will be negated even while the BRLS
signal is asserted. Even though the BGR signal is negated, a refresh operation will not begin
unless the BRLS signal is negated. Refreshing begins as soon as BRLS is negated.












204
```

## PDF page 221

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=221)

```text
   CKIO

   BRLS/CR
          /CR
    BGR


                       Bus request         Bus release                             Refresh request

                                Figure 7.49   Case in Which BGR is Negated

Case in Which BGR is Not Negated: If access processing for an external device has been
initiated before the refresh request is generated, and the device is waiting to acquire the bus, the
BGR signal will not be negated. After the BRLS signal is negated, refreshing begins after
completion of the access involving the external device that was waiting for the bus.




   CKIO

   BRLS/CR
          /CR
    BGR


                      Bus request         Bus release                             Refresh request

                             Figure 7.50   Case in Which BGR is Not Negated

When the SH7604 is being used in slave mode, the bus is released as soon as the bus access cycle
ends, but in the case of a slave designed by the user, multiple consecutive bus accesses may be
attempted in order to reduce the arbitration overhead. To ensure dependable refreshing in this case,
the design should provide for the bus to be released to prevent the slave holding the bus for longer
than the refresh cycle.


7.10.2      Slave Mode

In slave mode, the bus is usually released. External devices cannot be accessed unless the bus
arbitration sequence is performed to capture the bus. During a reset, the bus is released and the bus
arbitration sequence starts from the reset vector fetch.

The BREQ signal is asserted (to low level) synchronously with the clock fall for capturing the bus.
The assertion of the BACK signal (to low level) is sampled at the clock fall. When a BACK

                                                                                                                        205
```

## PDF page 222

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=222)

```text
assertion is detected, the bus control signals are immediately driven at the negate level. Thereafter
the address and data bus drivers turn on at the following clock rise and the bus cycle starts. The
last signal negated when the access cycle ended is synchronized with the clock rise. Half a cycle
after the clock rise, the BREQ signal is negated, the master notified that the bus is released, and
one cycle later the address and data output buffers turned off (high impedance). At the following
clock fall, the control signals become high impedance. Figure 7.48 shows the bus arbitration
timing for slave mode.

When the slave access cycle is for DRAM, synchronous DRAM or pseudo-SRAM, the bus is
released when the memory precharge finishes, just as for master mode. Since refresh control is
handled by the master mode device, any refresh control setting performed in the slave mode is
ignored. Figure 7.51 shows an example of master mode and slave mode connections.


    Master                                                                 Slave
     mode                                                                  mode
      CKIO                                                                CKIO
      BRLS/CR                                                             BREQ/CR
       BGR/CR                                                             BACK/CR
       CSn/CR                                                             CSn/CR
        BS                                                                BS
    RD/WR/CR                                                              RD/WR/CR
        RD/CR                                                             RD/CR
       WEn/CR                                                             WEn/CR
       RAS/CR                                                             RAS/CR
       CAS/CR                                                             CAS/CR
     IVECF/CR                                                             IVECF/CR
       CKE                                                                CKE
      WAIT/CR                                                             WAIT/CR
    A26–A0                                                                A26–A0
    D31–D0                                                                D31–D0
               CS/CRBS/CRWR/CRRD/CRAnDnCS/CROE/CRAnDnCK/CRCS/CRWE/CR/CRAnDn
                   RD/WEn/CRIVECF/CRWAIT/CRWEn/CRCKERAS/CRCAS/CRDQMxx


                 Interfaces with        SRAM              Synchronous DRAM
                  other devices

               Figure 7.51   Connection between Master and Slave Devices

7.10.3      Partial-Share Master Mode

In partial-share master mode, only the CS2 space is shared with other devices. Other CS spaces
can always be accessed. To set partial-share master mode, set to master mode using the external
mode pins and then set the PSHR bit in BCR to 1 in the power-on reset initialization procedure.

206
```

## PDF page 223

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=223)

```text
During a manual reset, the values of the bus state controller setting registers are held, so they do
not need to be set again.

Partial-share master mode is designed to be used with a chip in master mode. Figure 7.52 shows
an example of connections between a partial-share master mode and master mode SH7604. On the
master mode side, the CS3 space is connected to synchronous DRAM and the CS0 space to ROM.
On the partial-share master mode side, the CS0 space is connected to ROM, the master side
synchronous DRAM is connected to the CS2 space, and the CS3 space is connected to dedicated
synchronous DRAM. The partial-share master is also connected through the CS2 space to the
master synchronous DRAM so it can be accessed. The master, however, cannot access devices on
the partial-share master side. There is a buffer for addresses and control signals and a buffer for
data located between the partial-share master and the master. They are controlled by a buffer
control circuit. The buffers latch signals synchronous to the clock rise and match timing, so an AC
operating margin is assured. When the master side synchronous DRAM is read from the partial-
share master, however, address and control line output requires an extra cycle, and input of read
data requires an extra cycle. The CAS latency setting within the bus controller should be 2 higher
than the actual synchronous DRAM CAS latency. If the clock cycle is sufficiently long relative to
the time for addresses, control signals and write signals from the partial-share master to reach the
synchronous DRAM on the master side through the buffer and to the time for read data from the
synchronous DRAM on the master side to reach the partial-share master through the buffer, if the
respective setup time limits can be satisfied, then there is no need to delay by one cycle clock
signal synchronously with the clock. In this case, the previously described latch is not needed.

When a processor in the partial-share master mode accesses the CS2 space, it performs the
following procedure. The BREQ signal is asserted at the clock fall to request the bus from the
master. The BACK signal is sampled at every clock fall, and when an assertion is received, the
access cycle starts at the next clock rise. After the access ends, BREQ is negated at the clock fall.
Control of the buffer when a CS2 space device is being accessed from the partial-share master
references the BREQ and BACK signals. Notification that the bus is enabled for use is conducted
by the BACK connected to the partial-share master, but the BACK signal may be negated while
the bus is in use when the master requires the bus back to service a refresh or the like. For this
reason, the BREQ signal must be monitored to see whether the partial-share master can continue
using the bus after BACK is asserted. For address buffers, after the address buffer is turned on by
the detection of a BACK assertion, the buffer remains on until BREQ is negated. When BREQ is
negated, the buffer goes off. When the buffer is slow going off and it conflicts with the start of the
access cycle at the master, the BREQ signal output from the partial-share master as part of the
buffer control circuit must be delayed a clock and input to the BRLS signal.

When the bus is released after the CS2 space is accessed in partial-share master mode, the bus will
be released after waiting for the time required for auto-precharge if the CS2 space was
synchronous DRAM. Other spaces always have the bus themselves, so there is no precharge of
CS3 space memory upon release after a CS2 space bus request, even when DRAM, synchronous
DRAM or pseudo-SRAM is connected to the CS3 space. Partial-share master mode does not
refresh CS2 (it is ignored).
                                                                                                                     207
```

## PDF page 224

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=224)

```text
      Master                                                 Buffer                                Partial-share
       mode                                                  control                               master mode

          CKIO                                                                                        CKIO
         BRLS/CR                                                                                      BREQ/CR
          BGR/CR                                                                                      BACK/CR
             BS/CR                                                                                    BS/CR
            RD/CR                                                                                     RD/CR
        IVECF/CR                                       CK  EN                                         IVECF/CR
           CSn/CR                                                                                     CSn/CR
           RAS/CR                                                                                     RAS/CR
           CAS                                                                                        CAS
       RD/WR/CR                                                                                       RD/WR/CR
          WEn                                                                                         WEn
       A26–A0                                                                                         A26–A0
           CKE                                         CK  EN                                         CKE
          WAIT/CR                                           DIR                                       WAIT/CR
      D31–D0                                                                                          D31–D0

  Synchronous                                                                                      Synchronous
      DRAM                                                                                             DRAM
             CK                                                                                       CK
           CKE                                                                                        CKE
             CS/CR                                                                                    CS/CR
           RAS/CR                                                                                     RAS/CR
           CAS/CR                                                                                     CAS/CR
            WE/CR                                                                                     WE/CR
       DQMxx                                                                                          DQMxx
             An                                                                                       An
             Dn                                                                                       Dn

       ROM                                                                                              ROM
             CS/CR                                                                                    CS/CR
            OE/CR                                                                                     OE/CR
             An                                                                                       An
             Dn                                                                                       Dn

  Interfaces with
  other devices
             BS/CR
            RD/CR
        IVECF/CR
             CS/CR
      RD/WEn/CR
          WEn/CR
             An
          WAIT
             Dn

           Figure 7.52   Connection between Master and Partial-Share Master Devices

208
```

## PDF page 225

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=225)

```text
7.10.4      External Bus Address Monitor

The master and slave modes have a function to generate break interrupts and monitor the external
bus access cycle using the user break controller. The bus cycle is monitored by sampling the
external bus every time the clock rises while the bus is released. If the BS signal is found to be
asserted (low level) when sampling is performed, the address at that time (A26–A0) and read/write
signal RD/WR are fetched and compared as the access address and access type (read or write).

When an external device has captured the bus and the DRAM or synchronous DRAM is in an
access cycle, the following points are important to make the address monitor function correctly.
BS goes low in the DRAM or synchronous DRAM access cycle in synchronization with the cycle
that outputs the column address. Because only the address of the cycle in which the BS signal is
low is fetched and compared, even access to memories like these that multiplex addresses requires
outputting of the row address in the upper address bits. One of the bits of the address signal in the
column address output cycle in synchronous DRAM is used to specify the bank address, while the
other bit is used to specify whether to perform an auto-precharge. These always cause breaks on
the compared address, so mask these two bits when setting the comparison address. The masked
bit position is described in the section 7.5.2, Address Multiplexing.


7.10.5      Master/Slave Coordination

Roles must be shared between the master and slave to control system resources without
contradictions. DRAM, synchronous DRAM and pseudo-SRAM must be initialized before use.
When using standby operation to lower power consumption, the load must also be shared.

This SH7604 was designed with the idea that the master mode device would handle all controls,
such as initialization, refreshing, and standby control. When a 2-processor structure of connected
master and slave is used, all processing except for direct accesses to memory is controlled by the
master. When master mode is combined with partial-share master mode, the partial-share master
mode processor handles initialization, refreshing, and standby control for all CS spaces connected
to it except for the CS2 space. The master initializes memory connected directly to it.

The hardware or software sequence should be designed so that there are no slave-side processor
accesses until memory that requires initialization before use such as DRAM, synchronous DRAM
and pseudo-SRAM has completed its initialization. One method is to install an external circuit that
clears slave resets from the master. Another is to have the master write a flag when initialization is
complete to an SRAM or the like that does not require initialization, and then not to start access
until this flag is acknowledged by the slave. A third method is to install an external circuit that can
send an interrupt from master to slave and clear the slave’s standby state with an interrupt from the
master to the slave when initialization ends.

In standby mode or the like when synchronous DRAM and pseudo-SRAM are in self-refresh
mode, memory is not precharged until the mode is cleared, so the master cannot release the bus.
The design should provide for the master to put the slave to sleep before self-refresh mode starts
                                                                                                                     209
```

## PDF page 226

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=226)

```text
or otherwise prevent the slave access cycle from starting, which prevents the slave from producing
a bus release request. The slave accesses these types of memories after the master finishes any
processing necessary when the self-refresh mode is cleared, such as refresh settings.

7.11      Other Topics


7.11.1      Resets

The bus state controller is completely initialized only in a power-on reset. All signals are
immediately negated, regardless of where in the bus cycle the SH7604 is, and the output buffer is
turned off if the bus arbitration mode is slave. Signal negation is simultaneous with turning the
output buffer off. All control registers are initialized. In standby mode, sleep mode, and a manual
reset, no bus state controller control registers are initialized. When a manual reset is performed,
any executing bus cycles are completed, and then the SH7604 waits for an access. When a cache
fill or 16-byte DMAC transfer is executing, the CPU or DMAC that is the bus master ends the
access in a longword unit, since the access request is canceled by the manual reset. This means
that when a manual reset is executed during a cache fill, the cache contents can no longer be
guaranteed. During a manual reset, the RTCNT does not count up, so no refresh request is
generated, and a refresh cycle is not initiated. To preserve the data of the DRAM, synchronous
DRAM or pseudo-SRAM, the pulse width of the manual reset must be shorter than the refresh
interval. Master mode chips accept arbitration requests even when a manual reset signal is
asserted. When a reset is executed only for the chip in master mode while the bus is released, the
BGR signal is negated to indicate this. If the BRLS signal is continuously asserted, the bus release
state is maintained.


7.11.2      Access as Seen from the CPU or DMAC

The SH7604 is internally divided into three buses: cache, internal, and peripheral. The CPU and
cache memory are connected to the cache bus, the DMAC and bus state controller are connected
to the internal bus, and the low-speed peripherals and mode registers are connected to the
peripheral bus. The user break controller is connected to both the cache bus and the internal bus.
The internal bus can be accessed from the cache bus, but not the other way around. The peripheral
bus can be accessed from the internal bus, but not the other way around. This results in the
following.

Data cannot be written from the DMAC to cache memory. When the DMAC causes a write to
memory, the contents of memory and the cache contents will be different. To rewrite the contents
of memory, the cache memory must be purged by software if the possibility exists that the data for
that address exists in the cache.





210
```

## PDF page 227

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=227)

```text
When the CPU starts a read access to a cache area, it first takes a cycle to find the cache. If there is
data in the cache, it fetches it and completes the access. If there is no data in the cache, a cache
data fill is performed via the internal bus, so four consecutive longword reads occur. For misses
that occur when byte or word operands are accessed or branches occur to odd word boundaries (4n
+ 2 addresses), filling is always performed by longword accesses on the chip-external interface. In
the cache-through area, the access is to the actual access address. When the access is an instruction
fetch, the access size is always longword.

For cache-through areas and on-chip peripheral module read cycles, after an extra cycle is added
to determine the cycle, the read cycle is started through the internal bus. Read data is sent to the
CPU through the cache bus.

When word write cycles access the cache area, the cache is searched. When the data of the
relevant address is found, it is written here. In parallel to this, the actual writing occurs through the
internal bus. When the right to use the internal bus is held, the CPU is notified that the write is
completed without waiting for the actual writing to the on-chip peripheral module or off the chip
to end. When the right to use the internal bus is not held, as when it is being used by the DMAC or
the like, there is a wait until the bus is acquired before the CPU is notified of completion.

Accesses to cache-through areas and on-chip peripheral modules work the same as in the cache
area, except for the cache search and write.

Because the bus state controller has one level of write buffer, the internal bus can be used for
another access even when the chip-external bus cycle has not ended. After a write has been
performed to low-speed memory off the chip, performing a read or write with an on-chip
peripheral module enables an access to the on-chip peripheral module without having to wait for
the completion of the write to low-speed memory.

During reads, the CPU always has to wait for the end of the operation. To immediately continue
processing after checking that the write to the device of actual data has ended, perform a dummy
read access to the same address consecutively to check that the write has ended.

The bus state controller’s write buffer functions in the same way during accesses from the DMAC.
A dual-address DMA transfer thus starts in the next read cycle without waiting for the end of the
write cycle. When both the source address and destination address of the DMA are external spaces
to the chip, however, it must wait until the completion of the previous write cycle before starting
the next read cycle.











                                                                                                                       211
```

## PDF page 228

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=228)

```text
7.11.3      Emulator

When using the SH7604’s emulator, operation differs from real chip operation in the following
ways.

To get trace data with the emulator, all accesses performed by the CPU and DMAC must be output
externally. It is not possible to completely analyze program execution or the contents of the data
accessed with only traces of access cycles performed exterior to the chip.

Reads of the cache from the CPU can be performed using only the cache bus, but the access
address and data read must be able to use the internal bus and external bus to be output externally.
The external bus is not needed to access on-chip peripheral modules with the CPU or DMAC, but
it is needed to output trace data. This means that when the emulator is used in the trace data fetch
mode, internal access operations of the CPU or DMAC are not performed in parallel with the
external bus cycle, so extra execution time is required compared to actual chips. Parallel execution
of accesses that follow writing to external destinations also should be executed after writing is
completed to carry out traces. To precisely measure the actual execution time, an actual chip rather
than an emulator should be used.





































212
```
