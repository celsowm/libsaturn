# Section 10 Division Unit

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=303)

Physical PDF pages 303–310. Text extraction; consult the PDF for figures.

## Bookmarks

- [10.1 Overview](#pdf-page-303)
- [10.1.1 Features](#pdf-page-303)
- [10.1.2 Block Diagram](#pdf-page-304)
- [10.1.3 Register Configuration](#pdf-page-304)
- [10.2 Description of Registers](#pdf-page-305)
- [10.2.1 Divisor Register (DVSR)](#pdf-page-305)
- [10.2.2 Dividend Register L for 32-Bit Division (DVDNT)](#pdf-page-305)
- [10.2.3 Division Control Register (DVCR)](#pdf-page-306)
- [10.2.4 Vector Number Setting Register DIV (VCRDIV)](#pdf-page-307)
- [10.2.5 Dividend Register H (DVDNTH)](#pdf-page-307)
- [10.2.6 Dividend Register L (DVDNTL)](#pdf-page-308)
- [10.3 Operation](#pdf-page-308)
- [10.3.1 64-Bit ÷ 32-Bit Operations](#pdf-page-308)
- [10.3.2 32-Bit ÷ 32-Bit Operations](#pdf-page-308)
- [10.3.3 Handling of Overflows](#pdf-page-309)
- [10.4 Usage Notes](#pdf-page-309)
- [10.4.1 Access](#pdf-page-309)
- [10.4.2 Overflow Flag](#pdf-page-310)

## PDF page 303

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=303)

```text
                                   Section 10   Division Unit


10.1      Overview

The division unit (DIVU) divides 64 bits by 32 bits and 32 bits by 32 bits. The results are
expressed as a 32-bit quotient and a 32-bit remainder. When the operation produces an overflow,
an interrupt can be generated as specified.


10.1.1      Features

The division unit has the following features:

•    Performs signed division of 64 bits by 32 bits and 32 bits by 32 bits
•    Handles 32-bit quotient, 32-bit remainder
•    Completes operation execution in 39 cycles
•    Controls enabling/disabling of over/underflow interrupts
•    Even during the division process, instructions not accessing the division unit can be parallel-
    processed


































                                                                                                                     287
```

## PDF page 304

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=304)

```text
10.1.2      Block Diagram

Figure 10.1 shows a block diagram of the division unit.


                                                     Bus     Internal data bus
                                                   interface


                                   DVSR

                Division          DVDNT
                 oper-
                 ation
                 circuit      Module data busDVDNTH

                                  DVDNTL


                Division           DVCR
                control
                 circuit          VCRDIV



                            Internal interrupt signal

      DVSR:   Divisor register
     DVDNT:   Dividend register L for 32-bit division
    DVDNTH:   Dividend register H
    DVDNTL:   Dividend register L
      DVCR:   Division control register
     VCRDIV:   Vector number setting register DIV
                         Figure 10.1   Division Unit Block Diagram


10.1.3      Register Configuration

Table 10.1 shows the register configuration of the division unit.













288
```

## PDF page 305

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=305)

```text
Table 10.1    Division Unit Register Configuration

Register                                       Abbr.         R/W       Initial Value   Address           Access Size*1
Divisor register                              DVSR        R/W       Undefined       H'FFFFFF00     32
Dividend register L for 32-bit                DVDNT      R/W       Undefined       H'FFFFFF04     32
division
Division control register                DVCR        R/W       H'00000000    H'FFFFFF08     16, 32
Vector number setting register                VCRDIV    R/W       Undefined*2    H'FFFFFF0C    16, 32
DIV
Dividend register H                       DVDNTH   R/W       Undefined       H'FFFFFF10     32
Dividend register L                        DVDNTL    R/W       Undefined       H'FFFFFF14     32
Notes:  1.   Accesses to the division unit are read and written in 32-bit units. DVCR and VCRDIV
                permit 16 and 32-bit accesses. When registers other than CONT and VCRDIV are
                accessed with word accesses, undefined values are read or written.
           2.   The initial value of VCRDIV is H'0000**** (asterisks represent undefined values).


10.2      Description of Registers


10.2.1      Divisor Register (DVSR)

                     Bit:        31            30            29            …             3              2              1              0
            Bit name:                                                        …
         Initial value:        —            —            —            …            —            —            —            —
                   R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


The divisor register (DVSR) is a 32-bit read/write register in which the divisor for the operation is
written. It is not initialized by a power-on reset or manual reset, in standby mode, or during
module standbys.


10.2.2      Dividend Register L for 32-Bit Division (DVDNT)

                     Bit:        31            30            29            …             3              2              1              0
            Bit name:                                                        …
         Initial value:        —            —            —            …            —            —            —            —
                   R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W







                                                                                                                                     289
```

## PDF page 306

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=306)

```text
The dividend register L for 32-bit division (DVDNT) is a 32-bit read/write register in which the
32-bit dividend used for 32-bit ‚ 32-bit division operations is written. When 32-bit ‚ 32-bit
division is run, the value set as the dividend is lost and the quotient written at the end of division.
When this register is written to, the same value is written in the DVDNTL register. The MSB
written is sign-extended in the DVDNTH register. Writing to this register starts the 32-bit ‚ 32-bit
division operation. It is not initialized by a power-on reset or manual reset, in standby mode, or
during module standbys.


10.2.3      Division Control Register (DVCR)

                     Bit:        31            30            29            …             3              2              1              0
            Bit name:        —            —            —            …            —            —         OVFIE      OVF
         Initial value:         0              0              0             …             0              0              0              0
                   R/W:         R             R             R             …             R             R           R/W         R/W


The division control register (DVCR) is a 32-bit read/write register, but is also 16-bit accessible. It
controls enabling/disabling of the overflow interrupt. This register is initialized to H'00000000 by
a power-on reset or manual reset. It is not initialized in standby mode or during module standbys.

•    Bits 31 to 2: Reserved. These bits always read 0. The write value should always be 0.

•    Bit 1: OVF Interrupt Enable (OVFIE): Selects enabling or disabling of the OVF interrupt
    request (OVFI) upon overflow.

Bit 1:  OVFIE                       Description
0                                           Interrupt request (OVFI) caused by OVF disabled               (Initial value)
1                                           Interrupt request (OVFI) caused by OVF enabled
Note:   Always set the OVFIE bit before starting the operation whenever executing interrupt
          handling for overflows.


•    Bit 0: Overflow Flag (OVF). Flag indicating an overflow has occurred.

Bit 0:  OVF                          Description
0                                           No overflow has occurred                                                    (Initial value)
1                                           Overflow has occurred










290
```

## PDF page 307

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=307)

```text
10.2.4      Vector Number Setting Register DIV (VCRDIV)

                     Bit:        31            30            29            …            19            18            17            16
             Bit name:        —            —            —            …            —            —            —            —
         Initial value:         0              0              0             …             0              0              0              0
                   R/W:         R             R             R             …             R             R             R             R


                     Bit:        15            14            13            …             3              2              1              0
             Bit name:                                                        …
         Initial value:        —            —            —            …            —            —            —            —
                   R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


Vector number setting register DIV (VCRDIV) is a 32-bit read/write register, but is also 16-bit
accessible. The destination vector number is set in VCRDIV when an interrupt occurs in the
division unit due to an overflow or underflow. Values can be set in the 16 bits from bit 15 to bit 0,
but only the last 7 bits (bits 6–0) are valid. Always set 0 for the 9 bits from bit 15 to bit 7.
VCRDIV is not initialized by a power-on reset or manual reset, in standby mode, or during
module standbys.

•    Bits 31 to 7: Reserved. These bits always read 0. The write value should always be 0.

•    Bits 6 to 0: Interrupt Vector Number. Sets the interrupt destination vector number. Only the 7
    bits 6–0 are valid (as the vector number).


10.2.5      Dividend Register H (DVDNTH)

                     Bit:        31            30            39            …             3              2              1              0
             Bit name:                                                        …
         Initial value:        —            —            —            …            —            —            —            —
                   R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


Dividend register H (DVDNTH) is a 32-bit read/write register in which the upper 32 bits of the
dividend used for 64 bit ‚ 32 bit division operations are written. When a division operation is
executed, the value set as the dividend is lost and the remainder written here at the end of the
operation. The initial value of DVDNTH is undefined, and its value is also undefined after a
power-on reset or manual reset, in standby mode, and during in module standbys. When the
DVDNT register is set with a dividend value, the previous DVDNTH value is lost and the MSB of
the DVDNT register is extended to all bits in the DVDNTH register.




                                                                                                                                      291
```

## PDF page 308

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=308)

```text
10.2.6      Dividend Register L (DVDNTL)

                    Bit:        31            30            39            …             3              2              1              0
            Bit name:                                                        …
        Initial value:        —            —            —            …            —            —            —            —
                 R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


Dividend register L (DVDNTL) is a 32-bit read/write register in which the lower 32 bits of the
dividend used for 64-bit ‚ 32-bit division operations are written. When a value is set in this
register, the 64-bit ‚ 32-bit division operation begins. The value written in the DVDNT register
for 32-bit ‚ 32-bit division is also set in this register. When a 64-bit ‚ 32-bit division operation is
executed, the value set as the dividend is lost and the quotient written here at the end of the
operation. The contents of this register are undefined after a power-on reset or manual reset, in
standby mode, and during module standbys.

10.3      Operation


10.3.1      64-Bit ‚ 32-Bit Operations

64-bit ‚ 32-bit operations work as follows:

1.   The 32-bit divisor is set in the divisor register (DVSR).
2.   The 64-bit dividend is set in dividend registers H and L (DVDNTH and DVDNTL). First set
    the value in DVDNTH. When a value is written to DVDNTL, the 64-bit ‚ 32-bit operation
    begins.
3.   This unit finishes a single operation in 39 cycles (starting from the setting of the value in
    DVDNTL). When an overflow occurs, however, the operation ends in 6 cycles. See section
    10.3.3, Handling of Overflows, for more information. Note that operation is signed.
4.   After the operation, the 32-bit remainder is written to DVDNTH and the 32-bit quotient is
    written to DVDNTL.


10.3.2      32-Bit ‚ 32-Bit Operations

32-bit ‚ 32-bit operations work as follows:

1.   The 32-bit divisor is set in the divisor register (DVSR).
2.   The 32-bit dividend is set in dividend register L (DVDNT) for 32-bit division. When a value is
    written to DVDNT, the 32-bit ‚ 32-bit operation begins.
3.   This unit finishes a single operation in 39 cycles (starting from the setting of the value in
    DVDNT). When an overflow occurs, however, the operation ends in 6 cycles. See section
    10.3.3, Handling of Overflows, for more information. Note that the operation is signed.

292
```

## PDF page 309

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=309)

```text
4.   After the operation, the 32-bit remainder is written to DVDNTH and the 32-bit quotient is
    written to DVDNT.


10.3.3      Handling of Overflows

When the results of operations exceed the ranges expressed as signed 32 bits (when, in division
between two negative numbers, the quotient is the maximum value and a remainder (negative
number) is generated) or when the divisor is 0, an overflow will result.

When an overflow occurs, the OVF bit is set and an overflow interrupt is generated if interrupt
generation is enabled (the OVFIE bit in DVCR is 1). The operation will then end with the result
after 6 cycles of operation stored in the DVDNTH and DVDNTL registers. If interrupt generation
is disabled (the OVFIE bit is 0), the operation will end with the operation result at 6 cycles set in
DVDNTH and the maximum value H'7FFFFFFF or minimum value H'80000000 set in DVDNTL.
In the SH7604, the maximum value results when a positive quotient overflows; the minimum
value results when a negative quotient overflows. The first three cycles of the 6 cycles executed
when an overflow occurs are used for flag setting within the division unit and the next three for
division.

10.4      Usage Notes


10.4.1      Access

All accesses to the division unit except DVCR and VCRDIV must be 32-bit reads or writes. Word
accesses to registers other than DVCR and VCRDIV result in reading or writing of undefined
values. In the division unit, a read instruction is extended for one cycle immediately after an
instruction that writes to a register, even if the register is the same, to ensure that the value written
is accurately set in the destination register in the division unit.

When a read or write instruction is issued while the division unit is operating, the read or write
instruction is continuously extended until the operation ends. This means that instructions that do
not access the division unit can be parallel-processed. When an instruction is executed that writes
to any register of the division unit immediately following an instruction that writes to the division
start-up registers (DVDNTL or DVDNT), the correct value may not be set in the start-up register.
Specify an instruction other than one that writes to a division unit register for the instruction
immediately following instruction that writes to a start-up register.

Because of the above restrictions, efficient processing can be achieved by executing instructions
that do not access the division unit for 39 cycles after starting the operation, then issuing a read
instruction after the 39th cycle.





                                                                                                                      293
```

## PDF page 310

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=310)

```text
10.4.2      Overflow Flag

When an overflow occurs, the overflow flag (OVF) is set and is not automatically reset. When
OVF is set, the operation is not affected. When necessary, clear it before the operation. The states
of registers when overflow occurs are shown in table 10.2.

Table 10.2    Overflow Processing

Register       Overflow Interrupt Enabled                        Overflow Interrupt Disabled
DVSR            Holds the value written                                  Holds the value written
DVDNT         Holds the results of operations until                       The maximum value is set for overflow to
                 overflow generation is detected*                         the plus side, or the minimum value for
                                                                          overflow to the minus side
DVCR           The OVF bit is set                                         The OVF bit is set
VCRDIV        Holds the value written                                  Holds the value written
DVDNTH       Holds the results of operations until                        Holds the results of operations until
                 overflow generation is detected*                         overflow generation is detected *
DVDNTL       Holds the results of operations until                        The maximum value is set for overflow to
                 overflow generation is detected*                         the plus side, or the minimum value for
                                                                          overflow to the minus side
Note:   In division processing, the intermediate operation result is written for cycles up to detection
         of overflow generation.































294
```
