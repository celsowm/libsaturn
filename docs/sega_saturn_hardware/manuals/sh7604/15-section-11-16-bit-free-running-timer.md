# Section 11 16-Bit Free-Running Timer

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=311)

Physical PDF pages 311–334. Text extraction; consult the PDF for figures.

## Bookmarks

- [11.1 Overview](#pdf-page-311)
- [11.1.1 Features](#pdf-page-311)
- [11.1.2 Block Diagram](#pdf-page-312)
- [11.1.3 Pin Configuration](#pdf-page-313)
- [11.1.4 Register Configuration](#pdf-page-313)
- [11.2 Register Descriptions](#pdf-page-314)
- [11.2.1 Free-Running Counter (FRC)](#pdf-page-314)
- [11.2.2 Output Compare Registers A and B (OCRA and OCRB)](#pdf-page-314)
- [11.2.3 Input Capture Register (ICR)](#pdf-page-315)
- [11.2.4 Timer Interrupt Enable Register (TIER)](#pdf-page-315)
- [11.2.5 Free-Running Timer Control/Status Register (FTCSR)](#pdf-page-316)
- [11.2.6 Timer Control Register (TCR)](#pdf-page-318)
- [11.2.7 Timer Output Compare Control Register (TOCR)](#pdf-page-319)
- [11.3 CPU Interface](#pdf-page-320)
- [11.4 Operation](#pdf-page-323)
- [11.4.1 FRC Count Timing](#pdf-page-323)
- [11.4.2 Output Timing for Output Compare](#pdf-page-324)
- [11.4.3 FRC Clear Timing](#pdf-page-324)
- [11.4.4 Input Capture Input Timing](#pdf-page-325)
- [11.4.5 Input Capture Flag (ICF) Setting Timing](#pdf-page-326)
- [11.4.6 Output Compare Flag (OCFA, OCFB) Setting Timing](#pdf-page-326)
- [11.4.7 Timer Overflow Flag (OVF) Setting Timing](#pdf-page-327)
- [11.5 Interrupt Sources](#pdf-page-328)
- [11.6 Example of FRT Use](#pdf-page-328)
- [11.7 Usage Notes](#pdf-page-329)

## PDF page 311

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=311)

```text
                    Section 11   16-Bit Free-Running Timer


11.1      Overview

The SH7604 has a single-channel, 16-bit free-running timer (FRT) on-chip. The FRT is based on a
16-bit free-running counter (FRC) and can output two types of independent waveforms. The FRT
can also measure the width of input pulses and the cycle of external clocks.


11.1.1      Features

The FRT has the following features:

•    Allows selection between four types of counter input clocks. Select from external clock or
    three types of internal clocks (f/8, f/32, and f/128). (External events can be counted.)
•    Two independent comparators. Two types of waveforms can be output.
•    Input capture. Select rising edge or falling edge.
•    Counter clear can be specified. The counter value can be cleared upon compare match A.
•    Four types of interrupt sources. Two compare matches, one input capture, and one overflow
    are available as interrupt sources, and interrupts can be requested independently for each.

































                                                                                                                  295
```

## PDF page 312

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=312)

```text
11.1.2      Block Diagram

Figure 11.1 shows a block diagram of the FRT.


                                   Internal clock
                                   φ/8
   FTCI                            φ/32
                                   φ/128

              Clock select        Clock           OCRA (H/L)
                              Compare
                               match A           Comparator A
  FTOA                                                                                     Internal
  FTOB                        Overflow                                                     data bus
                                                  FRC (H/L)
                                Clear
     FTI                                         Comparator B
                              Compare                                                  Bus interface
                 Control       match B
                  logic                           OCRB (H/L)


                               Capture             ICR (H/L)


                                                         FTCSR                Module
                                                           TIER               data bus


                                                           TCR

                                                          TOCR
                             ICI         Interrupt
                             OCIA
                             OCIB     signals
                             OVI


   OCRA,B:   Output compare registers A,B (16 bits)
       FRC:   Free-running counter (16 bits)
        ICR:   Input capture register (16 bits)
       TCR:   Timer control register (8 bits)
       TIER:   Timer interrupt enable register (8 bits)
    FTCSR:   Free-running timer control/status register (8 bits)
     TOCR:   Timer output compare control register (8 bits)

                                 Figure 11.1   FRT Block Diagram


296
```

## PDF page 313

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=313)

```text
11.1.3      Pin Configuration

Table 11.1 lists FRT I/O pins and their functions.

Table 11.1    Pin Configuration

Channel                                             Pin          I/O           Function
Counter clock input pin                       FTCI        I               FRC counter clock input pin
Output compare A output pin             FTOA      O             Output pin for output compare A
Output compare B output pin             FTOB      O             Output pin for output compare B
Input capture input pin                        FTI           I               Input pin for input capture


11.1.4      Register Configuration

Table 11.2 shows the FRT register configuration.

Table 11.2    Register Configuration

                                                                                                    Initial
Register                                                      Abbreviation       R/W                Value    Address
Timer interrupt enable register                    TIER                     R/W           H'01       HFFFFFE10
Free-running timer control/status register   FTCSR                 R/(W)*1      H'00       HFFFFFE11
Free-running counter H                               FRC H                  R/W           H'00       HFFFFFE12
Free-running counter L                                FRC L                  R/W           H'00       HFFFFFE13
Output compare register A H                       OCRA H               R/W           H'FF      HFFFFFE14*2
Output compare register A L                       OCRA L               R/W           H'FF      HFFFFFE15*2
Output compare register B H                       OCRB H               R/W           H'FF      HFFFFFE14*2
Output compare register B L                       OCRB L               R/W           H'FF      HFFFFFE15*2
Timer control register                                  TCR                     R/W           H'00       HFFFFFE16
Timer output compare control register        TOCR                   R/W           H'E0      HFFFFFE17
Input capture register H                               ICR H                   R               H'00       HFFFFFE18
Input capture register L                               ICR L                    R               H'00       HFFFFFE19
Notes:  1.   Bits 7 to 1 are read-only. The only value that can be written is a 0, which is used to
                clear flags. Bit 0 can be read or written.
           2.   OCRA and OCRB have the same address. The OCRS bit in TOCR is used to switch
                between them.
           3.   Use byte-size access for all registers.





                                                                                                                                     297
```

## PDF page 314

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=314)

```text
11.2      Register Descriptions


11.2.1      Free-Running Counter (FRC)

                    Bit:        15            14            13            …             3              2              1              0
            Bit name:                                                        …
        Initial value:         0              0              0             …             0              0              0              0
                  R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


FRC is a 16-bit read/write up-counter. It increments upon input of a clock. The input clock can be
selected using clock select bits 1 and 0 (CKS1, CKS0) in TCR. FRC can be cleared upon compare
match A.

When FRC overflows (H'FFFF ﬁ   H'0000), the overflow flag (OVF) in FTCSR is set to 1. FRC
can be read or written to by the CPU, but because it is 16 bits long, data transfers involving the
CPU are performed via a temporary register (TEMP). See section 11.3, CPU Interface, for more
detailed information.

FRC is initialized to H'0000 by a reset, in standby mode, and when the module standby function is
used.


11.2.2      Output Compare Registers A and B (OCRA and OCRB)

                    Bit:        15            14            13            …             3              2              1              0
            Bit name:                                                        …
        Initial value:         1              1              1             …             1              1              1              1
                  R/W:      R/W         R/W         R/W           …          R/W         R/W         R/W         R/W


OCR is composed of two 16-bit read/write registers (OCRA and OCRB). The contents of OCR are
always compared to the FRC value. When the two values are the same, the output compare flags
in FTCSR (OCFA and OCFB) are set to 1.

When the OCR and FRC values are the same (compare match), the output level values set in the
output level bits (OLVLA and OLVLB) are output to the output compare pins (FTOA and FTOB).
After a reset, FTOA and FTOB output 0 until the first compare match occurs.

Because OCR is a 16-bit register, data transfers involving the CPU are performed via a temporary
register (TEMP). See section 11.3, CPU Interface, for more detailed information.

OCR is initialized to H'FFFF by a reset, in standby mode, and when the module standby function
is used.

298
```

## PDF page 315

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=315)

```text
11.2.3      Input Capture Register (ICR)

                     Bit:        15            14            13            …             3              2              1              0
             Bit name:                                                        …
         Initial value:         0              0              0             …             0              0              0              0
                   R/W:         R             R             R             …             R             R             R             R


ICR is a 16-bit read-only register. When a rising edge or falling edge of the input capture signal is
detected, the current FRC value is transferred to ICR. At the same time, the input capture flag
(ICF) in FTCSR is set to 1. The edge of the input signal can be selected using the input edge select
bit (IEDGA) in TCR.

Because ICR is a 16-bit register, data transfers involving the CPU are performed via a temporary
register (TEMP). See Section 11.3, CPU Interface, for more detailed information. To ensure that
the input capture operation is reliably performed, set the pulse width of the input capture input
signal to six system clocks (f) or more.

ICR is initialized to H'0000 by a reset, in standby mode, and when the module standby function is
used.


11.2.4      Timer Interrupt Enable Register (TIER)

                     Bit:         7              6              5              4              3              2              1              0
             Bit name:      ICIE           —            —            —        OCIAE     OCIBE      OVIE         —
         Initial value:         0              0              0              0              0              0              0              1
                   R/W:      R/W           —            —            —          R/W         R/W         R/W          —


TIER is an 8-bit read/write register that controls enabling of all interrupt requests. TIER is
initialized to H'01 by a reset, in standby mode, and when the module standby function is used.

•    Bit 7—Input Capture Interrupt Enable (ICIE): Selects enabling/disabling of the ICI interrupt
    request when the input capture flag (ICF) in FTCSR is set to 1.

Bit 7:  ICIE                           Description
0                                           Interrupt request (ICI) caused by ICF disabled                  (Initial value)
1                                           Interrupt request (ICI) caused by ICF enabled


•    Bits 6 to 4—Reserved: These bits always read 0. The write value should always be 0. Do not
    write 1.



                                                                                                                                       299
```

## PDF page 316

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=316)

```text
•    Bit 3—Output Compare Interrupt A Enable (OCIAE): Selects enabling/disabling of the OCIA
    interrupt request when the output compare flag A (OCFA) in FTCSR is set to 1.

Bit 3:  OCIAE                      Description
0                                           Interrupt request (OCIA) caused by OCFA disabled          (Initial value)
1                                           Interrupt request (OCIA) caused by OCFA enabled


•    Bit 2—Output Compare Interrupt B Enable (OCIBE): Selects enabling/disabling of the OCIB
    interrupt request when the output compare flag B (OCFB) in FTCSR is set to 1.

Bit 2:  OCIBE                      Description
0                                           Interrupt request (OCIB) caused by OCFB disabled          (Initial value)
1                                           Interrupt request (OCIB) caused by OCFB enabled


•    Bit 1—Timer Overflow Interrupt Enable (OVIE): Selects enabling/disabling of the OVI
    interrupt request when the overflow flag (OVF) in FTCSR is set to 1.

Bit 1:  OVIE                         Description
0                                           Interrupt request (FOVI) caused by OVF disabled             (initial value)
1                                           Interrupt request (FOVI) caused by OVF enabled


•    Bit 0—Reserved: This bit always reads 1. The write value should always be 1.


11.2.5      Free-Running Timer Control/Status Register (FTCSR)

                     Bit:         7              6              5              4              3              2              1              0
            Bit name:       ICF           —            —            —         OCFA      OCFB       OVF      CCLRA
         Initial value:         0              0              0              0              0              0              0              0
                  R/W:    R/(W)*         —            —            —        R/(W)*     R/(W)*     R/(W)*       R/W
Note: For bits 7, and 3 to 1, the only value that can be written is 0 (to clear the flags).

FTCSR is an 8-bit register that selects counter clearing and controls interrupt request signals.
FTCSR is initialized to H'00 by a reset, in standby mode, and when the module standby function is
used. See section 11.4, Operation, for the timing.

•    Bit 7—Input Capture Flag (ICF): Status flag that indicates that the FRC value has been sent to
    FICR by the input capture signal. This flag is cleared by software and set by hardware. It
    cannot be set by software.




300
```

## PDF page 317

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=317)

```text
Bit 7:  ICF                            Description
0                                           Clear conditions: When ICF is read while set to 1, and then 0 is
                                       written to it                                                                          (Initial value)
1                                           Set conditions: When the FRC value is sent to ICR by the input
                                       capture signal


•    Bits 6 to 4—Reserved: These bits always read 0. The write value should always be 0.

•    Bit 3—Output Compare Flag A (OCFA): Status flag that indicates when the values of the FRC
    and OCRA match. This flag is cleared by software and set by hardware. It cannot be set by
    software.

Bit 3:  OCFA                        Description
0                                           Clear conditions: When OCFA is read while set to 1, and then 0 is
                                       written to it                                                                          (Initial value)
1                                           Set conditions: When the FRC value becomes equal to OCRA


•    Bit 2—Output Compare Flag B (OCFB): Status flag that indicates when the values of FRC and
    OCRB match. This flag is cleared by software and set by hardware. It cannot be set by
    software.

Bit 2:  OCFB                        Description
0                                           Clear conditions: When OCFB is read while set to 1, and then 0 is
                                       written to it                                                                          (Initial value)
1                                           Set conditions: When the FRC value becomes equal to OCRB


•    Bit 1—Timer Overflow Flag (OVF): Status flag that indicates when FRC overflows (from
    H'FFFF to H'0000). This flag is cleared by software and set by hardware. It cannot be set by
    software.

Bit 1:  OVF                          Description
0                                           Clear conditions: When OVF is read while set to 1, and then 0 is
                                       written to it                                                                          (Initial value)
1                                           Set conditions: When the FRC value changes from H'FFFF to H'0000


•    Bit 0—Counter Clear A (CCLRA): Selects whether or not to clear FRC on compare match A
    (signal indicating match of FRC and OCRA).







                                                                                                                                     301
```

## PDF page 318

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=318)

```text
Bit 0:  CCLRA                     Description
0                                           FRC clear disabled                                                             (Initial value)
1                                           FRC cleared on compare match A


11.2.6      Timer Control Register (TCR)

                      Bit:         7              6              5              4              3              2              1              0
             Bit name:    IEDGA        —            —            —            —            —          CKS1      CKS0
         Initial value:         0              0              0              0              0              0              0              0
                    R/W:      R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


TCR is an 8-bit read/write register that selects the input edge for input capture and selects the
input clock for FRC. TCR is initialized to H'00 by a reset, in standby mode, and when the module
standby function is used.

•    Bit 7—Input Edge Select (IEDG): Selects whether to capture the input capture input (FTI) on
     the falling edge or rising edge.

Bit 7:  IEDG                         Description
0                                           Input captured on falling edge                                            (Initial value)
1                                           Input captured on rising edge


•    Bits 6 to 2—Reserved: These bits always read 0. The write value should always be 0. Do not
     write 1.

•    Bits 1 and 0—Clock Select (CKS1, CKS0): These bits select whether to use an external clock
     or one of three internal clocks for input to FRC. The external clock is counted at the rising
     edge.

Bit 1:  CKS1          Bit 0:  CKS0        Description
0                            0                           Internal clock: count at f/8                                   (Initial value)
                           1                           Internal clock: count at f/32
1                            0                           Internal clock: count at f/128
                           1                           External clock: count at rising edge









302
```

## PDF page 319

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=319)

```text
11.2.7      Timer Output Compare Control Register (TOCR)

                     Bit:         7              6              5              4              3              2              1              0
            Bit name:        —            —            —         OCRS        —            —        OLVLA    OLVLB
         Initial value:         1              1              1              0              0              0              0              0
                  R/W:        —            —            —           R/W         R/W         R/W         R/W         R/W


TOCR is an 8-bit read/write register that selects the output level for output compare, enables
output compare output, and controls switching between access of output compare registers A and
B. TOCR is initialized to H'E0 by a reset, in standby mode, and when the module standby function
is used.

Bits 7 to 5—Reserved: These bits always read 1. The write value should always be 1. Do not write
0.

Bit 4—Output Compare Register Select (OCRS): OCRA and OCRB share the same address. The
OCRS bit controls which register is selected when reading/writing to this address. It does not
affect the operation of OCRA and OCRB.

Bit 4:  OCRS                       Description
0                                           OCRA register selected                                                     (Initial value)
1                                           OCRB register selected


Bits 3 and 2—Reserved: These bits always read 0. The write value should always be 0. Do not
write 1.

Bit 1—Output Level A (OLVLA): Selects the level output to the output compare A output pin
upon compare match A (signal indicating match of FRC and OCRA).

Bit 1:  OLVLA                      Description
0                                           0 output on compare match A                                            (Initial value)
1                                           1 output on compare match A


Bit 0—Output Level B (OLVLB): Selects the level output to the output compare B output pin
upon compare match B (signal indicating match of FRC and OCRB).

Bit 0:  OLVLB                      Description
0                                           0 output on compare match B                                            (Initial value)
1                                           1 output on compare match B




                                                                                                                                   303
```

## PDF page 320

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=320)

```text
11.3      CPU Interface

FRC, OCRA, OCRB, and FICR are 16-bit registers. The data bus width between the CPU and
FRT, however, is only 8 bits. Access of these three types of registers from the CPU therefore
needs to be performed via an 8-bit temporary register called TEMP.

The following describes how these registers are read from and written to:

•    Writing to 16-bit Registers
    The upper byte is written, which results in the upper byte of data being stored in TEMP. The
    lower byte is then written, which results in 16 bits of data being written to the register when
    combined with the upper byte value in TEMP.

•    Reading from 16-bit Registers
    The upper byte of data is read, which results in the upper byte value being transferred to the
    CPU. The lower byte value is transferred to TEMP. The lower byte is then read, which results
    in the lower byte value in TEMP being sent to the CPU.

When registers of these three types are accessed, two byte accesses should always be performed,
first to the upper byte, then the lower byte. The same applies to accesses with the on-chip direct
memory access controller. If only the upper byte or lower byte is accessed, the data will not be
transferred properly.

Figure 11.2 and 11.3 show the flow of data when FRC is accessed. Other registers function in the
same way. When reading OCRA and OCRB, however, both upper and lower-byte data is
transferred directly to the CPU without passing through TEMP.


























304
```

## PDF page 321

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=321)

```text
(Write to upper byte)


  CPU                                                                     Data bus within module
(H'AA)             Bus interface
 upper
  byte
                                                                   TEMP
                                                                   (H'AA)



                                                       FRC H                   FRC L
                                                       (       )                (       )

(Write to lower byte)


 CPU                                                                     Data bus within module
(H'55)             Bus interface
 lower
  byte
                                                                   TEMP
                                                                  (H'AA)



                                                      FRC H                    FRC L
                                                       (H'AA)                  (H'55)


           Figure 11.2   FRC Access Operation (CPU Writes H'AA55 to FRC)























                                                                                                        305
```

## PDF page 322

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=322)

```text
    (Read from upper byte)


      CPU                                                                    Data bus within module
    (H'AA)              Bus interface
     upper
       byte
                                                                      TEMP
                                                                      (H'55)



                                                          FRC H                   FRC L
                                                          (H'AA)                   (H'55)

     (Read from lower byte)


      CPU                                                                    Data bus within module
     (H'55)             Bus interface
      lower
       byte
                                                                      TEMP
                                                                      (H'AA)



                                                          FRC H                    FRC L
                                                           (      )                (      )

              Figure 11.3   FRC Access Operation (CPU Reads H'AA55 from FRC)























306
```

## PDF page 323

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=323)

```text
11.4      Operation


11.4.1      FRC Count Timing

The FRC increments on clock input (internal or external).

Internal Clock Operation: Set the CKS1 and CKS0 bits in TCR to select which of the three
internal clocks created by dividing system clock f (f/8, f/32, f/128) is used. Figure 11.4 shows
the timing.



      Timer drive
              clock
    Internal clock

        FRC input
              clock

               FRC         N – 1                                        N                                                       N + 1

                         Figure 11.4   Count Timing (Internal Clock Operation)

External Clock Operation: Set the CKS1 and CKS0 bits in TCR to select the external clock.
External clock pulses are counted on the rising edge. The pulse width of the external clock must be
at least 6 system clocks (f). A smaller pulse width will result in inaccurate operation. Figures 11.5
shows the timing.


           Timer drive
                   clock
       External clock
              input pin
            FRC input
                   clock

                   FRC                                                   N                                       N + 1

                         Figure 11.5   Count Timing (External Clock Operation)









                                                                                                                        307
```

## PDF page 324

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=324)

```text
11.4.2      Output Timing for Output Compare

When a compare match occurs, the output level set in the OLVL bit in TOCR is output from the
output compare output pins (FTOA, FTOB). Figure 11.6 shows the timing for output of output
compare A.


  Timer drive
        clock

         FRC        NNN + 1                                                                       N + 1


       OCRA                  NN

    Compare
     match A
       signal                                       Clear*

      OLVLA
       Output
   compare A
   output pin
       FTOA

   Note:    ﬂ Indicates instruction execution by software
                     Figure 11.6   Output Timing for Output Compare A


11.4.3      FRC Clear Timing

FRC can be cleared on compare match A. Figure 11.7 shows the timing.



  Timer drive
        clock
    Compare
     match A
       signal


        FRC                    N                                                       H'0000

                        Figure 11.7   Compare Match A Clear Timing




308
```

## PDF page 325

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=325)

```text
11.4.4      Input Capture Input Timing

Either the rising edge or falling edge, can be selected for input capture input using the IEDG bit in
TCR. Figure 11.8 shows the timing when the rising edge is selected (IEDG = 1).



    Timer drive
           clock


  Input capture
       input pin


  Input capture
          signal

                          Figure 11.8   Input Capture Signal Timing (Normal)

When the input capture signal is input when ICR is read (upper-byte read), the input capture signal
is delayed by one cycle of the clock that drives the timer. Figure 11.9 shows the timing.


                                                                      ICR upper-byte read cycle
    Timer drive
           clock


 Input capture
       input pin


 Input capture
          signal

     Figure 11.9   Input Capture Signal Timing (Input Capture Input when ICR is Read)

















                                                                                                                    309
```

## PDF page 326

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=326)

```text
11.4.5      Input Capture Flag (ICF) Setting Timing

Input capture input sets the input capture flag (ICF) to 1 and simultaneously transfers the FRC
value to ICR. Figure 11.10 shows the timing.


    Timer drive
            clock


  Input capture
           signal



             ICF



            FRC                                                          N




             ICR                                                         N

                                     Figure 11.10   ICF Setting Timing


11.4.6      Output Compare Flag (OCFA, OCFB) Setting Timing

The compare match signal output (when OCRA or OCRB matches the FRC value) sets output
compare flag OCFA or OCFB to 1. The compare match signal is generated in the last state in
which the values matched (at the timing for updating the count value that matched the FRC). After
OCRA or OCRB matches the FRC, no compare match signal is generated until the increment lock
is generated. Figure 11.11 shows the timing for setting OCFA and OCFB.


















310
```

## PDF page 327

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=327)

```text
      Timer drive
             clock


              FRC                         N                                      N + 1



   OCRA, OCRB                             N



 Compare match
            signal



   OCFA, OCFB

                                  Figure 11.11   OCF Setting Timing


11.4.7      Timer Overflow Flag (OVF) Setting Timing

FRC overflow (from H'FFFF to H'0000) sets the timer overflow flag (OVF) to 1. Figure 11.12
shows the timing.


   Timer drive
          clock


           FRC               H'FFFF                             H'0000


      Overflow
         signal



           OVF

                                  Figure 11.12   OVF Setting Timing










                                                                                                          311
```

## PDF page 328

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=328)

```text
11.5      Interrupt Sources

There are four FRT interrupt sources of three types (ICI, OCIA/OCIB, and OVI). Table 11.3 lists
the interrupt sources and their priorities after a reset is cleared. The interrupt enable bits in TIER
are used to enable or disable the interrupt bits. Each interrupt request is sent to the interrupt
controller independently. See section 4, Exception Handling, for more information about priorities
and the relationship to interrupts other than those of the FRT.

Table 11.3    FRT Interrupt Sources and Priorities

Interrupt Source                         Description                                 Priority
ICI                                                Interrupt by ICF                            High
OCIA, OCIB                                 Interrupt by OCFA or OCFB         ›
OVI                                               Interrupt by OVF                          Low


11.6      Example of FRT Use

Figure 11.13 shows an example in which pulses with a 50% duty factor and arbitrary phase
relationship are output. The procedure is as follows:

1.   Set the CCLRA bit in FTCSR to 1.
2.   The OLVLA and OLVLB bits are inverted by software whenever a compare match occurs.


                                        FRC                             Counter clear
        H'FFFF

          OCRA

          OCRB

         H'0000


          FTOA




          FTOB

                                     Figure 11.13   Example of Pulse Output




312
```

## PDF page 329

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=329)

```text
11.7      Usage Notes

Note that the following contention and operations occur when the FRT is operating:

1.   FRC operates on the timer drive clock (f/4), which has a cycle of 4 times the system clock (f).
    For this reason, when the CPU performs an access, both the CPU and FRT will be operating,
    so a WAIT request will be generated from the FRT to the CPU. The number of access cycles
    thus varies by between 3 and 12 cycles.

2.   Contention between FRC Write and Clear
    When a counter clear signal is generated with the timing shown in figure 11.14 during the
    write cycle for the lower byte of FRC, writing does not occur to the FRC, and the FRC clear
    takes priority.


                                                 FRC lower-byte write cycle
                           Timer drive
                                   clock


                               Address                      FRC address

                          Internal write
                                  signal

                         Counter clear
                                  signal


                                    FRC                       N                         H'0000

                       Figure 11.14   Contention between FRC Write and Clear

3.   Contention between FRC Write and Increment
    When an increment occurs with the timing shown in figure 11.15 during the write cycle for the
    lower byte of FRC, no increment is performed and the counter write takes priority.
















                                                                                                                   313
```

## PDF page 330

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=330)

```text
                                           FRC upper-byte write cycle
                        Timer drive
                              clock


                           Address                  FRC address



                      Internal write
                             signal


                         FRC input
                              clock



                               FRC                    NM

                                                                Write data

                 Figure 11.15   Contention between FRC Write and Increment

4.   Contention between OCR Write and Compare Match
   When a compare match occurs with the timing shown in figure 11.16, during the write cycle
   for the lower byte of OCRA or OCRB, the OCR write takes priority and the compare match
   signal is disabled.




























314
```

## PDF page 331

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=331)

```text
                                              FRC lower-byte write cycle
                         Timer drive
                                clock


                             Address                    OCR address


                       Internal write
                               signal


                                 FRC                      N                            N + 1




                                 OCR                      NM

                                                                   Write data

                    Compare match
                               signal
                                                                                     Disabled

                   Figure 11.16   Contention between OCR and Compare Match

5.   Internal Clock Switching and Counter Operation
    FRC will sometimes begin incrementing because of the timing of switching between internal
    clocks. Table 11.4 shows the relationship between internal clock switching timing (CKS1 and
    CKS0 bit rewrites) and FRC operation.
    When an internal clock is used, the FRC clock is generated when the falling edge of an internal
    clock (created by dividing the system clock (f)) is detected. When a clock is switched to high
    before the switching and to low after switching, as shown in case 3 in table 11.4, the
    switchover is considered a falling edge and an FRC clock pulse is generated, causing FRC to
    increment. FRC may also increment when switching between an internal clock and an external
    clock.














                                                                                                               315
```

## PDF page 332

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=332)

```text
Table 11.4    Internal Clock Switching and FRC Operation

     Timing of Rewrite of
No.  CKS1 and CKS0 Bits        FRC Operation
1     Low-to-low switch

                 Clock before
                      switching


                    Clock after
                      switching



                     FRC clock



                            FRC            N                                        N + 1


                                              Rewrite of CKS bit
2     Low-to-high switch

                 Clock before
                      switching


                   Clock after
                      switching


                     FRC clock



                            FRC             N                         N + 1                  N + 2

                                                                       Rewrite of CKS bit















316
```

## PDF page 333

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=333)

```text
Table 11.4    Internal Clock Switching and FRC Operation (cont)

     Timing of Rewrite of
No.  CKS1 and CKS0 Bits        FRC Operation
3     High-to-low switch

                 Clock before
                      switching


                   Clock after
                      switching


                    FRC clock



                           FRC            N                  N + 1                     N + 2


                                                         Rewrite of CKS bit
4     High-to-high switch

                 Clock before
                      switching


                   Clock after
                      switching


                    FRC clock



                           FRC             N                         N + 1          N + 2


                                                                                 Rewrite of CKS bit
Note:   Because the switchover is considered a falling edge, FRC starts counting up.


6.   Timer Output (FTOA, FTOB)
    During a power-on reset, the timer outputs (FTOA, FTOB) will be unreliable until the
    oscillation stabilizes. The initial value is output after the oscillation settling time has elapsed.






                                                                                                                      317
```

## PDF page 334

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=334)

```text
318
```
