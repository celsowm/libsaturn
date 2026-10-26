# Section 3 Oscillator Circuits and Operating Modes

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=65)

Physical PDF pages 65–80. Text extraction; consult the PDF for figures.

## Bookmarks

- [3.1 Overview](#pdf-page-65)
- [3.2 On-Chip Clock Pulse Generator and Operating Modes](#pdf-page-65)
- [3.2.1 Clock Pulse Generator](#pdf-page-65)
- [3.2.2 Clock Operating Mode Settings](#pdf-page-67)
- [3.2.3 Connecting a Crystal Resonator](#pdf-page-68)
- [3.2.4 Inputting an External Clock](#pdf-page-70)
- [3.2.5 Selecting Operating Frequency with a Register](#pdf-page-70)
- [3.2.6 Operating Modes and Frequency Ranges](#pdf-page-76)
- [3.2.7 Notes on Board Design](#pdf-page-76)
- [3.3 Bus Width of the CS0 Area](#pdf-page-78)
- [3.4 Switching between Master Mode and Slave Mode](#pdf-page-79)

## PDF page 65

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=65)

```text
        Section 3   Oscillator Circuits and Operating Modes


3.1        Overview

Operation of the on-chip clock pulse generator, CS0 area bus width specification, and switching
between master and slave modes are controlled by the operating mode pins. A crystal resonator or
external clock can be selected as the clock source.

3.2        On-Chip Clock Pulse Generator and Operating Modes


3.2.1        Clock Pulse Generator

A block diagram of the on-chip clock pulse generator circuit is shown in figure 3.1.


                                                                                         Standby
           CAP1                                                                          control signal

           CKIO                                   PLL circuit 1                          Clock




           CAP2

          EXTAL
                                 Oscillator       PLL circuit 2
           XTAL

      CKPREQ/
            CKM

      Clock mode pins
            MD2                             Clock mode
                                            control circuit
            MD1

            MD0
      CKPACK*                            Note: See section 14.4.4, Clock Pause Function


                   Figure 3.1   Block Diagram of Clock Pulse Generator Circuit





                                                                                                           49
```

## PDF page 66

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=66)

```text
Pin Configuration: Table 3.1 lists the functions relating to the pins relating to the oscillator
circuit.

Table 3.1      Pin Functions

Pin Name            I/O       Function
CKIO                   I/O       External clock input pin or internal clock output pin
XTAL                   O         Connects to the crystal resonator.
EXTAL                 I           Connects to the crystal resonator or to the external clock input when
                                using PLL circuit 2.
CAP1                   I           Connects to capacitance for operating PLL circuit 1.
CAP2                   I           Connects to capacitance for operating PLL circuit 2.
MD0                    I           The level applied to these pins specifies the clock mode.
MD1                    I
MD2                    I
CKPREQ/CKM    I           Used as the clock pause request pin, or specifies operation of the crystal
                                oscillator.


PLL Circuit 1: PLL circuit 1 eliminates phase differences between external clocks and clocks
supplied internally within the chip. In high-speed operation, the phase difference between the
reference clocks and operating clocks in the chip directly affects the interface margin with
peripheral devices. On-chip PLL circuit 1 is provided to eliminate this effect.

PLL circuit 1 can also make the phase difference between the clocks 90 degrees, enabling high-
speed interface with synchronous DRAM.

PLL Circuit 2: PLL circuit 2 either leaves unchanged, doubles, or quadruples the frequency of
clocks provided from the crystal resonator or the EXTAL pin external clock input for the chip
operating frequency. The frequency modification register sets the clock frequency multiplication
factor.

















50
```

## PDF page 67

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=67)

```text
3.2.2        Clock Operating Mode Settings

Table 3.2 lists the functions and operation of clock modes 0 to 6.

Note that TBP-176 package products can only be used in clock modes 4 to 6.

Table 3.2      Operating Modes

Clock Mode     Function/Operation                                                                    Clock Source
0                       PLL circuits 1 and 2 operate. A clock with the same phase as                 Crystal resonator/
                    the internal chip clock is output from the CKIO pin.                             External clock input
1                       PLL circuits 1 and 2 operate. A clock shifted 90° from the                   Crystal resonator/
                    CKIO pin output is supplied to the internal chip clock.                          External clock input
2                       Only PLL circuit 2 operates. The clock from PLL circuit 2 is                 Crystal resonator/
                    output from the CKIO pin. Phases are not matched in this                         External clock input
                    mode.
3                       Only PLL circuit 2 operates. The CKIO pin is high impedance.                 Crystal resonator/
                    Phases are not matched in this mode.                                             External clock input
4                       Set this mode when the CKIO pin inputs a clock having a                      External clock input
                    frequency equivalent to the object operating frequency and
                    PLL circuit 1 synchronizes the phases of the input clock and
                    the internal clock.
5                       Set this mode when the CKIO pin inputs a clock having a                      External clock input
                    frequency equivalent to the object operating frequency and
                    PLL circuit 1 shifts the phases of the input clock and internal
                    clock by 90 degrees.
6                       Set this mode when a clock having a frequency equivalent to                  External clock input
                    that of clocks input from the CKIO pin are used. PLL circuits 1
                    and 2 do not operate.


When clock modes 0 to 3 are selected, the input frequency or its double or quadruple (produced by
PLL circuit 2) is used as the internal clock. When clock modes 4 to 6 are selected, the clock pause
function can modify the frequency of clocks input from the CKIO pin or can stop the sending of
clock signals (see section 14.4, Standby Mode). When clock modes 4 to 6 are set, PLL circuit 2
stops.

Table 3.3 lists the relationship between pins MD2 to MD0 and the clock operating mode. Do not
switch the MD2–MD0 pins while they are operating. Switching will cause operating errors.









                                                                                                                            51
```

## PDF page 68

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=68)

```text
Table 3.3      Clock Mode Pin Settings and States

                                             Pin*1
Clock                              CKPREQ/
Mode       MD2    MD1    MD0       CKM           EXTAL      XTAL         CKIO         Internal Clock
00000                                            Clock        Open        Output       Synchronized to a
                                   1             input                                 phase difference of
                                                 Crystal      Crystal                  0˚ from CKIO by PLL
                                                 oscillation  oscillation              circuit 1

10010                                            Clock        Open        Output       Synchronized to a
                                   1             input                                 phase difference of
                                                 Crystal      Crystal                  90˚ from CKIO by
                                                 oscillation  oscillation              PLL circuit 1

20100                                            Clock        Open        Output       CKIO
                                   1             input
                                                 Crystal      Crystal
                                                 oscillation  oscillation
30110                                            Clock        Open        High         PLL circuit 2 output
                                   1             input                    impe-
                                                 Crystal      Crystal     dance
                                                 oscillation  oscillation
4100*2         Open         Open         Clock                                         Synchronized to a
                                                                          input        phase difference of
                                                                                       0˚ from CKIO by PLL
                                                                                       circuit 1
5              1         0         1                             Open         Open         ClockSynchronized to a
                                                                          input        phase difference of
                                                                                       90˚ from CKIO by
                                                                                       PLL circuit 1
6              1         1         0                             Open         Open         ClockCKIO
                                                                          input
Notes:  1.   Do not use in combinations other than those listed.
         2.   In clock modes 4, 5, and 6, CKPREQ/CKM functions as the clock pause request pin.
         3.   For TBP-176 package products, only clock mode 4, 5, or 6 can be selected.


3.2.3        Connecting a Crystal Resonator

Connecting a Crystal Resonator: Figure 3.2 shows how to connect a crystal resonator. Use the
value shown in table 3.4 for the damping resistance Rd. The crystal resonator should be an AT-cut
parallel-resonance type. Be sure to connect load capacitors (CL1, CL2) as shown in the figure.




52
```

## PDF page 69

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=69)

```text
                                                            High level
              CKPREQ/CKM                                                 The CKIO pin is an output in clock
                                                                         modes 0,1, and 2. In mode 3,
                                               Output or high            it is high impedance.
                            CKIO               impedance

                                                                       CL1
                          EXTAL

                                                                       CL2
                            XTAL                                                              CL1 = CL2 = 18 pF–22 pF
                                             Rd

                             Figure 3.2   Example of Crystal Resonator Connection

Table 3.4      Damping Resistance (Reference Values)

Frequency (MHz)           4                                      6                                      8
Rd (W )                             500                                  200                                  0


Crystal Resonator: Figure 3.3 shows a crystal resonator equivalent circuit. Use a crystal
resonator that has the characteristics shown in table 3.5.


                                                              L             CL           Rs



                                EXTAL
                                                                            Co


                                Figure 3.3   Crystal Resonator Equivalent Circuit

Table 3.5      Crystal Resonator Characteristics (Reference Values)

                                                                         Frequency (MHz)
Parameters                    4                                      6                                      8
Rs max (W )                     120                                  100                                  80
Co max (pF)                                                                      7 pF max









                                                                                                                                        53
```

## PDF page 70

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=70)

```text
3.2.4        Inputting an External Clock

Input the external clock from the EXTAL pin or the CKIO pin, depending on the clock mode.

Clock Input from the EXTAL Pin: This can be used in clock modes 0,1, 2, and 3.


           CKPREQ/CKM                      Ground level           The CKIO pin is an output in clock
                                                                  modes 0,1, and 2. In mode 3,
                                      Output or high              it is high impedance.
                      CKIO            impedance


                    EXTAL                                         External clock input


                      XTAL           Open


                                  Figure 3.4   Inputting External Clock

Clock Input from the CKIO Pin: This can be used in clock modes 4, 5, and 6.



                    CKPREQ/CKM                        CKPREQ/CKM becomes the input
                                                      pin for clock pause requests

                               CKIO                                        External clock input


                             EXTAL            Open


                               XTAL           Open


                                  Figure 3.5   Inputting External Clock


3.2.5        Selecting Operating Frequency with a Register

PLL circuit 2 and the frequency modification register can double or quadruple the operating
frequency in clock modes 0 to 3. Figure 3.6 shows a block diagram of PLL circuit 2 and the
frequency modification register.








54
```

## PDF page 71

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=71)

```text
          CAP2
                                                                    PLL circuit 2
          XTAL
                           Oscillator     f        PLL     f × 4fFrequency          × 2fFrequency           × 1
                                                                      divider                divider
        EXTAL
                                                                    Frequency selection circuit


                                                                               Frequency
                                                                         modification register


                                                                                Data bus

 Figure 3.6   Relationship between PLL Circuit 2 and the Frequency Modification Register

PLL circuit 2 includes the PLL circuit (which quadruples frequency f of clocks generated by the
oscillator) and frequency dividers, which divide the output of the PLL circuit by 2 or 4. The clocks
(f · 1, f · 2, and f · 4) are input to the frequency selection circuit, where one is selected according
to the value set in the frequency modification register, and is then output from PLL circuit 2.

Frequency Modification Register: This register is only initialized by a power-on reset. It holds
its values in a manual reset and in standby mode. Table 3.6 shows the register configuration, and
the following figure shows the bit combinations and functions.

Table 3.6      Frequency Modification Register

Register                                         Abbrev.     R/W    Initial Value     Address           Access Size
Frequency modification register     FMR          R/W    H'00                  H'FFFFFE90     8


                    Bit:         7              6              5              4              3              2              1              0
            Bit name:        —            —            —            —            —            —           FR1         FR0
        Initial value:         0              0              0              0              0              0              0              0
                 R/W:         R             R             R             R             R             R           R/W         R/W


Bit 1:  FR1             Bit 0:  FR0           Description
0                            0                           No multiplication                                                    (Initial value)
                        1                              ·2 multiplication
10·4 multiplication
                        1                           Setting prohibited


                                                                                                                              55
```

## PDF page 72

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=72)

```text
Bits 2–7 are reserved. They always read 0, and the write value should always be 0.

Modifying Frequencies: In the following modifications, the device is running in clock modes 0 to
3, and the operating frequency is left unchanged, doubled, or quadrupled using PLL circuit 2.

•    Set the TME bit to 0 in or above the oscillation settling time that specifies the on-chip WDT’s
    overflow time.
•    Set the frequency modification register to the target value. (The chip will go internally to
    standby mode temporarily.)
•    All circuits involved in oscillation operate and the clock is supplied to the WDT. The WDT
    overflows with this clock.
•    When the WDT overflows, a clock at the frequency set within the chip begins to be supplied
    and the chip returns from standby mode.

Frequency Modification Guidelines:

•    Only write to the frequency modification register while the cache is disabled.
•    The frequency modification program is always in cache memory and so should be executed
    utilizing the forced access space of the data array. Figure 3.7 shows how the frequency
    modification register is set.
•    When the frequency modification program is executed, execute an associative or forced purge
    of the entries in the data array used.
•    Place at least eight consecutive NOP instructions after an instruction that writes to the
    frequency modification register.


                                                    Disable cache


                                               Transfer the frequency
                                                modification program
                                                   to the data array


                                                     Cache purge


                                               Execute the frequency
                                              modification program in
                                              data array forced space


                                                    Next program

                              Figure 3.7   Frequency Modification Flowchart


56
```

## PDF page 73

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=73)

```text
Frequency Modification Register Setting Program (Sample)

  ; DEFINE CONSTANTS
  CLR        .EQU         H'00000000
  WAIT_TIME  .EQU         H'00080000
  PURGE      .EQU         H'40000000
  ;
  MAP_ROM    .EQU         H'00000000
  MAP-IO     .EQU         H'FFFFF000
  ;
  DIRECT_RW  .EQU         H'C0000000
  MDC_FLCR   .EQU         H'FFFFFE90
  MDC_CCR    .EQU         H'FFFFFE92
  WTCSR      .EQU         H'FFFFFE80
  ;
  ; Program initialization
             MOV.L        #CLOCK2_START,R0
             MOV.L        #DIRECT_RW,R1
             MOV.L        #CLOCK2_END,R11
  ; Cache_CCR save, disable, and forced purge
             MOV.L        #MDC_CCR,R2
             MOV.B        @R2,R6
             MOV          #H'00,R3       ;Disable setting
             MOV.B        R3,@R2
             MOV.B        @R2,R3         ;Dummy read
             MOV          #H'10,R3       ;Forced purge setting
             MOV.B        R3,@R2
  ;
  ; Transfer frequency modification program to the data array
  ;
  PRG_TRNS
  ; Read from the main memory
             MOV.L        @R0,R2
             MOV.L        @(4,R0),R3*1
             MOV.L        @(8,R0),R4*1
             MOV.L        @(12,R0),R5*1
  ; Write to the data array
             MOV.L       R2,@R1
                                                                                          57
```

## PDF page 74

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=74)

```text
             MOV.L       R3,@(4,R1)*1
             MOV.L       R4,@(8,R1)*1
             MOV.L       R5,@(12,R1)*1
  ; Increment pointer
             ADD         #H'10,R0
             ADD         #H'10,R1
  ; Loop
             CMP/GT      R11,R0
             BF          PRG_TRNS
  ; Branch to the data array forced access space
             MOV.L       #DIRECT_RW,R0
             JMP         @R0
             NOP
             .CONST*2
  ;
  CLOCK2_START
             MOV.L        #NEXT_PROG,R3   ;Branch destination address
                                           for the next program


             MOV.L        #WTCSR,R0       ;WDT setting
             MOV.L        #H'0000A507,R1  ;Set enough time for PLL to be
                                           stabilized
             MOV.W        R1,@R0


             MOV.L        #MDC_FLCR,R2     ;Frequency modification
                                            register setting
             MOV          #H'01,R1         ;Double the internal frequency
             MOV.B        R1,@R2
  ; Wait during frequency modification
             NOP
             NOP
             NOP
             NOP
             NOP
             NOP
             NOP
             NOP

58
```

## PDF page 75

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=75)

```text
             NOP
             NOP
             NOP
             NOP
             NOP
             NOP
             NOP
  ; Branch to the next program
             JMP          @R3
             NOP
             .CONST*2
  CLOCK2_END


  ;
  ; Next program
  NEXT_PROG


  ; Cache_CCR load
             MOV.L        #MDC_CCR,R2
             MOV.B        R6,@R2
              .
              .
              .
Notes:  *1  This example shows Hitachi cross-assembler coding. With the Hitachi cross-assembler,
            the values to which scaling (·1, ·2, ·4) is applied are written. For coding in other cross-
            assemblers, see the notation rules.
        *2  This is a literal pool output control statement. Code according to the notation rule for the
            assembler used. The Hitachi cross-assembler is not required.
        *3  When the WTCSR set value is read, H'0000A51F’ is returned.
















                                                                                                         59
```

## PDF page 76

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=76)

```text
3.2.6        Operating Modes and Frequency Ranges

Table 3.7 shows the operating modes and the associated frequency ranges for input clocks.

Table 3.7      Operating Modes and Frequency Ranges

                                                                                Internal          CKIO
            PLL Circuit                      Clock Input                        Clock             Output
                                                      Input Frequency           Frequency         Frequency
Mode   PLL1   PLL2     Pin                            Range (MHz)               (MHz)             (MHz)           Comments
0          Active  Active    EXTAL               4–8*1                      4– max*2      7–max*2       Multiplication
1                                    (including                                                                                   ratio settable
2          Halted               when resonator                                                    4– max*2      (·1, ·2, ·4)
3                                    is used)
4          Active  Halted   CKIO                 7–max*2                 7–max*3                            Multiplication
5                                                                                                                                     not settable
6          Halted                                         4–max*2                 4–max*2                            (·1 only)
Notes:  1.   Make the setting so that the clock frequency output from the CKIO pin does not exceed
               the maximum operating frequency. For example, if 8 MHz is input, set the multiplication
               ratio of the frequency to ·1 or ·2. (If ·4 is used, a 32-MHz clock is output from the CKIO
               pin, which is outside the range for the PLL circuit 1 input frequency.)
           2.   “max” represents the maximum operating frequency of 28.7 MHz at 5 V operation and
               20.0 MHz at 3.3 V operation.
           3.   If CKIO output below 7 MHz is used during PLL circuit 1 operation, AC characteristics
               using the CKIO output are not guaranteed.


3.2.7        Notes on Board Design

When Using a Crystal Resonator: Place the crystal resonator and capacitors as close to the
EXTAL and XTAL pins as possible. Do not let the pins’ signal lines cross other signal lines. If
they do, induction may prevent proper oscillation.
















60
```

## PDF page 77

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=77)

```text
                                           CL1                     CL2      CL1 = CL2 = 18 pF–22 pF




           No crossing of signal lines



                                              EXTAL     XTAL


                                                   SH7604



               Figure 3.8   Design Considerations when Using a Crystal Resonator

When Using PLL Oscillation Circuits: Place oscillation settling capacitors C1 and C2 and
resistors R1 and R2 near the CAP1 and CAP2 pins, and keep the wiring from the CAP pins as
short as possible.  As the CAP pin circuits are susceptible to influence by other signals, avoid
crossing signal lines both on the board surface and in internal layers.  PLL-VCC and PLL-VSS
should be isolated from other VCC and VSS lines away from the board's power supply sources, and
bypass capacitors CPB and CB must be inserted near the pins.

In the clock circuits in this product, clock stability may be affected by reflected noise generated by
the CKIO pin.  This influence is especially great in clock modes 0 and 1, in which the PLL1 and
PLL2 circuits are used simultaneously, so the board design should ensure that reflected noise does
not occur in CKIO.  In clock mode 6, in which no PLLs are used, connect PLL-VCC to VCC and
PLL-VSS to VSS.

Table 3.8      Connected Resistance and Capacitance Reference Values

                                                                Mode Setting
Resistance/Capacitance0123456
R1 = 3 kW                       Needed   Needed   Not             Not         Needed   Needed   Not
C1 = 470 pF                                            needed     needed                             needed
R2 = 3 kW                       Needed   Needed   Needed   Needed   Not                  Not         Not
C2 = 470 pF                                                                   needed     needed      needed


When the PLL circuits are off, CAP1 and CAP2 should be left open or used as shown in the
recommended example.







                                                                                                              61
```

## PDF page 78

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=78)

```text
                                                         No crossing of signal lines
                                                         (do not cross signal lines in
                                                         adjacent layers on the board)

                                                    CAP1
                                                                 R2   C2

                                                    CAP2
                                                                 R2   C2
                                        SH7604
                                                                                Rp
                                                PLL–VCC
                                                                     CPB
                                                PLL–VSS

                                                      VCC
                                                                     CB
                                                       VSS


    Note:      CPB/CB: 0.1 mF (laminated ceramic)
                Rp: 300 W  resistance (recommended value)
              Figure 3.9   Design Considerations when Using PLL Oscillation Circuits

3.3        Bus Width of the CS0 Area

Pins MD3 and MD4 are used to specify the bus width of the CS0 area (boot ROM area). The pin
combination and functions are listed in table 3.9. Do not switch the MD4 and MD3 pins while
they are operating. Switching them will cause operating errors.

Table 3.9      Bus Width of the CS0 Area

                     Pin
MD4                     MD3                     Function
0                           0                           8-bit bus width selected
0                           1                           16-bit bus width selected
1                           0                           32-bit bus width selected
1                           1                           Setting prohibited









62
```

## PDF page 79

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=79)

```text
3.4        Switching between Master Mode and Slave Mode

The SH7604 has two master modes and a slave mode for bus rights that can be selected with the
MD5 pin. The master modes consist of a total master mode and a partial-share master mode,
which are specified using the MD5 pin and the partial-share space specification bit (PSHR) in bus
control register 1 (BCR1). When the slave mode is selected with the MD5 pin, the device enters
total slave mode. When master mode is selected with the MD5 pin and partial-space share is
specified with the PSHR bit, the device enters partial-share master mode. When partial-space
share is not specified with the PSHR bit, the device enters total master mode.

Total master mode has rights to bus use. External devices can be accessed freely. The bus can be
allocated to another CPU upon request.

Total slave mode does not have any rights to bus use. When an external device is accessed, bus
rights have to be requested from the master CPU, and permission to use the bus gained, before the
external device can be accessed.

Partial-share master mode lacks bus rights only for the CS2 space. To access the CS2 space, bus
rights must first be requested from the master CPU, and permission granted. This mode has bus
rights for all other spaces and does not require a request for the bus when accessing them (table
3.10). Do not change MD5 while external device accesses are in progress.

Table 3.10    Master Modes and Slave Mode

                   MD5                      PSHR
                   Total Slave Mode         Partial-Share
Mode               Specification Pin        Bit                    Function
Total slave        1                              (Not used)        Has no bus rights. To use the bus, it must
mode                                                            request the bus and receive permission from
                                                                the master CPU.
Partial-share      0                              1                       Has bus rights to CS0, CS1, and CS3 spaces,
master mode                                                     but not normally to CS2. To access CS2, it
                                                                must first request and be granted bus rights.
Total master       0                              0                       Always has bus rights. Grants bus rights to
mode                                                            slave CPUs.














                                                                                                                        63
```

## PDF page 80

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=80)

```text
64
```
