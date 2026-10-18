# Front matter

SH-1/SH-2 Programming Manual — Hitachi, September 3, 1996.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=1)

Physical PDF pages 1–17. Text extraction; consult the PDF for figures.

## PDF page 1

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=1)

```text
               SuperH RISC Engine
                       SH-1/SH-2

                    Programming Manual


























September 3, 1996
Hitachi America Ltd.
```

## PDF page 2

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=2)

```text
[No extractable text on this page.]
```

## PDF page 3

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=3)

```text
                                                   Notice


When using this document, keep the following in mind:

1.  This document may, wholly or partially, be subject to change without notice.

2.  All rights are reserved:  No one is permitted to reproduce or duplicate, in any form, the
    whole or part of this document without Hitachi’s permission.

3.  Hitachi will not be held responsible for any damage to the user that may result from
    accidents or any other reasons during operation of the user’s unit according to this
    document.

4.  Circuitry and other examples described herein are meant merely to indicate the
    characteristics and performance of Hitachi’s semiconductor products. Hitachi assumes no
    responsibility for any intellectual property claims or other problems that may result from
    applications based on the examples described herein.

5.  No license is granted by implication or otherwise under any patents or other rights of any
    third party or Hitachi, Ltd.

6.  MEDICAL APPLICATIONS: Hitachi’s products are not authorized for use in MEDICAL
    APPLICATIONS without the written consent of the appropriate officer of Hitachi’s sales
    company. Such use includes, but is not limited to, use in life support systems. Buyers of
    Hitachi’s products are requested to notify the relevant Hitachi sales offices when planning
    to use the products in MEDICAL APPLICATIONS.
```

## PDF page 4

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=4)

```text
[No extractable text on this page.]
```

## PDF page 5

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=5)

```text
                                             Introduction


The SuperH RISC engine family incorporates a RISC (Reduced Instruction Set Computer)
type CPU.  A basic instruction can be executed in one clock cycle, realizing high
performance operation.  A built-in multiplier can execute multiplication and addition as
quickly as DSP.

The SuperH RISC engine has SH-I CPU, SH-2 CPU, and SH-3 CPU cores.

The SH-1 CPU, SH-2 CPU and SH-3 CPU have an instruction system with upward
compatibility at the binary level.



                 SH-3 CPU                                                        MMU support:
                                                                                68 instructions
                 SH-2 CPU                         Operation instruction
                                                      enhancement:
                 SH-1 CPU
                            62 instructions
                 56 basic instructions


Refer to the programming manual for the method of executing the instructions or for the
architecture.  You can also refer to this programming manual to know the operation of the
pipe line, which is one of the features of the RISC CPU.

This programming manual describes in detail the instructions for the SH-1 CPU and SH-2
CPU instructions.  For the SH-3 CPU, refer to the separate volume of SH-3 CPU programming
manual.

For the hardware, refer to individual hardware manuals for each unit.
























                                                                                                   HITACHI  1
```

## PDF page 6

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=6)

```text
Organization of This Manual

Table 1 describes how this manual is organized. Table 2 lists the relationships between the
items and the sections listed within this manual that cover those items.

Table 1     Manual Organization

Category          Section Title       Contents
Introduction        1. Features                 CPU features
Architecture (1)          2. Register                   Types and configuration of general registers,
                            Configuration               control registers and system registers
                            3. Data Formats            Data formats for registers and memory
Introduction to             4. Instruction              Instruction features, addressing modes, and
instructions                Features                    instruction formats
                            5. Instruction Sets       Summary of instructions by category and list in
                                                        alphabetic order
Detailed information        6. Instruction              Operation of each instruction in alphabetical
on instructions             Descriptions                order
Architecture (2)          7. Pipeline Operation   Pipeline flow, and pipeline flows with operation
                                                        for each instruction
Instruction code       Appendixes:                      Operation code map
                            Instruction Code

































2 HITACHI
```

## PDF page 7

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=7)

```text
Table 2     Subjects and Corresponding Sections

Category        Topic                             Section Title
Introduction and       CPU features                                         1.          Features
features
                       Instruction features                          4.1  RISC-Type Instruction Set
                       Pipelines                                        7.1  Basic Configuration of
                                                                                      Pipelines
                                                                                7.2  Slot and Pipeline Flow
Architecture        Register configuration                           2.    Register Configuration
                       Data formats                                                    3.     Data Formats
                       Pipeline operation                                7.    Pipeline Operation
Introduction to        Instruction features                             4.   Instruction Features
instructions
                       Addressing modes                                 4.2  Addressing Modes
                       Instruction formats                            4.3  Instruction Formats
List of                Instruction sets                              5.1  Instruction Set by
instructions                                                                          Classification
                                                                                5.2  Instruction Set in
                                                                                      Alphabetical Order
                                                                                Appendix A.1  Instruction Set by
                                                                                                   Addressing Mode
                                                                                Appendix A.2  Instruction Set by
                                                                                                   Instruction Format
                       Instruction code                                 Appendix A.3  Instruction Set in
                                                                                                   Order by
                                                                                                   Instruction Code
                                                                                Appendix A.4  Operation Code
                                                                                                   Map
Detailed               Detailed information on instruction                      6.   Instruction Description
information on         operation                                                7.7  Instruction Pipeline
instructions                                                                          Operations

                       Number of instruction execution states       7.3  Number of Instruction
                                                                                      Execution States













                                                                                                             HITACHI  3
```

## PDF page 8

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=8)

```text
Functions Listed by CPU Type

This manual is common for both the SH-1 and SH-2 CPU. However, not all CPUs can use all
the instructions and functions. Table 3 lists the usable functions by CPU type.

Table 3      Functions by CPU Type

Item                                SH-1 CPU      SH-2 CPU
Instructions         BF/S             No            Yes
                             BRAF              No             Yes
                             BSRF              No             Yes
                             BT/S             No            Yes
                             DMULS.L          No            Yes
                             DMULU.L           No            Yes
                             DT               No            Yes
                             MAC.L             No            Yes
                             MAC.W*1 (MAC)*2         16 x 16 + 42 ﬁ            16 x 16 + 64 ﬁ   64
                                                         42
                             MUL.L                           No                          Yes
                             All others                      Yes                       Yes
States for                   16 x 16 ﬁ   32              Executed in 1–        Executed in 1–3*3states
multiplication operation     (MULS.W,                    3* 3 states
                             MULU.W)*2
                             32 x 32 ﬁ   32 (MUL.L)   No                         Executed in 2–4 *3states
                             32 x 32 ﬁ   64              No                         Executed in 2–4 *3states
                             (DMULS.L, DMULU.L)
States for multiply and      16 x 16 + 42 ﬁ   42         Executed in           No
accumulate operation         (SH-1, MAC.W)               3/(2)*3 states
                             16 x 16 + 64 ﬁ   64         No                         Executed in  states 3/(2)*3
                             (SH-2, MAC.W)
                             32 x 32 + 64 ﬁ   64         No                        Executed in 2–4 states
                             (MAC.L)                                           3/(2~4)*3
Notes:  1.  MAC.W works differently on different LSIs.
         2.   MAC and MAC.W are the same.  MULS is also the same as MULS.W and MULU the
            same as MULU.W.
         3.   The normal minimum number of execution cycles (The number in parentheses in the
            number in contention with preceding/following instructions).









4 HITACHI
```

## PDF page 9

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=9)

```text
                                                                                     Contents


Section 1   Features..............................................................................................................................................    14

Section 2   Register Configuration............................................................................................................    16
2.1 General Registers................................................................................................................................................   16
2.2 Control Registers.................................................................................................................................................   16
2.3 System Registers.................................................................................................................................................   19
2.4  Initial Values of Registers.............................................................................................................................   19

Section 3   Data Formats..................................................................................................................................    21
3.1 Data Format in Registers................................................................................................................................   21
3.2 Data Format in Memory..................................................................................................................................   21
3.3 Immediate Data Format...................................................................................................................................   22

Section 4   Instruction Features...................................................................................................................    23
4.1 RISC-Type Instruction Set..............................................................................................................................   23
             4.1.1 16-Bit Fixed Length..........................................................................................................................   23
             4.1.2 One Instruction/Cycle......................................................................................................................   23
             4.1.3 Data Length..........................................................................................................................................   23
             4.1.4 Load-Store Architecture..................................................................................................................   23
             4.1.5 Delayed Branch Instructions.........................................................................................................   23
             4.1.6 Multiplication/Accumulation Operation.................................................................................   24
             4.1.7 T Bit.........................................................................................................................................................    24
             4.1.8 Immediate Data..................................................................................................................................   25
             4.1.9 Absolute Address................................................................................................................................    25
             4.1.10 16-Bit/32-Bit Displacement........................................................................................................   25
4.2 Addressing Modes...............................................................................................................................................   26
4.3 Instruction Format...............................................................................................................................................   29

Section 5   Instruction Set................................................................................................................................    34
5.1 Instruction Set by Classification..................................................................................................................   34
             5.1.1 Data Transfer Instructions..............................................................................................................   39
             5. 1.2 Arithmetic Instructions...................................................................................................................   42
             5.1.3 Logic Operation Instructions.........................................................................................................   44
             5.1.4 Shift Instructions.................................................................................................................................    45
             5.1.5 Branch Instructions............................................................................................................................   46
             5.1.6 System Control Instructions..........................................................................................................   47
5.2 Instruction Set in Alphabetical Order........................................................................................................   48

Section 6   Instruction Descriptions.........................................................................................................    57
6.1 Sample Description (Name): Classification..........................................................................................   57


                                                                                                                                                                              HITACHI  5
```

## PDF page 10

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=10)

```text
6.2 ADD (ADD Binary): Arithmetic Instruction..........................................................................................    60
6.3 ADDC (ADD with Carry): Arithmetic Instruction..............................................................................    61
6.4 ADDV (ADD with V Flag Overflow Check): Arithmetic Instruction........................................    62
6.5 AND (AND Logical): Logic Operation Instruction............................................................................    63
6.6 BF (Branch if False): Branch Instruction...............................................................................................   65
6.7 BF/S (Branch if False with Delay Slot): Branch Instruction (SH-2 CPU)............................   66
6.8 BRA (Branch): Branch Instruction............................................................................................................   68
6.9 BRAF (Branch Far): Branch Instruction (SH-2 CPU).....................................................................   70
6.10 BSR (Branch to Subroutine): Branch Instruction.............................................................................   72
6.11 BSRF (Branch to Subroutine Far): Branch Instruction (SH-2 CPU)......................................   74
6.12 BT (Branch if True): Branch Instruction..............................................................................................   75
6.13 BT/S (Branch if True with Delay Slot): Branch Instruction (SH-2 CPU)............................   76
6.14 CLRMAC (Clear MAC Register): System Control Instruction.................................................   78
6.15 CLRT (Clear T Bit): System Control Instruction.............................................................................   79
6.16 CMP/cond (Compare Conditionally): Arithmetic Instruction....................................................   80
6.17 DIV0S (Divide Step 0 as Signed): Arithmetic Instruction...........................................................   84
6.18 DIV0U (Divide Step 0 as Unsigned): Arithmetic Instruction.....................................................    85
6.19 DIV1 (Divide Step 1): Arithmetic Instruction....................................................................................   86
6.20 DMULS.L (Double-Length Multiply as Signed): Arithmetic Instruction (SH-2 CPU)..   91
6.21 DMULU.L (Double-Length Multiply as Unsigned): Arithmetic Instruction
          (SH-2 CPU).....................................................................................................................................................   93
6.22 DT (Decrement and Test): Arithmetic Instruction (SH-2 CPU)...............................................   95
6.23 EXTS (Extend as Signed): Arithmetic Instruction...........................................................................   96
6.24 EXTU (Extend as Unsigned): Arithmetic Instruction.....................................................................    97
6.25 JMP (Jump): Branch Instruction...............................................................................................................   98
6.26 JSR (Jump to Subroutine): Branch Instruction..................................................................................   99
6.27 LDC (Load to Control Register): System Control Instruction....................................................  101
6.28 LDS (Load to System Register): System Control Instruction....................................................  103
6.29 MAC.L (Multiply and Accumulate Long): Arithmetic Instruction (SH-2 CPU)...............  105
6.30 MAC (Multiply and Accumulate): Arithmetic Instruction (SH-1 CPU)...............................  108
6.31 MAC.W (Multiply and Accumulate Word): Arithmetic Instruction.......................................  109
6.32 MOV (Move Data): Data Transfer Instruction...................................................................................  112
6.33 MOV (Move Immediate Data): Data Transfer Instruction...........................................................  117
6.34 MOV (Move Peripheral Data): Data Transfer Instruction............................................................  119
6.35 MOV (Move Structure Data): Data Transfer Instruction..............................................................  122
6.36 MOVA (Move Effective Address): Data Transfer Instruction....................................................  125
6.37 MOVT (Move T Bit): Data Transfer Instruction...............................................................................  126
6.38 MUL.L (Multiply Long): Arithmetic Instruction (SH-2 CPU)...................................................  127
6.39 MULS.W (Multiply as Signed Word): Arithmetic Instruction...................................................  128
6.40 MULU.W (Multiply as Unsigned Word): Arithmetic Instruction.............................................  129
6.41 NEG (Negate): Arithmetic Instruction...................................................................................................  130
6.42 NEGC (Negate with Carry): Arithmetic Instruction........................................................................  131
6.43 NOP (No Operation): System Control Instruction............................................................................  132



6 HITACHI
```

## PDF page 11

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=11)

```text
6.44 NOT (NOT—Logical Complement): Logic Operation Instruction........................................... 133
6.45 OR (OR Logical) Logic Operation Instruction................................................................................... 134
6.46 ROTCL (Rotate with Carry Left): Shift Instruction......................................................................... 136
6.47 ROTCR (Rotate with Carry Right): Shift Instruction..................................................................... 137
6.48 ROTL (Rotate Left): Shift Instruction.................................................................................................... 138
6.49 ROTR (Rotate Right): Shift Instruction................................................................................................ 139
6.50 RTE (Return from Exception): System Control Instruction......................................................... 140
6.51 RTS (Return from Subroutine): Branch Instruction......................................................................... 141
6.52 SETT (Set T Bit): System Control Instruction................................................................................... 143
6.53 SHAL (Shift Arithmetic Left): Shift Instruction................................................................................ 144
6.54 SHAR (Shift Arithmetic Right): Shift Instruction............................................................................ 145
6.55 SHLL (Shift Logical Left): Shift Instruction....................................................................................... 146
6.56 SHLLn (Shift Logical Left n Bits): Shift Instruction...................................................................... 147
6.57 SHLR (Shift Logical Right): Shift Instruction................................................................................... 149
6.58 SHLRn (Shift Logical Right n Bits): Shift Instruction................................................................... 150
6.59 SLEEP (Sleep): System Control Instruction....................................................................................... 152
6.60 STC (Store Control Register): System Control Instruction.......................................................... 153
6.61 STS (Store System Register): System Control Instruction.......................................................... 155
6.62 SUB (Subtract Binary): Arithmetic Instruction................................................................................. 157
6.63 SUBC (Subtract with Carry): Arithmetic Instruction...................................................................... 158
6.64 SUBV (Subtract with V Flag Underflow Check): Arithmetic Instruction............................. 159
6.65 SWAP (Swap Register Halves): Data Transfer Instruction......................................................... 160
6.66 TAS (Test and Set): Logic Operation Instruction............................................................................. 161
6.67 TRAPA (Trap Always): System Control Instruction....................................................................... 162
6.68 TST (Test Logical): Logic Operation Instruction............................................................................. 163
6.69 XOR (Exclusive OR Logical): Logic Operation Instruction........................................................ 165
6.70 XTRCT (Extract): Data Transfer Instruction....................................................................................... 167

Section 7   Pipeline Operation......................................................................................................................  168
7.1 Basic Configuration of Pipelines................................................................................................................. 168
7.2 Slot and Pipeline Flow..................................................................................................................................... 169
           7.2.1 Instruction Execution........................................................................................................................ 169
           7.2.2 Slot Sharing.......................................................................................................................................... 169
           7.2.3 Slot Length............................................................................................................................................ 170
7.3 Number of Instruction Execution States................................................................................................... 171
7.4 Contention Between Instruction Fetch (IF) and Memory Access (MA)................................. 172
           7.4.1 Basic Operation When IF and MA are in Contention...................................................... 172
           7.4.2 The Relationship Between IF and the Location of Instructions in On-Chip
                          ROM/RAM or On-Chip Memory......................................................................................... 173
           7.4.3 Relationship Between Position of Instructions Located in On-Chip
                         ROM/RAM or On-Chip Memory and Contention Between IF and MA............. 174
7.5 Effects of Memory Load Instructions on Pipelines............................................................................. 175
7.6 Programming Guide........................................................................................................................................... 176



                                                                                                                                                 HITACHI  7
```

## PDF page 12

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=12)

```text
7.7 Operation of Instruction Pipelines..............................................................................................................  177
             7.7.1 Data Transfer Instructions..............................................................................................................  184
             7.7.2 Arithmetic Instructions....................................................................................................................  186
             7.7.3 Logic Operation Instructions........................................................................................................  237
             7.7.4 Shift Instructions................................................................................................................................  238
             7.7.5 Branch Instructions...........................................................................................................................  239
             7.7.6 System Control Instructions..........................................................................................................  242
             7.7.7 Exception Processing.......................................................................................................................  248

Appendix A   Instruction Code...................................................................................................................  251
A.1 Instruction Set by Addressing Mode.........................................................................................................  251
             A.1.1 No Operand..........................................................................................................................................  253
             A.1.2 Direct Register Addressing..........................................................................................................  254
             A.1.3 Indirect Register Addressing.......................................................................................................  257
             A.1.4 Post Increment Indirect Register Addressing......................................................................  257
             A.1.5 Pre Decrement Indirect Register Addressing......................................................................  258
             A.1.6 Indirect Register Addressing with Displacement..............................................................  259
             A.1.7 Indirect Indexed Register Addressing.....................................................................................  259
             A.1.8 Indirect GBR Addressing with Displacement.....................................................................  260
             A.1.9 Indirect Indexed GBR Addressing............................................................................................  260
             A.1.10 PC Relative Addressing with Displacement.....................................................................  260
             A.1.11 PC Relative Addressing with Rm..........................................................................................  261
             A.1.12 PC Relative Addressing..............................................................................................................  261
             A.1.13 Immediate..........................................................................................................................................  262
A.2 Instruction Sets by Instruction Format.....................................................................................................  262
             A.2.1 0 Format................................................................................................................................................  264
             A.2.2 n Format................................................................................................................................................  265
             A.2.3 m Format..............................................................................................................................................  267
             A.2.4 nm Format............................................................................................................................................  269
             A.2.5 md Format............................................................................................................................................  272
             A.2.6 nd4 Format...........................................................................................................................................  272
             A.2.7 nmd Format.........................................................................................................................................  272
             A.2.8 d Format................................................................................................................................................  273
             A.2.9 d12 Format...........................................................................................................................................  274
             A.2.10 nd8 Format........................................................................................................................................  274
             A.2.11 i Format..............................................................................................................................................  274
             A.2.12 ni Format............................................................................................................................................  275
A.3 Instruction Set in Order by Instruction Code........................................................................................  276
A.4 Operation Code Map........................................................................................................................................  284
Appendix B Pipeline Operation and Contention.........................................................................................  288

Figures
Figure 2.1   General Registers.............................................................................................................................   16



8 HITACHI
```

## PDF page 13

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=13)

```text
Figure 2.2   Control Registers...............................................................................................................................   18
Figure 2.3   System Registers...............................................................................................................................   19
Figure 3.1   Longword Operand............................................................................................................................    21
Figure 3.2   Byte, Word, and Longword Alignment...................................................................................   21
Figure 3.3   Byte, Word, and Longword Alignment in little endian format (SH7604 only)...  22
Figure 6.1   Using R0 after MOV........................................................................................................................ 119
Figure 6.2   Using R0 after MOV........................................................................................................................ 122
Figure 6.3   Rotate with Carry Left.................................................................................................................... 136
Figure 6.4   Rotate with Carry Right................................................................................................................. 137
Figure 6.5   Rotate Left........................................................................................................................................... 138
Figure 6.6   Rotate Right........................................................................................................................................ 139
Figure 6.7   Shift Arithmetic Left....................................................................................................................... 144
Figure 6.8   Shift Arithmetic Right.................................................................................................................... 145
Figure 6.9   Shift Logical Left.............................................................................................................................. 146
Figure 6.10   Shift Logical Left n Bits............................................................................................................. 147
Figure 6.11   Shift Logical Right........................................................................................................................ 149
Figure 6.12   Shift Logical Right n Bits.......................................................................................................... 150
Figure 6.13   Extract.................................................................................................................................................. 167
Figure 7.1   Basic Structure of Pipeline Flow.............................................................................................. 168
Figure 7.2   Impossible Pipeline Flow 1.......................................................................................................... 169
Figure 7.3   Impossible Pipeline Flow 2.......................................................................................................... 169
Figure 7.4   Slots Requiring Multiple Cycles............................................................................................... 170
Figure 7.5   How Instruction Execution States Are Counted................................................................. 171
Figure 7.6   Operation When IF and MA Are in Contention................................................................. 172
Figure 7.7   Relationship Between IF and Location of Instructions in On-Chip Memory....... 174
Figure 7.8   Relationship Between the Location of Instructions in On-Chip Memory and
                       Contention Between IF and MA.............................................................................................. 175
Figure 7.9   Effects of Memory Load Instructions on the Pipeline..................................................... 176
Figure 7.10   Register-Register Transfer Instruction Pipeline............................................................... 184
Figure 7.11   Memory Load Instruction Pipeline......................................................................................... 185
Figure 7.12   Memory Store Instruction Pipeline........................................................................................ 186
Figure 7.13   Pipeline for Arithmetic Instructions between Registers Except
                       Multiplication Instructions.......................................................................................................... 187
Figure 7.14   Multiply/Accumulate Instruction Pipeline......................................................................... 188
Figure 7.15   Unrelated Instructions between MAC.W Instructions................................................... 189
Figure 7.16   Consecutive MAC.Ws without Misalignment.................................................................. 189
Figure 7.17   MA and IF Contention................................................................................................................. 190
Figure 7.18   MULS.W Instruction Immediately After a MAC.W Instruction............................... 191
Figure 7.19   STS (Register) Instruction Immediately After a MAC.W Instruction................... 192
Figure 7.20   STS.L (Memory) Instruction Immediately After a MAC.W Instruction............... 193
Figure 7.21   LDS (Register) Instruction Immediately After a MAC.W Instruction.................. 194
Figure 7.22   LDS.L (Memory) Instruction Immediately After a MAC.W Instruction.............. 195
Figure 7.23   Multiply/Accumulate Instruction Pipeline......................................................................... 196



                                                                                                                                                     HITACHI  9
```

## PDF page 14

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=14)

```text
Figure 7.24   MAC.W Instruction That Immediately Follows Another MAC.W instruction.. 197
Figure 7.25   Consecutive MAC.Ws with Misalignment........................................................................  197
Figure 7.26   MA and IF Contention.................................................................................................................  198
Figure 7.27   MAC.L Instructions Immediately After a MAC.W Instruction.................................  198
Figure 7.28   MULS.W Instruction Immediately After a MAC.W Instruction..............................  199
Figure 7.29   DMULS.L Instructions Immediately After a MAC.W Instruction...........................  199
Figure 7.30   STS (Register) Instruction Immediately After a MAC.W Instruction..................  200
Figure 7.31   STS.L (Memory) Instruction Immediately After a MAC.W Instruction..............  201
Figure 7.32   LDS (Register) Instruction Immediately After a MAC.W Instruction..................  202
Figure 7.33   LDS.L (Memory) Instruction Immediately After a MAC.W Instruction..............  203
Figure 7.34   Multiply/Accumulate Instruction Pipeline.........................................................................  204
Figure 7.35   MAC.L Instruction Immediately After Another MAC.L Instruction.......................  205
Figure 7.36   Consecutive MAC.Ls with Misalignment..........................................................................  205
Figure 7.37   MA and IF Contention.................................................................................................................  206
Figure 7.38   MAC.W Instruction Immediately After a MAC.L Instruction...................................  207
Figure 7.39  DMULS.L Instruction Immediately After a MAC.L Instruction................................  208
Figure 7.40   MULS.W Instruction Immediately After a MAC.L Instruction................................  209
Figure 7.41   STS (Register) Instruction Immediately After a MAC.L Instruction....................  210
Figure 7.42   STS.L (Memory) Instruction Immediately After a MAC.L Instruction................  211
Figure 7.43   LDS (Register) Instruction Immediately After a MAC.L Instruction....................  212
Figure 7.44   LDS.L (Memory) Instruction Immediately After a MAC.L Instruction................  213
Figure 7.45   Multiplication Instruction Pipeline........................................................................................  214
Figure 7.46   MAC.W Instruction Immediately After a MULS.W Instruction..............................  215
Figure 7.47   MULS.W Instruction Immediately After Another MULS.W Instruction..............  216
Figure 7.48   MULS.W Instruction Immediately After Another MULS.W Instruction (IF
                  and MA Contention)...................................................................................................................... 217
Figure 7.49   STS (Register) Instruction Immediately After a MULS.W Instruction................  218
Figure 7.50   STS.L (Memory) Instruction Immediately After a MULS.W Instruction............  219
Figure 7.51   LDS (Register) Instruction Immediately After a MULS.W Instruction...............  220
Figure 7.52   LDS.L (Memory) Instruction Immediately After a MULS.W Instruction...........  221
Figure 7.53   Multiplication Instruction Pipeline........................................................................................  222
Figure 7.54   MAC.W Instruction Immediately After a MULS.W Instruction..............................  223
Figure 7.55   MAC.L Instruction Immediately After a MULS.W Instruction................................  223
Figure 7.56   MULS.W Instruction Immediately After Another MULS.W Instruction..............  224
Figure 7.57   MULS.W Instruction Immediately After Another MULS.W Instruction (IF
                  and MA contention)....................................................................................................................... 224
Figure 7.58   DMULS.L Instruction Immediately After a MULS.W Instruction..........................  225
Figure 7.59   STS (Register) Instruction Immediately After a MULS.W Instruction................  226
Figure 7.60   STS.L (Memory) Instruction Immediately After a MULS.W Instruction............  227
Figure 7.61   LDS (Register) Instruction Immediately After a MULS.W Instruction...............  228
Figure 7.62   LDS.L (Memory) Instruction Immediately After a MULS.W Instruction...........  229
Figure 7.63   Multiplication Instruction Pipeline........................................................................................  229
Figure 7.64   MAC.L Instruction Immediately After a DMULS.L Instruction...............................  231



10 HITACHI
```

## PDF page 15

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=15)

```text
Figure 7.65   MAC.W Instruction Immediately After a DMULS.L Instruction............................. 231
Figure 7.66   DMULS.L Instruction Immediately After Another DMULS.L Instruction........... 232
Figure 7.67   DMULS.L Instruction Immediately After Another DMULS.L Instruction (IF
                     and MA Contention)...................................................................................................................... 233
Figure 7.68   MULS.W Instruction Immediately After a DMULS.L Instruction........................... 233
Figure 7.69   MULS.W Instruction Immediately After a DMULS.L Instruction (IF and
                     MA Contention)............................................................................................................................... 234
Figure 7.70   STS (Register) Instruction Immediately After a DMULS.L Instruction............... 234
Figure 7.71   STS.L (Memory) Instruction Immediately After a DMULS.L Instruction........... 235
Figure 7.72   LDS (Register) Instruction Immediately After a DMULS.L Instruction.............. 236
Figure 7.73   LDS.L (Memory) Instruction Immediately After a DMULS.L Instruction.......... 237
Figure 7.74   Register-Register Logic Operation Instruction Pipeline.............................................. 237
Figure 7.75   Memory Logic Operation Instruction Pipeline................................................................. 238
Figure 7.76   TAS Instruction Pipeline............................................................................................................. 238
Figure 7.77   Shift Instruction Pipeline............................................................................................................ 239
Figure 7.78   Branch Instruction When Condition is Satisfied............................................................. 240
Figure 7.79   Branch Instruction When Condition is Not Satisfied.................................................... 240
Figure 7.80   Branch Instruction When Condition is Satisfied............................................................. 241
Figure 7.81   Branch Instruction When Condition is Not Satisfied.................................................... 241
Figure 7.82   Unconditional Branch Instruction Pipeline........................................................................ 242
Figure 7.83   System Control ALU Instruction Pipeline.......................................................................... 243
Figure 7.84   LDC.L Instruction Pipeline........................................................................................................ 243
Figure 7.85   STC.L Instruction Pipeline......................................................................................................... 243
Figure 7.86   LDS.L Instruction (PR) Pipeline............................................................................................. 244
Figure 7.87   STS.L Instruction (PR) Pipeline............................................................................................. 244
Figure 7.88   Register ﬁ   MAC Transfer Instruction Pipeline............................................................... 245
Figure 7.89   Memory ﬁ   MAC Transfer Instruction Pipeline............................................................... 245
Figure 7.90   MAC ﬁ   Register Transfer Instruction Pipeline............................................................... 246
Figure 7.91   MAC ﬁ   Memory Transfer Instruction Pipeline............................................................... 246
Figure 7.92   RTE Instruction Pipeline............................................................................................................. 247
Figure 7.93   TRAP Instruction Pipeline......................................................................................................... 247
Figure 7.94   SLEEP Instruction Pipeline....................................................................................................... 248
Figure 7.95   Interrupt Exception Processing Pipeline.............................................................................. 248
Figure 7.96   Address Error Exception Processing Pipeline................................................................... 249
Figure 7.97   Illegal Instruction Exception Processing Pipeline.......................................................... 249

Tables
Table 1 Manual Organization...............................................................................................................................    2
Table 2 Subjects and Corresponding Sections.............................................................................................    3
Table 3 Functions by CPU Type.........................................................................................................................     4
Table 1.1 SH-1  and SH-2 CPU Features........................................................................................................   15
Table 2.1 Initial Values of Registers.................................................................................................................   20
Table 4.1 Sign Extension of Word Data..........................................................................................................   23



                                                                                                                                           HITACHI  11
```

## PDF page 16

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=16)

```text
Table 4.2 Delayed Branch Instructions............................................................................................................   24
Table 4.3 T Bit............................................................................................................................................................    24
Table 4.4 Immediate Data Accessing..............................................................................................................   25
Table 4.5 Absolute Address...................................................................................................................................    25
Table 4.6 Displacement Accessing...................................................................................................................   26
Table 4.7 Addressing Modes and Effective Addresses.............................................................................   26
Table 4.8 Instruction Formats...............................................................................................................................   30
Table 5.1 Classification of Instructions...........................................................................................................   34
Table 5.2 Instruction Code Format....................................................................................................................   38
Table 5.3 Data Transfer Instructions.................................................................................................................    40
Table 5.4 Arithmetic Instructions.......................................................................................................................   42
Table 5.5 Logic Operation Instructions............................................................................................................    44
Table 5.5 Logic Operation Instructions (cont).............................................................................................   45
Table 5.6 Shift Instructions....................................................................................................................................    45
Table 5.7 Branch Instructions...............................................................................................................................   46
Table 5.8 System Control Instructions.............................................................................................................   47
Table 5.9 Instruction Set.........................................................................................................................................   49
Table 6.1 CMP Mnemonics..................................................................................................................................   81
Table 7.1 Format for the Number of Stages and Execution States for Instructions....................  177
Table 7.2 Number of Instruction Stages and Execution States............................................................  177
Table A.1 Instruction Set by Addressing Mode...........................................................................................  252
Table A.2 No Operand.............................................................................................................................................  253
Table A.3 Destination Operand Only................................................................................................................  254
Table A.4 Source and Destination Operand..................................................................................................  254
Table A.5 Load and Store with Control Register or System Register..............................................  256
Table A.6 Destination Operand Only................................................................................................................  257
Table A.7 Data Transfer with Direct Register Addressing.....................................................................  257
Table A.8 Multiply/Accumulate Operation................................................................................................... 257
Table A.9 Data Transfer from Direct Register Addressing.....................................................................  258
Table A.10 Load to Control Register or System Register......................................................................  258
Table A.11 Data Transfer from Direct Register Addressing..................................................................  258
Table A.12 Store from Control Register or System Register................................................................  259
Table A.13 Indirect Register Addressing with Displacement...............................................................  259
Table A.14 Indirect Indexed Register Addressing......................................................................................  259
Table A.15 Indirect GBR Addressing with Displacement......................................................................  260
Table A.16 Indirect Indexed GBR Addressing.............................................................................................  260
Table A.17 PC Relative Addressing with Displacement........................................................................  260
Table A.18 PC Relative Addressing with Rm..............................................................................................  261
Table A.19 PC Relative Addressing.................................................................................................................  261
Table A.20 Arithmetic Logical Operation with Direct Register Addressing.................................  262
Table A.21 Specify Exception Processing Vector......................................................................................  262
Table A.22 Instruction Sets by Format............................................................................................................  263
Table A.23 0 Format.................................................................................................................................................  264



12 HITACHI
```

## PDF page 17

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=17)

```text
Table A.24 Direct Register Addressing............................................................................................................ 265
Table A.25 Direct Register Addressing (Store with Control and System Registers)................. 265
Table A.26 Indirect Register Addressing......................................................................................................... 266
Table A.27 Pre Decrement Indirect Register................................................................................................ 266
Table A.28 Direct Register Addressing (Load with Control and System Registers)................. 267
Table A.29 Indirect Register................................................................................................................................. 267
Table A.30 Post Increment Indirect Register................................................................................................ 267
Table A.31 PC Relative Addressing with Rm.............................................................................................. 268
Table A.32 Direct Register Addressing............................................................................................................ 269
Table A.33 Indirect Register Addressing......................................................................................................... 271
Table A.34 Post Increment Indirect Register (Multiply/Accumulate Operation)........................ 271
Table A.35 Post Increment Indirect Register................................................................................................ 271
Table A.36 Pre Decrement Indirect Register................................................................................................ 271
Table A.37 Indirect Indexed Register............................................................................................................... 272
Table A.38 md Format............................................................................................................................................. 272
Table A.39 nd4 Format............................................................................................................................................ 272
Table A.40 nmd Format........................................................................................................................................... 272
Table A.41 Indirect GBR with Displacement............................................................................................... 273
Table A.42 PC Relative with Displacement................................................................................................. 273
Table A.43 PC Relative Addressing.................................................................................................................. 273
Table A.44 d12 Format............................................................................................................................................ 274
Table A.45 nd8 Format............................................................................................................................................ 274
Table A.46 Indirect Indexed GBR Addressing.............................................................................................. 274
Table A.47 Immediate Addressing (Arithmetic Logical Operation with Direct Register)...... 275
Table A.48 Immediate Addressing (Specify Exception Processing Vector).................................. 275
Table A.49 ni Format................................................................................................................................................ 275
Table A.50 Instruction Set by Instruction Code........................................................................................... 276
Table A.51 Operation Code Map........................................................................................................................ 284
Table B.1 Instructions and Their Contention Patterns.............................................................................. 289























                                                                                                                                                        HITACHI  13
```
