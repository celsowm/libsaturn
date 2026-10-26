# Section 2 CPU

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=31)

Physical PDF pages 31–64. Text extraction; consult the PDF for figures.

## Bookmarks

- [2.1 Register Configuration](#pdf-page-31)
- [2.1.1 General Registers](#pdf-page-31)
- [2.1.2 Control Registers](#pdf-page-32)
- [2.1.3 System Registers](#pdf-page-33)
- [2.1.4 Initial Values of Registers](#pdf-page-33)
- [2.2 Data Formats](#pdf-page-34)
- [2.2.1 Data Format in Registers](#pdf-page-34)
- [2.2.2 Data Format in Memory](#pdf-page-34)
- [2.2.3 Immediate Data Format](#pdf-page-35)
- [2.3 Instruction Features](#pdf-page-35)
- [2.3.1 RISC-Type Instruction Set](#pdf-page-35)
- [2.3.2 Addressing Modes](#pdf-page-38)
- [2.3.3 Instruction Formats](#pdf-page-41)
- [2.4 Instruction Set](#pdf-page-45)
- [2.4.1 Instruction Set by Classification](#pdf-page-45)
- [2.4.2 Operation Code Map](#pdf-page-58)
- [2.5 Processing States](#pdf-page-60)
- [2.5.1 State Transitions](#pdf-page-60)
- [2.5.2 Power-Down State](#pdf-page-62)

## PDF page 31

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=31)

```text
                                            Section 2   CPU


2.1        Register Configuration

The register set consists of sixteen 32-bit general registers, three 32-bit control registers and four
32-bit system registers.


2.1.1        General Registers

The 16 general registers (R0–R15) are shown in figure 2.1. General registers are used for data
processing and address calculation. R0 is also used as an index register, and several instructions
use R0 as a fixed source or destination register. R15 is used as the hardware stack pointer (SP).
Saving and recovering the status register (SR) and program counter (PC) in exception handling is
accomplished by referencing the stack using R15.


                                    31                                         0
                                                         R0                       *1
                                                         R1
                                                         R2
                                                         R3
                                                         R4
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
                                       R15, SP (hardware stack pointer)           *2

  Notes:    1.   R0 functions as an index register in the indirect indexed register addressing mode
                 and indirect indexed GBR addressing mode. In some instructions, R0 functions as a
                 fixed source register or destination register.
             2.   R15 functions as a hardware stack pointer (SP) during exception handling.
                                        Figure 2.1   General Registers




                                                                                                                     15
```

## PDF page 32

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=32)

```text
2.1.2        Control Registers

The 32-bit control registers consist of the 32-bit status register (SR), global base register (GBR),
and vector base register (VBR) (figure 2.2). The status register indicates processing states. The
global base register functions as a base address for the GBR indirect addressing mode to transfer
data to the registers of on-chip peripheral modules. The vector base register functions as the base
address of the exception handling vector area (including interrupts).


       31                   9876543210
    SR                     MQI3 I2  I1 I0    ST            SR: Status register

                                                           T bit: The MOVT, CMP/cond, TAS, TST,
                                                                 BT (BT/S), BF (BF/S), SETT, and CLRT
                                                                 instructions use the T bit to indicate
                                                                 true (1) or false (0). The ADDV, ADDC,
                                                                 SUBV, SUBC, DIV0U, DIV0S, DIV1,
                                                                 NEGC, SHAR, SHAL, SHLR, SHLL,
                                                                 ROTR, ROTL, ROTCR, and ROTCL
                                                                 instructions also use the T bit to indicate
                                                                 carry/borrow or overflow/underflow.
                                                           S bit: Used by the MAC instruction.
                                                           Reserved bits. 0 is read, and only 0 must be
                                                           written.
                                                           Bits I0–I3: Interrupt mask bits.
                                                           M and Q bits: Used by the DIV0U, DIV0S,
                                                                            and  DIV1 instructions.

       31                                               0  Global base register (GBR):
                             GBR                           Indicates the base address of the indirect
                                                           GBR addressing mode. The indirect GBR
                                                           addressing mode is used in data transfer
                                                           for on-chip peripheral module register
                                                           areas and in logic operations.

       31                                               0  Vector base register (VBR):
                              VBR                          Indicates the base address of the exception
                                                           handling vector area.

                                        Figure 2.2   Control Registers











16
```

## PDF page 33

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=33)

```text
2.1.3        System Registers

System registers consist of four 32-bit registers: high and low multiply-and-accumulate registers
(MACH and MACL), the procedure register (PR), and the program counter (PC) (figure 2.3). The
multiply-and-accumulate registers store the results of multiply and accumulate operations. The
procedure register stores the return address from the subroutine procedure. The program counter
stores program addresses to control the flow of the processing.


        31                                                                      0Multiply and accumulate (MAC)
                                   MACH                                   registers high and low (MACH,
                                   MACL                                   MACL): Store the results of
                                                                          multiply-and-accumulate operations.

        31                                                           0    Procedure register (PR): Stores
                                     PR                                   a return address from a
                                                                          subroutine procedure.

        31                                                           0    Program counter (PC): Indicates
                                     PC                                   the fourth byte (second instruction)
                                                                          after the current instruction.

                                            Figure 2.3   System Registers


2.1.4        Initial Values of Registers

Table 2.1 lists the values of the registers after a reset.

Table 2.1      Initial Values of Registers

Classification         Register                      Initial Value
General registers     R0–R14                       Undefined
                          R15A (SP)                   Value of the stack pointer in the vector address table
Control registers      SR                               Bits I3–I0 are 1111 (H'F), reserved bits are 0, and other
                                                        bits are undefined
                          GBR                            Undefined
                          VBR                             H'00000000
System registers      MACH, MACL, PR      Undefined
                          PC                               Value of the program counter in the vector address table







                                                                                                                              17
```

## PDF page 34

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=34)

```text
2.2        Data Formats


2.2.1        Data Format in Registers

Register operands are always longwords (32 bits) (figure 2.4). When the memory operand is only
a byte (8 bits) or a word (16 bits), it is sign-extended into a longword when loaded into a register.


                                 31                                                                      0
                                                          Longword

                                          Figure 2.4   Longword Operand


2.2.2        Data Format in Memory

Memory data formats are classified into bytes, words, and longwords. Byte data can be accessed at
any address, but an address error will occur if you try to access word data starting from an address
other than 2n or longword data starting from an address other than 4n. In such cases, the data
accessed cannot be guaranteed (figure 2.5). The hardware stack area, referred to by the hardware
stack pointer (SP, R15), uses only longword data starting from address 4n because this area holds
the program counter and status register.

This microprocessor has a function that allows access of CS2 space (area 2) in little-endian format,
which enables the microprocessor to share memory with processors that access memory in little-
endian format. The microprocessor arranges byte data differently for little-endian and the more
usual big-endian format.


             Address m + 1      Address m + 3                                       Address m + 2         Address m
   Address m             Address m + 2                                  Address m + 3           Address m + 1
     31                                                    023                          71531                                                   023                          715
       Byte        Byte        Byte        Byte                                Byte        Byte        Byte        Byte
             Word                     Word               Address 2n                 Word                     Word
                      Longword                           Address 4n                          Longword


                 Big-endian format                                                          Little endian format

                             Figure 2.5   Byte, Word, and Longword Alignment








18
```

## PDF page 35

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=35)

```text
2.2.3        Immediate Data Format

Byte (8-bit) immediate data resides in an instruction code. Immediate data accessed by the MOV,
ADD, and CMP/EQ instructions is sign-extended and handled in registers as longword data.
Immediate data accessed by the TST, AND, OR, and XOR instructions is zero-extended and
handled as longword data. Consequently, AND instructions with immediate data always clear the
upper 24 bits of the destination register.

Word or longword immediate data is not located in the instruction code: it is stored in a memory
table. An immediate data transfer instruction (MOV) accesses the memory table using the PC
relative with displacement addressing mode. Specific examples are given in table 2.5, Immediate
Data Accessing.

2.3        Instruction Features


2.3.1        RISC-Type Instruction Set

All instructions are RISC type. This section details their functions.

16-Bit Fixed Length: All instructions are 16 bits long, increasing program code efficiency.

One Instruction per Cycle: The microprocessor can execute basic instructions in one cycle using
the pipeline system. Instructions are executed in 35 ns at 28.7 MHz.

Data Length: Longword is the standard data length for all operations. Memory can be accessed in
bytes, words, or longwords. Byte or word data accessed from memory is sign-extended and
handled as longword data (table 2.2). Immediate data is sign-extended for arithmetic operations or
zero-extended for logic operations. It also is handled as longword data.

Table 2.2      Sign Extension of Word Data

SH7604 CPU                             Description                              Example of Conventional CPU
MOV.W  @(disp,PC),R1                  Data is sign-extended to 32          ADD.W     #H'1234,R0
ADD    R1,R0                          bits, and R1 becomes
    .........                         H'00001234. It is next
                                      operated upon by an ADD
.DATA.W      H'1234                   instruction.
Note:  @(disp, PC) accesses the immediate data.


Load-Store Architecture: Basic operations are executed between registers. For operations that
involve memory access, data is loaded into the registers and executed (load-store architecture).
Instructions such as AND that manipulate bits, however, are executed directly in memory.



                                                                                                                    19
```

## PDF page 36

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=36)

```text
Delayed Branch Instructions: Unconditional branch instructions are delayed. Executing the
instruction that follows the branch instruction, before branching reduces pipeline disruption during
branching (table 2.3).

Table 2.3      Delayed Branch Instructions

SH7604 Series CPU                 Description                               Example of Conventional CPU
BRA    TRGET                         Executes ADD before                   ADD.W    R1,R0
ADD    R1,R0                         branching to TRGET                    BRA      TRGET


Multiply and Multiply-and-Accumulate Operations: 16-bit · 16-bit ﬁ   32-bit multiply
operations are executed in one to two states. 16-bit · 16-bit + 64-bit ﬁ   64-bit multiply-and-
accumulate operations are executed in two to three states. 32-bit · 32-bit ﬁ   64-bit multiply and
32-bit · 32-bit + 64bit ﬁ   64-bit multiply-and-accumulate operations are executed in two to four
states.

T Bit: The T bit in the status register changes according to the result of the comparison, and in
turn is the condition (true/false) that determines if the program will branch (table 2.4). The number
of instructions that change the T bit is kept to a minimum to improve the processing speed.

Table 2.4      T Bit

SH7604 CPU                         Description                                     Example of Conventional CPU
CMP/GE   R1,R0                     T bit is set when R0 ‡ R1. The             CMP.W    R1,R0
BT       TRGET0                    program branches to TRGET0                 BGE      TRGET0
BF       TRGET1                    when R0 ‡ R1 and to TRGET1                 BLT      TRGET1
                                   when R0 < R1.
ADD      #–1,R0                    T bit is not changed by ADD.               SUB.W    #1,R0
CMP/EQ   #0,R0                     T bit is set when R0 = 0.                  BEQ      TRGET
BT       TRGET                     The program branches if R0 = 0.


Immediate Data: Byte (8-bit) immediate data resides in the instruction code. Word or longword
immediate data is not input via instruction codes but is stored in a memory table. An immediate
data transfer instruction (MOV) accesses the memory table using the PC relative with
displacement addressing mode (table 2.5).











20
```

## PDF page 37

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=37)

```text
Table 2.5      Immediate Data Accessing

Classification         SH7604 CPU                                           Example of Conventional CPU
8-bit immediate    MOV      #H'12,R0             MOV.B  #H'12,R0
16-bit immediate   MOV.W    @(disp,PC),R0                             MOV.W  #H'1234,R0
                          .................
                       .DATA.W  H'1234
32-bit immediate   MOV.L    @(disp,PC),R0                             MOV.L  #H'12345678,R0
                          .................
                       .DATA.L  H'12345678
Note:  @(disp, PC) accesses the immediate data.


Absolute Address: When data is accessed by absolute address, the absolute address value is
placed in the memory table beforehand. Loading the immediate data when the instruction is
executed transfers that value to the register and the data is accessed in the register indirect
addressing mode (table 2.6).

Table 2.6      Absolute Address Accessing

Classification             SH7604 CPU                                           Example of Conventional CPU
Absolute address    MOV.L    @(disp,PC),R1                              MOV.B   @H'12345678,R0
                         MOV.B    @R1,R0
                             ..................
                         .DATA.L   H'12345678
Note:  @(disp,PC) accesses the immediate data.


16-Bit/32-Bit Displacement: When data is accessed by 16-bit or 32-bit displacement, the
displacement value is placed in the memory table beforehand. Loading the immediate data when
the instruction is executed transfers that value to the register and the data is accessed in the
indexed register indirect addressing addressing mode (table 2.7).

Table 2.7      16/32-Bit Displacement Accessing

Classification             SH7604 CPU                                        Example of Conventional CPU
16-bit displacement  MOV.W     @(disp,PC),R0                          MOV.W   @(H'1234,R1),R2
                         MOV.W     @(R0,R1),R2
                            ..................
                         .DATA.W   H'1234
Note:  @(disp,PC) accesses the immediate data.





                                                                                                               21
```

## PDF page 38

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=38)

```text
2.3.2        Addressing Modes

Table 2.8 shows addressing modes and effective address calculation.

Table 2.8      Addressing Modes and Effective Addresses

Addressing            Instruction
Mode                  Format         Effective Addresses Calculation                             Equation
Register direct    Rn                 The effective address is register Rn. (The operand                   —
                                       is the contents of register Rn.)
Register              @Rn             The effective address is the contents of register                    Rn
indirect                               Rn.

                                               Rn                                            Rn

Register              @Rn+           The effective address is the contents of register                     Rn
indirect with                          Rn.                                                                 (After the
post-increment                         A constant is added to the contents of Rn after the                 instruction
                                       instruction is executed. 1 is added for a byte                      executes)
                                       operation, 2 for a word operation, and 4 for a
                                       longword operation.                                                 Byte: Rn + 1
                                                                                                           ﬁ   Rn
                                               Rn                                            Rn            Word: Rn + 2
                                                    Rn + 1/2/4      +                                      ﬁ   Rn
                                                                                                           Longword: Rn
                                              1/2/4                                                        + 4 ﬁ   Rn

Register              @–Rn           The effective address is the value obtained by                        Byte: Rn – 1
indirect with                          subtracting a constant from Rn. 1 is subtracted for                 ﬁ   Rn
pre-decrement                          a byte operation, 2 for a word operation, and 4 for                 Word: Rn – 2
                                       a longword operation.                                               ﬁ   Rn

                                               Rn                                                          Longword:
                                                    Rn – 1/2/4                                             Rn – 4 ﬁ   Rn
                                                                    –             Rn – 1/2/4               (Instruction
                                                                                                           executed with
                                              1/2/4                                                        Rn after
                                                                                                           calculation)















22
```

## PDF page 39

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=39)

```text
Table 2.8      Addressing Modes and Effective Addresses (cont)

Addressing          Instruction
Mode                Format         Effective Addresses Calculation                             Equation
Register            @(disp:4,       The effective address is Rn plus a 4-bit                        Byte: Rn +
indirect with       Rn)             displacement (disp). The value of disp is zero-                 disp
displacement                        extended, and remains the same for a byte                       Word: Rn +
                                    operation, is doubled for a word operation, and is              disp · 2
                                    quadrupled for a longword operation.
                                                                                                    Longword:
                                             Rn                                                     Rn + disp · 4

                                            disp                 +        Rn + disp × 1/2/4
                                     (zero-extended)
                                                            ×

                                            1/2/4

Indexed             @(R0, Rn)    The effective address is the Rn value plus R0.                     Rn + R0
register indirect
                                            Rn

                                                                +            Rn + R0

                                            R0

GBR indirect        @(disp:8,       The effective address is the GBR value plus an                  Byte: GBR +
with                GBR)            8-bit displacement (disp). The value of disp is                 disp
displacement                        zero-extended, and remains the same for a byte                  Word: GBR +
                                    opera-tion, is doubled for a word operation, and is             disp · 2
                                    quadrupled for a longword operation.                            Longword:

                                            GBR                                                     GBR + disp ·
                                                                                GBR                 4
                                            disp                   +       + disp × 1/2/4
                                     (zero-extended)
                                                            ×

                                            1/2/4

Indexed GBR         @(R0,           The effective address is the GBR value plus the                 GBR + R0
indirect            GBR)            R0 value.

                                           GBR

                                                                +           GBR + R0

                                            R0




                                                                                                                   23
```

## PDF page 40

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=40)

```text
Table 2.8      Addressing Modes and Effective Addresses (cont)

Addressing           Instruction
Mode                 Format         Effective Addresses Calculation                             Equation
PC relative          @(disp:8,       The effective address is the PC value plus an 8-bit             Word: PC +
with                 PC)             displacement (disp). The value of disp is zero-                 disp · 2
displacement                         extended, and remains the same for a byte                       Longword:
                                     operation, is doubled for a word operation, and is              PC &
                                     quadrupled for a longword operation. For a                      H'FFFFFFFC
                                     longword operation, the lowest two bits of the PC               + disp · 4
                                     value are masked.

                                              PC

                                                             &  (for longword operation)
                                                                              PC + disp × 2
                                        H'FFFFFFFC                 +                 or
                                                                           PC & H'FFFFFFFC
                                             disp                                + disp × 4
                                      (zero-extended)
                                                             ×

                                              2/4

PC relative          disp:8            The effective address is the PC value sign-                   PC + disp · 2
                                     extended with an 8-bit displacement (disp),
                                     doubled, and added to the PC value.

                                             PC

                                             disp                   +       PC + disp × 2
                                     (zero-extended)
                                                             ×

                                              2

                     disp:12          The effective address is the PC value sign-                    PC + disp · 2
                                     extended with a 12-bit displacement (disp),
                                     doubled, and added to the PC value.

                                             PC

                                             disp                   +       PC + disp × 2
                                     (zero-extended)
                                                             ×

                                              2





24
```

## PDF page 41

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=41)

```text
Table 2.8      Addressing Modes and Effective Addresses (cont)

Addressing           Instruction
Mode                 Format         Effective Addresses Calculation                             Equation
PC relative          Rn                 The effective address is the register PC value                 PC + Rn
(cont)                                plus Rn.

                                             PC

                                                                  +             PC + Rn

                                              Rn

Immediate           #imm:8          The 8-bit immediate data (imm) for the TST,                        —
                                      AND, OR, and XOR instructions is zero-extended.
                     #imm:8          The 8-bit immediate data (imm) for the MOV,                       —
                                      ADD, and CMP/EQ instructions is sign-extended.
                     #imm:8          The 8-bit immediate data (imm) for the TRAPA                      —
                                      instruction is zero-extended and quadrupled.


2.3.3        Instruction Formats

Table 2.9 shows instruction formats and source and destination operands. The meaning of the
operands depends on the instruction code. The following symbols are used in the table:

•    xxxx: Instruction code
•    mmmm: Source register
•    nnnn: Destination register
•    iiii: Immediate data
•    dddd: Displacement




















                                                                                                                       25
```

## PDF page 42

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=42)

```text
Table 2.9      Instruction Formats

                                                                       Destination
Instruction Format                            Source Operand           Operand                  Example
0 format                                     ——NOP
15                                               0
    xxxx                  xxxx    xxxxxxxx

n format                                     —              nnnn: Register                      MOVT  Rn
15                                               0                     direct
    xxxx                  xxxx    xxxxnnnn

                                             Control register or       nnnn: Register           STS   MACH,Rn
                                             system register           direct
                                             Control register or       nnnn: Register           STC.L  SR,@-Rn
                                             system register           indirect with
                                                                       pre-decrement
m format                                     mmmm: Register            Control register or      LDC    Rm,SR
15                                               0direct               system register
   xxxx                              xxxxmmmmxxxx

                                             mmmm: Register            Control register or      LDC.L  @Rm+,SR
                                             indirect with             system register
                                             post-increment
                                             mmmm: Register            —             JMP   @Rm
                                             indirect
                                             mmmm: PC relative         —             BRAF  Rm
                                             using Rm

























26
```

## PDF page 43

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=43)

```text
Table 2.9      Instruction Formats (cont)

                                                                         Destination
Instruction Format                            Source Operand             Operand                  Example
nm format                    mmmm: Register                              nnnn: Register            ADD    Rm,Rn
                                               direct                    direct


15                                               0mmmm: Register         nnnn: Register            MOV.L  Rm,@Rn
   xxxx                              xxxxnnnnmmmmdirect                  indirect

                                               mmmm: Register            MACH, MACL     MAC.W
                                               indirect with post-                                 @Rm+,@Rn+
                                               increment (multiply-
                                               and-accumulate)
                                               nnnn: Register
                                               indirect with post-
                                               increment (multiply-
                                               and-accumulate)*
                                               mmmm: Register            nnnn: Register            MOV.L  @Rm+,Rn
                                               indirect with             direct
                                               post-increment
                                               mmmm: Register            nnnn: Register            MOV.L  Rm,@-Rn
                                               direct                    indirect with
                                                                         pre-decrement
                                               mmmm: Register            nnnn: Indexed             MOV.L
                                               direct                    register indirect         Rm,@(R0,Rn)

md format                                      mmmmdddd:                 R0 (Register              MOV.B
15                                               0Register indirect      direct)                   @(disp,Rn),R0
   xxxx                              ddddxxxxmmmmwith displacement

nd4 format                                     R0 (Register direct) nnnndddd:                      MOV.B
15                                               0                       Register indirect         R0,@(disp,Rn)
   xxxx                              ddddxxxxnnnn                        with displacement

nmd format                                     mmmm: Register            nnnndddd:                 MOV.L
15                                               0direct                 Register indirect         Rm,@(disp,Rn)
   xxxx                              ddddnnnnmmmm                        with displacement

                                               mmmmdddd:                 nnnn: Register            MOV.L
                                               Register indirect         direct                    @(disp,Rm),Rn
                                               with displacement







                                                                                                                         27
```

## PDF page 44

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=44)

```text
Table 2.9      Instruction Formats (cont)

                                                                        Destination
Instruction Format                            Source Operand            Operand                  Example
d format                                      dddddddd: GBR             R0 (Register             MOV.L
15                                               0indirect with         direct)                  @(disp,GBR),R0
   xxxx      xxxx                 dddddddd    displacement

                                              R0 (Register direct) dddddddd: GBR                 MOV.L
                                                                        indirect with            R0,@(disp,GBR)
                                                                        displacement
                                              dddddddd: PC              R0 (Register             MOVA
                                              relative with             direct)                  @(disp,PC),R0
                                              displacement
                                              dddddddd: PC              —             BF     label
                                              relative
d12 format                                    dddddddddddd:             —             BRA    label
15                                               0PC relative
   xxxx      dddd     dddd     dddd                                                              (label = disp + PC)

nd8 format                                    dddddddd: PC              nnnn: Register           MOV.L
15                                               0relative with         direct                   @(disp,PC),Rn
   xxxx      nnnn     dddd      dddd          displacement

i format                      iiiiiiii:                                 Indexed GBR              AND.B
                                              Immediate                 indirect                 #imm,@(R0,GBR)


15                                               0iiiiiiii:             R0 (Register             AND    #imm,R0
   xxxx                              i i i ixxxxi i i iImmediate        direct)

                                              iiiiiiii:                 —             TRAPA  #imm
                                              Immediate
ni format                                     iiiiiiii:                 nnnn: Register           ADD    #imm,Rn
15                                               0Immediate             direct
   xxxx      nnnn     i i i i   i i i i

Note:   In multiply-and-accumulate instructions, nnnn is the source register.













28
```

## PDF page 45

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=45)

```text
2.4        Instruction Set


2.4.1        Instruction Set by Classification

Table 2.10    Instruction Set by Classification

                                Operation                                                                       Number of
Classification   Types          Code           Function                                                         Instructions
Data transfer     5           MOV            Data transfer, immediate data transfer,                            39
                                                peripheral module data transfer, structure data
                                                transfer
                                MOVA         Effective address transfer
                                MOVT          T bit transfer
                                SWAP         Swap of upper and lower bytes
                                XTRCT        Extraction of middle of connected registers
Arithmetic          21         ADD            Binary addition                                                      33
operations                      ADDC          Binary addition with carry

                                ADDV          Binary addition with overflow check
                                CMP/cond   Comparison
                                DIV1            Division
                                DIV0S          Initialization of signed division
                                DIV0U         Initialization of unsigned division
                                DMULS       Signed double-length multiplication
                                DMULU       Unsigned double-length multiplication
                                DT               Decrement and test
                                EXTS           Sign extension
                                EXTU          Zero extension
                                MAC            Multiply-and-accumulate, double-length multiply-
                                                and-accumulate operation
                                MUL            Double-length multiplication
                                MULS          Signed multiplication
                                MULU          Unsigned multiplication
                                NEG            Negation
                                NEGC          Negation with borrow
                                SUB             Binary subtraction
                                SUBC          Binary subtraction with borrow
                                SUBV          Binary subtraction with underflow check

                                                                                                                               29
```

## PDF page 46

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=46)

```text
Table 2.10    Instruction Set by Classification (cont)

                                  Operation                                                                            Number of
Classification   Types            Code           Function                                                              Instructions
Logic                 6           AND            Logical AND                                                          14
operations                      NOT            Bit inversion

                                  OR               Logical OR
                                  TAS             Memory test and bit set
                                  TST             Logical AND and T bit set
                                  XOR            Exclusive OR
Shift                   10         ROTL          One-bit left rotation                                               14
                                  ROTR          One-bit right rotation
                                  ROTCL        One-bit left rotation with T bit
                                  ROTCR       One-bit right rotation with T bit
                                  SHAL           One-bit arithmetic left shift
                                  SHAR          One-bit arithmetic right shift
                                  SHLL           One-bit logical left shift
                                  SHLLn         n-bit logical left shift
                                  SHLR          One-bit logical right shift
                                  SHLRn        n-bit logical right shift
Branch               9           BF               Conditional branch, conditional branch with                          11
                                                   delay (T = 0)
                                  BT               Conditional branch, conditional branch with
                                                   delay (T = 1)
                                  BRA             Unconditional branch
                                  BRAF          Unconditional branch
                                  BSR             Branch to subroutine procedure
                                  BSRF          Branch to subroutine procedure
                                  JMP             Unconditional branch
                                  JSR             Branch to subroutine procedure
                                  RTS             Return from subroutine procedure










30
```

## PDF page 47

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=47)

```text
Table 2.10    Instruction Set by Classification (cont)

                                   Operation                                                                              Number of
Classification   Types             Code           Function                                                                Instructions
System              11         CLRT           T bit clear                                                              31
control                            CLRMAC     MAC register clear

                                   LDC             Load to control register
                                   LDS             Load to system register
                                   NOP            No operation
                                   RTE             Return from exception handling
                                   SETT           T bit set
                                   SLEEP        Shift to power-down state
                                   STC             Store control register data
                                   STS             Store system register data
                                   TRAPA        Trap exception handling
               Total:62                                                                                                           142




































                                                                                                                                          31
```

## PDF page 48

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=48)

```text
Table 2.11      Instruction Code Format

Item                   Format                       Explanation
Instruction                                        OP: Operation code
mnemonic    OP.SzSRC,DEST                          Sz: Size (B: byte, W: word, or L: longword)
                                                   SRC: Source
                                                   DEST: Destination
                                                   Rm: Source register
                                                   Rn: Destination register
                                                   imm: Immediate data
                                                   disp: Displacement*1
Instruction
code                MSB «   LSB             mmmm: Source registernnnn: Destination register
                                                               0000: R0
                                                               0001: R1
                                                                    ...........
                                                               1111: R15
                                                   iiii: Immediate data
                                                   dddd: Displacement
Operation           ﬁ  , ‹                            Direction of transfer
summary           (xx)                              Memory operand

                     M/Q/T                          Flag bits in SR
                     &                                 Logical AND of each bit
                     |                                   Logical OR of each bit
                     ^                                  Exclusive OR of each bit
                     ~                                 Logical NOT of each bit
                     <<n, >>n                     n-bit shift
Execution            —                                Value when no wait states are inserted*2
states
T bit                   —                                Value of T bit after instruction is executed. An em-dash (—)
                                                   in the column means no change.
Notes:  1.   Depending on the operand size, displacement is scaled ·1, ·2, or ·3. For details, see
               the SH-1/SH-2 programming manual.
          2.   Instruction execution states: The execution states shown in the table are minimums.
               The actual number of states may be increased when:
               •  Contention occurs between instruction fetch and data access
               •  The destination register of the load instruction (memory ﬁ   register) and the register
                  used by the next instruction are the same.








32
```

## PDF page 49

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49)

```text
Table 2.12    Data Transfer Instructions

                                                                                               Execu-
                                                                                               tion        T
Instruction                              Instruction Code           Operation                  States      Bit
MOV    #imm,Rn         1110nnnniiiiiiii     #imm ﬁ   Sign extension ﬁ                          1—
                                                              Rn
MOV.W  @(disp,PC),Rn   1001nnnndddddddd     (disp · 2 + PC) ﬁ   Sign                           1—
                                                              extension ﬁ   Rn
MOV.L  @(disp,PC),Rn   1101nnnndddddddd     (disp · 4 + PC) ﬁ   Rn             1              —
MOV    Rm,Rn           0110nnnnmmmm0011     Rm ﬁ   Rn                                1              —
MOV.B  Rm,@Rn          0010nnnnmmmm0000     Rm ﬁ   (Rn)                             1              —
MOV.W  Rm,@Rn          0010nnnnmmmm0001     Rm ﬁ   (Rn)                             1              —
MOV.L  Rm,@Rn          0010nnnnmmmm0010     Rm ﬁ   (Rn)                             1              —
MOV.B  @Rm,Rn          0110nnnnmmmm0000     (Rm) ﬁ   Sign extension ﬁ                          1—
                                                              Rn
MOV.W  @Rm,Rn          0110nnnnmmmm0001     (Rm) ﬁ   Sign extension ﬁ                          1—
                                                              Rn
MOV.L  @Rm,Rn          0110nnnnmmmm0010     (Rm) ﬁ   Rn                             1              —
MOV.B  Rm,@–Rn         0010nnnnmmmm0100     Rn–1 ﬁ   Rn, Rm ﬁ   (Rn)        1              —
MOV.W  Rm,@–Rn         0010nnnnmmmm0101     Rn–2 ﬁ   Rn, Rm ﬁ   (Rn)        1              —
MOV.L  Rm,@–Rn         0010nnnnmmmm0110     Rn–4 ﬁ   Rn, Rm ﬁ   (Rn)        1              —
MOV.B  @Rm+,Rn         0110nnnnmmmm0100     (Rm) ﬁ   Sign extension ﬁ                          1—
                                                              Rn,Rm + 1 ﬁ   Rm
MOV.W  @Rm+,Rn         0110nnnnmmmm0101     (Rm) ﬁ   Sign extension ﬁ                          1—
                                                              Rn,Rm + 2 ﬁ   Rm
MOV.L  @Rm+,Rn         0110nnnnmmmm0110     (Rm) ﬁ   Rn,Rm + 4 ﬁ   Rm     1              —
MOV.B  R0,@(disp,Rn)   10000000nnnndddd     R0 ﬁ   (disp + Rn)                   1              —
MOV.W  R0,@(disp,Rn)   10000001nnnndddd     R0 ﬁ   (disp · 2 + Rn)              1              —
MOV.L  Rm,@(disp,Rn)   0001nnnnmmmmdddd     Rm ﬁ   (disp · 4 + Rn)             1              —
MOV.B  @(disp,Rm),R0   10000100mmmmdddd     (disp + Rm) ﬁ   Sign                               1—
                                                              extension ﬁ   R0
MOV.W  @(disp,Rm),R0   10000101mmmmdddd     (disp · 2 + Rm) ﬁ   Sign                           1—
                                                              extension ﬁ   R0
MOV.L  @(disp,Rm),Rn   0101nnnnmmmmdddd     (disp · 4 + Rm) ﬁ   Rn             1              —
MOV.B  Rm,@(R0,Rn)     0000nnnnmmmm0100     Rm ﬁ   (R0 + Rn)                    1              —
MOV.W  Rm,@(R0,Rn)     0000nnnnmmmm0101     Rm ﬁ   (R0 + Rn)                    1              —
MOV.L  Rm,@(R0,Rn)     0000nnnnmmmm0110     Rm ﬁ   (R0 + Rn)                    1              —


                                                                                                             33
```

## PDF page 50

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=50)

```text
Table 2.12    Data Transfer Instructions (cont)

                                                                                           Execu-
                                                                                           tion       T
Instruction                              Instruction Code           Operation              States     Bit
MOV.B  @(R0,Rm),Rn     0000nnnnmmmm1100     (R0 + Rm) ﬁ   Sign                             1—
                                                           extension ﬁ   Rn
MOV.W  @(R0,Rm),Rn     0000nnnnmmmm1101     (R0 + Rm) ﬁ   Sign                             1—
                                                           extension ﬁ   Rn
MOV.L  @(R0,Rm),Rn     0000nnnnmmmm1110     (R0 + Rm) ﬁ   Rn                    1              —
MOV.B  R0,@(disp,GBR)  11000000dddddddd     R0 ﬁ   (disp + GBR)                1              —
MOV.W  R0,@(disp,GBR)  11000001dddddddd     R0 ﬁ   (disp · 2 + GBR)          1              —
MOV.L  R0,@(disp,GBR)  11000010dddddddd     R0 ﬁ   (disp · 4 + GBR)          1              —
MOV.B  @(disp,GBR),R0  11000100dddddddd     (disp + GBR) ﬁ   Sign                          1—
                                                           extension ﬁ   R0
MOV.W  @(disp,GBR),R0  11000101dddddddd     (disp · 2 + GBR) ﬁ   Sign                      1—
                                                           extension ﬁ   R0
MOV.L  @(disp,GBR),R0  11000110dddddddd     (disp · 4 + GBR) ﬁ   R0           1              —
MOVA   @(disp,PC),R0   11000111dddddddd     disp · 4 + PC ﬁ   R0                1              —
MOVT   Rn              0000nnnn00101001     T ﬁ   Rn                                   1              —
SWAP.B Rm,Rn           0110nnnnmmmm1000     Rm ﬁ   Swap the bottom                         1—
                                                           two bytes ﬁ   Rn
SWAP.W Rm,Rn           0110nnnnmmmm1001     Rm ﬁ   Swap two                                1—
                                                           consecutive words ﬁ   Rn
XTRCT  Rm,Rn           0010nnnnmmmm1101     Rm: Middle 32 bits of                          1—
                                                           Rn ﬁ   Rn























34
```

## PDF page 51

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=51)

```text
Table 2.13    Arithmetic Instructions

                                                                                        Execution
Instruction                     Instruction Code          Operation                     States           T Bit
ADD      Rm,Rn    0011nnnnmmmm1100    Rn + Rm ﬁ   Rn              1                    —
ADD      #imm,Rn  0111nnnniiiiiiii    Rn + imm ﬁ   Rn            1                    —
ADDC     Rm,Rn    0011nnnnmmmm1110    Rn + Rm + T ﬁ   Rn,                               1                    Carry
                                                           Carry ﬁ   T
ADDV     Rm,Rn    0011nnnnmmmm1111    Rn + Rm ﬁ   Rn,                                   1                    Overflow
                                                           Overflow ﬁ   T
CMP/EQ   #imm,R0  10001000iiiiiiii    If R0 = imm, 1 ﬁ   T        1                    Comparison
                                                                                                         result
CMP/EQ   Rm,Rn    0011nnnnmmmm0000    If Rn = Rm, 1 ﬁ   T         1                    Comparison
                                                                                                         result
CMP/HS   Rm,Rn    0011nnnnmmmm0010    If Rn‡Rm with                                     1                    Comparison
                                                           unsigned data, 1 ﬁ   T                        result
CMP/GE   Rm,Rn    0011nnnnmmmm0011    If Rn ‡ Rm with                                   1                    Comparison
                                                           signed data, 1 ﬁ   T                          result
CMP/HI   Rm,Rn    0011nnnnmmmm0110    If Rn > Rm with                                   1                    Comparison
                                                           unsigned data, 1 ﬁ   T                        result
CMP/GT   Rm,Rn    0011nnnnmmmm0111    If Rn > Rm with                                   1                    Comparison
                                                           signed data, 1 ﬁ   T                          result
CMP/PZ   Rn       0100nnnn00010001    If Rn ‡ 0, 1 ﬁ   T             1                    Comparison
                                                                                                         result
CMP/PL   Rn       0100nnnn00010101    If Rn > 0, 1 ﬁ   T            1                    Comparison
                                                                                                         result
CMP/ST   Rm,Rn    0010nnnnmmmm1100    If Rn and Rm have                                 1                    Comparison
                                                           an equivalent byte,                           result
                                                           1 ﬁ   T
DIV1     Rm,Rn    0011nnnnmmmm0100    Single-step division                              1                    Calculation
                                                           (Rn ‚ Rm)                                     result
DIV0S    Rm,Rn    0010nnnnmmmm0111    MSB of Rn ﬁ   Q,                                  1                    Calculation
                                                           MSB of Rm ﬁ   M,                              result
                                                           M ^ Q ﬁ   T
DIV0U              0000000000011001    0 ﬁ   M/Q/T                    1                    0
DMULS.   Rm,Rn    0011nnnnmmmm1101    Signed operation of                               2 to 4*           —
                                                           Rn · Rm ﬁ   MACH,
                                                           MACL 32 · 32 ﬁ   64
                                                           bits





                                                                                                                     35
```

## PDF page 52

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=52)

```text
Table 2.13    Arithmetic Instructions (cont)

                                                                               Execution
Instruction                     Instruction Code          Operation            States           T Bit
DMULU.LRm,Rn      0011nnnnmmmm0101    Unsigned operation                       2 to 4*           —
                                                     of Rn · Rm ﬁ
                                                     MACH, MACL 32 ·
                                                     32 ﬁ   64 bit
DT     Rn         0100nnnn00010000    Rn – 1 ﬁ   Rn, when                      1                    Comparison
                                                     Rn is 0, 1 ﬁ   T                        result
                                                     When Rn is nonzero,
                                                     0 ﬁ   T
EXTS.B Rm,Rn      0110nnnnmmmm1110    A byte in Rm is sign-                    1—
                                                     extended ﬁ   Rn
EXTS.W Rm,Rn      0110nnnnmmmm1111    A word in Rm is sign-                    1—
                                                     extended ﬁ   Rn
EXTU.B Rm,Rn      0110nnnnmmmm1100    A byte in Rm is zero-                    1—
                                                     extended ﬁ   Rn
EXTU.W Rm,Rn      0110nnnnmmmm1101    A word in Rm is zero-                    1—
                                                     extended ﬁ   Rn
MAC.L  @Rm+,@Rn+  0000nnnnmmmm1111    Signed operation of                      3/(2 to 4)*      —
                                                     (Rn) · (Rm) ﬁ   MAC
                                                     ﬁ   MAC 32 · 32 ﬁ
                                                     64 bits
MAC    @Rm+,@Rn+  0100nnnnmmmm1111    Signed operation of                      3/(2)*             —
                                                     (Rn) · (Rm) + MAC
                                                     ﬁ   MAC 16 · 16 + 64
                                                     ﬁ   64 bits
MUL.L  Rm,Rn      0000nnnnmmmm0111    Rn · Rm ﬁ   MACL,                        2 to 4*           —
                                                     32 · 32 ﬁ   32 bits
MULS.W Rm,Rn      0010nnnnmmmm1111    Signed operation of                      1 to 3*           —
                                                     Rn · Rm ﬁ   MAC 16
                                                     · 16 ﬁ   32 bits
MULU.W Rm,Rn      0010nnnnmmmm1110    Unsigned operation                       1 to 3*           —
                                                     of Rn · Rm ﬁ   MAC
                                                     16 · 16 ﬁ   32 bits
NEG    Rm,Rn      0110nnnnmmmm1011    0–Rm ﬁ   Rn                  1                    —
NEGC   Rm,Rn      0110nnnnmmmm1010    0–Rm–T ﬁ   Rn,                           1                    Borrow
                                                     Borrow ﬁ   T







36
```

## PDF page 53

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=53)

```text
Table 2.13    Arithmetic Instructions (cont)

                                                                                     Execution
Instruction                     Instruction Code          Operation                  States           T Bit
SUB    Rm,Rn       0011nnnnmmmm1000    Rn–Rm ﬁ   Rn                1                    —
SUBC   Rm,Rn       0011nnnnmmmm1010    Rn–Rm–T ﬁ   Rn,                               1                    Borrow
                                                         Borrow ﬁ   T
SUBV   Rm,Rn       0011nnnnmmmm1011    Rn–Rm ﬁ   Rn,                                 1                    Underflow
                                                         Underflow ﬁ   T
Note:   The normal minimum number of execution cycles. (The number in parentheses is the
        number of cycles when there is contention with preceding or following instructions.)


Table 2.14    Logic Operation Instructions

                                                                                                Execu-
                                                                                                tion
Instruction                              Instruction Code           Operation                   States     T Bit
AND    Rm,Rn           0010nnnnmmmm1001     Rn & Rm ﬁ   Rn                  1              —
AND    #imm,R0         11001001iiiiiiii     R0 & imm ﬁ   R0                 1              —
AND.B  #imm,@(R0,GBR)  11001101iiiiiiii     (R0 + GBR) & imm ﬁ                                  3—
                                                                (R0 + GBR)
NOT    Rm,Rn           0110nnnnmmmm0111     ~Rm ﬁ   Rn                         1              —
OR     Rm,Rn           0010nnnnmmmm1011     Rn | Rm ﬁ   Rn                    1              —
OR     #imm,R0         11001011iiiiiiii     R0 | imm ﬁ   R0                   1              —
OR.B   #imm,@(R0,GBR)  11001111iiiiiiii     (R0 + GBR) | imm ﬁ                                  3—
                                                                (R0 + GBR)
TAS.B  @Rn             0100nnnn00011011     If (Rn) is 0, 1 ﬁ   T; 1 ﬁ                          4              Test
                                                                MSB of (Rn)                                 result
TST    Rm,Rn           0010nnnnmmmm1000     Rn & Rm; if the result is                           1              Test
                                                                0, 1 ﬁ   T                                  result
TST    #imm,R0         11001000iiiiiiii     R0 & imm; if the result is                          1              Test
                                                                0, 1 ﬁ   T                                  result
TST.B  #imm,@(R0,GBR)  11001100iiiiiiii     (R0 + GBR) & imm; if the                            3              Test
                                                                result is 0, 1 ﬁ   T                        result
XOR    Rm,Rn           0010nnnnmmmm1010     Rn ^ Rm ﬁ   Rn                   1              —
XOR    #imm,R0         11001010iiiiiiii     R0 ^ imm ﬁ   R0                  1              —
XOR.B  #imm,@(R0,GBR)  11001110iiiiiiii     (R0 + GBR) ^ imm ﬁ                                  3—
                                                                (R0 + GBR)





                                                                                                                 37
```

## PDF page 54

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=54)

```text
Table 2.15    Shift Instructions

                                                                                                        Execution
Instruction               Instruction Code             Operation                                        States            T Bit
ROTL   Rn       0100nnnn00000100       T ‹   Rn ‹   MSB                       1                     MSB
ROTR   Rn       0100nnnn00000101       LSB ﬁ   Rn ﬁ   T                       1                     LSB
ROTCL  Rn       0100nnnn00100100       T ‹   Rn ‹   T                            1                     MSB
ROTCR  Rn       0100nnnn00100101       T ﬁ   Rn ﬁ   T                            1                     LSB
SHAL   Rn       0100nnnn00100000       T ‹   Rn ‹   0                            1                     MSB
SHAR   Rn       0100nnnn00100001       MSB ﬁ   Rn ﬁ   T                      1                     LSB
SHLL   Rn       0100nnnn00000000       T ‹   Rn ‹   0                            1                     MSB
SHLR   Rn       0100nnnn00000001       0 ﬁ   Rn ﬁ   T                            1                     LSB
SHLL2  Rn       0100nnnn00001000       Rn<<2 ﬁ   Rn                           1                     —
SHLR2  Rn       0100nnnn00001001       Rn>>2 ﬁ   Rn                           1                     —
SHLL8  Rn       0100nnnn00011000       Rn<<8 ﬁ   Rn                           1                     —
SHLR8  Rn       0100nnnn00011001       Rn>>8 ﬁ   Rn                           1                     —
SHLL16 Rn       0100nnnn00101000       Rn<<16 ﬁ   Rn                         1                     —
SHLR16 Rn       0100nnnn00101001       Rn>>16 ﬁ   Rn                         1                     —
































38
```

## PDF page 55

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=55)

```text
Table 2.16    Branch Instructions

                                                                                        Execution      T
Instruction            Instruction Code          Operation                              States         Bit
BF     label  10001011dddddddd    If T = 0, disp · 2 + PC ﬁ   PC; if T =                3/1*                —
                                               1, nop
BF/S   label  10001111dddddddd    Delayed branch, if T = 0, disp · 2 +                  2/1*                —
                                               PC ﬁ   PC; if T = 1, nop
BT     label  10001001dddddddd    Delayed branch, if T = 1, disp · 2 +                  3/1*                —
                                               PC ﬁ   PC; if T = 0, nop
BT/S   label  10001101dddddddd    If T = 1, disp · 2 + PC ﬁ   PC; if T =                2/1*                —
                                               0, nop
BRA    label  1010dddddddddddd    Delayed branch, disp · 2 + PC ﬁ                       2—
                                               PC
BRAF   Rm     0000mmmm00100011    Delayed branch, Rm + PC ﬁ   PC        2                     —
BSR    label  1011dddddddddddd    Delayed branch, PC ﬁ   PR, disp ·                     2—
                                               2 + PC ﬁ   PC
BSRF   Rm     0000mmmm00000011    Delayed branch, PC ﬁ   PR,                            2—
                                               Rm + PC ﬁ   PC
JMP    @Rm    0100mmmm00101011    Delayed branch, Rm ﬁ   PC                2                     —
JSR    @Rm    0100mmmm00001011    Delayed branch, PC ﬁ   PR,                            2—
                                               Rm ﬁ   PC
RTS           0000000000001011    Delayed branch, PR ﬁ   PC                 2                     —
Note:   One state when the instruction does not branch.


























                                                                                                         39
```

## PDF page 56

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=56)

```text
Table 2.17    System Control Instructions

                                                                                                     Execu-
                                                                                                     tion        T
Instruction                   Instruction Code           Operation                                   States      Bit
CLRT             0000000000001000     0 ﬁ   T10
CLRMAC           0000000000101000     0                                                    ﬁ   MACH, MACL                          1              —
LDC    Rm,SR      0100mmmm00001110     Rm ﬁ   SR                                        1              LSB
LDC    Rm,GBR     0100mmmm00011110     Rm ﬁ   GBR                                     1              —
LDC    Rm,VBR     0100mmmm00101110     Rm ﬁ   VBR                                      1              —
LDC.L  @Rm+,SR    0100mmmm00000111     (Rm) ﬁ   SR, Rm + 4 ﬁ   Rm             3              LSB
LDC.L  @Rm+,GBR   0100mmmm00010111     (Rm) ﬁ   GBR, Rm + 4 ﬁ   Rm          3              —
LDC.L  @Rm+,VBR   0100mmmm00100111     (Rm) ﬁ   VBR, Rm + 4 ﬁ   Rm          3              —
LDS    Rm,MACH    0100mmmm00001010     Rm ﬁ   MACH                                  1              —
LDS    Rm,MACL    0100mmmm00011010     Rm ﬁ   MACL                                   1              —
LDS    Rm,PR      0100mmmm00101010     Rm ﬁ   PR                                        1              —
LDS.L  @Rm+,MACH  0100mmmm00000110     (Rm) ﬁ   MACH, Rm + 4 ﬁ   Rm       1              —
LDS.L  @Rm+,MACL  0100mmmm00010110     (Rm) ﬁ   MACL, Rm + 4 ﬁ   Rm        1              —
LDS.L  @Rm+,PR    0100mmmm00100110     (Rm) ﬁ   PR, Rm + 4 ﬁ   Rm             1              —
NOP              0000000000001001     No operation                                   1              —
RTE              0000000000101011     Delayed branch, stack area ﬁ                                   4—
                                                          PC/SR
SETT             0000000000011000     1 ﬁ   T11
SLEEP            0000000000011011     Sleep                                               3*             —
STC    SR,Rn      0000nnnn00000010     SR                                                    ﬁ   Rn                                         1              —
STC    GBR,Rn     0000nnnn00010010     GBR ﬁ   Rn                                      1              —
STC    VBR,Rn     0000nnnn00100010     VBR ﬁ   Rn                                      1              —
STC.L  SR,@–Rn    0100nnnn00000011     Rn–4 ﬁ   Rn, SR ﬁ   (Rn)                  2              —
STC.L  GBR,@–Rn   0100nnnn00010011     Rn–4 ﬁ   Rn, GBR ﬁ   (Rn)               2              —
STC.L  VBR,@–Rn   0100nnnn00100011     Rn–4 ﬁ   Rn, VBR ﬁ   (Rn)               2              —












40
```

## PDF page 57

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=57)

```text
Table 2.17    System Control Instructions (cont)

                                                                                                       Execu-
                                                                                                       tion        T
Instruction                   Instruction Code           Operation                                     States      Bit
STS    MACH,Rn    0000nnnn00001010     MACH ﬁ   Rn                                   1              —
STS    MACL,Rn    0000nnnn00011010     MACL ﬁ   Rn                                    1              —
STS    PR,Rn      0000nnnn00101010     PR ﬁ   Rn                                         1              —
STS.L  MACH,@–Rn  0100nnnn00000010     Rn–4 ﬁ   Rn, MACH ﬁ   (Rn)            1              —
STS.L  MACL,@–Rn  0100nnnn00010010     Rn–4 ﬁ   Rn, MACL ﬁ   (Rn)            1              —
STS.L  PR,@–Rn    0100nnnn00100010     Rn–4 ﬁ   Rn, PR ﬁ   (Rn)                  1              —
TRAPA  #imm       11000011iiiiiiii     PC/SR ﬁ   stack area, (imm) ﬁ                                   8—
                                                           PC
Note:   The number of execution states before the chip enters the sleep mode.
         Instruction states: The values shown for the execution states are minimums. The actual
         number of states may be increased when:
         •    Contention occurs between instruction fetch and data access
         •    The destination register of the load instruction (memory ﬁ   register) and the register used
             by the next instruction are the same.


































                                                                                                                       41
```

## PDF page 58

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=58)

```text
2.4.2        Operation Code Map

Table 2.18    Operation Code Map

    Instruction Code         Fx: 0000              Fx: 0001              Fx: 0010              Fx: 0011–1111
MSB                      LSB        MD: 00                 MD: 01                 MD: 10                 MD: 11
0000   Rn       Fx    0000
0000   Rn       Fx    0001
0000   Rn       Fx    0010   STC SR,Rn    STC GBR,Rn   STC VBR,Rn
0000   Rm      Fx    0011   BSRF Rm                    BRAF Rm
0000   Rn       Rm  01MD  MOV.B RM,              MOV.W RM,           MOV.L RM,          MUL.L Rm,Rn
                              @(R0,Rn)           @(R0,Rn)            @(R0,Rn)
0000   0000   Fx    1000   CLRT         SETT         CLRMAC
0000   0000   Fx    1001   NOP          DIVOU
0000   0000   Fx    1010
0000   0000   Fx    1011   RTS          SLEEP        RTE
0000   Rn       Fx    1000
0000   Rn       Fx    1001                              MOVT Rn
0000   Rn       Fx    1010   STS MACH,Rn  STS MACL,Rn  STS PR,Rn
0000   Rn       Fx    1011
0000   Rn       Rm  11MD  MOV.B                  MOV.W               MOV.L              MAC.L
                              @(R0,Rm),Rn        @(R0,Rm),Rn         @(R0,Rm),Rn        @Rm+,@Rn+
0001   Rn       Rm  disp    MOV.L Rm,@(disp:4,Rn)
0010   Rn       Rm  00MD  MOV.B Rm,@Rn MOV.W Rm,@Rn MOV.L Rm,@Rn
0010   Rn       Rm  01MD  MOV.B Rm,              MOV.W Rm,           MOV.L Rm,          DIV0S Rm,Rn
                              @-Rn               @–Rn                @–Rn
0010   Rn       Rm  10MD  TST Rm,Rn    AND Rm,Rn    XOR Rm,Rn    OR Rm,Rn
0010   Rn       Rm  11MD  CMP/STR                XTRCT               MULU.W Rm,Rn MULS.W Rm,Rn
                              Rm,Rn              Rm,Rn
0011   Rn       Rm  00MD  CMP/EQ Rm,Rn               CMP/HS Rm,Rn CMP/GE Rm,Rn
0011   Rn       Rm  01MD  DIV1 Rm,Rn   DMULU.L                       CMP/HI Rm,Rn CMP/GT Rm,Rn
                                                 Rm,Rn
0011   Rn       Rm  10MD  SUB Rm,Rn                  SUBC Rm,Rn   SUBV Rm,Rn
0011   Rn       Rm  11MD  ADD Rm,Rn    DMULS.L                       ADDC Rm,Rn   ADDV Rm,Rn
                                                 Rm,Rn
0100   Rn       Fx    0000   SHLL Rn      DT Rn        SHAL Rn




42
```

## PDF page 59

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=59)

```text
Table 2.18    Operation Code Map (cont)

     Instruction Code          Fx: 0000              Fx: 0001              Fx: 0010              Fx: 0011–1111
MSB                          LSB     MD: 00                 MD: 01                 MD: 10                 MD: 11
0100   Rn         Fx     0001  SHLR Rn      CMP/PZ Rn     SHAR Rn
0100   Rn         Fx     0010  STS.L MACH,         STS.L MACL,         STS.L PR,
                                @–Rn               @–Rn                @–Rn
0100   Rn         Fx     0011  STC.L               STC.L               STC.L
                                SR,@–Rn            GBR,@–Rn            VBR,@–Rn
0100   Rn         Fx     0100  ROTL Rn                    ROTCL Rn
0100   Rn         Fx     0101  ROTR Rn      CMP/PL Rn    ROTCR Rn
0100   Rm        Fx     0110  LDS.L                LDS.L               LDS.L
                                @Rm+,MACH          @Rm+,MACL           @Rm+,PR
0100   Rm        Fx     0111  LDC.L                LDC.L               LDC.L
                                @Rm+,SR            @Rm+,GBR            @Rm+,VBR
0100   Rn         Fx     1000  SHLL2 Rn     SHLL8 Rn     SHLL16 Rn
0100   Rn         Fx     1001  SHLR2 Rn     SHLR8 Rn     SHLR16 Rn
0100   Rm        Fx     1010  LDS Rm,MACH  LDS Rm,MACL  LDS Rm,PR
0100   Rm/Rn  Fx     1011  JSR @Rm      TAS.B @Rn    JMP @Rm
0100   Rm        Fx     1100
0100   Rm        Fx     1101
0100   Rm        Fx     1110  LDC Rm,SR    LDC Rm,GBR   LDC Rm,VBR
0100   Rn         Rm    1111  MAC.W @Rm+,@Rn+
0101   Rn         Rm    disp   MOV.L @(disp:4,Rm),Rn
0110   Rn         Rm    00MD MOV.B @Rm,Rn MOV.W @Rm,Rn MOV.L @Rm,Rn MOV Rm,Rn
0110   Rn         Rm    01MD MOV.B                 MOV.W               MOV.L               NOT
                                @Rm+,Rn            @Rm+,Rn             @Rm+,Rn             Rm,Rn
0110   Rn         Rm    10MD SWAP.B Rm,Rn SWAP.W Rm,Rn NEGC Rm,Rn   NEG Rm,Rn
0110   Rn         Rm    11MD EXTU.B Rm,Rn EXTU.W Rm,Rn EXTS.B Rm,Rn EXTS.W Rm,Rn
0111   Rn               imm    ADD #imm:8,Rn
1000   00MD    Rn     disp   MOV.B R0,             MOV.W R0,
                                @(disp:4,Rn)       @(disp:4,Rn)
1000   01MD    Rm    disp   MOV.B                  MOV.W
                                @(disp:4,          @(disp:4,
                                Rm),R0             Rm),R0






                                                                                                          43
```

## PDF page 60

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=60)

```text
Table 2.18    Operation Code Map (cont)

     Instruction Code         Fx: 0000              Fx: 0001              Fx: 0010              Fx: 0011–1111
MSB                          LSB   MD: 00                 MD: 01                 MD: 10                 MD: 11
1000   10MD     imm/disp  CMP/EQ                       BT label:8                                         BF label:8
                                 #imm:8,R0
1000   10MD     imm/disp                                 BT/S label:8                                      BF/S label:8
1001   Rn              disp    MOV.W @(disp:8,PC),Rn
1010                disp       BRA label:12
1011                disp       BSR label:12
1100   00MD     imm/disp  MOV.B R0,                    MOV.W R0,             MOV.L R0,             TRAPA #imm:8
                                 @(disp:8,             @(disp:8,             @(disp:8,
                                 GBR)                  GBR)                  GBR)
1100   01MD         disp    MOV.B                      MOV.W                 MOV.L                 MOVA
                                 @(disp:8,             @(disp:8,             @(disp:8,             @(disp:8,
                                 GBR),R0               GBR),R0               GBR),R0               PC),R0
1100   10MD         imm    TST                         AND                   XOR                   OR
                                 #imm:8,R0             #imm:8,R0             #imm:8,R0             #imm:8,R0
1100   11MD         imm    TST.B                       AND.B                 XOR.B                 OR.B
                                 #imm:8,               #imm:8,               #imm:8,               #imm:8,
                                 @(R0,GBR)             @(R0,GBR)             @(R0,GBR)             @(R0,GBR)
1101   Rn              disp    MOV.L @(disp:8,PC),Rn
1110   Rn              imm    MOV #imm:8,Rn
1111                  ...


2.5        Processing States


2.5.1        State Transitions

The CPU has five processing states: reset, exception handling, bus-released, program execution,
and power-down. Figure 2.6 shows the transitions between the states. See section 14, Power-
Down State, for more information on the power-down state.












44
```

## PDF page 61

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=61)

```text
From any state when                                From any state when
RES = 0 and NMI = 1                                RES = 0 and NMI = 0

                                                   RES = 0, NMI = 0
                     Power-on reset state                                          Manual reset state
                                                   RES = 0, NMI = 1

                                      RES = 1,                              RES = 1,            Reset state
                                        NMI = 1                             NMI = 0
   Interrupt or DMA
   address error                               Exception handling state



                         Bus request
                               cleared                                         NMI interrupt
                                           Bus request
                                           generated

                                               Exception          Exception
      Bus-released state                                          handling
                                                                  ends

                   Bus request
                     generated             Bus request
                                           cleared

Bus request         Bus request                 Program execution state
  generated         cleared

                                   SLEEP                                         SLEEP
                          instruction with        MSTP            MSTP           instruction with
                         SBY bit cleared       bit cleared         bit set       SBY bit set



                          Sleep mode                                                    Standby mode


                                                     Module standby

                                                                                        Power-down state

                    Figure 2.6   Transitions between Processing States








                                                                                                               45
```

## PDF page 62

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=62)

```text
Reset State: The CPU resets in the reset state. This occurs when the RES pin level goes low.
When the NMI pin is high, the result is a power-on reset; when it is low, a manual reset will occur.

Exception Handling State: The exception handling state is a transient state that occurs when an
exception handling source such as a reset or interrupt alters the CPU’s processing state flow.

For a reset, the initial values of the program counter (PC) (execution start address) and stack
pointer (SP) are fetched from the exception vector table and stored; the CPU then branches to the
execution start address and execution of the program begins.

For an interrupt, the stack pointer (SP) is accessed and the program counter (PC) and status
register (SR) are saved to the stack area. The exception service routine start address is fetched
from the exception processing vector table; the CPU then branches to that address and the program
starts executing, thereby entering the program execution state.

Program Execution State: In the program execution state, the CPU sequentially executes the
program.

Power-Down State: In the power-down state, the CPU operation halts and power consumption
declines. The SLEEP instruction places the CPU in the power-down state. This state has two
modes: sleep mode and standby mode. See section 2.5.2 for more details.

Bus-Released State: In the bus-released state, the CPU releases the bus to the device that has
requested it.


2.5.2        Power-Down State

Besides the ordinary program execution states, the CPU also has a power-down state in which
CPU operation halts, lowering power consumption (table 2.19). There are two power-down state
modes, sleep mode and standby mode, and also a module standby function.

Sleep Mode: When standby bit SBY (in the standby control register SBYCR) is cleared to 0 and a
SLEEP instruction executed, the CPU moves from program execution state to sleep mode. The on-
chip peripheral modules other than the CPU do not halt in the sleep mode. To return from sleep
mode, use a reset, any interrupt, or a DMA address error; the CPU returns to the ordinary program
execution state through the exception handling state.

Software Standby Mode: To enter the standby mode, set the standby bit SBY (in the standby
control register SBYCR) to 1 and execute a SLEEP instruction. In standby mode, all CPU, on-chip
peripheral module, and oscillator functions are halted. When entering the standby mode, confirm
that the DMAC master enable bit is 0. If a multiply instruction is in progress on entry to standby
mode, the MACL and MACH registers will be invalid. CPU internal register contents and on-chip
RAM data are retained. Cache (and on-chip RAM) data is not retained.



46
```

## PDF page 63

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=63)

```text
To return from standby mode, use a reset or an external NMI interrupt. For resets, the CPU returns
to the ordinary program execution state through the exception handling state when placed in a
reset state after the oscillator stabilization time has elapsed. For NMI interrupts, the CPU returns
to the ordinary program execution state through the exception handling state after the oscillator
stabilization time has elapsed. Turn the cache off before entering standby mode. In this mode,
power consumption drops substantially because the oscillator stops.

Module Standby Function: The module standby function is available for the multiplier (MULT),
divider (DIVU), 16-bit free-running timer (FRT), serial communication interface (SCI), and DMA
controller (DMAC) on-chip peripheral modules.

The supply of the clock to these on-chip peripheral modules can be halted by setting the
corresponding bits 4–0 (MSTP4–MSTP0) in the standby control register (SBYCR). By using this
function, the power consumption can be reduced.

The external pins of the on-chip peripheral modules in module standby mode are reset and all
registers except DMAC, MULT, and DIVU are initialized. The module standby function is cleared
by clearing the MSTP4–MSTP0 bits to 0.

When MULT has entered the software standby mode, do not execute the DMULS.L, DMULU.L,
MAC.L, MAC.W, MUL.L, MULS, and MULU instructions (all of which are multiply
instructions) or any instructions that access the MACH and MACL registers (CLRMAC, LDS
MACH/MACL, STS MACH/MACL).

When the DMAC module standby function is used, set the DMAC’s DMA master enable bit to 0.




























                                                                                                                  47
```

## PDF page 64

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=64)

```text
Table 2.19    Power-Down State

                                                                  State
                                                       On-Chip
                                                       Peripheral             CPU
Mode         Conditions    Clock     CPU               Modules                Registers   RAM           Canceling
Sleep         Execute           Active     Halted     Active                   Held            Held            1.  Interrupt
              SLEEP                                                                                          2.  DMA
              instruction                                                                                        address
              with SBY bit                                                                                       error
              cleared to 0                                                                                   3.  Power-on
              in SBYCR                                                                                           reset

                                                                                                             4.  Manual
                                                                                                                 reset
Standby     Execute             Halted    Halted     Halted and
              SLEEP                                    initialized*1           Held            Undefined   1.  NMI2.  Power-on
              instruction                                                                                        reset
              with SBY bit
              set to 1 in                                                                                    3.  Manual
              SBYCR                                                                                              reset

Module        MSTP4–            Active     Active      Supply of clock        Held            Held            Clear bits
standby       MSTP0 bits                   (MULT       to affected                                           MSTP 4–0 of
              of SBYCR                     is          module is halted                                      SBYCR to 0
              set to 1                     halted.)    and module
                                                       initialized.*2
Notes:  1.   Depends on peripheral module and pin.
          2.   The DMAC, MULT, and DIV registers and the specified interrupt vectors retain their
              settings.
























48
```
