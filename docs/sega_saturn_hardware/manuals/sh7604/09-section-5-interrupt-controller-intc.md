# Section 5 Interrupt Controller (INTC)

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=95)

Physical PDF pages 95–122. Text extraction; consult the PDF for figures.

## Bookmarks

- [5.1 Overview](#pdf-page-95)
- [5.1.1 Features](#pdf-page-95)
- [5.1.2 Block Diagram](#pdf-page-95)
- [5.1.3 Pin Configuration](#pdf-page-97)
- [5.1.4 Register Configuration](#pdf-page-97)
- [5.2 Interrupt Sources](#pdf-page-98)
- [5.2.1 NMI Interrupt](#pdf-page-98)
- [5.2.2 User Break Interrupt](#pdf-page-98)
- [5.2.3 IRL Interrupts](#pdf-page-99)
- [5.2.4 On-chip Peripheral Module Interrupts](#pdf-page-101)
- [5.2.5 Interrupt Exception Vectors and Priority Order](#pdf-page-102)
- [5.3 Description of Registers](#pdf-page-104)
- [5.3.1 Interrupt Priority Level Setting Register A (IPRA)](#pdf-page-104)
- [5.3.2 Interrupt Priority Level Setting Register B (IPRB)](#pdf-page-105)
- [5.3.3 Vector Number Setting Register WDT (VCRWDT)](#pdf-page-107)
- [5.3.4 Vector Number Setting Register A (VCRA)](#pdf-page-107)
- [5.3.5 Vector Number Setting Register B (VCRB)](#pdf-page-108)
- [5.3.6 Vector Number Setting Register C (VCRC)](#pdf-page-109)
- [5.3.7 Vector Number Setting Register D (VCRD)](#pdf-page-110)
- [5.3.8 Interrupt Control Register (ICR)](#pdf-page-111)
- [5.4 Interrupt Operation](#pdf-page-113)
- [5.4.1 Interrupt Sequence](#pdf-page-113)
- [5.4.2 Stack after Interrupt Exception Handling](#pdf-page-115)
- [5.5 Interrupt Response Time](#pdf-page-116)
- [5.6 Sampling of Pins IRL3-IRL0](#pdf-page-117)
- [5.7 Usage Notes](#pdf-page-119)

## PDF page 95

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=95)

```text
                      Section 5   Interrupt Controller (INTC)


5.1        Overview

The interrupt controller (INTC) ascertains the priority of interrupt sources and controls interrupt
requests to the CPU. The INTC has registers for setting the priority of each interrupt which allow
the user to set the order of priority in which interrupt requests are handled.


5.1.1        Features

The INTC has the following features:

•    16 interrupt priority levels: By setting the two interrupt-priority level registers, the priorities of
    on-chip peripheral module interrupts can be set at 16 levels for different request sources.
•    Settable vector numbers for on-chip peripheral module interrupts: two vector number setting
    registers enable on-chip peripheral module interrupt vector numbers to be set in the range 0–
    127 by interrupt source.
•    The IRL interrupt vector number setting method can be selected: Either of two modes can be
    selected by a register setting: auto-vector mode in which vector numbers are determined
    internally, and external vector mode in which vector numbers are set externally.

5.1.2        Block Diagram

Figure 5.1 shows a block diagram of the INTC.


























                                                                                                                       79
```

## PDF page 96

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=96)

```text
       NMI
  IR3–IR0
    A3–A0                     Input/
    IVECF                    output
                             control
   D7–D0                                                     Com-
                                                             parator         Interrupt request
                                             Priority                               SR
                                             decision                        I3    I2    I1    I0
      UBC       (Interrupt request)            logic
    DMAC        (Interrupt request)
     DIVU       (Interrupt request)                                                 CPU
      FRT       (Interrupt request)
       SCI      (Interrupt request)
     WDT        (Interrupt request)
      REF       (Interrupt request)





                           ICR                      IPR
                                           IPRA, IPRB




                               Module bus                   Businterface
                                                                           Peripheral bus


                                  VCRWDT                   Vector                Vector       DIV
                                                           number               number          DMAC
                            VCRWDT, VCRA–VCRD                              Internal bus

                                                               INTC

 UBC:      User break controller                          ICR:             Interrupt control register
 DMAC:   Direct memory access controller        IPRA/B:       Interrupt priority level setting
 DIVU:     Division unit                                                            registers A and B
 FRT:      Free-running timer                              VCRWDT:   Vector number setting register WDT
 SCI:       Serial communication interface          VCRA–D:    Vector number setting registers A–D
 WDT:     Watchdog timer                                  SR:              Status register
 REF:      Refresh request within bus state controller


                                  Figure 5.1   INTC Block Diagram


80
```

## PDF page 97

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=97)

```text
5.1.3        Pin Configuration

Table 5.1 shows the INTC pin configuration.

Table 5.1      Pin Configuration

Name                                                Abbreviation     I/O      Function
Nonmaskable interrupt input pin       NMI                    I          Input of nonmaskable interrupt request
                                                                                signal
Level request interrupt input pins      IRL3–IRL0          I          Input of maskable interrupt request
                                                                                signals
Interrupt acceptance level output               A3–A0                O        In external vector mode, output an
pins                                                                            interrupt level signal when an IRL
                                                                                interrupt is accepted
External vector fetch pin                   IVECF                O        Indicates external vector read cycle
External vector number input pins    D7–D0                I          Input external vector number


5.1.4        Register Configuration

The INTC has the eight registers shown in table 5.2. These registers perform various INTC
functions including setting interrupt priority, and controlling external interrupt input signal
detection.






























                                                                                                                                 81
```

## PDF page 98

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=98)

```text
Table 5.2      Register Configuration

                                                                               Initial                           Access
Name                                                         Abbr.          R/WValue      Address                Size*
Interrupt priority level setting register A     IPRA           R/W     H'0000     H'FFFFFEE2     8, 16
Interrupt priority level setting register B     IPRB           R/W     H'0000     H'FFFFFE60      8, 16
Vector number setting register A               VCRA          R/W     H'0000     H'FFFFFE62      8, 16
Vector number setting register B               VCRB          R/W     H'0000     H'FFFFFE64      8, 16
Vector number setting register C               VCRC         R/W     H'0000     H'FFFFFE66      8, 16
Vector number setting register D               VCRD         R/W     H'0000     H'FFFFFE68      8, 16
Vector number setting register WDT         VCRWDT    R/W     H'0000     H'FFFFFEE4     8, 16
Vector number setting register DIV           VCRDIV      R/W     —            H'FFFFFF0C     32
Vector number setting register DMAC0     VCRDMA0  R/W     —            H'FFFFFFA0      32
Vector number setting register DMAC1     VCRDMA1  R/W     —            H'FFFFFFA8      32
Interrupt control register                            ICR             R/W     H'8000/       H'FFFFFEE0     8, 16
                                                                               H'0000*
—: Undefined
Note:   The value when the NMI pin is high is H'8000; when the NMI pin is low, it is H'0000.
         See the sections 9, Direct Memory Access Controller, and 10, Division Unit, for more
         information on VCRDIV, VCRDMA0, and VCRDMA1.


5.2        Interrupt Sources

There are four types of interrupt sources: NMI, user breaks, IRL, and on-chip peripheral modules.
Each interrupt has a priority expressed as a priority level (0 to 16, with 0 the lowest and 16 the
highest). Giving an interrupt a priority level of 0 masks it.


5.2.1        NMI Interrupt

The NMI interrupt has priority 16 and is always accepted. Input at the NMI pin is detected by
edge. Use the NMI edge select bit (NMIE) in the interrupt control register (ICR) to select either
the rising or falling edge. NMI interrupt exception handling sets the interrupt mask level bits (I3–
I0) in the status register (SR) to level 15.


5.2.2        User Break Interrupt

A user break interrupt has priority level 15 and occurs when the break condition set in the user
break controller (UBC) is satisfied. User break interrupt exception handling sets the interrupt mask
level bits (I3–I0) in the status register (SR) to level 15. For more information about the user break
interrupt, see section 6, User Break Controller.

82
```

## PDF page 99

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=99)

```text
5.2.3        IRL Interrupts

IRL interrupts are requested by input from pins IRL3–IRL0. Fifteen interrupts, IRL15–IRL1, can
be input externally via pins IRL3–IRL0. The priority levels of interrupts IRL15–IRL0 are 15–1,
respectively, and their vector numbers are 71–64. Set the vector numbers with the IRL interrupt
vector mode select (VECMD) bit of the interrupt control register (ICR) to enable external input.
External input of vector numbers consists of vector numbers 0–127 from the external vector input
pins (D7–D0). Internal vectors are called auto-vectors and vectors input externally are called
external vectors. Table 5.3 lists IRL priority levels and auto vector numbers.

When an IRL interrupt is accepted in external vector mode, the IRL interrupt level is output from
the interrupt acceptance level output pins (A3–A0). The external vector fetch pin (IVECF) is also
asserted. The external vector number is read from pins D7–D0 at this time. Figures 5.2 and 5.3
show interrupt connection examples.

IRL interrupt exception processing sets the interrupt mask level bits (I3 to I0) in the status register
(SR) to the priority level value of the IRL interrupt that was accepted.

Table 5.3      IRL Interrupt Priority Levels and Auto-Vector Numbers

                   Pin                                                   Priority              Vector
IRL3                   IRL2                   IRL1                   IRL0                   Level                 Number
00001571
000114
00101370
001112
01001169
010110
0110968
01118
1000767
10016
1010566
10114
1100365
11012
1110164





                                                              83
```

## PDF page 100

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=100)

```text
                                                                                     SH7604



          Interrupt                       Priority               4
          requests                       encoder                              IRL0–IRL3
                                                          IRL0–IRL3


                                     Vector number                            A0–A3
                                        generator                             IVECF
                                           circuit                            RD

                                                D0–D7

                                                                              D0–D7



           Figure 5.2   Example of Connections for External Vector Mode Interrupts


                                                                                   SH7604



            Interrupt                   Priority                4
            requests                    encoder                             IRL0–IRL3
                                                         IRL0–IRL3





             Figure 5.3   Example of Connections for Auto-Vector Mode Interrupts

Figure 5.4 shows the interrupt fetch cycle for the external vector mode. During this cycle, CS0–
CS3 stay high. A26–A4 output undefined values. The WAIT pin is sampled, but programmable
waits are not valid.















84
```

## PDF page 101

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=101)

```text
        CKIO

  CS0–CS3

           BS


      A3–A0                                                 Interrupt priority level


       IVECF


     RD/WR


           RD


      D7–D0                                                                                           Vector number input


        WAIT



                   Figure 5.4   External Vector Mode Interrupt Vector Fetch Cycle


5.2.4        On-chip Peripheral Module Interrupts

On-chip peripheral module interrupts are interrupts generated by the following 6 on-chip
peripheral modules:

•    Division unit (DIVU)
•    Direct memory access controller (DMAC)
•    Serial communication interface (SCI)
•    Bus state controller (BSC)
•    Watchdog timer (WDT)
•    Free-running timer (FRT)

A different interrupt vector is assigned to each interrupt source, so the exception service routine
does not have to decide which interrupt has occurred. Priority levels between 0 and 15 can be
assigned to individual on-chip peripheral modules in interrupt priority registers A and B (IPRA
and IPRB). On-chip peripheral module interrupt exception handling sets the interrupt mask level
bits (I3–I0) in the status register (SR) to the priority level value of the on-chip peripheral module
interrupt that was accepted.



                                                                                                                           85
```

## PDF page 102

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=102)

```text
5.2.5        Interrupt Exception Vectors and Priority Order

Table 5.4 lists interrupt sources and their vector numbers, vector table address offsets and interrupt
priorities.

Each interrupt source is allocated a different vector number and vector table address offset. Vector
table addresses are calculated from vector numbers and address offsets. In interrupt exception
handling, the exception service routine start address is fetched from the vector table entry
indicated by the vector table address. See table 4.4, Calculating Exception Vector Table
Addresses, for more information on this calculation.

IRL interrupts IRL15–IRL1 have interrupt priority levels of 15–1, respectively. On-chip peripheral
module interrupt priorities can be set freely between 0 and 15 for each module by setting interrupt
priority registers A and B (IPRA and IPRB). The ranking of interrupt sources for IPRA and IPRB,
however, must be the order listed under Priority Within IPR Setting Unit in table 5.4 and cannot
be changed. A reset assigns priority level 0 to on-chip peripheral module interrupts. If the same
priority level is assigned to two or more interrupt sources and interrupts from those sources occur
simultaneously, their priority order is the default priority order indicated at the right in table 5.4.





































86
```

## PDF page 103

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=103)

```text
Table 5.4      Interrupt Exception Vectors and Priority Order

                                          Interrupt                                  Priority
                                          Priority                                   within
                                          Order                                      IPR                         Vectors
                                          (Initial          IPR (Bit                 Setting          Vector          Vector Table                Default
Interrupt Source                          Value)            Numbers)                 Unit             No.             Address                     Priority
NMI                                   16              —                     —            11            VBR + (vector    High
User break                        15              —                     —            12            No. · 4)
IRL15                                15              —                     —            71*1
IRL14                                14              —                     —
IRL13                                13              —                     —            70*1
IRL12                                12              —                     —
IRL11                                11              —                     —            69*1
IRL10                                10              —                     —
IRL9                                  9                —                     —            68*1
IRL8                                  8                —                     —
IRL7                                  7                —                     —            67*1
IRL6                                  6                —                     —
IRL5                                  5                —                     —            66*1
IRL4                                  4                —                     —
IRL3                                  3                —                     —            65*1
IRL2                                  2                —                     —
IRL1                                  1                —                     —            64*1
DIVU             OVFI           0–15 (0)     IPRA (15–12)                   0–127*2                              Low





















                                                                                                                                                              87
```

## PDF page 104

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=104)

```text
Table 5.4      Interrupt Exception Vectors and Priority Order (cont)

                                    Interrupt                             Priority
                                    Priority                              within
                                    Order                                 IPR                         Vectors
                                    (Initial        IPR (Bit              Setting       Vector         Vector Table            Default
Interrupt Source                    Value)          Numbers)              Unit          No.            Address                 Priority
DMAC0   Transfer end     0–15 (0)     IPRA (11–8)     1              0–127*2   VBR + (vector
DMAC1   Transfer end                                               0              0–127*2   No. · 4)
WDT        ITI                     0–15 (0)     IPRA (7–4)       1              0–127*2
REF*3      CMI                                                             0              0–127*2
SCI          ERI                   0–15 (0)     IPRB (15–12)   3              0–127*2
              RXI                                                              2              0–127*2
              TXI                                                              1              0–127*2
              TEI                                                              0              0–127*2
FRT         ICI                     0–15 (0)     IPRB (11–8)     2              0–127*2
              OCI                                                             1              0–127*2
              OVI                                                             0              0–127*2
Reserved                          —              —                     —            128–255  —                       Low
Notes:  1.   An external vector number fetch can be performed without using the auto-vector
                numbers shown in this table. The external vector numbers are 0–127.
           2.   Vector numbers are set in the on-chip vector number register.
           3.   REF is the refresh control unit within the bus state controller.


5.3        Description of Registers


5.3.1        Interrupt Priority Level Setting Register A (IPRA)

Interrupt priority level setting register A (IPRA) is a 16-bit read/write register that assigns priority
levels from 0 to 15 to on-chip peripheral module interrupts. IPRA is initialized to H'0000 by a
reset. It is not initialized in standby mode. Unless otherwise specified, ‘reset’ refers to both power-
on and manual resets throughout this manual.











88
```

## PDF page 105

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=105)

```text
                    Bit:        15            14            13            12            11            10             9              8
            Bit name:      DIVU           DIVU         DIVU         DIVU         DMAC         DMAC         DMAC         DMAC
                              IP3           IP2          IP1          IP0          IP3          IP2          IP1          IP0
        Initial value:         0              0              0              0              0              0              0              0
                  R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


                    Bit:         7              6              5              4              3              2              1              0
            Bit name:      WDT             WDT          WDT          WDT           ————
                              IP3           IP2          IP1          IP0
        Initial value:         0              0              0              0              0              0              0              0
                  R/W:      R/W         R/W         R/W         R/W           R             R             R             R


•    Bits 15 to 12—Division Unit (DIVU) Interrupt Priority Level (DIVUIP3–DIVUIP0): These
    bits set the division unit (DIVU) interrupt priority level. There are four bits, so levels 0–15 can
    be set.

•    Bits 11 to 8—DMA Controller Interrupt Priority Level (DMACIP3–DMACIP0): These bits set
    the DMA controller (DMAC) interrupt priority level. There are four bits, so levels 0–15 can be
    set. The same level is set for both DMAC channels. When interrupts occur simultaneously,
    channel 0 has priority.

•    Bits 7 to 4—Watchdog Timer (WDT) Interrupt Priority Level (WDTIP3–WDTIP0): These bits
    set the watchdog timer (WDT) interrupt priority level and bus state controller (BSC) interrupt
    priority level. There are four bits, so levels 0–15 can be set. When WDT and BSC interrupts
    occur simultaneously, the WDT interrupt has priority.

•    Bits 3 to 0—Reserved: These bits always read 0. The write value should always be 0.


5.3.2        Interrupt Priority Level Setting Register B (IPRB)

Interrupt priority level setting register B (IPRB) is a 16-bit read/write register that assigns priority
levels from 0 to 15 to on-chip peripheral module interrupts. IPRB is initialized to H'0000 by a
reset. It is not initialized in standby mode.













                                                                                                                               89
```

## PDF page 106

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=106)

```text
                     Bit:        15            14            13            12            11            10             9              8
             Bit name:    SCIIP3     SCIIP2    SCIIP1     SCIIP0    FRTIP3    FRTIP2    FRTIP1   FRTIP0
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


                     Bit:         7              6              5              4              3              2              1              0
             Bit name:        —            —            —            —            —            —            —            —
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R             R             R             R             R             R             R             R


•    Bits 15 to 12—Serial Communication Interface (SCI) Interrupt Priority Level (SCIIP3–
    SCIIP0): These bits set the serial communication interface (SCI) interrupt priority level. There
    are four bits, so levels 0–15 can be set.

•    Bits 11 to 8—Free-Running Timer (FRT) Interrupt Priority Level (FRTIP3–FRTIP0): These
    bits set the free-running timer (FRT) interrupt priority level. There are four bits, so levels 0–15
    can be set.

•    Bits 7 to 0—Reserved: These bits always read 0. The write value should always be 0.

Table 5.5 shows the relationship between on-chip peripheral module interrupts and interrupt
priority level setting registers.

Table 5.5      Interrupt Request Sources and IPRA/IPRB

Register                 Bits 15 to 12          Bits 11 to 8             Bits 7 to 4              Bits 3 to 0
IPRA                       DIVU                       DMAC0, DMAC1     WDT                       Reserved
IPRB                       SCI                          FRT                         Reserved                Reserved


As table 5.5 shows, two or three on-chip peripheral modules are assigned to each interrupt priority
register. Set the priority levels by setting the corresponding 4-bit groups (bits 15 to 12, bits 11 to 8,
and bits 7 to 4) with values in the range of H'0 (0000) to H'F (1111). H'0 is interrupt priority level
0 (the lowest); H'F is level 15 (the highest). When two on-chip peripheral modules are assigned to
the same bits (DMAC0 and DMAC1, or WDT and DRAM refresh control unit), those two
modules have the same priority. A reset initializes IPRA and IPRB to H'0000. They are not
initialized in standby mode.








90
```

## PDF page 107

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=107)

```text
5.3.3        Vector Number Setting Register WDT (VCRWDT)

Vector number setting register WDT (VCRWDT) is a 16-bit read/write register that sets the WDT
interval interrupt and BSC compare match interrupt vector numbers (0–127). VCRWDT is
initialized to H'0000 by a reset. It is not initialized in standby mode.

                    Bit:        15            14            13            12            11            10             9              8
            Bit name:        —         WITV6     WITV5     WITV4     WITV3     WITV2     WITV1     WITV0
         Initial value:         0              0              0              0              0              0              0              0
                  R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W


                    Bit:         7              6              5              4              3              2              1              0
            Bit name:        —        BCMV6   BCMV5    BCMV4   BCMV3    BCMV2    BCMV1   BCMV0
         Initial value:         0              0              0              0              0              0              0              0
                  R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W


•    Bits 15, 7—Reserved: These bits always read 0. The write value should always be 0.

•    Bits 14 to 8—Watchdog Timer (WDT) Interval Interrupt Vector Number (WITV6–WITV0):
    These bits set the vector number for the interval interrupt (ITI) of the watchdog timer (WDT).
    There are seven bits, so the value can be set between 0 and 127.

•    Bits 6 to 0—Bus State Controller (BSC) Compare Match Interrupt Vector Number (BCMV6–
    BCMV0): These bits set the vector number for the compare match interrupt (CMI) of the bus
    state controller (BSC). There are seven bits, so the value can be set between 0 and 127.


5.3.4        Vector Number Setting Register A (VCRA)

Vector number setting register A (VCRA) is a 16-bit read/write register that sets the SCI receive-
error interrupt and receive-data-full interrupt vector numbers (0–127). VCRA is initialized to
H'0000 by a reset. It is not initialized in standby mode.
















                                                                                                                                 91
```

## PDF page 108

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=108)

```text
                      Bit:        15            14            13            12            11            10             9              8
             Bit name:        —        SERV6    SERV5    SERV4    SERV3    SERV2    SERV1    SERV0
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W


                      Bit:         7              6              5              4              3              2              1              0
             Bit name:        —        SRXV6    SRXV5    SRXV4    SRXV3    SRXV2    SRXV1    SRXV0
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W


•    Bits 15, 7—Reserved: These bits always read 0. The write value should always be 0.

•    Bits 14 to 8—Serial Communication Interface (SCI) Receive-Error Interrupt Vector Number
     (SERV6–SERV0): These bits set the vector number for the serial communication interface
     (SCI) receive-error interrupt (ERI). There are seven bits, so the value can be set between 0 and
     127.

•    Bits 6 to 0—Serial Communication Interface (SCI) Receive-Data-Full Interrupt Vector
     Number (SRXV6–SRXV0): These bits set the vector number for the serial communication
     interface (SCI) receive-data-full interrupt (RXI). There are seven bits, so the value can be set
     between 0 and 127.


5.3.5        Vector Number Setting Register B (VCRB)

Vector number setting register B (VCRB) is a 16-bit read/write register that sets the SCI transmit-
data-empty interrupt and transmit-end interrupt vector numbers (0–127). VCRB is initialized to
H'0000 by a reset. It is not initialized in standby mode.

                      Bit:        15            14            13            12            11            10             9              8
             Bit name:        —        STXV6    STXV5     STXV4    STXV3     STXV2     STXV1    STXV0
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W


                      Bit:         7              6              5              4              3              2              1              0
             Bit name:        —        STEV6    STEV5     STEV4    STEV3     STEV2     STEV1    STEV0
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W





92
```

## PDF page 109

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=109)

```text
•    Bits 15, 7—Reserved: These bits always read 0. The write value should always be 0.

•    Bits 14 to 8—Serial Communication Interface (SCI) Transmit-Data-Empty Interrupt Vector
    Number (STXV6–STXV0): These bits set the vector number for the serial communication
    interface (SCI) transmit-data-empty interrupt (TXI). There are seven bits, so the value can be
    set between 0 and 127.

•    Bits 6 to 0—Serial Communication Interface (SCI) Transmit-End Interrupt Vector Number
    (STEV6–STEV0): These bits set the vector number for the serial communication interface
    (SCI) transmit-end interrupt (TEI). There are seven bits, so the value can be set between 0 and
    127.


5.3.6        Vector Number Setting Register C (VCRC)

Vector number setting register C (VCRC) is a 16-bit read/write register that sets the FRT input-
capture interrupt and output-compare interrupt vector numbers (0–127). VCRC is initialized to
H'0000 by a reset. It is not initialized in standby mode.

                    Bit:        15            14            13            12            11            10             9              8
            Bit name:        —         FICV6     FICV5      FICV4     FICV3      FICV2      FICV1     FICV0
        Initial value:         0              0              0              0              0              0              0              0
                  R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W


                    Bit:         7              6              5              4              3              2              1              0
            Bit name:        —        FOCV6    FOCV5    FOCV4    FOCV3    FOCV2    FOCV1    FOCV0
        Initial value:         0              0              0              0              0              0              0              0
                  R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W


•    Bits 15, 7—Reserved: These bits always read 0. The write value should always be 0.

•    Bits 14 to 8—Free-Running Timer (FRT) Input-Capture Interrupt Vector Number (FICV6–
    FICV0): These bits set the vector number for the free-running timer (FRT) input-capture
    interrupt (ICI). There are seven bits, so the value can be set between 0 and 127.

•    Bits 6 to 0—Free-Running Timer (FRT) Output-Compare Interrupt Vector Number (FOCV6–
    FOCV0): These bits set the vector number for the free-running timer (FRT) output-compare
    interrupt (OCI). There are seven bits, so the value can be set between 0 and 127.








                                                                                                                                93
```

## PDF page 110

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=110)

```text
5.3.7        Vector Number Setting Register D (VCRD)

Vector number setting register D (VCRD) is a 16-bit read/write register that sets the FRT overflow
interrupt vector number (0–127). VCRD is initialized to H'0000 by a reset. It is not initialized in
standby mode.

                     Bit:        15            14            13            12            11            10             9              8
            Bit name:        —        FOVV6    FOVV5    FOVV4    FOVV3    FOVV2    FOVV1    FOVV0
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R           R/W         R/W         R/W         R/W         R/W         R/W         R/W


                     Bit:         7              6              5              4              3              2              1              0
            Bit name:        —            —            —            —            —            —            —            —
         Initial value:         0              0              0              0              0              0              0              0
                   R/W:         R             R             R             R             R             R             R             R


•    Bits 15, 7–0—Reserved: These bits always read 0. The write value should always be 0.

•    Bits 14 to 8—Free-Running Timer (FRT) Overflow Interrupt Vector Number (FOVV6–
    FOVV0): These bits set the vector number for the free-running timer (FRT) overflow interrupt
    (OVI). There are seven bits, so the value can be set between 0 and 127.

Tables 5.6 and 5.7 show the relationship between on-chip peripheral module interrupts and
interrupt vector number setting registers.

Table 5.6      Interrupt Request Sources and Vector Number Setting Registers (1)

                                                                                          Bits
Register                                          14–8                                        6–0
Vector number setting register                   Interval interrupt (WDT)          Compare-match interrupt (BSC)
WDT
Vector number setting register A     Receive-error interrupt (SCI)  Receive-data-full interrupt (SCI)
Vector number setting register B     Transmit-data-empty interruptTransmit-end interrupt (SCI)
                                                 (SCI)
Vector number setting register C     Input-capture interrupt (FRT)  Output-compare interrupt (FRT)
Vector number setting register D     Overflow interrupt (FRT)         Reserved








94
```

## PDF page 111

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=111)

```text
As table 5.6 shows, two on-chip peripheral module interrupts are assigned to each register. Set the
vector numbers by setting the corresponding 7-bit groups (bits 14 to 8 and bits 6 to 0) with values
in the range of H'00 (0000000) to H'7F (1111111). H'00 is vector number 0 (the lowest); H'7F is
vector number 127 (the highest). The vector table address is calculated by the following equation.

            Vector table address = VBR + (vector number · 4)

A reset initializes a vector number setting register to H'0000. They are not initialized in standby
mode.

Table 5.7 lists functions for vector number setting registers DIV, DMAC0, and DMAC1. The
vector number for DIV overflow interrupts is set in VCRDIV and the vector numbers for DMAC
transfer-end interrupts are set in VCRDMA0 and VCRDMA1. See sections 9, Direct Memory
Access Controller, and 10, Division Unit, for more details.

Table 5.7      Interrupt Request Sources and Vector Number Setting Registers (2)

Register                                                                 Setting Function
Vector number setting register DIV (VCRDIV)       Overflow interrupts for division unit
Vector number setting register DMAC0                                Channel 0 transfer end interrupt for DMAC
(VCRDMA0)
Vector number setting register DMAC1                                Channel 1 transfer end interrupt for DMAC
(VCRDMA1)


5.3.8        Interrupt Control Register (ICR)

ICR is a 16-bit register that sets the input signal detection mode of external interrupt input pin
NMI and indicates the input signal level at the NMI pin. It also sets the IRL interrupt vector mode.
A reset initializes ICR to H'8000 or H'0000 but the standby mode does not.

                     Bit:        15            14            13            12            11            10             9              8
            Bit name:      NMIL          —            —            —            —            —            —          NMIE
         Initial value:       0/1*            0              0              0              0              0              0              0
                  R/W:         R             R             R             R             R             R             R           R/W


                     Bit:         7              6              5              4              3              2              1              0
            Bit name:        —            —            —            —            —            —            —        VECMD
         Initial value:         0              0              0              0              0              0              0              0
                  R/W:         R             R             R             R             R             R             R           R/W
Note:   When NMI input is high: 1; when NMI input is low: 0



                                                                                                                                    95
```

## PDF page 112

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=112)

```text
•    Bit 15—NMI Input Level (NMIL): Sets the level of the signal input at the NMI pin. This bit
    can be read to determine the NMI pin level. This bit cannot be modified.

Bit 15:  NMIL                       Description
0                                           NMI input level is low
1                                           NMI input level is high


•    Bits 14 to 9—Reserved: These bits always read 0. The write value should always be 0.

•    Bit 8—NMI Edge Select (NMIE): Selects whether the falling or rising edge of the interrupt
    request signal to the NMI pin is detected.

Bit 8:  NMIE                         Description
0                                           Interrupt request is detected on falling edge of NMI input  (Initial value)
1                                           Interrupt request is detected on rising edge of NMI input


•    Bits 7 to 1—Reserved: These bits always read 0. The write value should always be 0.

•    Bit 0—IRL Interrupt Vector Mode Select (VECMD): This bit selects auto-vector mode or
    external vector mode for IRL interrupt vector number setting. In auto-vector mode, an
    internally determined vector number is set. The IRL15 and IRL14 interrupt vector numbers are
    set to 71 and the IRL1 vector number is set to 64. In external vector mode, a value between 0
    and 127 can be input as the vector number from the external vector number input pins (D7–
    D0).

Bit 0:  VECMD                     Description
0                                           Auto vector mode, vector number automatically set internally
                                                                                                                 (Initial value)
1                                           External vector mode, vector number set by external input



















96
```

## PDF page 113

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=113)

```text
5.4        Interrupt Operation


5.4.1        Interrupt Sequence

The sequence of interrupt operations (figure 5.5) is explained below:

1.   The interrupt request sources send interrupt request signals to the interrupt controller.
2.   The interrupt controller selects the highest-priority interrupt among the interrupt requests sent,
    according to the priority levels set in interrupt priority level setting registers A and B (IPRA
    and IPRB). Lower-priority interrupts are held pending. If two of these interrupts have the same
    priority level or if multiple interrupts occur within a single module, the interrupt with the
    highest default priority or the highest priority within its IPR setting unit (as indicated in table
    5.4) is selected.
3.   The interrupt controller compares the priority level of the selected interrupt request with the
    interrupt mask bits (I3–I0) in the CPU’s status register (SR). If the request priority level is
    equal to or less than the level set in I3–I0, the request is held pending. If the request priority
    level is higher than the level in bits I3–I0, the interrupt controller accepts the interrupt and
    sends an interrupt request signal to the CPU.
4.   The CPU detects the interrupt request sent from the interrupt controller when it decodes the
    next instruction to be executed. Instead of executing the decoded instruction, the CPU starts
    interrupt exception handling.
5.   SR and PC are saved onto the stack.
6.   The priority level of the accepted interrupt is copied to the interrupt mask level bits (I3 to I0) in
    the status register (SR).
7.   When external vector mode is specified for the IRL interrupt, the vector number is read from
    the external vector number input pins (D7–D0).
8.   The CPU reads the start address of the exception service routine from the exception vector
    table entry for the accepted interrupt, jumps to that address, and starts executing the program
    there. This jump is not a delayed branch.



















                                                                                                                            97
```

## PDF page 114

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=114)

```text
     Program execution
             state


           Interrupt          No
         generated?

                  Yes

             NMI?              No


                  Yes       User break?        No


                                    Yes          Level 15          No
                                                interrupt?

                                                       Yes              Level 14           No
                                      Yes                               interrupt?
                                                I3 to I0 ≤                                                     No
      Save SR to stack                          level 14?                      Yes              Level 1
                                                                                               interrupt?
                                                       No    Yes        I3 to I0 ≤
      Save PC to stack                                                  level 13?                     Yes

       Copy accepted                                                     No       Yes          I3 to I0 =
   interrupt level to I3–I0                                                                     level 0?

                                                                                                       No
    Read vector number*


       Read exception
         vector table


    Branch to exception
        service routine



    I3-I0:   Status register interrupt mask bits.
    Note:   The vector number is only read from an external source when an external
             vector number is specified for the IRL interrupt vector number.
                                Figure 5.5   Interrupt Sequence Flowchart





98
```

## PDF page 115

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=115)

```text
5.4.2        Stack after Interrupt Exception Handling

Figure 5.6 shows the stack after interrupt exception handling.



                         Address
                           4n – 8                      PC*                  32 bits          SP

                           4n – 4                      SR                   32 bits

                                4n



    Note:   PC: Start address of next instruction after the executing instruction (return destination
             instruction)
                         Figure 5.6   Stack after Interrupt Exception Handling







































                                                                                                                       99
```

## PDF page 116

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=116)

```text
5.5        Interrupt Response Time

Table 5.8 shows the interrupt response time, which is the time from the occurrence of an interrupt
request until interrupt exception handling starts and fetching of the first instruction of the interrupt
service routine begins. Figure 5.7 shows the pipeline when an IRL interrupt is accepted.

Table 5.8      Interrupt Response Time

                                                   Number of States
                                            Peripheral
Item                                     NMIModule           IRL                              Notes
Compare identified inter-          25—
rupt priority with SR mask
level
Wait for completion of             X (‡ 0)                                                             The longest sequence is
sequence currently being                                                                   for interrupt or address-
executed by CPU                                                                            error exception handling
                                                                                           (X = 4 + m1 + m2 + m3 +
                                                                                           m4). If an interrupt-
                                                                                           masking instruction
                                                                                           follows, however, the
                                                                                           time may be even longer.
Time from interrupt                5 + m1 + m2 + m3                                           —
exception handling (SR
and PC saves and vector
address fetch) until fetch
of first instruction of
exception service routine
starts
Interrupt                    Total:  7 + m1 + m2 + m3       10 + m1 + m2 + m3     —
response            Minimum:  10                                13                                —

                    Maximum:  11 + 2 (m1 + m2 +                14 + 2 (m1 + m2 +           —
                                   m3) + m4                    m3) + m4
Note:   m1–m4 are the number of states needed for the following memory accesses
         m1: SR save (longword write)
         m2: PC save (longword write)
         m3: Vector address read (longword read)
         m4: Fetch of first instruction of interrupt service routine










100
```

## PDF page 117

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=117)

```text
                                              Interrupt accepted

                                                                5 + m1 + m2 + m3
                                                  53m1 m2  1  m3  1
                             IRL3–IRL0

             Instruction (instructions
             replaced during interrupt                FDEEMMEMEE
                  exception handling)

                         Overrun fetch                     F
             Interrupt service routine                                                       FDE
                       start instruction

                                        When m1 = m2 = m3 = 1, the
                                    interrupt response time is 13 cycles

    F:        Instruction fetch (instruction fetched from memory where program is stored)
    D:        Instruction decoding (fetched instruction is decoded)
    E:        Instruction execution (data operation and address calculation is performed
            according to the results of decoding)
    M:        Memory access (data in memory is accessed)
                       Figure 5.7   Pipeline when an IRL Interrupt is Accepted

5.6        Sampling of Pins IRL3–IRL0

Signals on interrupt pins IRL3 to IRL0 pass through the noise canceler before being sent by the
interrupt controller to the CPU as interrupt requests. The noise canceler cancels noise that changes
in short cycles. The CPU samples the interrupt requests between executing instructions. During
this period, the noise canceler output changes according to the noise-eliminated pin level, so the
pin level must be held until the CPU samples it. This means that interrupt sources generally must
not be cleared inside interrupt routines.

When an external vector is fetched, the interrupt source can also be cleared when the external
vector fetch cycle is detected.

Figure 5.8 shows a block diagram of the interrupt response procedure. Figure 5.9 shows interrupt
response timing.











                                                                                                                  101
```

## PDF page 118

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=118)

```text
                                                                                   Interrupt
                 IRL0                                                              request
                 IRL1                 Noise                    Interrupt                              CPU
                 IRL2               canceler                   controller
                 IRL3                                                              Interrupt
                                                                                  accepted
                                            Pin level cleared
                                      when interrupt is accepted

                                 Figure 5.8   Interrupt Response Block Diagram


                       1011 for
                       1 clock
     IRL3–IRL0         due to
     pin level         noise                                         Level 2 interrupt                        Level 6 interrupt
         1111                       1111                                   1101                                          1001

     Noise canceler
     output                                           Cleared when interrupt is accepted
                       1111                                    1101                                                         1001



     Interrupt request
     to CPU



     Interrupt acceptance
     signal to CPU

                                  Figure 5.9   Interrupt Response Timing Chart




















102
```

## PDF page 119

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=119)

```text
5.7        Usage Notes

1.   Do not execute module standby for modules that have the module-stop function when the
    possibility remains that an interrupt request may be output.
2.   As shown in figure 5.10, the point at which the NMI request is cleared is the state following
    the decoding stage for the instruction replaced by the interrupt exception handling.


                    NMI request
                          (at fall)


       Instruction replaced by
            interrupt exception                          FDEEMMEMEE
                        handling
             Start instruction in                                                                   FDE
                    NMI routine


                                                            NMI request
                                                            clearing timing

                               Figure 5.10   NMI Request Clearing Timing

3.   Clearing Interrupt Sources:
    External Interrupt Sources: When an interrupt source is cleared by writing to an I/O address,
    another instruction will be executed before the write can be completed because of the write
    buffer. To ensure that the next instruction is executed after the write is completed, read from
    the same address after the write to obtain total synchronization.
    •     Returning from interrupt handling with an RTE instruction: Figure 5.11 shows how a
        minimum interval of 1 cycle is required between the read instruction used for
        synchronization and the RTE instruction. A read instruction for synchronization and a
        minimum of 1 instruction should thus be executed between the source clear and the RTE
        instruction.
    •     Changing the level during interrupt handling: Figure 5.12 shows how a minimum interval
        of 4 cycles is required between the synchronization instruction and the LDC instruction
        when an LDC instruction is used to enable another overlapping interrupt by changing the
        SR value. A read instruction for synchronization and a minimum of 4 instructions should
        thus be executed between the source clear and the LDC instruction.









                                                                                                                   103
```

## PDF page 120

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=120)

```text
                                                                                     Next interrupt can
                                                              Writing complete          be accepted

               Interrupt clear instruction                               External write
               ex.         mov         r0,@r1    F   D   E   M           Min. 2 cycles
             Synchronization instruction             F   D   E   M              W    External read
               ex.         mov         @r1,r0                                        Min. 2 cycles
                                                        1 cycle

                           RTE instruction                   F   D               E  M    M

                    Delay slot instruction                       F                           D   E

               Instruction at destination                                                    F   D   E
                  of return from interrupt
                                                                                 5 cycles


                                 IRL3–IRL0


                          Figure 5.11   Pipeline Operation in Return with RTE


                                                                                     Next interrupt can
                                                              Writing complete          be accepted

               Interrupt clear instruction                               External write
               ex.         mov         r0,@r1    F   D   E   M           Min. 2 cycles
             Synchronization instruction             F   D   E   M              W    External read
               ex.         mov         @r1,r0                                        Min. 2 cycles
                                                             4 cycles

                           LDC instruction                                           F   D   E

            Interrupt-disable instruction                                                F   D   E

                      Ordinary instruction                                                   F   D   E

                                                                                 5 cycles


                                 IRL3–IRL0


        Figure 5.12   Pipeline Operation when Interrupts are Enabled by Modifying SR







104
```

## PDF page 121

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=121)

```text
On-Chip Interrupt Sources: Pipeline operation must be taken into account to ensure that the
same interrupt does not occur again when the interrupt source is from an on-chip peripheral
module. At least 2 cycles are required for the CPU to recognize that the interrupt is from an on-
chip peripheral module. Two cycles are also required for the fact that there is no longer an
interrupt request to be relayed.

•    Returning from interrupt handling with an RTE instruction: Figure 5.13 shows how an extra
    cycle is required after the read instruction used for synchronization before interrupts are
    accepted, even when an RTE instruction is executed. A read instruction for synchronization
    should thus be executed between the source clear and the RTE instruction.
•    Changing the level during interrupt handling: Figure 5.14 shows how a minimum interval of 2
    cycles is required between the synchronization instruction and the LDC instruction when an
    LDC instruction is used to enable another overlapping interrupt by changing the SR value. A
    read instruction for synchronization and a minimum of 2 instructions should thus be executed
    between the source clear and the LDC instruction.


                                                                           Next interrupt can
                                                                              be accepted

                                                           Writing complete
                Interrupt clear instruction                            On-chip peripheral write
                ex.         mov         r0,@r1    F   D   E   M        Min. 1 cycle
              Synchronization instruction             F   D   E   M       W    On-chip peripheral read
                ex.         mov         @r1,r0                                 Min. 1 cycle


                            RET instruction               F   D    E      M   M

                     Delay slot instruction                   F                    D   E

                 Instruction at destination                                        F   D   E
                   of return from interrupt
                                                                       2 cycles    1-cycle margin


             On-chip peripheral interrupt


                          Figure 5.13   Pipeline Operation in Return with RTE










                                                                                                                      105
```

## PDF page 122

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=122)

```text
                                                                      Next interrupt can
                                                                         be accepted

                                                         Writing complete
                Interrupt clear instruction                           On-chip peripheral write
               ex.         mov         r0,@r1    F   D   E   M        Min. 1 cycle
             Synchronization instruction             F   D    E  M       W    On-chip peripheral read
               ex.         mov         @r1,r0                                 Min. 1 cycle
                                                          2 cycles

                            LDC instruction                       F      D    E

             Interrupt-disable instruction                               F   D    E

                       Ordinary instruction                                   F   D   E

                                                                      2 cycles


             On-chip peripheral interrupt


      Figure 5.14   Pipeline Operation when Interrupts are Enabled by Changing the SR


































106
```
