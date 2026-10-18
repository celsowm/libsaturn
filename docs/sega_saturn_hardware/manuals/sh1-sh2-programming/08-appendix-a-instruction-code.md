# Appendix A Instruction Code

SH-1/SH-2 Programming Manual — Hitachi, September 3, 1996.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=255)

Physical PDF pages 255–293. Text extraction; consult the PDF for figures.

## PDF page 255

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=255)

```text
                          Appendix A   Instruction Code


See “6. Instruction Descriptions” for details.

A.1      Instruction Set by Addressing Mode

Table A.1 lists instruction codes and execution states by addressing modes.






















































                                                                                        HITACHI  251
```

## PDF page 256

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=256)

```text
Table A.1   Instruction Set by Addressing Mode

                                                                                          Types
Addressing Mode                Category                               Sample Instruction                 SH-2    SH-1
No operand                   —                   NOP    88

Direct register addressing      Destination operand only     MOVT   Rn                            18       17

                            Source and destination       ADD    Rm,Rn                  34      31
                            operand
                            Load and store with control  LDC    Rm,SR                    12       12
                            register or system register  STS    MACH,Rn

Indirect register                 Source operand only       JMP    @Rm   22
addressing                      Destination operand only    TAS.B  @Rn   11

                            Data transfer with direct    MOV.L  Rm,@Rn  66
                            register addressing
Post increment indirect     Multiply/accumulate          MAC.W  @Rm+,@Rn+  21
register addressing         operation
                            Data transfer from direct    MOV.L  @Rm+,Rn  33
                            register addressing
                            Load to control register or  LDC.L  @Rm+,SR  66
                            system register
Pre decrement indirect      Data transfer from direct    MOV.L  Rm,@–Rn  33
register addressing         register addressing
                            Store from control register orSTC.L  SR,@–Rn  66
                            system register
Indirect register addressingData transfer with direct    MOV.L  Rm,@(disp,Rn) 66
with displacement           register addressing
Indirect indexed register   Data transfer with direct    MOV.L  Rm,@(R0,Rn) 66
addressing                  register addressing
Indirect GBR addressing     Data transfer with direct    MOV.L  R,@(disp,GBR) 66
with displacement           register addressing
Indirect indexed GBR        Immediate data transfer     AND.B   #imm,@(R0,GBR) 44
addressing
PC relative addressing with Data transfer to direct      MOV.L  @(disp,PC),Rn 33
displacement                register addressing
PC relative addressing with Branch instruction         BRAF   Rm   20
Rm
PC relative addressing         Branch instruction         BRA    label  64

Immediate addressing         Arithmetic logical operationsADD    #imm,Rn  77
                            with direct register
                            addressing
                            Specify exception processing TR APA #imm  11
                            vector
                                                                               Total: 142 133



252 HITACHI
```

## PDF page 257

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=257)

```text
A.1.1    No Operand

Table A.2   No Operand

Instruction    Code                          Operation                     State    T Bit
CLRT       0000000000001000              0 ﬁ   T10
CLRMAC      0000000000101000                 0                                                ﬁ   MACH, MACL                1           —
DIV0U       0000000000011001                0 ﬁ  M/Q/T                       1          0
NOP        0000000000001001             No operation                       1           —
RTE        0000000000101011                Delayed branch, Stack area                  4            LSB
                                                    ﬁ   PC/SR
RTS         0000000000001011                 Delayed branch, PR ﬁ   PC        2              —
SETT         0000000000011000                     1  ﬁ   T11
SLEEP        0000000000011011                 Sleep                                          3              —










































                                                                                          HITACHI  253
```

## PDF page 258

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=258)

```text
A.1.2    Direct Register Addressing

Table A.3   Destination Operand Only

Instruction        Code                  Operation            State    T Bit
CMP/PL Rn      0100nnnn00010101       Rn > 0, 1 ﬁ   T                1              Comparison result
CMP/PZ Rn        0100nnnn00010001         Rn  ‡ 0, 1 ﬁ   T                1              Comparison result
DT     Rn*       0100nnnn00010000        Rn – 1  ﬁ   Rn                                      1            Comparison result
                                                             When Rn is 0, 1 ﬁ   T,
                                                             when Rn is nonzero,
                                                             0  ﬁ   T
MOVT   Rn       0000nnnn00101001      T ﬁ   Rn                                1                 —
ROTL   Rn        0100nnnn00000100       T ‹   Rn ‹   MSB         1          MSB
ROTR   Rn       0100nnnn00000101     LSB ﬁ   Rn ﬁ   T             1             LSB
ROTCL  Rn         0100nnnn00100100         T  ‹   Rn ‹   T                  1             MSB
ROTCR  Rn         0100nnnn00100101          T  ﬁ   Rn ﬁ   T                  1             LSB
SHAL   Rn         0100nnnn00100000         T  ‹   Rn ‹    0                      1                MSB
SHAR    Rn          0100nnnn00100001             MSB    ﬁ   Rn ﬁ   T            1             LSB
SHLL   Rn         0100nnnn00000000         T  ‹   Rn ‹    0                      1                MSB
SHLR    Rn          0100nnnn00000001                       0      ﬁ   Rn ﬁ   T                  1             LSB
SHLL2  Rn         0100nnnn00001000          Rn<<2  ﬁ   Rn                       1                 —
SHLR2  Rn        0100nnnn00001001        Rn>>2  ﬁ   Rn                       1                 —
SHLL8  Rn        0100nnnn00011000        Rn<<8  ﬁ   Rn                       1                 —
SHLR8  Rn        0100nnnn00011001        Rn>>8  ﬁ   Rn                       1                 —
SHLL16 Rn        0100nnnn00101000        Rn<<16  ﬁ   Rn                    1                 —
SHLR16 Rn        0100nnnn00101001        Rn>>16  ﬁ   Rn                    1                 —
Note:  SH-2 CPU instruction


Table A.4  Source and Destination Operand

Instruction        Code                     Operation               State    T Bit
ADD   Rm,Rn    0011nnnnmmmm1100            Rn + Rm  ﬁ   Rn                        1                  —
ADDC   Rm,Rn    0011nnnnmmmm1110           Rn + Rm + T ﬁ   Rn,                                        1              Carry
                                                                  carry ﬁ   T
ADDV   Rm,Rn    0011nnnnmmmm1111            Rn + Rm  ﬁ   Rn,                                          1            Overflow
                                                                  overflow  ﬁ   T
AND    Rm,Rn    0010nnnnmmmm1001           Rn & Rm ﬁ    Rn                       1                  —






254 HITACHI
```

## PDF page 259

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=259)

```text
Table A.4   Source and Destination Operand (cont)

Instruction         Code                      Operation              State    T Bit
CMP/EQ Rm,Rn    0011nnnnmmmm0000           When Rn = Rm, 1 ﬁ   T     1              Comparison
                                                                                                 result
CMP/HS Rm,Rn     0011nnnnmmmm0010          When unsigned and Rn                       1              Comparison
                                                        ‡ Rm, 1 ﬁ   T                            result
CMP/GE Rm,Rn     0011nnnnmmmm0011          When signed and Rn ‡                       1              Comparison
                                                        Rm, 1  ﬁ   T                             result
CMP/HI Rm,Rn     0011nnnnmmmm0110          When unsigned and Rn                       1              Comparison
                                                        > Rm, 1 ﬁ   T                            result
CMP/GT Rm,Rn     0011nnnnmmmm0111          When signed and Rn >                       1              Comparison
                                                        Rm, 1  ﬁ   T                             result
CMP/STR                  0010nnnnmmmm1100          When a byte in Rn                  1              Comparison
          Rm,Rn                                         equals bytes in Rm, 1                    result
                                                        ﬁ   T
DIV1   Rm,Rn     0011nnnnmmmm0100         1-step division (Rn ‚                       1            Calculation
                                                        Rm)                                      result
DIV0S  Rm,Rn     0010nnnnmmmm0111         MSB of Rn ﬁ   Q, MSB                        1             Calculation
                                                        of Rm ﬁ   M, M ^ Q ﬁ                     result
                                                        T
DMULS.L                                                                               2 to 4*1   —
          Rm,Rn*2   0011nnnnmmmm1101         Signed, Rn · Rm ﬁMACH, MACL
DMULU.L                                                                               2 to 4*1   —
          Rm,Rn*2   0011nnnnmmmm0101          Unsigned, Rn · Rm ﬁMACH, MACL

EXTS.B Rm,Rn     0110nnnnmmmm1110         Sign – extends Rm                           1—
                                                        from byte ﬁ    Rn
EXTS.W Rm,Rn     0110nnnnmmmm1111         Sign – extends Rm                           1—
                                                        from word ﬁ   Rn
EXTU.B Rm,Rn     0110nnnnmmmm1100         Zero – extends Rm                           1—
                                                        from byte ﬁ    Rn
EXTU.W Rm,Rn     0110nnnnmmmm1101         Zero – extends Rm                           1—
                                                        from word ﬁ   Rn
MOV    Rm,Rn     0110nnnnmmmm0011               Rm  ﬁ    Rn                                1                  —
MUL.L  Rm,Rn*2   0000nnnnmmmm0111           Rn · Rm  ﬁ   MACL          2 to 4*1   —
MULS.W Rm,Rn     0010nnnnmmmm1111         Signed, Rn · Rm ﬁ                           1 to 3*1   —
                                                        MAC
MULU.W Rm,Rn     0010nnnnmmmm1110          Unsigned, Rn · Rm ﬁ                        1 to 3*1   —
                                                        MAC
NEG    Rm,Rn     0110nnnnmmmm1011           0 – Rm  ﬁ   Rn                         1                  —
NEGC   Rm,Rn     0110nnnnmmmm1010           0 – Rm – T ﬁ   Rn,                        1            Borrow
                                                        Borrow ﬁ   T
Notes:    1.   The normal minimum number of execution states
          2.  SH-2 CPU instruction



                                                                                               HITACHI  255
```

## PDF page 260

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=260)

```text
Table A.4   Source and Destination Operand (cont)

Instruction        Code                    Operation                State    T Bit
NOT   Rm,Rn    0110nnnnmmmm0111       ~Rm ﬁ    Rn                                  1                  —
OR     Rm,Rn    0010nnnnmmmm1011         Rn | Rm ﬁ    Rn                            1                  —
SUB    Rm,Rn    0011nnnnmmmm1000         Rn – Rm  ﬁ   Rn                           1                  —
SUBC   Rm,Rn    0011nnnnmmmm1010        Rn – Rm – T ﬁ   Rn,                                     1            Borrow
                                                            Borrow ﬁ   T
SUBV   Rm,Rn    0011nnnnmmmm1011         Rn – Rm  ﬁ   Rn,                                       1             Underflow
                                                            Underflow ﬁ   T
SWAP.B Rm,Rn    0110nnnnmmmm1000           Rm  ﬁ   Swap upper and                               1—
                                                            lower halves of lower 2
                                                            bytes ﬁ   Rn
SWAP.W Rm,Rn    0110nnnnmmmm1001           Rm  ﬁ   Swap upper and                               1—
                                                            lower word ﬁ   Rn
TST    Rm,Rn    0010nnnnmmmm1000        Rn & Rm, when result is                                 1           Test results
                                                            0, 1 ﬁ   T
XOR   Rm,Rn    0010nnnnmmmm1010         Rn ^ Rm ﬁ    Rn                           1                  —
XTRCT  Rm,Rn    0010nnnnmmmm1101       Center 32 bits of  Rm                                    1—
                                                            and Rn ﬁ    Rn


Table A.5   Load and Store with Control Register or System Register

Instruction            Code                            Operation          State     T Bit
LDC   Rm,SR      0100mmmm00001110              Rm ﬁ   SR                  1                LSB
LDC    Rm,GBR      0100mmmm00011110                       Rm  ﬁ   GBR               1                —
LDC    Rm,VBR      0100mmmm00101110                       Rm  ﬁ   VBR           1            —
LDS   Rm,MACH    0100mmmm00001010                Rm ﬁ   MACH         1            —
LDS   Rm,MACL    0100mmmm00011010                Rm ﬁ   MACL           1             —
LDS    Rm,PR      0100mmmm00101010                   Rm  ﬁ    PR                       1                    —
STC    SR,Rn       0000nnnn00000010                  SR ﬁ    Rn                        1                    —
STC    GBR,Rn      0000nnnn00010010                 GBR ﬁ    Rn                     1                    —
STC    VBR,Rn      0000nnnn00100010                VBR ﬁ    Rn                     1                    —
STS    MACH,Rn     0000nnnn00001010                MACH ﬁ    Rn                 1                    —
STS    MACL,Rn     0000nnnn00011010                MACL  ﬁ   Rn                 1                    —
STS    PR,Rn       0000nnnn00101010                    PR  ﬁ   Rn                        1                    —








256 HITACHI
```

## PDF page 261

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=261)

```text
A.1.3    Indirect Register Addressing

Table A.6   Destination Operand Only

Instruction   Code                  Operation                     State      T Bit
JMP   @Rm  0100mmmm00101011     Delayed branch, Rm ﬁ   PC        2                 —
JSR    @Rm  0100mmmm00001011     Delayed branch, PC ﬁ   PR,                              2—
                                                 Rm  ﬁ   PC
TAS.B  @Rn  0100nnnn00011011     When (Rn) is 0, 1 ﬁ   T, 1 ﬁ                            4             Test results
                                                 MSB of (Rn)


Table A.7   Data Transfer with Direct Register Addressing

Instruction        Code                      Operation                     State     T Bit
MOV.B  Rm,@Rn   0010nnnnmmmm0000        Rm ﬁ   (Rn)                                      1                  —
MOV.W  Rm,@Rn   0010nnnnmmmm0001             Rm  ﬁ   (Rn)                                      1                  —
MOV.L  Rm,@Rn   0010nnnnmmmm0010             Rm  ﬁ   (Rn)                                      1                  —
MOV.B  @Rm,Rn   0110nnnnmmmm0000           (Rm)  ﬁ   sign extension ﬁ   Rn      1                    —
MOV.W  @Rm,Rn   0110nnnnmmmm0001           (Rm)  ﬁ   sign extension ﬁ   Rn      1                    —
MOV.L  @Rm,Rn   0110nnnnmmmm0010           (Rm)  ﬁ   Rn                                           1                    —


A.1.4    Post Increment Indirect Register Addressing

Table A.8  Multiply/Accumulate Operation

Instruction            Code                   Operation                   State     T
                                                                                                                  Bit
MAC.L  @Rm+,@Rn+*2 0000nnnnmmmm1111     Signed, (Rn) · (Rm) + MAC                                  3/(2 to  4)*1              —
                                                              ﬁ   MAC
MAC.W  @Rm+,@Rn+    0100nnnnmmmm1111     Signed, (Rn) · (Rm) + MAC                                 3/(2)*1       —
                                                              ﬁ   MAC
Notes:   1.  The normal minimum number of execution states (The number in parentheses is the
               number of states when there is contention with  preceding/following instructions).
           2.  SH-2 CPU instruction












                                                                                                       HITACHI  257
```

## PDF page 262

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=262)

```text
Table A.9   Data Transfer from Direct Register Addressing

Instruction            Code                   Operation                    State    T Bit
MOV.B  @Rm+,Rn     0110nnnnmmmm0100    (Rm) ﬁ   sign extension ﬁ                                     1—
                                                              Rn, Rm + 1 ﬁ    Rm
MOV.W  @Rm+,Rn      0110nnnnmmmm0101      (Rm)  ﬁ   sign extension ﬁ                                 1—
                                                              Rn, Rm + 2 ﬁ    Rm
MOV.L  @Rm+,Rn      0110nnnnmmmm0110      (Rm)  ﬁ   Rn, Rm + 4 ﬁ    Rm             1                    —


Table A.10 Load to Control Register or System Register

Instruction           Code                     Operation                       State    T Bit
LDC.L  @Rm+,SR   0100mmmm00000111    (Rm) ﬁ   SR, Rm + 4 ﬁ    Rm                  3                    L S B
LDC.L  @Rm+,GBR   0100mmmm00010111      (Rm)  ﬁ   GBR, Rm + 4 ﬁ    Rm               3                    —
LDC.L  @Rm+,VBR   0100mmmm00100111      (Rm)  ﬁ   VBR, Rm + 4 ﬁ    Rm               3                    —
LDS.L  @Rm+,MACH  0100mmmm00000110      (Rm)  ﬁ   MACH, Rm + 4 ﬁ    Rm          1                    —
LDS.L  @Rm+,MACL  0100mmmm00010110      (Rm)  ﬁ   MACL, Rm + 4 ﬁ    Rm           1                    —
LDS.L  @Rm+,PR    0100mmmm00100110      (Rm)  ﬁ   PR, Rm + 4 ﬁ    Rm                   1                    —


A.1.5    Pre Decrement Indirect Register Addressing

Table A.11 Data Transfer from Direct Register Addressing

Instruction          Code                   Operation                      State    T Bit
MOV.B  Rm,@–Rn   0010nnnnmmmm0100      Rn – 1  ﬁ   Rn, Rm ﬁ   (Rn)                1                —
MOV.W  Rm,@–Rn    0010nnnnmmmm0101      Rn – 2  ﬁ   Rn, Rm ﬁ   (Rn)                1                —
MOV.L  Rm,@–Rn    0010nnnnmmmm0110      Rn – 4  ﬁ   Rn, Rm ﬁ   (Rn)                1                —






















258 HITACHI
```

## PDF page 263

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=263)

```text
Table A.12 Store from Control Register or System Register

Instruction             Code                    Operation                   State     T Bit
STC.L  SR,@-Rn     0100nnnn00000011      Rn – 4  ﬁ   Rn, SR ﬁ   (Rn)          2                 —
STC.L  GBR,@-Rn    0100nnnn00010011      Rn – 4  ﬁ   Rn, GBR ﬁ   (Rn)       2                 —
STC.L  VBR,@-Rn    0100nnnn00100011      Rn – 4  ﬁ   Rn, VBR ﬁ   (Rn)       2                 —
STS.L  MACH,@–Rn    0100nnnn00000010      Rn – 4  ﬁ   Rn, MACH ﬁ   (Rn)    1                 —
STS.L  MACL,@–Rn    0100nnnn00010010      Rn – 4  ﬁ   Rn, MACL ﬁ   (Rn)     1                 —
STS.L  PR,@–Rn      0100nnnn00100010      Rn – 4  ﬁ   Rn, PR ﬁ   (Rn)           1                 —


A.1.6    Indirect Register Addressing with Displacement

Table A.13 Indirect Register Addressing with Displacement

Instruction                 Code                   Operation              State    T Bit
MOV.B  R0,@(disp,Rn)   10000000nnnndddd     R0 ﬁ   (disp + Rn)             1              —
MOV.W  R0,@(disp,Rn)    10000001nnnndddd        R0  ﬁ   (disp  · 2 + Rn)      1              —
MOV.L  Rm,@(disp,Rn)    0001nnnnmmmmdddd         Rm  ﬁ   (disp  · 4 + Rn)      1              —
MOV.B  @(disp,Rm),R0    10000100mmmmdddd      (disp + Rm) ﬁ   sign                              1—
                                                                  extension  ﬁ   R0
MOV.W  @(disp,Rm),R0    10000101mmmmdddd      (disp  · 2 + Rm) ﬁ                                1—
                                                                  sign extension ﬁ    R0
MOV.L  @(disp,Rm),Rn    0101nnnnmmmmdddd      (disp  · 4 + Rm) ﬁ    Rn       1                  —


A.1.7    Indirect Indexed Register Addressing

Table A.14 Indirect Indexed Register Addressing

Instruction            Code                 Operation                   State    T Bit
MOV.B  Rm,@(R0,Rn)  0000nnnnmmmm0100     Rm ﬁ   (R0 + Rn)                        1               —
MOV.W  Rm,@(R0,Rn)  0000nnnnmmmm0101        Rm  ﬁ   (R0 + Rn)                        1               —
MOV.L  Rm,@(R0,Rn)  0000nnnnmmmm0110        Rm  ﬁ   (R0 + Rn)                        1               —
MOV.B  @(R0,Rm),Rn  0000nnnnmmmm1100      (R0 + Rm)  ﬁ   sign                                   1—
                                                            extension  ﬁ   Rn
MOV.W  @(R0,Rm),Rn  0000nnnnmmmm1101      (R0 + Rm)  ﬁ   sign                                   1—
                                                            extension  ﬁ   Rn
MOV.L  @(R0,Rm),Rn  0000nnnnmmmm1110      (R0 + Rm)  ﬁ   Rn                            1                  —





                                                                                                  HITACHI  259
```

## PDF page 264

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=264)

```text
A.1.8    Indirect GBR Addressing with Displacement

Table A.15 Indirect GBR Addressing with Displacement

Instruction                    Code                  Operation            State    T Bit
MOV.B  R0,@(disp,GBR)    11000000dddddddd   R0 ﬁ   (disp + GBR)      1             —
MOV.W  R0,@(disp,GBR)     11000001dddddddd     R0  ﬁ   (disp  · 2 +                1—
                                                           GBR)
MOV.L  R0,@(disp,GBR)     11000010dddddddd     R0  ﬁ   (disp  · 4 +                1—
                                                           GBR)
MOV.B  @(disp,GBR),R0     11000100dddddddd    (disp + GBR) ﬁ   sign                1—
                                                           extension  ﬁ   R0
MOV.W  @(disp,GBR),R0     11000101dddddddd    (disp  · 2 + GBR) ﬁ                  1—
                                                           sign extension ﬁ    R0
MOV.L  @(disp,GBR),R0     11000110dddddddd    (disp  · 4 + GBR) ﬁ                  1—
                                                           R0


A.1.9    Indirect Indexed GBR Addressing

Table A.16 Indirect Indexed GBR Addressing

Instruction               Code                 Operation                State   T Bit
AND.B  #imm,@(R0,GBR) 11001101iiiiiiii    (R0 + GBR) & imm ﬁ   (R0                3—
                                                      + GBR)
OR.B   #imm,@(R0,GBR)  11001111iiiiiiii    (R0 + GBR) | imm ﬁ   (R0               3—
                                                      + GBR)
TST.B  #imm,@(R0,GBR)  11001100iiiiiiii    (R0 + GBR) & imm, when                 3            Test
                                                      result is 0, 1 ﬁ   T                  results
XOR.B  #imm,@(R0,GBR)  11001110iiiiiiii    (R0 + GBR) ^ imm ﬁ   (R0               3—
                                                      + GBR)


A.1.10   PC Relative Addressing with Displacement

Table A.17 PC Relative Addressing with Displacement

Instruction                Code                   Operation               State    T Bit
MOV.W  @(disp,PC),Rn  1001nnnndddddddd     (disp  · 2 + PC) ﬁ   sign              1—
                                                      extension  ﬁ   Rn
MOV.L  @(disp,PC),Rn  1101nnnndddddddd     (disp  · 4 + PC) ﬁ    Rn         1                  —
MOVA   @(disp,PC),R0  11000111dddddddd     disp  · 4 + PC ﬁ    R0            1                  —




260 HITACHI
```

## PDF page 265

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=265)

```text
A.1.11   PC Relative Addressing with Rm

Table A.18 PC Relative Addressing with Rm

Instruction    Code                     Operation                           State    T Bit
BRAF  Rm*2  0000mmmm00100011      Delayed branch, Rm + PC ﬁ   PC          2              —
BSRF   Rm*2  0000mmmm00000011     Delayed branch, PC ﬁ   PR, Rm + PC                 2—
                                            ﬁ   PC
Notes:    2. SH-2 CPU instruction


A.1.12   PC Relative Addressing

Table A.19 PC Relative Addressing

Instruction        Code                    Operation                         State    T Bit
B F label 10001011dddddddd     When T = 0, disp · 2 + PC ﬁ   PC;                       3/1*3       —
                                                When T = 1, nop
BF/S   label*2  10001111dddddddd     When T = 0, disp · 2 + PC ﬁ   PC;                 2/1*3       —
                                                When T = 1, nop
B T label 10001001dddddddd     When T = 1, disp · 2+ PC ﬁ   PC;                        3/1*3       —
                                                When T = 0, nop
BT/S   label*2  10001101dddddddd     When T = 1, disp · 2 + PC ﬁ   PC;                 2/1*3       —
                                                When T = 0, nop
BRA    label   1010dddddddddddd     Delayed branch, disp · 2 + PC ﬁ                    2—
                                                PC
BSR    label    1011dddddddddddd     Delayed branch, PC ﬁ   PR, disp ·                 2—
                                                2 + PC ﬁ   PC
Notes:  2.  SH-2 CPU instruction
        3.  One state when it does not branch






















                                                                                         HITACHI  261
```

## PDF page 266

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=266)

```text
A.1.13    Immediate

Table A.20 Arithmetic Logical Operation with Direct Register Addressing

Instruction        Code                   Operation                   State   T Bit
ADD   #imm,Rn  0111nnnniiiiiiii      Rn + imm  ﬁ   Rn                              1                —
AND    #imm,R0  11001001iiiiiiii     R0 & imm ﬁ    R0                              1                —
CMP/EQ #imm,R0  10001000iiiiiiii     When R0 = imm, 1 ﬁ   T            1            Compariso
                                                                                                       n result
MOV    #imm,Rn  1110nnnniiiiiiii      imm  ﬁ   sign extension ﬁ   Rn    1                —
O R #imm,R0 11001011iiiiiiii      R0 | imm ﬁ    R0                                1                —
TST    #imm,R0  11001000iiiiiiii     R0 & imm, when result is 0,                            1         Test results
                                                       1  ﬁ   T
XOR   #imm,R0  11001010iiiiiiii      R0 ^ imm ﬁ    R0                               1                —


Table A.21 Specify Exception Processing Vector

Instruction      Code                  Operation                         State     T Bit
TRAPA  #imm   11000011iiiiiiii    PC/SR ﬁ   Stack area, (imm · 4 +                               8—
                                                     VBR) ﬁ   PC


A.2       Instruction Sets by Instruction Format

Tables A.22 to A.48 list instruction codes and execution states by instruction formats.




























262 HITACHI
```

## PDF page 267

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=267)

```text
Table A.22 Instruction Sets by Format

                                                                              Types
Format   Category                                Sample Instruction      SH-2 SH-1
0—                    NOP    88
n            Direct register addressing              MOVT   Rn                           18       17
         Direct register addressing (store with control
         or system registers)                  STS   MACH,Rn  66
         Indirect register addressing             TAS.B  @Rn   11
         Pre decrement indirect register addressing  STC.L  SR,@–Rn  66
m          Direct register addressing (load with control
         or system registers)                  LDC    Rm,SR   66
         PC relative addressing with Rn           BRAF   Rm   20
         Direct register addressing              JMP   @Rm   22
         Post increment indirect register addressing  LDC.L  @Rm+,SR  66
nm         Direct register addressing              ADD    Rm,Rn                      34       31
         Indirect register addressing             MOV.L  Rm,@Rn  66
         Post increment indirect register addressing
         (multiply/accumulate operation)          MAC.W  @Rm+,@Rn+  21
         Post increment indirect register addressing  MOV.L  @Rm+,Rn  33
         Pre decrement indirect register addressing  MOV.L  Rm,@–Rn  33
         Indirect indexed register addressing      MOV.L  Rm,@(R0,Rn) 66
md         Indirect register addressing with
         displacement                      MOV.B  @(disp,Rm),R0 22
nd4        Indirect register addressing with
         displacement                      MOV.B  R0,@(disp,Rn) 22
nmd       Indirect register addressing with
         displacement                      MOV.L  Rm,@(disp,Rn) 22
d           Indirect GBR addressing with displacement  MOV.L  R0,@(disp,GBR) 66
         Indirect PC addressing with displacement   MOVA   @(disp,PC),R0 11
         PC relative addressing                 BF     label   42
d12         PC relative addressing                 BRA    label   22
nd8         PC relative addressing with displacement   MOV.L  @(disp,PC),Rn 22
i            Indirect indexed GBR addressing         AND.B  #imm,@(R0,GBR) 44
         Immediate addressing (arithmetic and logical
         operations with direct register)          AND   #imm,R0  55
         Immediate addressing (specify exception
         processing vector)                 TRAPA  #imm   11
ni           Immediate addressing (direct register
         arithmetic operations and data transfers )    ADD    #imm,Rn  22
                                                                     Total:    142     133




                                                                          HITACHI  263
```

## PDF page 268

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=268)

```text
A.2.1      0 Format

Table A.23 0 Format

Instruction       Code                          Operation                  State    T Bit
CLRT         0000000000001000             0 ﬁ   T10
CLRMAC         0000000000101000                0                                           ﬁ   MACH, MACL            1           —
DIV0U         0000000000011001               0 ﬁ  M/Q/T                    1          0
NOP          0000000000001001            No operation                   1           —
RTE          0000000000101011              Delayed branching, stack                  4            LSB
                                                      area ﬁ   PC/SR
RTS            0000000000001011                Delayed branching, PR ﬁ               2—
                                                      PC
SETT           0000000000011000                    1  ﬁ   T11
SLEEP          0000000000011011                Sleep                                     3*4           —
Notes:  4.  This is the number of states until a transition is made to the Sleep state.








































264 HITACHI
```

## PDF page 269

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=269)

```text
A.2.2     n Format

Table A.24 Direct Register Addressing

Instruction     Code                 Operation              State    T Bit
CMP/PL Rn    0100nnnn00010101     Rn > 0, 1 ﬁ   T                   1              Comparison result
CMP/PZ Rn     0100nnnn00010001     Rn  ‡ 0, 1 ﬁ   T                   1              Comparison result
DT     Rn*2   0100nnnn00010000     Rn - 1  ﬁ   Rn;                                          1            Comparison result
                                                         If Rn is 0, 1 ﬁ   T, if Rn
                                                         is nonzero, 0 ﬁ   T
MOVT   Rn     0000nnnn00101001    T ﬁ   Rn                                    1                  —
ROTL   Rn     0100nnnn00000100    T ‹   Rn ‹   MSB           1          MSB
ROTR   Rn    0100nnnn00000101   LSB ﬁ   Rn ﬁ   T                1              LSB
ROTCL  Rn     0100nnnn00100100     T  ‹   Rn ‹   T                    1              MSB
ROTCR  Rn      0100nnnn00100101      T  ﬁ   Rn ﬁ   T                     1              LSB
SHAL   Rn     0100nnnn00100000     T  ‹   Rn ‹    0                          1                 MSB
SHAR    Rn      0100nnnn00100001        MSB    ﬁ   Rn ﬁ   T               1              LSB
SHLL   Rn     0100nnnn00000000     T  ‹   Rn ‹    0                          1                 MSB
SHLR    Rn      0100nnnn00000001              0      ﬁ   Rn ﬁ   T                     1              LSB
SHLL2  Rn     0100nnnn00001000      Rn<<2  ﬁ   Rn                          1                  —
SHLR2  Rn     0100nnnn00001001     Rn>>2  ﬁ   Rn                          1                  —
SHLL8  Rn     0100nnnn00011000     Rn<<8  ﬁ   Rn                          1                  —
SHLR8  Rn     0100nnnn00011001     Rn>>8  ﬁ   Rn                          1                  —
SHLL16 Rn     0100nnnn00101000     Rn<<16  ﬁ   Rn                        1                  —
SHLR16 Rn     0100nnnn00101001     Rn>>16  ﬁ   Rn                        1                  —
Notes:    2.  SH-2 CPU instruction.


Table A.25 Direct Register Addressing (Store with Control and System Registers)

Instruction            Code                           Operation          State     T Bit
STC   SR,Rn      0000nnnn00000010            SR ﬁ    Rn                        1                    —
STC    GBR,Rn      0000nnnn00010010                 GBR ﬁ    Rn                     1                    —
STC    VBR,Rn      0000nnnn00100010                VBR ﬁ    Rn                     1                    —
STS    MACH,Rn     0000nnnn00001010                MACH ﬁ    Rn                 1                    —
STS    MACL,Rn     0000nnnn00011010                MACL  ﬁ   Rn                 1                    —
STS    PR,Rn       0000nnnn00101010                    PR  ﬁ   Rn                        1                    —





                                                                                                                  HITACHI  265
```

## PDF page 270

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=270)

```text
Table A.26 Indirect Register Addressing

Instruction   Code                  Operation                     State     T Bit
TAS.B  @Rn  0100nnnn00011011    When (Rn) is 0, 1 ﬁ   T, 1 ﬁ                          4            Test results
                                               MSB of (Rn)


Table A.27 Pre Decrement Indirect Register

Instruction           Code                 Operation                   State     T Bit
STC.L  SR,@-Rn     0100nnnn00000011      Rn – 4  ﬁ   Rn, SR ﬁ   (Rn)          2                 —
STC.L  GBR,@-Rn    0100nnnn00010011      Rn – 4  ﬁ   Rn, GBR ﬁ   (Rn)       2                 —
STC.L  VBR,@-Rn    0100nnnn00100011      Rn – 4  ﬁ   Rn, VBR ﬁ   (Rn)       2                 —
STS.L  MACH,@–Rn    0100nnnn00000010      Rn – 4  ﬁ   Rn, MACH ﬁ   (Rn)    1                 —
STS.L  MACL,@–Rn    0100nnnn00010010      Rn – 4  ﬁ   Rn, MACL ﬁ   (Rn)     1                 —
STS.L  PR,@–Rn      0100nnnn00100010      Rn – 4  ﬁ   Rn, PR ﬁ   (Rn)           1                 —










































266 HITACHI
```

## PDF page 271

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=271)

```text
A.2.3    m Format

Table A.28 Direct Register Addressing (Load with Control and System Registers)

Instruction            Code                           Operation          State     T Bit
LDC   Rm,SR      0100mmmm00001110              Rm ﬁ   SR                  1                LSB
LDC    Rm,GBR      0100mmmm00011110                       Rm  ﬁ   GBR               1                —
LDC    Rm,VBR      0100mmmm00101110                       Rm  ﬁ   VBR           1            —
LDS   Rm,MACH    0100mmmm00001010                Rm ﬁ   MACH         1            —
LDS   Rm,MACL    0100mmmm00011010                Rm ﬁ   MACL           1             —
LDS    Rm,PR      0100mmmm00101010                   Rm  ﬁ    PR                       1                    —


Table A.29 Indirect Register

Instruction   Code                  Operation                     State     T Bit
JMP   @Rm  0100mmmm00101011    Delayed branch, Rm ﬁ   PC        2                —
JSR    @Rm  0100mmmm00001011    Delayed branch, PC ﬁ   PR,                               2—
                                                 Rm  ﬁ   PC


Table A.30 Post Increment Indirect Register

Instruction         Code                   Operation                      State    T Bit
LDC.L  @Rm+,SR   0100mmmm00000111      (Rm) ﬁ   SR, Rm + 4 ﬁ    Rm                3                    L S B
LDC.L  @Rm+,GBR  0100mmmm00010111         (Rm)  ﬁ   GBR, Rm + 4 ﬁ    Rm            3                    —
LDC.L  @Rm+,VBR  0100mmmm00100111         (Rm)  ﬁ   VBR, Rm + 4 ﬁ    Rm            3                    —
LDS.L                      0100mmmm00000110         (Rm)  ﬁ   MACH, Rm + 4 ﬁ    Rm       1                    —
           @Rm+,MAC
H
LDS.L                      0100mmmm00010110         (Rm)  ﬁ   MACL, Rm + 4 ﬁ    Rm        1                    —
           @Rm+,MAC
L
LDS.L  @Rm+,PR   0100mmmm00100110         (Rm)  ﬁ   PR, Rm + 4 ﬁ    Rm                1                    —















                                                                                                         HITACHI  267
```

## PDF page 272

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=272)

```text
Table A.31 PC Relative Addressing with Rm

Instruction    Code                     Operation                           State    T Bit
BRAF  Rm*2  0000mmmm00100011      Delayed branch, Rm + PC ﬁ   PC          2              —
BSRF   Rm*2  0000mmmm00000011     Delayed branch, PC ﬁ   PR, Rm + PC                   2—
                                             ﬁ   PC
Notes:    2.  SH-2 CPU instruction























































268 HITACHI
```

## PDF page 273

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=273)

```text
A.2.4    nm Format

Table A.32 Direct Register Addressing

Instruction          Code                  Operation                 State   T Bit
ADD   Rm,Rn     0011nnnnmmmm1100      Rn + Rm  ﬁ   Rn                           1                 —
ADDC   Rm,Rn      0011nnnnmmmm1110      Rn + Rm + T ﬁ   Rn, carry                      1             Carry
                                                       ﬁ   T
ADDV   Rm,Rn      0011nnnnmmmm1111      Rn + Rm  ﬁ   Rn, overflow                      1           Overflow
                                                       ﬁ   T
AND    Rm,Rn      0010nnnnmmmm1001      Rn & Rm ﬁ    Rn                           1                 —
CMP/EQ Rm,Rn      0011nnnnmmmm0000     When Rn = Rm, 1 ﬁ   T         1             Comparison
                                                                                                 result
CMP/HS Rm,Rn      0011nnnnmmmm0010     When unsigned and Rn ‡                          1             Comparison
                                                       Rm, 1  ﬁ   T                              result
CMP/GE Rm,Rn      0011nnnnmmmm0011     When signed and Rn ‡                            1             Comparison
                                                       Rm, 1  ﬁ   T                              result
CMP/HI Rm,Rn      0011nnnnmmmm0110     When unsigned and Rn >                          1             Comparison
                                                       Rm, 1  ﬁ   T                              result
CMP/GT Rm,Rn      0011nnnnmmmm0111     When signed and Rn >                            1             Comparison
                                                       Rm, 1  ﬁ   T                              result
CMP/STR                    0010nnnnmmmm1100     When a byte in Rn equals               1             Comparison
          Rm,Rn                                        a byte in Rm, 1 ﬁ   T                     result
DIV1   Rm,Rn      0011nnnnmmmm0100     1-step division (Rn ‚ Rm)    1             Calculation
                                                                                                 result
DIV0S  Rm,Rn      0010nnnnmmmm0111     MSB of Rn ﬁ   Q, MSB of                         1             Calculation
                                                       Rm ﬁ   M, M ^ Q ﬁ   T                     result
DMULS.L                                                                                2 to
          Rm,Rn*2    0011nnnnmmmm1101     Signed, Rn x Rm ﬁMACH, MACL                  4* 1         —
DMULU.L                                                                                2 to
          Rm,Rn*2    0011nnnnmmmm0101     Unsigned, Rn x Rm ﬁMACH, MACL                4* 1         —
EXTS.B Rm,Rn      0110nnnnmmmm1110    Sign-extends Rm from                             1—
                                                       byte ﬁ    Rn
EXTS.W Rm,Rn      0110nnnnmmmm1111    Sign-extends Rm from                             1—
                                                       word ﬁ   Rn
EXTU.B Rm,Rn      0110nnnnmmmm1100    Zero-extends Rm from                             1—
                                                       byte ﬁ    Rn
EXTU.W Rm,Rn      0110nnnnmmmm1101    Zero-extends Rm from                             1—
                                                       word ﬁ   Rn
MOV    Rm,Rn      0110nnnnmmmm0011       Rm  ﬁ    Rn                                     1                 —
Notes:    1.   The normal minimum number of execution states
          2.  SH-2 CPU instruction




                                                                                               HITACHI  269
```

## PDF page 274

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=274)

```text
Table A.32 Direct Register Addressing (cont)

Instruction        Code                     Operation                 State    T Bit
MUL.L  Rm,Rn*2  0000nnnnmmmm0111      Rn · Rm ﬁ   MACL             2 to 4*1   —
MULS.W Rm,Rn    0010nnnnmmmm1111       Signed, Rn · Rm ﬁ   MAC   1 to 3*1   —
MULU.W Rm,Rn    0010nnnnmmmm1110        Unsigned, Rn · Rm ﬁ                               1 to 3*1   —
                                                        MAC
NEG    Rm,Rn    0110nnnnmmmm1011         0 – Rm  ﬁ   Rn                              1                  —
NEGC   Rm,Rn    0110nnnnmmmm1010        0 – Rm – T ﬁ   Rn, borrow                         1            Borrow
                                                        ﬁ   T
NOT    Rm,Rn    0110nnnnmmmm0111          ~Rm  ﬁ    Rn                                   1                  —
O R     Rm,Rn    0010nnnnmmmm1011         Rn | Rm ﬁ    Rn                             1                  —
SUB    Rm,Rn    0011nnnnmmmm1000         Rn – Rm  ﬁ   Rn                            1                  —
SUBC   Rm,Rn    0011nnnnmmmm1010        Rn – Rm – T ﬁ   Rn,                               1            Borrow
                                                        borrow ﬁ   T
SUBV   Rm,Rn    0011nnnnmmmm1011         Rn – Rm  ﬁ   Rn, underflow                       1             Underflow
                                                        ﬁ   T
SWAP.B Rm,Rn    0110nnnnmmmm1000           Rm  ﬁ   Swap upper and                         1—
                                                        lower halves of lower 2
                                                        bytes ﬁ   Rn
SWAP.W Rm,Rn    0110nnnnmmmm1001           Rm  ﬁ   Swap upper and                         1—
                                                        lower word ﬁ   Rn
TST    Rm,Rn    0010nnnnmmmm1000        Rn & Rm, when result is                           1           Test results
                                                        0, 1 ﬁ   T
XOR   Rm,Rn   0010nnnnmmmm1010         Rn ^ Rm ﬁ    Rn                            1                  —
XTRCT  Rm,Rn    0010nnnnmmmm1101       Center 32 bits of Rm and                           1—
                                                        Rn  ﬁ   Rn
Notes:  1.  The normal minimum number of execution cycles.
         2.  SH-2 CPU instructions


















270 HITACHI
```

## PDF page 275

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=275)

```text
Table A.33 Indirect Register Addressing

Instruction         Code                    Operation                    State      T Bit

MOV.B  Rm,@Rn   0010nnnnmmmm0000      Rm ﬁ   (Rn)                                    1                    —
MOV.W  Rm,@Rn    0010nnnnmmmm0001          Rm  ﬁ   (Rn)                                    1                    —
MOV.L  Rm,@Rn    0010nnnnmmmm0010          Rm  ﬁ   (Rn)                                    1                    —
MOV.B  @Rm,Rn    0110nnnnmmmm0000        (Rm)  ﬁ   sign extension ﬁ   Rn    1                      —
MOV.W  @Rm,Rn    0110nnnnmmmm0001        (Rm)  ﬁ   sign extension ﬁ   Rn    1                      —
MOV.L  @Rm,Rn    0110nnnnmmmm0010        (Rm)  ﬁ   Rn                                         1                      —


Table A.34 Post Increment Indirect Register (Multiply/Accumulate Operation)

Instruction              Code                 Operation                 State     T Bit
MAC.L  @Rm+,@Rn+*2   0000nnnnmmmm1111    Signed, (Rn) · (Rm) +                                    3/(2 to
                                                               MAC  ﬁ   MAC                       4)* 1           —

MAC.W  @Rm+,@Rn+     0100nnnnmmmm1111   Signed, (Rn) · (Rm) +                                     3/(2)*1      —
                                                               MAC  ﬁ   MAC
Notes:  1.  The normal minimum number of execution cycles.(The  number in parentheses in the
              number of cycles when there is contention with preceding/following instructions).
         2.  SH-2 CPU instruction.

Table A.35 Post Increment Indirect Register

Instruction            Code                  Operation                    State    T Bit
MOV.B  @Rm+,Rn     0110nnnnmmmm0100    (Rm) ﬁ   sign extension ﬁ                                    1—
                                                             Rn, Rm + 1 ﬁ    Rm
MOV.W  @Rm+,Rn      0110nnnnmmmm0101      (Rm)  ﬁ   sign extension ﬁ                                1—
                                                             Rn, Rm + 2 ﬁ    Rm
MOV.L  @Rm+,Rn      0110nnnnmmmm0110      (Rm)  ﬁ   Rn, Rm + 4 ﬁ    Rm             1                    —

Table A.36 Pre Decrement Indirect Register

Instruction         Code                 Operation                      State    T Bit
MOV.B  Rm,@–Rn   0010nnnnmmmm0100      Rn – 1  ﬁ   Rn, Rm ﬁ   (Rn)                1                —
MOV.W  Rm,@–Rn    0010nnnnmmmm0101      Rn – 2  ﬁ   Rn, Rm ﬁ   (Rn)                1                —
MOV.L  Rm,@–Rn    0010nnnnmmmm0110      Rn – 4  ﬁ   Rn, Rm ﬁ   (Rn)                1                —










                                                                                                      HITACHI  271
```

## PDF page 276

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=276)

```text
Table A.37 Indirect Indexed Register

Instruction            Code                  Operation                   Cycles  T Bit
MOV.B  Rm,@(R0,Rn)  0000nnnnmmmm0100     Rm ﬁ   (R0 + Rn)                        1               —
MOV.W  Rm,@(R0,Rn)  0000nnnnmmmm0101        Rm  ﬁ   (R0 + Rn)                        1               —
MOV.L  Rm,@(R0,Rn)  0000nnnnmmmm0110        Rm  ﬁ   (R0 + Rn)                        1               —
MOV.B  @(R0,Rm),Rn  0000nnnnmmmm1100      (R0 + Rm)  ﬁ   sign                               1—
                                                          extension  ﬁ   Rn
MOV.W  @(R0,Rm),Rn  0000nnnnmmmm1101      (R0 + Rm)  ﬁ   sign                               1—
                                                          extension  ﬁ   Rn
MOV.L  @(R0,Rm),Rn  0000nnnnmmmm1110      (R0 + Rm)  ﬁ   Rn                            1                  —


A.2.5    md Format

Table A.38 md Format

Instruction                   Code                    Operation           State    T Bit
MOV.B  @(disp,Rm),R0    10000100mmmmdddd       (disp + Rm) ﬁ   sign                         1—
                                                                   extension  ﬁ   R0
MOV.W  @(disp,Rm),R0     10000101mmmmdddd       (disp · 2 + Rm) ﬁ                           1—
                                                                   sign extension ﬁ
                                                                   R0


A.2.6    nd4 Format

Table A.39 nd4 Format

Instruction                  Code                   Operation             State    T Bit
MOV.B  R0,@(disp,Rn)    10000000nnnndddd    R0 ﬁ   (disp + Rn)           1              —
MOV.W  R0,@(disp,Rn)    10000001nnnndddd      R0  ﬁ   (disp · 2+ Rn)       1              —



A.2.7    nmd Format

Table A.40 nmd Format

Instruction                 Code                   Operation              State    T Bit
MOV.L  Rm,@(disp,Rn)   0001nnnnmmmmdddd     Rm ﬁ   (disp · 4 + Rn)       1               —
MOV.L  @(disp,Rm),Rn    0101nnnnmmmmdddd      (disp · 4+ Rm) ﬁ   Rn          1                  —





272 HITACHI
```

## PDF page 277

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=277)

```text
A.2.8     d Format

Table A.41 Indirect GBR with Displacement

Instruction                   Code                 Operation            State     T Bit
MOV.B  R0,@(disp,GBR)    11000000ddddddd                   R0 ﬁ   (disp + GBR)      1               —
                                   d
MOV.W  R0,@(disp,GBR)    11000001ddddddd                   R0  ﬁ   (disp  · 2 +    1—
                                   d                       GBR)
MOV.L  R0,@(disp,GBR)    11000010ddddddd                   R0  ﬁ   (disp  · 4 +    1—
                                   d                       GBR)
MOV.B  @(disp,GBR),R0    11000100ddddddd                   (disp + GBR) ﬁ   sign   1—
                                   d                       extension  ﬁ   R0
MOV.W  @(disp,GBR),R0    11000101ddddddd                   (disp · 2 + GBR) ﬁ      1—
                                   d                       sign extension ﬁ    R0
MOV.L  @(disp,GBR),R0    11000110ddddddd                   (disp · 4 + GBR) ﬁ      1—
                                   d                       R0


Table A.42 PC Relative with Displacement

Instruction                Code                    Operation               State    T Bit
MOVA  @(disp,PC),R0  11000111dddddddd    disp · 4 + PC ﬁ    R0              1                  —


Table A.43 PC Relative Addressing

Instruction      Code                   Operation                            State    T Bit
BF    label  10001011dddddddd    When T = 0, disp  · 2 + PC ﬁ   PC;                  3/1*3      —
                                            When T = 1, nop
BF/S               10001111dddddddd    When T = 0, disp  · 2 + PC ﬁ   PC;            2/1*3      —
         label                              When T = 1, nop
*2
BT     label  10001001dddddddd    When T = 1, disp  · 2 + PC ﬁ   PC;                 3/1*3      —
                                            When T = 0, nop
BT/S               10001101dddddddd    When T = 1, disp  · 2 + PC ﬁ   PC;            2/1*3      —
         label                              When T = 0, nop
*2
Notes:  2.  SH-2 CPU instruction
        3.  One state when it does not branch









                                                                                       HITACHI  273
```

## PDF page 278

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=278)

```text
A.2.9      d12 Format

Table A.44 d12 Format

Instruction    Code                   Operation                             State    T Bit
BRA              1010dddddddddddd     Delayed branch, disp  · 2+ PC ﬁ   PC     2              —
         labe
l
BSR              1011dddddddddddd     Delayed branching, PC ﬁ   PR, disp ·           2—
         labe                             2 + PC ﬁ   PC
l


A.2.10   nd8 Format

Table A.45 nd8 Format

Instruction               Code                  Operation                State    T Bit
MOV.W  @(disp,PC),Rn  1001nnnndddddddd   (disp · 2 + PC) ﬁ   sign                  1—
                                                       extension  ﬁ   Rn
MOV.L  @(disp,PC),Rn  1101nnnndddddddd    (disp · 4 + PC) ﬁ    Rn             1                  —


A.2.11   i Format

Table A.46 Indirect Indexed GBR Addressing

Instruction                Code                Operation               State    T Bit
AND.B  #imm,@(R0,GBR)  11001101iiiiiiii    (R0 + GBR) & imm ﬁ                       3—
                                                        (R0 + GBR)
OR.B                            11001111iiiiiiii    (R0 + GBR) | imm ﬁ   (R0        3—
         #imm,@(R0,GBR)                                 + GBR)
TST.B  #imm,@(R0,GBR)  11001100iiiiiiii    (R0 + GBR) & imm,                        3             Test
                                                        when result is 0, 1 ﬁ   T             results
XOR.B  #imm,@(R0,GBR)  11001110iiiiiiii    (R0 + GBR) ^ imm ﬁ                       3—
                                                        (R0 + GBR)














274 HITACHI
```

## PDF page 279

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=279)

```text
Table A.47 Immediate Addressing (Arithmetic Logical Operation with Direct Register)

Instruction           Code                  Operation              State   T Bit
AND   #imm,R0    11001001iiiiiiii    R0 & imm ﬁ    R0                    1                 —
CMP/EQ #imm,R0    10001000iiiiiiii    When R0 = imm, 1 ﬁ   T   1             Comparison
                                                                                                  results
O R #imm,R0 11001011iiiiiiii    R0 | imm ﬁ    R0                      1                 —
TST    #imm,R0    11001000iiiiiiii    R0 & imm, when result                            1          Test results
                                                         is 0, 1 ﬁ   T
XOR   #imm,R0    11001010iiiiiiii    R0 ^ imm ﬁ    R0                     1                 —


Table A.48 Immediate Addressing (Specify Exception Processing Vector)

Instruction      Code                  Operation                         State     T Bit
TRAPA  #imm   11000011iiiiiiii    PC/SR ﬁ   Stack area, (imm · 4 +                              8—
                                                    VBR) ﬁ   PC


A.2.12   ni Format

Table A.49 ni Format

Instruction       Code                    Operation                       State    T Bit
ADD                    0111nnnniiiiiiii         Rn + imm  ﬁ   Rn                                      1                  —
          #imm,R
n
MOV                    1110nnnniiiiiiii          imm  ﬁ   sign extension ﬁ   Rn            1                  —
          #imm,R
n























                                                                                                    HITACHI  275
```

## PDF page 280

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=280)

```text
A.3       Instruction Set in Order by Instruction Code

Table A.50 lists instruction codes and execution states in order by instruction code.

Table A.50 Instruction Set by Instruction Code

Instruction            Code                     Operation             State    T Bit
CLRT             0000000000001000      0 ﬁ   T10
NOP                0000000000001001      No operation              1           —
RTS              0000000000001011       Delayed branch, PR                 2—
                                                  ﬁ   PC
SETT               0000000000011000          1  ﬁ   T11
DIV0U              0000000000011001        0                                   ﬁ  M/Q/T               1          0














































276 HITACHI
```

## PDF page 281

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=281)

```text
Table A.50 Instruction Set by Instruction Code (cont)

Instruction                Code                     Operation             State    T Bit
SLEEP              0000000000011011    Sleep                    3         —
CLRMAC             0000000000101000     0 ﬁ   MACH, MACL       1           —
RTE                 0000000000101011      Delayed branch, stack                               4            LSB
                                                                 area  ﬁ   PC/SR
STC    SR,Rn          0000nnnn00000010       SR ﬁ    Rn                               1                  —
BSRF   Rm*2           0000mmmm00000011      Delayed branch, PC                                2—
                                                                 ﬁ   PR, Rm + PC ﬁ   PC
STS    MACH,Rn        0000nnnn00001010       MACH ﬁ    Rn                        1                  —
STC    GBR,Rn         0000nnnn00010010       GBR ﬁ    Rn                            1                  —
STS    MACL,Rn        0000nnnn00011010       MACL  ﬁ   Rn                         1                  —
STC    VBR,Rn         0000nnnn00100010       VBR ﬁ    Rn                            1                  —
BRAF   Rm*2           0000mmmm00100011       Delayed branch, Rm +                             2—
                                                                 PC ﬁ   PC
MOVT   Rn             0000nnnn00101001       T ﬁ   Rn                                  1                  —
STS    PR,Rn          0000nnnn00101010        PR  ﬁ   Rn                               1                  —
MOV.B  Rm,@(R0,Rn)    0000nnnnmmmm0100         Rm  ﬁ   (R0 + Rn)               1               —
MOV.W  Rm,@(R0,Rn)    0000nnnnmmmm0101         Rm  ﬁ   (R0 + Rn)               1               —
MOV.L  Rm,@(R0,Rn)    0000nnnnmmmm0110         Rm  ﬁ   (R0 + Rn)               1               —
MUL.L  Rm,Rn*2        0000nnnnmmmm0111       Rn x Rm  ﬁ   MACL         2
                                                                                              (to 4)*1   —
MOV.B  @(R0,Rm),Rn    0000nnnnmmmm1100       (R0 + Rm)  ﬁ   sign                              1—
                                                                 extension  ﬁ   Rn
MOV.W  @(R0,Rm),Rn    0000nnnnmmmm1101       (R0 + Rm)  ﬁ   sign                              1—
                                                                 extension  ﬁ   Rn
MOV.L  @(R0,Rm),Rn    0000nnnnmmmm1110       (R0 + Rm)  ﬁ   Rn1—
MAC.L  @Rm+,@Rn+*2    0000nnnnmmmm1111       Signed, (Rn) x (Rm) +                            3/ (2
                                                                 MAC  ﬁ   MAC                 to 4)*1     —

MOV.L  Rm,@(disp,Rn)  0001nnnnmmmmdddd         Rm  ﬁ   (disp · 4 + Rn)      1               —
MOV.B  Rm,@Rn         0010nnnnmmmm0000          Rm  ﬁ   (Rn)                         1                —
MOV.W  Rm,@Rn         0010nnnnmmmm0001          Rm  ﬁ   (Rn)                         1                —
Notes:  1.   The normal minimum number of execution states (The number in parentheses is the
             number of states when there is contention with preceding/following instructions)
         2.  SH-2 CPU instruction







                                                                                                    HITACHI  277
```

## PDF page 282

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=282)

```text
Table A.50 Instruction Set by Instruction Code (cont)

Instruction             Code                    Operation             State    T Bit
MOV.L  Rm,@Rn      0010nnnnmmmm0010     Rm ﬁ   (Rn)                        1                —
MOV.B  Rm,@-Rn      0010nnnnmmmm0100     Rn – 1  ﬁ   Rn, Rm ﬁ                       1—
                                                         (Rn)
MOV.W  Rm,@–Rn      0010nnnnmmmm0101     Rn – 2  ﬁ   Rn, Rm ﬁ                       1—
                                                         (Rn)
MOV.L  Rm,@–Rn      0010nnnnmmmm0110     Rn – 4  ﬁ   Rn, Rm ﬁ                       1—
                                                         (Rn)
DIV0S  Rm,Rn        0010nnnnmmmm0111     MSB of Rn ﬁ   Q,                           1              Calculation
                                                         MSB of Rm ﬁ   M, M ^                   result
                                                         Q ﬁ   T
TST    Rm,Rn        0010nnnnmmmm1000     Rn & Rm, when result                       1           Test results
                                                         is 0, 1 ﬁ   T
AND   Rm,Rn       0010nnnnmmmm1001      Rn & Rm ﬁ    Rn                    1                  —
XOR    Rm,Rn        0010nnnnmmmm1010      Rn ^ Rm ﬁ    Rn                     1                  —
O R Rm,Rn  0010nnnnmmmm1011      Rn | Rm ﬁ    Rn                      1                  —
CMP/STR                      0010nnnnmmmm1100     When a byte in Rn                 1              Comparison
          Rm,Rn                                          equals a byte in Rm, 1                 result
                                                         ﬁ   T
XTRCT  Rm,Rn        0010nnnnmmmm1101     Center 32 bits of Rm                       1—
                                                         and Rn ﬁ    Rn
MULU.W Rm,Rn        0010nnnnmmmm1110     Unsigned, Rn · Rm ﬁ                        1 to 3*1   —
                                                         MAC
MULS.W Rm,Rn        0010nnnnmmmm1111     Signed, Rn · Rm ﬁ                          1 to 3*1   —
                                                         MAC
CMP/EQ Rm,Rn        0011nnnnmmmm0000     When Rn = Rm, 1 ﬁ   T   1              Comparison
                                                                                                result
CMP/HS Rm,Rn        0011nnnnmmmm0010     When unsigned and                          1              Comparison
                                                         Rn  ‡ Rm, 1 ﬁ   T                      result
CMP/GE Rm,Rn        0011nnnnmmmm0011     When signed and Rn                         1              Comparison
                                                         ‡ Rm, 1 ﬁ   T                          result
DIV1   Rm,Rn        0011nnnnmmmm0100    1-step division (Rn ‚                       1            Calculation
                                                         Rm)                                    result
DMULU.L                                                                             2 to 4*1   —
          Rm,Rn*2      0011nnnnmmmm0101     Unsigned, Rn x Rm ﬁMACH, MACL
Notes:  1.   The normal minimum number of execution states
        2.  SH-2 CPU instruction







278 HITACHI
```

## PDF page 283

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=283)

```text
Table A.50 Instruction Set by Instruction Code (cont)

Instruction                 Code                     Operation        State    T Bit
CMP/HI Rm,Rn         0011nnnnmmmm0110     When unsigned                                 1              Comparison
                                                                and Rn > Rm, 1                     result
                                                                ﬁ   T
CMP/GT Rm,Rn           0011nnnnmmmm0111      When signed and                            1              Comparison
                                                                Rn > Rm, 1 ﬁ   T                   result
SUB    Rm,Rn           0011nnnnmmmm1000       Rn – Rm  ﬁ   Rn           1                  —
SUBC   Rm,Rn           0011nnnnmmmm1010       Rn – Rm – T ﬁ                             1            Borrow
                                                                Rn, borrow ﬁ   T
SUBV   Rm,Rn           0011nnnnmmmm1011       Rn – Rm  ﬁ   Rn,                          1             Underflow
                                                                underflow  ﬁ   T
ADD    Rm,Rn           0011nnnnmmmm1100       Rm + Rn  ﬁ   Rn           1                  —
DMULS.L       Rm,Rn*2  0011nnnnmmmm1101      Signed, Rn x Rm                            2 to 4*1   —
                                                                ﬁ  MACH, MACL
ADDC   Rm,Rn          0011nnnnmmmm1110       Rn + Rm + T ﬁ                              1              Carry
                                                                Rn, carry ﬁ   T
ADDV   Rm,Rn           0011nnnnmmmm1111       Rn + Rm  ﬁ   Rn,                          1            Overflow
                                                                overflow  ﬁ   T
SHLL   Rn              0100nnnn00000000      T ‹   Rn ‹    0               1                 MSB
SHLR    Rn                  0100nnnn00000001                     0      ﬁ   Rn ﬁ   T            1              LSB
STS.L  MACH,@–Rn        0100nnnn00000010       Rn – 4  ﬁ   Rn,                          1—
                                                                MACH ﬁ   (Rn)
STC.L  SR,@-Rn         0100nnnn00000011       Rn – 4  ﬁ   Rn, SR                        2—
                                                                ﬁ   (Rn)
ROTL   Rn              0100nnnn00000100      T ‹   Rn ‹   MSB    1          MSB
ROTR   Rn            0100nnnn00000101     LSB ﬁ   Rn ﬁ   T       1              LSB
LDS.L  @Rm+,MACH        0100mmmm00000110          (Rm)  ﬁ   MACH,                       1—
                                                                Rm + 4  ﬁ    Rm
LDC.L  @Rm+,SR         0100mmmm00000111       (Rm)  ﬁ   SR, Rm +                        3            LSB
                                                                4  ﬁ    Rm
SHLL2  Rn              0100nnnn00001000       Rn<<2  ﬁ   Rn               1                  —
SHLR2  Rn              0100nnnn00001001       Rn>>2  ﬁ   Rn               1                  —
LDS    Rm,MACH         0100mmmm00001010         Rm  ﬁ   MACH        1           —
Notes: 1.   The normal minimum number of execution states
         2.  SH-2 CPU instruction








                                                                                                  HITACHI  279
```

## PDF page 284

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=284)

```text
Table A.50 Instruction Set by Instruction Code (cont)

Instruction            Code                     Operation             State    T Bit
JSR   @Rm       0100mmmm00001011      Delayed branch, PC                             2—
                                                         ﬁ   PR, Rm ﬁ   PC
LDC    Rm,SR       0100mmmm00001110          Rm  ﬁ   SR                        1              LSB
D T Rn*2  0100nnnn00010000       Rn - 1  ﬁ   Rn; if Rn is                            1              Comparison
                                                         0, 1 ﬁ   T, if Rn is                    result
                                                         nonzero, 0 ﬁ   T
CMP/PZ Rn          0100nnnn00010001        Rn  ‡ 0, 1 ﬁ   T                 1              Comparison
                                                                                                 result
STS.L  MACL,@–Rn   0100nnnn00010010       Rn – 4  ﬁ   Rn, MACL                       1—
                                                         ﬁ   (Rn)
STC.L  GBR,@-Rn    0100nnnn00010011       Rn – 4  ﬁ   Rn, GBR ﬁ                      2—
                                                         (Rn)
CMP/PL Rn          0100nnnn00010101       Rn > 0, 1 ﬁ   T                 1              Comparison
                                                                                                 result
LDS.L  @Rm+,MACL   0100mmmm00010110        (Rm)  ﬁ   MACL, Rm +                      1—
                                                         4  ﬁ    Rm
LDC.L  @Rm+,GBR    0100mmmm00010111        (Rm)  ﬁ   GBR, Rm + 4                     3—
                                                         ﬁ    Rm
SHLL8  Rn          0100nnnn00011000        Rn<<8  ﬁ   Rn                        1                  —
SHLR8  Rn          0100nnnn00011001        Rn>>8  ﬁ   Rn                        1                  —
LDS    Rm,MACL     0100mmmm00011010          Rm  ﬁ   MACL                1            —
TAS.B  @Rn        0100nnnn00011011       When (Rn) is 0, 1 ﬁ                         4           Test results
                                                         T, 1 ﬁ  MSB of (Rn)
LDC   Rm,GBR     0100mmmm00011110       Rm ﬁ   GBR                     1              —
SHAL   Rn          0100nnnn00100000       T ‹   Rn ‹    0                        1                 MSB
SHAR    Rn             0100nnnn00100001             MSB    ﬁ   Rn ﬁ   T             1              LSB
STS.L  PR,@–Rn     0100nnnn00100010       Rn – 4  ﬁ   Rn, PR ﬁ                       1—
                                                         (Rn)
STC.L  VBR,@-Rn    0100nnnn00100011       Rn – 4  ﬁ   Rn, VBR ﬁ                      2—
                                                         (Rn)
ROTCL  Rn          0100nnnn00100100       T ‹   Rn ‹   T                   1              MSB
ROTCR  Rn           0100nnnn00100101          T  ﬁ   Rn ﬁ   T                   1              LSB
LDS.L  @Rm+,PR     0100mmmm00100110          (Rm)  ﬁ   PR, Rm + 4 ﬁ                  1—
                                                         Rm
LDC.L  @Rm+,VBR    0100mmmm00100111        (Rm)  ﬁ   VBR, Rm + 4                     3—
                                                         ﬁ    Rm
Notes:    2.  SH-2 CPU instruction



280 HITACHI
```

## PDF page 285

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=285)

```text
Table A.50 Instruction Set by Instruction Code (cont)

Instruction                 Code                    Operation            State    T Bit
SHLL16 R n 0100nnnn00101000Rn<<16 ﬁ   Rn                   1                  —
SHLR16 R n 0100nnnn00101001Rn>>16 ﬁ   Rn                   1                  —
LDS    Rm,PR           0100mmmm00101010        Rm  ﬁ    PR                            1                  —
JMP    @Rm             0100mmmm00101011     Delayed branch, Rm                        2—
                                                            ﬁ   PC
LDC    Rm,VBR          0100mmmm00101110        Rm  ﬁ   VBR              1          —
MAC.W  @Rm+,@Rn+      0100nnnnmmmm1111     Signed, (Rn) · (Rm)                        3/(2)*1     —
                                                            + MAC ﬁ   MAC
MOV.L  @(disp,Rm),Rn   0101nnnnmmmmdddd      (disp + Rm) ﬁ    Rn            1                  —
MOV.B  @Rm,Rn          0110nnnnmmmm0000      (Rm)  ﬁ   sign                           1—
                                                            extension  ﬁ   Rn
MOV.W  @Rm,Rn          0110nnnnmmmm0001      (Rm)  ﬁ   sign                           1—
                                                            extension  ﬁ   Rn
MOV.L  @Rm,Rn          0110nnnnmmmm0010      (Rm)  ﬁ   Rn                         1                  —
MOV    Rm,Rn           0110nnnnmmmm0011        Rm  ﬁ    Rn                            1                  —
MOV.B  @Rm+,Rn         0110nnnnmmmm0100      (Rm)  ﬁ   sign                           1—
                                                            extension  ﬁ   Rn, Rm
                                                            + 1 ﬁ    Rm
MOV.W  @Rm+,Rn         0110nnnnmmmm0101      (Rm)  ﬁ   sign                           1—
                                                            extension  ﬁ   Rn, Rm
                                                            + 2 ﬁ    Rm
MOV.L  @Rm+,Rn         0110nnnnmmmm0110      (Rm)  ﬁ   Rn, Rm + 4                     1—
                                                            ﬁ    Rm
NOT    Rm,Rn           0110nnnnmmmm0111       ~Rm  ﬁ    Rn                          1                  —
SWAP.B Rm,Rn           0110nnnnmmmm1000        Rm  ﬁ   Swap upper                     1—
                                                            and lower halves of
                                                            lower 2 bytes ﬁ    Rn
SWAP.W Rm,Rn           0110nnnnmmmm1001        Rm  ﬁ   Swap upper                     1—
                                                            and lower word ﬁ
                                                            Rn
NEGC   Rm,Rn          0110nnnnmmmm1010      0 – Rm – T ﬁ   Rn,                        1            Borrow
                                                            borrow ﬁ   T
NEG    Rm,Rn           0110nnnnmmmm1011      0 – Rm  ﬁ   Rn                     1                  —
Notes:  1   The  normal minimum number of execution states (The number in parentheses is the
            number in contention with preceding/following instructions)







                                                                                             HITACHI  281
```

## PDF page 286

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=286)

```text
Table A.50 Instruction Set by Instruction Code (cont)

Instruction                Code                    Operation              State   T Bit
EXTU.B Rm,Rn         0110nnnnmmmm1100    Zero-extends Rm                         1—
                                                      from byte ﬁ    Rn
EXTU.W Rm,Rn          0110nnnnmmmm1101    Zero-extends Rm                        1—
                                                      from word ﬁ   Rn
EXTS.B Rm,Rn          0110nnnnmmmm1110    Sign-extends Rm                        1—
                                                      from byte ﬁ    Rn
EXTS.W Rm,Rn          0110nnnnmmmm1111    Sign-extends Rm                        1—
                                                      from word ﬁ   Rn
ADD    #imm,Rn        0111nnnniiiiiiii      Rn + imm  ﬁ   Rn                     1             —
MOV.B  R0,@(disp,Rn)  10000000nnnndddd      R0  ﬁ   (disp + Rn)             1          —
MOV.W  R0,@(disp,Rn)  10000001nnnndddd      R0  ﬁ   (disp · 2 +                  1—
                                                      Rn)
MOV.B  @(disp,Rm),R0  10000100mmmmdddd     (disp + Rm) ﬁ   sign                  1—
                                                      extension  ﬁ   R0
MOV.W  @(disp,Rm),R0  10000101mmmmdddd     (disp · 2  + Rm) ﬁ                    1—
                                                      sign extension ﬁ
                                                      R0
CMP/EQ #imm,R0        10001000iiiiiiii     When R0 = imm, 1                      1          Compariso
                                                      ﬁ   T                              n results
B T label  10001001dddddddd     When T = 1, disp ·                               3/1*3    —
                                                      2  + PC ﬁ   PC;
                                                      When T = 0, nop.
BT/S   label*         10001101dddddddd    When T = 1, disp ·                     2/1*3    —
                                                      2  + PC ﬁ   PC;
                                                      When T = 1, nop.
B F label  10001011dddddddd     When T = 0, disp ·                               3/1*3    —
                                                      2  + PC ﬁ   PC;
                                                      When T = 0, nop
BF/S   label*        10001111dddddddd    When T = 0, disp ·                      2/1*3    —
                                                      2 + PC ﬁ   PC;
                                                      When T = 1, nop
MOV.W  @(disp,PC),Rn  1001nnnndddddddd    (disp · 2  + PC) ﬁ                     1—
                                                      sign extension ﬁ
                                                      Rn
BRA    label          1010dddddddddddd     Delayed branch,                       2—
                                                      disp · 2  + PC ﬁ
                                                      PC
Notes:  2.  SH-2 CPU instruction
        3.  One state when it does not branch




282 HITACHI
```

## PDF page 287

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=287)

```text
Table A.50 Instruction Set by Instruction Code (cont)

Instruction                  Code                   Operation                 State   T Bit
BSR   label          1011dddddddddddd   Delayed branch, PC ﬁ                              2—
                                                           PR, disp · 2 + PC ﬁ   PC
MOV.B  R0,@(disp,GBR)  11000000dddddddd    R0  ﬁ   (disp  + GBR)             1           —
MOV.W  R0,@(disp,GBR)  11000001dddddddd    R0  ﬁ   (disp  · 2 + GBR)       1           —
MOV.L  R0,@(disp,GBR)  11000010dddddddd    R0  ﬁ   (disp  · 4 + GBR)       1           —
TRAPA  #imm            11000011iiiiiiii   PC/SR ﬁ   Stack area,                           8—
                                                           (imm  · 4 + VBR) ﬁ   PC
MOV.B  @(disp,GBR),R0  11000100dddddddd   (disp + GBR) ﬁ   sign                           1—
                                                           extension  ﬁ   R0
MOV.W  @(disp,GBR),R0  11000101dddddddd   (disp · 2 + GBR) ﬁ   sign                       1—
                                                           extension  ﬁ   R0
MOV.L  @(disp,GBR),R0  11000110dddddddd   (disp · 4 + GBR) ﬁ    R0           1               —
MOVA   @(disp,PC),R0   11000111dddddddd   disp · 4 + PC ﬁ   R0                 1               —
TST    #imm,R0         11001000iiiiiiii    R0 & imm, when result is                       1           Test
                                                           0, 1 ﬁ   T                               results
AND    #imm,R0         11001001iiiiiiii    R0 & imm ﬁ    R0                          1               —
XOR    #imm,R0         11001010iiiiiiii    R0 ^ imm ﬁ    R0                           1               —
O R #imm,R0  11001011iiiiiiii    R0 | imm ﬁ    R0                            1               —
TST.B  #imm,@(R0,GBR)  11001100iiiiiiii   (R0 + GBR) & imm, when                          3           Test
                                                           result is 0, 1 ﬁ   T                     results
AND.B  #imm,@(R0,GBR)  11001101iiiiiiii    (R0 + GBR) & imm ﬁ   (R0                       3—
                                                           + GBR)
XOR.B  #imm,@(R0,GBR)  11001110iiiiiiii    (R0 + GBR) ^ imm ﬁ   (R0                       3—
                                                           + GBR)
OR.B   #imm,@(R0,GBR)  11001111iiiiiiii    (R0 + GBR) | imm ﬁ   (R0 +                     3—
                                                           GBR)
MOV.L  @(disp,PC),Rn   1101nnnndddddddd   (disp · 4 + PC) ﬁ   Rn              1               —
MOV    #imm,Rn         1110nnnniiiiiiii    imm  ﬁ   sign extension ﬁ                      1—
                                                           Rn
















                                                                                           HITACHI  283
```

## PDF page 288

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=288)

```text
A.4      Operation Code Map

Table A.51 is an operation code map.

Table A.51 Operation Code Map

Instruction  Code       Fx: 0000       Fx: 0001        Fx: 0010       Fx:  0011–1111
MSB             LSB  MD: 00         MD: 01          MD: 10        MD: 11
0000  Rn    Fx    0000
0000  Rn    Fx    0001
0000  Rn    Fx    0010 STC   SR,Rn* STC   GBR,Rn STC   VBR,Rn
0000  Rm    Fx    0011 BSRF  Rm*              BRAF  Rm*
0000  Rn    Rm    01MD MOV.B              MOV.W           MOV.L           MUL.L
                         Rm,@(R0,Rn)      Rm,@(R0,Rn)     Rm,@(R0,Rn)     Rm,Rn*
0000  0000  Fx    1000 CLRT       SETT        CLRMAC
0000  0000  Fx    1001 NOP        DIV0U
0000  0000  Fx    1010
0000  0000  Fx    1011 RTS        SLEEP       RTE
0000  Rn    Fx    1000
0000  Rn    Fx    1001                      MOVT  Rn
0000  Rn    Fx    1010 STS MACH,Rn  STS MACL,Rn   STS    PR,Rn
0000   Rn       Fx       1011
0000   Rn       Fx       11MD MOV.B       MOV.W           MOV.L           MAC.L
                         @(R0,Rm),Rn      @(R0,Rm),Rn     @(R0,Rm),Rn     @Rm+,@Rn+*
0001   Rn       Rm      disp  MOV.L  Rm,@(disp:4,Rn)
0010   Rn       Rm      00MD MOV.B  Rm,@Rn MOV.W  Rm,@Rn MOV.L  Rm,@Rn
0010   Rn       Rm      01MD MOV.B        MOV.W           MOV.L           DIV0S  Rm,Rn
                         Rm,@-Rn          Rm,@-Rn         Rm,@-Rn
0010   Rn       Rm      10MD TST    Rm,Rn  AND    Rm,Rn  XOR    Rm,Rn  O R Rm,Rn
0010Rn Rm 11MDCMP/STR                     XTRCTRm,RnMULU.WRm,RnMULS.WRm,Rn
                         Rm,Rn
0011Rn Rm 00MDCMP/EQRm,Rn   CMP/HSRm,RnCMP/GERm,Rn
0011Rn Rm 01MDDIV1 Rm,RnDMULU.L                           CMP/HIRm,RnCMP/GTRm,Rn
                                          Rm,Rn*
0011Rn Rm 10MDSUB Rm,Rn   SUBC Rm,RnSUBV Rm,Rn
0011Rn Rm 11MDADD Rm,RnDMULS.L                            ADDC Rm,RnADDV Rm,Rn
                                          Rm,Rn*
0100Rn Fx 0000SHLL Rn DT Rn* SHAL Rn



284 HITACHI
```

## PDF page 289

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=289)

```text
HITACHI  285
```

## PDF page 290

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=290)

```text
Table A.51 Operation Code Map (cont)

Instruction  Code       Fx: 0000       Fx: 0001       Fx: 0010       Fx:  0011–1111
MSB             LSB  MD: 00         MD: 01         MD: 10         MD: 11
0100  Rn    Fx    0001 SHLR  Rn    CMP/PZ Rn    SHAR  Rn
0100  Rn    Fx    0010 STS.L                STS.L            STS.L
                          MACH,@–Rn         MACL,@–Rn        PR,@–Rn
0100  Rn    Fx    0011 STC.L                STC.L            STC.L
                          SR,@–Rn           GBR,@–Rn         VBR,@–Rn
0100  Rn    Fx    0100 ROTL  Rn              ROTCL Rn
0100  Rn    Fx    0101 ROTR  Rn    CMP/PL Rn    ROTCR Rn
0100  Rm    Fx    0110 LDS.L                LDS.L            LDS.L
                          @Rm+,MACH         @Rm+,MACL        @Rm+,PR
0100  Rm    Fx    0111 LDC.L                LDC.L            LDC.L
                          @Rm+,SR           @Rm+,GBR         @Rm+,VBR
0100  Rn    Fx    1000 SHLL2 Rn    SHLL8 Rn    SHLL16 Rn
0100  Rn    Fx    1001 SHLR2 Rn    SHLR8 Rn    SHLR16 Rn
0100  Rm    Fx    1010 LDS  Rm,MACH LDS  Rm,MACL LDS   Rm,PR
0100  Rm/    Fx    1011 JSR   @Rm   TAS.B @Rn   JMP   @Rm
      Rn
0100  Rm    Fx    1100
0100  Rm    Fx    1101
0100  Rn    Fx    1110 LDC   Rm,SR LDC   Rm,GBR LDC   Rm,VBR
0100  Rn    Rm    1111 MAC.W @Rm+,@Rn+
0101  Rn    Rm   disp  MOV.L @(disp:4,Rm),Rn
0110  Rn    Rm    00MD MOV.B Rm,Rn MOV.W @Rm,Rn MOV.L @Rm,Rn MOV   Rm,Rn
0110  Rn    Rm    01MD MOV.B Rm+,Rn MOV.W  @Rm+,Rn MOV.L  @Rm+,Rn NOT   Rm,Rn
0110  Rn    Rm    10MD SWAP.B               SWAP.W           NEGC  Rm,Rn NEG   Rm,Rn
                          Rm,Rn             Rm,Rn
0110  Rn    Rm    11MD EXTU.B Rm,Rn EXTU.W Rm,Rn EXTS.B Rm,Rn EXTS.W Rm,Rn
0111  Rn         imm   ADD   #imm:8,Rn
1000  00MD Rn    disp  MOV.B  R0,           MOV.W  R0,
                          @(disp:4,Rn)      @(disp:4,Rn)
1000  01MD Rm   disp  MOV.B                 MOV.W
                          @(disp:4,         @(disp:4,
                          Rm),R0            Rm),R0
1000  10MD   imm/disp  CMP/EQ               BT  label:8            BF  label:8
                          #imm:8,R0




286 HITACHI
```

## PDF page 291

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=291)

```text
Table A.51 Operation Code Map (cont)

Instruction  Code       Fx: 0000       Fx: 0001       Fx: 0010       Fx:  0011–1111
MSB             LSB  MD: 00         MD: 01         MD: 10         MD: 11
1000  11MD   imm/disp            BT/S                                                BF/S
                                                label:8*                             label:8*
1001  Rn        disp    MOV.W @(disp:8,PC),Rn
1010               disp      BRA    label:12
1011               disp      BSR    label:12
1100   00MD     imm/disp  MOV.B  R0,            MOV.W  R0,        MOV.L  R0,         TRAPA  #imm:8
                             @(disp:8,          @(disp:8,         @(disp:8,
                             GBR)               GBR)              GBR)
1100   01MD        disp    MOV.B                MOV.W             MOV.L              MOVA
                             @(disp:8,          @(disp:8,         @(disp:8,          @(disp:8,
                             GBR),R0            GBR),R0           GBR),R0            PC),R0
1100   10MD         imm    TST                  AND               XOR                O R
                             #imm:8,R0          #imm:8,R0         #imm:8,R0          #imm:8,R0
1100   11MD         imm    TST.B                AND.B             XOR.B              OR.B
                             #imm:8,            #imm:8,           #imm:8,            #imm:8,
                             @(R0,GBR)          @(R0,GBR)         @(R0,GBR)          @(R0,GBR)
1101   Rn             disp    MOV.L  @(disp:8,PC),R0
1110   Rn              imm    MOV    #imm:8,Rn
1111                    ...
Note:    SH-2 CPU instructions




























                                                                                        HITACHI  287
```

## PDF page 292

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=292)

```text
          Appendix B        Pipeline Operation and Contention


The SH-1 and SH-2 CPU is designed so that basic instructions are executed in one state. Two
or more states are required for instructions when, for example, the branch destination address
is changed by a branch instruction or when the number of states is increased by contention
between MA and IF. Table B.1 gives the number of execution states and stages for different
types of contention and their instructions. Instructions without contention and instructions that
require 2 or more cycles even without contention are also shown.

Instructions experience contention in the following ways:

•   Operations and transfers between registers are executed in one state with no contention.
•   No contention occurs, but the instruction still requires 2 or more cycles.
•  Contention occurs, increasing the number of execution states. Contention combinations
    are as follows:
    ¾   MA contends with IF
    ¾   MA contends with IF and sometimes with memory loads as well
    ¾   MA contends with IF and sometimes with the multiplier as well
    ¾   MA contends with IF and sometimes with memory loads and sometimes with the
        multiplier



































288 HITACHI
```

## PDF page 293

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=293)

```text
Table B.1   Instructions and Their Contention Patterns

Contention                       State    Stage   Instruction
None                                 1         3           Transfer between registers
                                                                     Operation between registers (except
                                                                     multiplication instruction)
                                                                     Logical operation between registers
                                                                     Shift instruction
                                                                     System control ALU instruction
                                            2          3          Unconditional branch
                                            3/1*3      3           Conditional branch
                                            3           3           SLEEP instruction
                                            4           5           RTE instruction
                                            8           9           TRAP instruction
MA contends with IF                  1           4           Memory store instruction and STS.L
                                                                     instruction (PR)
                                            2         4         STC.L instruction
                                            3          6          Memory logic operations
                                            4         6         TAS instruction
MA contends with IF and                     1           5           Memory load instructions and LDS.L
sometimes with memory loads as                                       instruction (PR)
well                                      3          5          LDC.L instruction

MA contends with IF and                     1           4           Register to MAC transfer instruction,
sometimes with the multiplier as                                     memory to MAC transfer instruction and
well                                                                 MAC to memory transfer instruction
                                            1 to 3
                                            *2            6/7*1     Multiplication instruction
                                            3/(2)*2    7/8*1      Multiply/accumulate instruction
                                            3/(2 to
                                            4)* 2        9           Double-length multiply/accumulateinstruction (SH-2 only)
                                            2 to 4*2   9          Double-length multiplication instruction
                                                                     (SH-2 only)
MA contends with IF and                     1           5           MAC to register transfer instruction
sometimes with memory loads
and sometimes with the multiplier
Notes: 1.  With the SH-2 CPU, multiply/accumulate instructions are 7 stages and multiplication
              instructions are 6 stages, while with the SH-1 CPU, multiply/accumulate instructions
              are 8 stages and multiplication instructions are 7 stages.
         2.   The normal minimum number of execution states (The number in parentheses is the
              number in contention with preceding/following instructions).
         3.  One stage when it does not branch.






                                                                                                        HITACHI  289
```
