# Section 2 Register Configuration

SH-1/SH-2 Programming Manual — Hitachi, September 3, 1996.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=20)

Physical PDF pages 20–24. Text extraction; consult the PDF for figures.

## PDF page 20

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=20)

```text
                           Section 2   Register Configuration


The register set consists of sixteen 32-bit general registers, three 32-bit control registers and
four 32-bit system registers.

2.1        General Registers

There are 16 general registers (Rn) numbered R0–R15, which are 32 bits in length (figure
2.1). General registers are used for data processing and address calculation. R0 is also used as
an index register. Several instructions use R0 as a fixed source or destination register. R15 is
used as the hardware stack pointer (SP). Saving and recovering the status register (SR) and
program counter (PC) in exception processing is accomplished by referencing the stack using
R15.


    31                                                                     0
                              R0*1                            1. R0 functions as an index register in the
                              R1                                 indirect indexed register addressing
                              R2                                 mode and indirect indexed GBR
                                                                 addressing mode.  In some instructions,
                              R3                                 R0 functions as a fixed source register
                              R4                                 or destination register.
                              R5
                              R6
                              R7
                              R8
                              R9
                             R10
                             R11
                             R12
                             R13
                             R14
                          R15, SP    (hardware stack pointer)      2.*2R15 functions as a hardware stack
                                                                 pointer (SP) during exception
                                                                 processing.

                                       Figure 2.1   General Registers

2.2        Control Registers

The 32-bit control registers consist of the 32-bit status register (SR), global base register
(GBR), and vector base register (VBR) (figure 2.2). The status register indicates processing
states. The global base register functions as a base address for the indirect GBR addressing



16 HITACHI
```

## PDF page 21

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=21)

```text
mode to transfer data to the registers of on-chip peripheral modules. The vector base register
functions as the base address of the exception processing vector area (including interrupts).






























































                                                                                                      HITACHI  17
```

## PDF page 22

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=22)

```text
      31                      9876543210
  SR                          M Q I3 I2  I1 I0    ST         SR: Status register
                                                             T bit:  The MOVT, CMP/cond, TAS, TST,
                                                                    BT (BT/S), BF (BF/S), SETT, and CLRT
                                                                    instructions use the T bit to indicate
                                                                    true (1) or false (0). The ADDV/C,
                                                                    SUBV/C, DIV0U/S, DIV1, NEGC,
                                                                    SHAR/L, SHLR/L, ROTR/L, and
                                                                    ROTCR/L instructions also use bit T
                                                                    to indicate carry/borrow or overflow/
                                                                    underflow
                                                             S bit:  Used by the multiply/accumulate
                                                                        instruction.
                                                             Reserved bits: Always reads as 0, and should
                                                             always be written with 0.
                                                             Bits I3–I0: Interrupt mask bits.
                                                             M and Q bits: Used by the DIV0U/S and
                                                                              DIV1 instructions.

      31                                                   0Global base register (GBR):
                                                             Indicates the base address of the indirect
                            GBR                              GBR addressing mode. The indirect GBR
                                                             addressing  mode is used in data transfer
                                                             for on-chip peripheral module register
                                                             areas and in logic operations.


      31                                                   0  Vector base register (VBR):
                            VBR                               Indicates the base address of the exception
                                                              processing vector area.


















                                                     SW J04

                                       Figure 2.2   Control Registers





18 HITACHI
```

## PDF page 23

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=23)

```text
2.3        System Registers

The system registers consist of four 32-bit registers: high and low multiply and accumulate
registers (MACH and MACL), the procedure register (PR), and the program counter (PC)
(figure 2.3). The multiply and accumulate registers store the results of multiply and
accumulate operations. The procedure register stores the return address from the subroutine
procedure. The program counter stores program addresses to control the flow of the processing.


              31                                             9                       0Multiply and accumulate (MAC)
(SH-1 CPU)             (sign extended)                     MACH             registers high and low (MACH/L):
                                        MACL                                Store the results of multiply and
                                                                            accumulate operations. In the
              31                                                                      0SH-1 CPU, MACH is sign-extended
(SH-2 CPU)                              MACH                                to 32 bits when read because only
                                                                            the lowest 10 bits are valid. In the
                                        MACL                                SH-2 CPU, all 32 bits of MACH are
                                                                            valid.
              31                                                        0   Procedure register (PR): Stores a
                                          PR                                return address from a subroutine
                                                                            procedure.
              31                                                        0   Program counter (PC): Indicates the
                                          PC                                fourth byte (second instruction) after
                                                                            the current instruction.

                                         Figure 2.3   System Registers

2.4        Initial Values of Registers

Table 2.1 lists the values of the registers after reset.























                                                                                                          HITACHI  19
```

## PDF page 24

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=24)

```text
Table 2.1   Initial Values of Registers

Classification      Register           Initial Value
General register      R0–R14                     Undefined
                         R15 (SP)                 Value of the stack pointer in the vector address table
Control register      SR                             Bits I3–I0 are 1111 (H'F), reserved bits are 0, and
                                                      other bits are undefined
                         GBR                       Undefined
                         VBR                        H'00000000
System register      MACH, MACL, PR     Undefined
                         PC                         Value of the program counter in the vector address
                                                      table














































20 HITACHI
```
