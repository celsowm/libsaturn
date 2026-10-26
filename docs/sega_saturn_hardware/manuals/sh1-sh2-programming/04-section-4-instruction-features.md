# Section 4 Instruction Features

SH-1/SH-2 Programming Manual — Hitachi, September 3, 1996.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=27)

Physical PDF pages 27–37. Text extraction; consult the PDF for figures.

## PDF page 27

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=27)

```text
                            Section 4   Instruction Features


4.1       RISC-Type Instruction Set

All instructions are RISC type. Their features are detailed in this section.

4.1.1     16-Bit Fixed Length
All instructions are 16 bits long, increasing program coding efficiency.

4.1.2    One Instruction/Cycle
Basic instructions can be executed in one cycle using the pipeline system. Instructions are
executed in 50 ns at 20 MHz, in 35 ns at 28.7MHz.

4.1.3      Data Length
Longword is the standard data length for all operations. Memory can be accessed in bytes,
words, or longwords. Byte or word data accessed from memory is sign-extended and
calculated with longword data (table 4.1). Immediate data is sign-extended for arithmetic
operations or zero-extended for logic operations. It also is calculated with longword data.

Table 4.1    Sign Extension of Word Data

SH-1/SH-2 CPU         Description                  Example for Other CPU
MOV.W @(disp,PC),R1                  Data is sign-extended to 32         ADD.W     #H'1234,R0
ADD  R1,R0                           bits, and R1 becomes
    .........                        H'00001234. It is next
                                     operated upon by an ADD
.DATA.W       H'1234                 instruction.
Note:   The address of the immediate data is accessed by @(disp, PC).

4.1.4    Load-Store Architecture

Basic operations are executed between registers. For operations that involve memory access,
data is loaded to the registers and executed (load-store architecture). Instructions such as
AND that manipulate bits, however, are executed directly in memory.


4.1.5    Delayed Branch Instructions

Unconditional branch instructions are delayed. Pipeline disruption during branching is reduced
by first executing the instruction that follows the branch instruction, and then branching (table
4.2). With delayed branching, branching occurs after execution of the slot instruction.
However, instructions such as register changes etc. are executed in the order of delayed
branch instruction, then delay slot instruction. For example, even if the register in which the


                                                                                                 HITACHI  23
```

## PDF page 28

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=28)

```text
branch destination address has been loaded is changed by the delay slot instruction, the
branch will still be made using the value of the register prior to the change as the branch
destination address.

Table 4.2   Delayed Branch Instructions

SH-1/SH-2 CPU         Description                  Example for Other CPU
BRA    TRGET                         Executes an ADD before             ADD.W    R1,R0
ADD    R1,R0                         branching to TRGET.                BRA      TRGET



4.1.6    Multiplication/Accumulation Operation

SH-1 CPU: 16bit · 16bit ﬁ   32-bit multiplication operations are executed in one to three
cycles. 16bit · 16bit + 42bit ﬁ   42-bit multiplication/accumulation operations are executed in
two to three cycles.

SH-2 CPU: 16bit · 16bit ﬁ   32-bit multiplication operations are executed in one to two
cycles. 16bit · 16bit + 64bit ﬁ   64-bit multiplication/accumulation operations are executed in
two to three cycles. 32bit · 32bit ﬁ   64-bit multiplication and 32bit · 32bit + 64bit ﬁ   64-bit
multiplication/accumulation operations are executed in two to four cycles.


4.1.7      T Bit

The T bit in the status register changes according to the result of the comparison, and in turn
is the condition (true/false) that determines if the program will branch (table 4.3). The
number of instructions after T bit in the status register is kept to a minimum to improve the
processing speed.

Table 4.3   T Bit

SH-1/SH-2 CPU       Description                     Example for Other CPU
CMP/GE   R1,R0                    T bit is set when R0 ‡ R1. The        CMP.W    R1,R0
BT       TRGET0                   program branches to TRGET0            BGE      TRGET0
BF       TRGET1                   when R0 ‡ R1 and to TRGET1            BLT      TRGET1
                                  when R0 < R1.
ADD      #–1,R0                   T bit is not changed by ADD. T        SUB.W    #1,R0
CMP/EQ   #0,R0                    bit is set when R0 = 0. The           BEQ      TRGET
BT       TRGET                    program branches if R0 = 0.










24 HITACHI
```

## PDF page 29

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=29)

```text
4.1.8     Immediate Data

Byte immediate data is located in instruction code. Word or longword immediate data is not
input via instruction codes but is stored in a memory table. The memory table is accessed by
an immediate data transfer instruction (MOV) using the PC relative addressing mode with
displacement (table 4.4).

Table 4.4   Immediate Data Accessing

Classification    SH-1/SH-2 CPU                     Example for Other CPU
8-bit immediate    MOV      #H'12,R0             MOV.B  #H'12,R0
16-bit immediate   MOV.W    @(disp,PC),R0                          MOV.W  #H'1234,R0
                          .................
                       .DATA.W  H'1234
32-bit immediate   MOV.L    @(disp,PC),R0                          MOV.L  #H'12345678,R0
                          .................
                       .DATA.L  H'12345678
Note: The address of the immediate data is accessed by @(disp, PC).


4.1.9     Absolute Address

When data is accessed by absolute address, the value already in the absolute address is
placed in the memory table. Loading the immediate data when the instruction is executed
transfers that value to the register and the data is accessed in the indirect register addressing
mode.

Table 4.5   Absolute Address

Classification    SH-1/SH-2 CPU                  Example for Other CPU
Absolute address    MOV.L     @(disp,PC),R1                     MOV.B   @H'12345678,R0
                        MOV.B    @R1,R0
                            ..................
                        .DATA.L   H'12345678


4.1.10    16-Bit/32-Bit Displacement

When data is accessed by 16-bit or 32-bit displacement, the pre-existing displacement value
is placed in the memory table. Loading the immediate data when the instruction is executed
transfers that value to the register and the data is accessed in the indirect indexed register
addressing mode.




                                                                                             HITACHI  25
```

## PDF page 30

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=30)

```text
Table 4.6   Displacement Accessing

Classification      SH-1/SH-2 CPU                   Example for Other CPU
16-bit displacement    MOV.W     @(disp,PC),R0                           MOV.W   @(H'1234,R1),R2
                            MOV.W     @(R0,R1),R2
                               ..................
                            .DATA.W   H'1234


4.2      Addressing Modes

Addressing modes and effective address calculation are described in table 4.7.

Table 4.7   Addressing Modes and Effective Addresses

Addressin       Instruction
g Mode          Format      Effective Addresses Calculation                 Formula
Direct          Rn                The effective address is register Rn. (The operand             —
register                         is the contents of register Rn.)
addressing
Indirect        @Rn            The effective address is the content of register Rn.              Rn
register                                 Rn                                    Rn
addressing
Post-           @Rn +          The effective address is the content of register Rn.              Rn
increment                        A constant is added to the content of Rn after the              (After the
indirect                         instruction is executed. 1 is added for a byte                  instruction is
register                         operation, 2 for a word operation, or 4 for a                   executed)
addressing                       longword operation.                                             Byte: Rn + 1

                                         Rn                            Rn                        ﬁ   Rn
                                             Rn + 1/2/4    +                                     Word: Rn + 2
                                                                                                 ﬁ   Rn
                                       1/2/4                                                     Longword:
                                                                                                 Rn + 4  ﬁ   Rn
Pre-            @–Rn           The effective address is the value obtained by                    Byte: Rn – 1
decrement                        subtracting a constant from Rn. 1 is subtracted for a           ﬁ   Rn
indirect                         byte operation, 2 for a  word operation, or 4 for a             Word: Rn – 2
register                         longword operation.                                             ﬁ   Rn
addressing
                                         Rn                                                      Longword:
                                             Rn – 1/2/4                                          Rn – 4  ﬁ   Rn
                                                            –           Rn – 1/2/4               (Instruction
                                                                                                 executed
                                       1/2/4                                                     with Rn after
                                                                                                 calculation)





26 HITACHI
```

## PDF page 31

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=31)

```text
Table 4.7   Addressing Modes and Effective Addresses (cont)

Addressin       Instruction
g Mode          Format      Effective Addresses Calculation                 Formula
Indirect        @(disp:4,        The effective address is Rn plus a 4-bit                       Byte: Rn +
register        Rn)              displacement (disp). The value of disp is zero-                disp
addressing                       extended, and remains the same for a byte
with                             operation, is doubled for a word operation, or is              Word: Rn +
displace-                        quadrupled for a longword operation.                           disp · 2
ment
                                         Rn                                                     Longword:
                                         disp
                                                  Rn + disp · 4
                                                               +             Rn
                                  (zero-extended)                      + disp × 1/2/4
                                                        ×

                                        1/2/4
Indirect        @(R0, Rn)      The effective address is the Rn value plus R0.                   Rn + R0
indexed
register                                Rn
addressing                                                 +
                                                                        Rn + R0

                                        R0

Indirect        @(disp:8,        The effective address is the GBR value plus an 8-              Byte: GBR +
GBR             GBR)             bit displacement (disp). The value of disp is zero-            disp
addressing                       extended, and remains the same for a byte
with                             operation, is doubled for a word operation, or is              Word: GBR +
displace-                        quadrupled for a longword operation.                           disp · 2
ment
                                         GBR                                                    Longword:
                                         disp
                              GBR
                GBR + disp ·
                                                               +       + disp × 1/2/4           4
                                  (zero-extended)
                                                        ×

                                         1/2/4
Indirect        @(R0,            The effective address is the GBR value plus R0.                GBR + R0
indexed         GBR)
GBR                                    GBR
addressing                                                 +
                                                                       GBR + R0

                                        R0








                                                                                                  HITACHI  27
```

## PDF page 32

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=32)

```text
Table 4.7   Addressing Modes and Effective Addresses (cont)

Addressin       Instruction
g Mode          Format      Effective Addresses Calculation                 Formula
PC relative     @(disp:8,        The effective address is the PC value plus an 8-bit             Word: PC +
addressing      PC)              displacement (disp). The value of disp is zero-                 disp · 2
with                             extended, and disp is doubled for a word                        Longword:
displace-                        operation, or is quadrupled for a longword                      PC &
ment                             operation. For a longword operation, the lowest                 H'FFFFFFFC
                                 two bits of the PC are masked.                                  + disp · 4

                                         PC
                                                          (for longword)
                                                       &
                                                                      PC + disp × 2
                                   H'FFFFFFFC               +                or
                                         disp
                      PC&H'FFFFFFFC
                                  (zero-extended)                       + disp × 4

                                                       x

                                         2/4

PC relative     disp:8           The effective address is the PC value sign-                     PC + disp ·
addressing                       extended with an 8-bit displacement (disp),                     2
                                 doubled, and added to the PC.

                                         PC

                                         disp
                 +       PC + disp × 2
                                  (sign-extended)
                                                        ×

                                          2
                disp:12         The effective address is the PC value sign-                      PC + disp ·
                                 extended with a 12-bit displacement (disp),                     2
                                 doubled, and added to the PC.

                                         PC

                                         disp
                 +       PC + disp × 2
                                  (sign-extended)
                                                         ×

                                           2









28 HITACHI
```

## PDF page 33

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=33)

```text
Table 4.7   Addressing Modes and Effective Addresses (cont)

Addressin       Instruction
g Mode          Format      Effective Addresses Calculation                 Formula
PC relative     Rn                The effective address is the register PC plus Rn.             PC + Rn
addressing
(cont)                                  PC

                                                           +            PC + R0

                                        R0

Immediate       #imm:8           The 8-bit immediate data (imm) for the TST, AND,               —
addressing                       OR, and XOR instructions are zero-extended.
                #imm:8           The 8-bit immediate data (imm) for the MOV, ADD,               —
                                 and CMP/EQ instructions are sign-extended.
                #imm:8           Immediate data (imm) for the TRAPA  instruction is             —
                                 zero-extended and is quadrupled.


4.3       Instruction Format

The instruction format table, table 4.8, refers to the source operand and the destination
operand. The meaning of the operand depends on the instruction code. The symbols are used
as follows:

•   xxxx: Instruction code
•  mmmm: Source register
•  nnnn: Destination register
•  iiii: Immediate data
•  dddd: Displacement























                                                                                                  HITACHI  29
```

## PDF page 34

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=34)

```text
Table 4.8    Instruction Formats

                                                 Source                  Destination
Instruction Formats                              Operand                 Operand          Example
0 format                                         ——  NOP
15                                               0
    xxxx                  xxxx    xxxxxxxx

n format                                              —                        nnnn: Direct        MOVT  Rn
                                                                         register

15                                               0        Control registernnnn: Direct             STS   MACH,Rn
    xxxx                  xxxx    xxxxnnnn       or system               register
                                                 register















































30 HITACHI
```

## PDF page 35

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=35)

```text
Table 4.8   Instruction Formats (cont)

                                                 Source                  Destination
Instruction Formats                              Operand                 Operand          Example
n format (cont)                                 Control register         nnnn: Indirect           STC.L  SR,@-Rn
                                                 or system               pre-decrement
                                                 register                register
m format                                                   mmmm: Direct  Control register         LDC    Rm,SR
                                                 register                or system register
15                                               0           mmmm: IndirectControl register       LDC.L  @Rm+,SR
   xxxx                              xxxxmmmmxxxxpost-increment          or system register
                                                 register
                                                 mmmm: Direct            —              JMP   @Rm
                                                 register
                                                 mmmm: PC                —              BRAF   Rm
                                                 relative using Rm
nm format                                                 mmmm: Direct   nnnn: Direct             ADD    Rm,Rn
                                                 register                register
15                                               0mmmm: Direct           nnnn: Indirect           MOV.L  Rm,@Rn
   xxxx                              xxxxnnnnmmmmregister                register

                                                 mmmm: Indirect          MACH, MACL    MAC.W
                                                 post-increment                                   @Rm+,@Rn+
                                                 register
                                                 (multiply/
                                                 accumulate)
                                                 nnnn*: Indirect
                                                 post-increment
                                                 register
                                                 (multiply/
                                                 accumulate)
                                                 mmmm: Indirect          nnnn: Direct             MOV.L  @Rm+,Rn
                                                 post-increment          register
                                                 register
                                                 mmmm: Direct            nnnn: Indirect           MOV.L  Rm,@-Rn
                                                 register                pre-decrement
                                                                         register
                                                 mmmm: Direct            nnnn: Indirect           MOV.L
                                                 register                indexed register         Rm,@(R0,Rn)
md format                                        mmmmdddd:               R0 (Direct               MOV.B
15                                          0    indirect register       register)                @(disp,Rm),R0
   xxxx                           ddddxxxxmmmm   with
                                                 displacement







                                                                                                         HITACHI  31
```

## PDF page 36

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=36)

```text
nd4 format                                      R0 (Direct              nnnndddd:                MOV.B
15                                       0      register)               Indirect register        R0,@(disp,Rn)
   xxxx      xxxx     nnnn     dddd                                     with displacement

Note:   In multiply/accumulate instructions, nnnn is the source register.



























































32 HITACHI
```

## PDF page 37

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=37)

```text
Table 4.8   Instruction Formats (cont)

                                                Source                 Destination
Instruction Formats                             Operand                Operand          Example
nmd format                                      mmmm: Direct           nnnndddd:                 MOV.L
15                                          0   register               Indirect register         Rm,@(disp,Rn)
   xxxx                           ddddnnnnmmmm                         with displacement

                                                mmmmdddd:              nnnn: Direct              MOV.L
                                                Indirect register      register                  @(disp,Rm),Rn
                                                with
                                                displacement
d format                                        dddddddd:              R0 (Direct                MOV.L
15                                      0       Indirect GBR           register)                 @(disp,GBR),R0
   xxxx      xxxx              dddddddd         with
                                                displacement
                                                R0(Direct              dddddddd:                 MOV.L
                                                register)              Indirect GBR with         R0,@(disp,GBR)
                                                                       displacement
                                                dddddddd: PC           R0 (Direct                MOVA
                                                relative with          register)                 @(disp,PC),R0
                                                displacement
                                                dddddddd: PC           —              BF     label
                                                relative
d12 format                                      ddddddddddd            —              BRA    label
15                                       0      d: PC relative                                   (label = disp
   xxxx     dddd      dddd    dddd                                                               + PC)

nd8 format                                      dddddddd: PC           nnnn: Direct              MOV.L
15                                       0      relative with          register                  @(disp,PC),Rn
   xxxx      nnnn     dddd      dddd            displacement

i format                                                    iiiiiiii: Immediate  Indirect indexedAND.B
                                                                       GBR                       #imm,@(R0,GBR)
15                                               0iiiiiiii: Immediate    R0 (Direct              AND    #imm,R0
   xxxx      xxxx                  i i i ii i i i                      register)

                                                iiiiiiii: Immediate    —              TRAPA  #imm
ni format                                       iiiiiiii: Immediate    nnnn: Direct              ADD    #imm,Rn
15                                               0                     register
   xxxx      nnnn     i i i i   i i i i










                                                                                                        HITACHI  33
```
