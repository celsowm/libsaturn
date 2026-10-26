# Section 4 Exception Handling

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=81)

Physical PDF pages 81–94. Text extraction; consult the PDF for figures.

## Bookmarks

- [4.1 Overview](#pdf-page-81)
- [4.1.1 Types of Exception Handling and Priority Order](#pdf-page-81)
- [4.1.2 Exception Handling Operations](#pdf-page-82)
- [4.1.3 Exception Vector Table](#pdf-page-83)
- [4.2 Resets](#pdf-page-85)
- [4.2.1 Types of Resets](#pdf-page-85)
- [4.2.2 Power-On Reset](#pdf-page-86)
- [4.2.3 Manual Reset](#pdf-page-86)
- [4.3 Address Errors](#pdf-page-87)
- [4.3.1 Sources of Address Errors](#pdf-page-87)
- [4.3.2 Address Error Exception Handling](#pdf-page-88)
- [4.4 Interrupts](#pdf-page-88)
- [4.4.1 Interrupt Sources](#pdf-page-88)
- [4.4.2 Interrupt Priority Levels](#pdf-page-89)
- [4.4.3 Interrupt Exception Handling](#pdf-page-89)
- [4.5 Exceptions Triggered by Instructions](#pdf-page-90)
- [4.5.1 Instruction-Triggered Exception Types](#pdf-page-90)
- [4.5.2 Trap Instructions](#pdf-page-90)
- [4.5.3 Illegal Slot Instructions](#pdf-page-90)
- [4.5.4 General Illegal Instructions](#pdf-page-91)
- [4.6 When Exception Sources are Not Accepted](#pdf-page-91)
- [4.6.1 Immediately after a Delayed Branch Instruction](#pdf-page-91)
- [4.6.2 Immediately after an Interrupt-Disabled Instruction](#pdf-page-92)
- [4.7 Stack Status after Exception Handling](#pdf-page-92)
- [4.8 Usage Notes](#pdf-page-92)
- [4.8.1 Value of Stack Pointer (SP)](#pdf-page-92)
- [4.8.2 Value of Vector Base Register (VBR)](#pdf-page-92)
- [4.8.3 Address Errors Caused by Stacking of Address Error Exception Handling](#pdf-page-93)
- [4.8.4 Manual Reset during Register Access](#pdf-page-93)

## PDF page 81

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=81)

```text
                               Section 4   Exception Handling


4.1        Overview


4.1.1        Types of Exception Handling and Priority Order

Exception handling is initiated by four sources: resets, address errors, interrupts, and instructions
(table 4.1). When several exception handling sources occur at once, they are processed according
to priority.

Table 4.1      Types of Exception Handling and Priority Order

Exception    Source                                                                                                             Priority
Reset            Power-on reset                                                                                                High
                 Manual reset
Address        CPU address error
error              DMA address error

Interrupt        NMI
                 User break
                 IRL (IRL1–IRL15 (set with IRL3, IRL2, IRL1, IRL0 pins))
                 On-chip peripheral modules        Division unit (DIVU)
                                                           Direct memory access controller (DMAC)
                                                           Watchdog timer (WDT)
                                                           Compare match interrupt (part of the bus
                                                           state controller)
                                                           Serial communication interface (SCI)
                                                           16-bit free-running timer (FRT)
Instructions   Trap instruction (TRAPA)
                 General illegal instructions (undefined code)
                 Illegal slot instructions (undefined code placed directly following a delayed                    Low
                 branch instruction*1 or instructions that rewrite the PC*2)
Notes:  1.   Delayed branch instructions: JMP, JSR, BRA, BSR, RTS, RTE, BF/S, BT/S, BSRF,
              BRAF
          2.   Instructions that rewrite the PC: JMP, JSR, BRA, BSR, RTS, RTE, BT, BF, TRAPA,
              BF/S, BT/S, BSRF, BRAF






                                                                                                                            65
```

## PDF page 82

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=82)

```text
4.1.2        Exception Handling Operations

Exception handling sources are detected, and exception handling started, according to the timing
shown in table 4.2.

Table 4.2      Timing of Exception Source Detection and Start of Exception Handling

Exception Source                         Timing of Source Detection and Start of Handling
Reset              Power-on reset        Starts when the NMI pin is high and the RES pin changes from
                                            low to high.
                  Manual reset            Starts when the NMI pin is low and the RES pin changes from
                                            low to high.
Address error                                  Detected when instruction is decoded and starts when the
                                            previous executing instruction finishes executing.
Interrupts                                        Detected when instruction is decoded and starts when the
                                            previous executing instruction finishes executing.
Instructions     Trap instruction        Starts from the execution of a TRAPA instruction.
                  General illegal           Starts from the decoding of undefined code anytime except after
                  instructions              a delayed branch instruction (delay slot).
                  Illegal slot              Starts from the decoding of undefined code placed directly
                  instructions              following a delayed branch instruction (delay slot) or of an
                                            instruction that rewrites the PC.


When exception handling starts, the CPU operates as follows:

1.   Exception handling triggered by reset
    The initial values of the program counter (PC) and stack pointer (SP) are fetched from the
    exception vector table (PC and SP are respectively addresses H'00000000 and H'00000004 for
    a power-on reset and addresses H'00000008 and H'0000000C addresses for a manual reset).
    See section 4.1.3, Exception Vector Table, for more information. 0 is then written to the vector
    base register (VBR) and 1111 is written to the interrupt mask bits (I3–I0) of the status register.
    The program begins running from the PC address fetched from the exception vector table.
2.   Exception handling triggered by address errors, interrupts, and instructions
    SR and PC are saved to the stack address indicated by R15. For interrupt exception handling,
    the interrupt priority level is written to the SR’s interrupt mask bits (I3–I0). For address error
    and instruction exception handling, the I3–I0 bits are not affected. The start address is then
    fetched from the exception vector table and the program begins running from that address.








66
```

## PDF page 83

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=83)

```text
4.1.3        Exception Vector Table

Before exception handling begins, the exception vector table must be written in memory. The
exception vector table stores the start addresses of exception service routines. (The reset exception
table holds the initial values of PC and SP.)

All exception sources are given different vector numbers and vector table address offsets, from
which the vector table addresses are calculated. During exception handling, the start addresses of
the exception service routines are fetched from the exception vector table.

Table 4.3 lists the vector numbers and vector table address offsets. Table 4.4 shows vector table
address calculations.

Table 4.3      Exception Vector Table

                                               Vector          Vector Table Address
Exception Source                               Number          Offset                                      Vector Address
Power-on reset            PC                 0                 H'00000000–H'00000003        Vector number · 4
                             SP                 1                 H'00000004–H'00000007
Manual reset               PC                 2                 H'00000008–H'0000000B
                             SP                 3                 H'0000000C–H'0000000F
General illegal instruction                 4                 H'00000010–H'00000013        VBR + (vector
(Reserved by system)                       5                 H'00000014–H'00000017        number · 4)
Slot illegal instruction                        6                 H'00000018–H'0000001B
(Reserved by system)                       7                 H'0000001C–H'0000001F
                                               8                 H'00000020–H'00000023
CPU address error                            9                 H'00000024–H'00000027
DMA address error                           10               H'00000028–H'0000002B
Interrupt                       NMI               11               H'0000002C–H'0000002F
                             User break    12               H'00000030–H'00000033
(Reserved by system)                       13                  H'00000034–H'00000037
                                               :                                  :
                                               31              H'0000007C–H'0000007F
Trap instruction (user vector)            32                   H'00000080–H'00000083
                                               :                                  :
                                               63              H'000000FC–H'000000FF






                                                                                                                               67
```

## PDF page 84

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=84)

```text
Table 4.3      Exception Processing Vector Table (cont)

                                              Vector         Vector Table Address
Exception Source                              Number         Offset                                      Vector Addresses
Interrupt                       IRL1*1           64*2             H'00000100–H'00000103        VBR + (vector
                            IRL2*1           65*2             H'00000104–H'00000107        number · 4)
                            IRL3*1
                            IRL4*1           66*2             H'00000108–H'0000010B
                            IRL5*1
                            IRL6*1           67*2             H'0000010C–H'0000010F
                            IRL7*1
                            IRL8*1           68*2             H'00000110–H'00000113
                            IRL9*1
                            IRL10*1         69*2             H'00000114–H'00000117
                            IRL11*1
                            IRL12*1         70*2             H'00000118–H'0000011B
                            IRL13*1
                            IRL14*1         71*2             H'0000011C–H'0000011F
                            IRL15*1
                            On-chip           0*4            H'00000000–H'00000003
                            peripheral        :                                :
                            module*3
                                              255*4          H'000003FC–H'000003FF
Notes:  1.   When 1110 is input to the IRL3, IRL2, IRL1, and IRL0 pins, an IRL1 interrupt results.
              When 0000 is input, an IRL15 interrupt results.
          2.   External vector number fetches can be performed without using the auto-vector
              numbers in this table.
          3.   The vector numbers and vector table address offsets for each on-chip peripheral
              module interrupt are given in section 5, Interrupt Controller, and table 5.4, Interrupt
              Exception Vectors and Priorities.
          4.   Vector numbers are set in the on-chip vector number register. See section 5.3,
              Description of Registers, section 9, Direct Memory Access Controller, and section 10,
              Division Unit, for more information.












68
```

## PDF page 85

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=85)

```text
Table 4.4      Calculating Exception Vector Table Addresses

Exception Source                    Vector Table Address Calculation
Power-on reset                           (Vector table address)   = (vector table address offset)
Manual reset                                                            = (vector number) · 4

Other exception handling          (Vector table address)   = VBR + (vector table address offset)
                                                                        = VBR + (vector number) · 4
Note:   VBR: Vector base register
         Vector table address offset: See table 4.3.
         Vector number: See table 4.3.


4.2        Resets


4.2.1        Types of Resets

Resets have the highest priority of any exception source. There are two types of resets: manual
resets and power-on resets. As table 4.5 shows, both types of resets initialize the internal status of
the CPU. In power-on resets, all registers of the on-chip peripheral modules are initialized; in
manual resets, registers of all on-chip peripheral modules except the bus state controller (BSC),
user break controller (UBC) and frequency modification register are initialized. (Use the power-on
reset when turning the power on.)

Table 4.5      Types of Resets

                                  Conditions for
                          Transition to Reset Status                               Internal Status
Type                      NMI Pin             RES Pin               CPU            On-Chip Peripheral Modules
Power-on reset      High                   Low                       Initialized     Initialized
Manual reset         Low                   Low                       Initialized     Initialized except for BSC, UBC,
                                                                                     and FMR register

















                                                                                                                             69
```

## PDF page 86

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=86)

```text
4.2.2        Power-On Reset

When the NMI pin is high and the RES pin is driven low, the device performs a power-on reset.
For a reliable reset, the RES pin should be kept low for at least the duration of the oscillation
settling time (when the PLL circuit is halted) or for 20 clock cycles (when the PLL circuit is
running). During a power-on reset, the CPU’s internal state and all on-chip peripheral module
registers are initialized. See appendix A, Pin States, for the state of individual pins in the power-on
reset state.

In a power-on reset, power-on reset exception handling starts when the NMI pin is kept high and
the RES pin is first driven low for a set period of time and then returned to high. The CPU will
then operate as follows:

1.   The initial value (execution start address) of the program counter (PC) is fetched from the
    exception vector table.
2.   The initial value of the stack pointer (SP) is fetched from the exception vector table.
3.   The vector base register (VBR) is cleared to H'00000000 and the interrupt mask bits (I3–I0) of
    the status register (SR) are set to H'F (1111).
4.   The values fetched from the exception vector table are set in the PC and SP, and the program
    begins executing.


4.2.3        Manual Reset

When the NMI pin is low and the RES pin is driven low, the device executes a manual reset. For a
reliable reset, the RES pin should be kept low for at least 20 clock cycles. During a manual reset,
the CPU’s internal state is initialized. Registers of all on-chip peripheral modules except the bus
state controller (BSC), user break controller (UBC) and the frequency modification register are
initialized. Since the BSC is not affected, the DRAM and synchronous DRAM refresh control
functions remain operational even if the manual reset state continues for a long period of time.
When the chip enters the manual reset state in the middle of a bus cycle, manual reset exception
handling does not start until the bus cycle has ended. Thus, manual resets do not abort bus cycles.
See appendix A, Pin States, for the state of individual pins in the manual reset state.

In a manual reset, manual reset exception handling starts when the NMI pin is kept low and the
RES pin is first kept low for a set period of time and then returned to high. The CPU will then
operate in the same way as for a power-on reset.











70
```

## PDF page 87

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=87)

```text
4.3        Address Errors


4.3.1        Sources of Address Errors

Address errors occur when instructions are fetched or data read or written, as shown in table 4.6.

Table 4.6      Bus Cycles and Address Errors

     Bus Cycle
              Bus
Type          Master    Bus Cycle Description                                                    Address Errors
Instruction   CPU        Instruction fetched from even address                              None (normal)
fetch                           Instruction fetched from odd address                                Address error occurs

                          Instruction fetched from other than on-chip peripheral                None (normal)
                          module space
                          Instruction fetched from on-chip peripheral module                    Address error occurs
                          space
Data            CPU or   Word data accessed from even address                          None (normal)
read/write   DMAC     Word data accessed from odd address                            Address error occurs

                          Longword data accessed from a longword boundary       None (normal)
                          Longword data accessed from other than a longword                     Address error occurs
                          boundary
                          Access of cache purge space, address array read/                      Address error occurs
                          write space or on-chip I/O space by PC-relative
                          addressing
                          Access of cache purge space, address array read/                      Address error occurs
                          write space, data array read/write space or on-chip
                          I/O space by a TAS.B instruction
                          Byte data accessed in on-chip peripheral module                       Address error occurs
                          space at addresses H'FFFFFF00 to H'FFFFFFFF
                          Word or longword data accessed in on-chip peripheral                  None (normal)
                          module space at addresses H'FFFFFF00 to
                          H'FFFFFFFF
                          Longword data accessed in on-chip peripheral module                   Address error occurs
                          space at addresses H'FFFFFE00 to H'FFFFFEFF
                          Word or byte data accessed in on-chip peripheral                      None (normal)
                          module space at addresses H'FFFFFE00 to
                          H'FFFFFEFF
Notes:  1.   Address errors do not occur during the synchronous DRAM mode register write cycle.
         2.   16-byte DMAC transfers use longword accesses.



                                                                                                                       71
```

## PDF page 88

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=88)

```text
4.3.2        Address Error Exception Handling

When an address error occurs, address error exception handling begins after the end of the bus
cycle in which the error occurred and completion of the executing instruction. The CPU operates
as follows:

1.   The status register (SR) is saved to the stack.
2.   The program counter (PC) is saved to the stack. The PC value saved is the start address of the
    instruction to be executed after the last instruction executed .
3.   The exception service routine start address is fetched from the exception vector table entry that
    corresponds to the address error that occurred, and the program starts executing from that
    address. The jump that occurs is not a delayed branch.

4.4        Interrupts


4.4.1        Interrupt Sources

Table 4.7 shows the sources that initiate interrupt exception handling. These are divided into NMI,
user breaks, IRL, and on-chip peripheral modules. Each interrupt source is allocated a different
vector number and vector table address offset. See table 5.4, Interrupt Exception Vectors and
Priority Order, in section 5, Interrupt Controller, for more information.

Table 4.7      Types of Interrupt Sources

Type                                        Request Source                                               Number of Sources
NMI                                          NMI pin (external input)                                     1
User break                               User break controller                                         1
IRL                                           IRL1–IRL15 (external input)                               15
On-chip peripheral module      Direct memory access controller (DMAC)         2
                                         Division unit (DIVU)                                           1
                                         Serial communication interface (SCI)                4
                                         Free-running timer (FRT)                                   3
                                         Watchdog timer (WDT)                                      1
                                         Bus state controller (BSC)                                 1










72
```

## PDF page 89

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=89)

```text
4.4.2        Interrupt Priority Levels

The interrupt priority order is predetermined. When multiple interrupts occur simultaneously, the
interrupt controller (INTC) determines their relative priorities and begins exception handling
accordingly.

The priority order of interrupts is expressed as priority levels 0–16, with priority 0 the lowest and
priority 16 the highest. The NMI interrupt has priority 16 and cannot be masked, so it is always
accepted. The user break interrupt priority level is 15 and IRL interrupts have priorities of 1–15.
On-chip peripheral module interrupt priority levels can be set freely using the INTC’s interrupt
priority level setting registers A and B (IPRA and IPRB) as shown in table 4.8. The priority levels
that can be set are 0–15. Level 16 cannot be set. For more information on IPRA and IPRB, see
sections 5.3.1 and 5.3.2, Interrupt Priority Level Setting Registers A and B (IPRA and IPRB).

Table 4.8      Interrupt Priority Order

Type                                      Priority Level   Comment
NMI                                        16                      Fixed priority level. Cannot be masked.
User break                             15                      Fixed priority level.
IRL                                         1–15                  Set with IRL3–IRL0 pins.
On-chip peripheral module    0–15                  Set with interrupt priority level setting registers A and
                                                           B (IPRA and IPRB).


4.4.3        Interrupt Exception Handling

When an interrupt occurs, its priority level is ascertained by the interrupt controller (INTC). NMI
is always accepted, but other interrupts are only accepted if they have a priority level higher than
the priority level set in the interrupt mask bits (I3–I0) of the status register (SR).

When an interrupt is accepted, exception handling begins. In interrupt exception handling, the
CPU saves SR and the program counter (PC) to the stack. The priority level value of the accepted
interrupt is written to SR bits I3–I0. For NMI, however, the priority level is 16, but the value set in
I3–I0 is H'F (level 15). Next, the start address of the exception service routine is fetched from the
exception vector table for the accepted interrupt, that address is jumped to and execution begins.
For more information about interrupt exception handling, see section 5.4, Interrupt Operation.











                                                                                                                              73
```

## PDF page 90

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=90)

```text
4.5        Exceptions Triggered by Instructions


4.5.1        Instruction-Triggered Exception Types

Exception handling can be triggered by a trap instruction, general illegal instruction or illegal slot
instruction, as shown in table 4.9.

Table 4.9      Types of Exceptions Triggered by Instructions

Type                      Source Instruction                      Comment
Trap instruction      TRAPA                                          —
Illegal slot           Undefined code placed                      Delayed branch instructions: JMP, JSR,
instruction            immediately after a delayed                BRA, BSR, RTS, RTE, BF/S, BT/S, BSRF,
                       branch instruction (delay slot)            BRAF
                       and instructions that rewrite the          Instructions that rewrite the PC: JMP, JSR,
                       PC                                         BRA, BSR, RTS, RTE, BT, BF, TRAPA,
                                                                  BF/S, BT/S, BSRF, BRAF
General illegal        Undefined code anywhere                    —
instruction            besides in a delay slot


4.5.2        Trap Instructions

When a TRAPA instruction is executed, trap instruction exception handling starts. The CPU
operates as follows:

1.   The status register (SR) is saved to the stack.
2.   The program counter (PC) is saved to the stack. The PC value saved is the start address of the
    instruction to be executed after the TRAPA instruction.
3.   The exception service routine start address is fetched from the exception vector table entry that
    corresponds to the vector number specified by the TRAPA instruction. That address is jumped
    to and the program starts executing. The jump that occurs is not a delayed branch.


4.5.3        Illegal Slot Instructions

An instruction placed immediately after a delayed branch instruction is said to be placed in a delay
slot. If the instruction placed in the delay slot is undefined code, illegal slot exception handling
begins when the undefined code is decoded. Illegal slot exception handling also starts up when an
instruction that rewrites the program counter (PC) is placed in a delay slot. The exception handling
starts when the instruction is decoded. The CPU handles an illegal slot instruction as follows:





74
```

## PDF page 91

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=91)

```text
1.   The status register (SR) is saved to the stack.
2.   The program counter (PC) is saved to the stack. The PC value saved is the jump address of the
    delayed branch instruction immediately before the undefined code or the instruction that
    rewrites the PC.
3.   The exception service routine start address is fetched from the exception vector table entry that
    corresponds to the exception that occurred. That address is jumped to and the program starts
    executing. The jump that occurs is not a delayed branch.


4.5.4        General Illegal Instructions

When undefined code placed anywhere other than immediately after a delayed branch instruction
(i.e., in a delay slot) is decoded, general illegal instruction exception handling starts. The CPU
handles general illegal instructions in the same way as illegal slot instructions. Unlike processing
of illegal slot instructions, however, the program counter value stored is the start address of the
undefined code.

4.6        When Exception Sources are Not Accepted

When an address error or interrupt is generated after a delayed branch instruction or interrupt-
disabled instruction, it is sometimes not immediately accepted but is stored instead, as described in
table 4.10. When this happens, it will be accepted when an instruction for which exception
acceptance is possible is decoded.

Table 4.10    Exception Source Generation Immediately after a Delayed Branch Instruction
                 or Interrupt-Disabled Instruction

                                                                                         Exception Source
Point of Occurrence                                                                 Address Error          Interrupt
Immediately after a delayed branch instruction*1                       Not accepted             Not accepted
Immediately after an interrupt-disabled instruction*2                  Accepted                   Not accepted
Notes:  1.   Delayed branch instructions: JMP, JSR, BRA, BSR, RTS, RTE, BF/S, BT/S, BSRF,
              BRAF
          2.   Interrupt-disabled instructions: LDC, LDC.L, STC, STC.L, LDS, LDS.L, STS, STS.L


4.6.1        Immediately after a Delayed Branch Instruction

When an instruction placed immediately after a delayed branch instruction (delay slot) is decoded,
neither address errors nor interrupts are accepted. The delayed branch instruction and the
instruction located immediately after it (delay slot) are always executed consecutively, so no
exception handling occurs between the two.



                                                                                                                         75
```

## PDF page 92

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=92)

```text
4.6.2        Immediately after an Interrupt-Disabled Instruction

When an instruction immediately following an interrupt-disabled instruction is decoded, interrupts
are not accepted. Address errors are accepted.

4.7        Stack Status after Exception Handling

The status of the stack after exception handling ends is as shown in table 4.11.

Table 4.11    Stack Status after Exception Handling

Type                                   Stack Status
Address error                      SP ﬁ     Address of instruction after executed instruction               32 bits
                                                 SR                                                                                     32 bits
Trap instruction                   SP ﬁ     Address of instruction after TRAPA instruction                 32 bits
                                                 SR                                                                                     32 bits
General illegal instruction   SP ﬁ     Start address of illegal instruction                                     32 bits
                                                 SR                                                                                     32 bits
Interrupt                              SP ﬁ     Address of instruction after executed instruction               32 bits
                                                 SR                                                                                     32 bits
Illegal slot instruction          SP ﬁ     Jump destination address of delayed branch instruction  32 bits
                                                 SR                                                                                     32 bits


4.8        Usage Notes


4.8.1        Value of Stack Pointer (SP)

The value of the stack pointer must always be a multiple of four, otherwise an address error will
occur when the stack is accessed during exception handling.


4.8.2        Value of Vector Base Register (VBR)

The value of the vector base register must always be a multiple of four, otherwise an address error
will occur when the vector table is accessed during exception handling.









76
```

## PDF page 93

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=93)

```text
4.8.3        Address Errors Caused by Stacking of Address Error Exception Handling

If the stack pointer value is not a multiple of four, an address error will occur during stacking of
the exception handling (interrupts, etc.). Address error exception handling will begin as soon as
the first exception handling is ended, but address errors will continue to occur. To ensure that
address error exception handling does not go into an endless loop, no address errors are accepted
at that point. This allows program control to be shifted to the address error exception service
routine and enables error handling to be carried out.

When an address error occurs during exception handling stacking, the stacking bus cycle (write) is
executed. In stacking of the status register (SR) and program counter (PC), the SP is decremented
by 4 for both, so the value of SP will not be a multiple of four after the stacking either. The
address value output during stacking is the SP value, so the address where the error occurred is
itself output. This means that the write data stacked will be undefined.


4.8.4        Manual Reset during Register Access

Do not initiate a manual reset during access of a bus state controller (BSC) or user break controller
(UBC) register, otherwise a write error may result.



































                                                                                                                         77
```

## PDF page 94

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=94)

```text
78
```
