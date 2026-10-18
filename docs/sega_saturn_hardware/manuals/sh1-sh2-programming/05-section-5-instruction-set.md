# Section 5 Instruction Set

SH-1/SH-2 Programming Manual — Hitachi, September 3, 1996.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=38)

Physical PDF pages 38–60. Text extraction; consult the PDF for figures.

## PDF page 38

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=38)

```text
                                 Section 5   Instruction Set


5.1        Instruction Set by Classification

Table 5.1 lists instructions by classification.

Table 5.1   Classification of Instructions

                                                                                   Applicable
                                                                                   Instructions

                            Operation                                                             No. of
Classification    Types     Code           Function                                    SH-2    SH-1Instructions
Data transfer      5            MOV            Data transfer                      XX39
                                           Immediate data transfer
                                           Peripheral module data transfer
                                           Structure data transfer
                            MOVA       Effective address transfer          X       X
                            MOVT      T-bit transfer                        X      X
                            SWAP        Swap of upper and lower                   XX
                                           bytes
                            XTRCT      Extraction of the middle of                XX
                                           registers connected


































34 HITACHI
```

## PDF page 39

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=39)

```text
Table 5.1    Classification of Instructions (cont)

                                                                                        Applicable
                                                                                        Instructions

                              Operation                                                                 No. of
Classification     Types      Code           Function                                      SH-2    SH-1 Instructions
Arithmetic        21        ADD          Binary addition                        X       X       33
operations                  ADDC        Binary addition with carry          X       X

                              ADDV       Binary addition with overflow                 XX
                                              check
                              CMP/con         Comparison                         X      X
                              d
                              DIV1         Division                              X      X
                              DIV0S       Initialization of signed division    X       X
                              DIV0U       Initialization of unsigned                   XX
                                              division
                              DMULS     Signed double-length                           X
                                              multiplication
                              DMULU     Unsigned double-length                         X
                                              multiplication
                              DT             Decrement and test                   X
                              EXTS        Sign extension                      X      X
                              EXTU       Zero extension                    X      X
                              MAC         Multiply/accumulate, double-                 XX
                                              length multiply/accumulate
                                              operation*1
                              MUL         Double-length multiplication      X
                              MULS       Signed multiplication               X      X
                              MULU       Unsigned multiplication            X       X
                              NEG         Negation                             X      X
                              NEGC       Negation with borrow              X      X
                              SUB          Binary subtraction                   X       X
                              SUBC       Binary subtraction with borrow   X      X
                              SUBV       Binary subtraction with                       XX
                                              underflow check
Notes   1.  Double-length multiply/accumulate is an SH-2 function.














                                                                                                        HITACHI  35
```

## PDF page 40

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=40)

```text
Table 5.1    Classification of Instructions (cont)

                                                                                         Applicable
                                                                                         Instructions

                                Operation                                                                  No. of
Classification     Types        Code           Function                                      SH-2    SH-1  Instructions
Logic               6           AND          Logical AND                            X       X       14
operations                 NOT        Bit inversion                        X      X

                                OR             Logical OR                                 X        X
                                TAS          Memory test and bit set            X       X
                                TST           Logical AND and T-bit set           X        X
                                XOR          Exclusive OR                           X       X
Shift               10       ROTL       One-bit left rotation                X      X      14
                                ROTR       One-bit right rotation              X      X
                                ROTCL     One-bit left rotation with T bit    X       X
                                ROTCR     One-bit right rotation with T bit   X      X
                                SHAL        One-bit arithmetic left shift         X       X
                                SHAR       One-bit arithmetic right shift       X       X
                                SHLL        One-bit logical left shift            X       X
                                SHLLn      n-bit logical left shift                X       X
                                SHLR       One-bit logical right shift          X      X
                                SHLRn      n-bit logical right shift               X       X
Branch           9         BF           Conditional branch, conditional                  XX11
                                               branch with delay*2 (T = 0)
                                BT           Conditional branch, conditional             XX
                                               branch with delay*2 (T = 1)
                                BRA         Unconditional branch              X      X
                                BRAF       Unconditional branch              X
                                BSR         Branch to subroutine                         XX
                                               procedure
                                BSRF       Branch to subroutine                          X
                                               procedure
                                JMP         Unconditional branch              X      X
                                JSR          Branch to subroutine                        XX
                                               procedure
                                RTS          Return from subroutine                      XX
                                               procedure
Notes   2.  Conditional branch with delay is an SH-2 CPU function.










36 HITACHI
```

## PDF page 41

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=41)

```text
Table 5.1    Classification of Instructions (cont)

                                                                                            Applicable
                                                                                            Instructions

                                 Operation                                                                    No. of
Classification     Types         Code             Function                                   SH-2    SH-1     Instructions
System             11          CLRT          T-bit clear                            X       X       31
control                       CLRMAC      MAC register clear                  X       X

                                 LDC           Load to control register          X       X
                                 LDS             Load to system register           X       X
                                 NOP           No operation                       X       X
                                 RTE            Return from exception                       XX
                                                   processing
                                 SETT         T-bit set                           X      X
                                 SLEEP        Shift into power-down mode   X      X
                                 STC           Storing control register data    X       X
                                 STS            Storing system register data    X       X
                                 TRAPA       Trap exception processing     X      X
           Total:  62                                                                         142


































                                                                                                              HITACHI  37
```

## PDF page 42

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=42)

```text
Instruction codes, operation, and execution states are listed in table 5.2 in order by
classification.

Table 5.2   Instruction Code Format

Item            Format                Explanation
Instruction        OP.Sz  SRC,DEST    OP: Operation code
mnemonic                                       Sz: Size
                                               SRC: Source
                                               DEST: Destination
                                               Rm: Source register
                                               Rn: Destination register
                                               imm: Immediate data
                                               disp: Displacement*
Instruction        MSB «   LSB              mmmm: Source register
code                                           nnnn: Destination register
                                                          0000: R0
                                                          0001: R1
                                                               ...........
                                                          1111: R15
                                               iiii: Immediate data
                                               dddd: Displacement
Operation          ﬁ , ‹                       Direction of transfer
summary            (xx)                        Memory operand
                   M/Q/T                       Flag bits in the SR
                   &                           Logical AND of each bit
                   |                           Logical OR of each bit
                   ^                           Exclusive OR of each bit
                   ~                           Logical NOT of each bit
                   <<n, >>n                    n-bit left/right shift
Execution                                      Value when no wait states are inserted
cycle
Instruction                                    The execution cycles shown in the table are minimums.
execution                                      The actual number of cycles may be increased:
cycles                                         1.  When contention occurs between instruction fetches
                                                   and data access, or
                                               2.   When the destination register of the load instruction
                                                   (memory ﬁ   register) and the register used by the next
                                                   instruction are the same.
T bit                                          Value of T bit after instruction is executed
—                                                    No change


Note:   Scaling (x1, x2, x4) is performed according to the instruction operand size. See "6.
         Instruction Descriptions" for details.





38 HITACHI
```

## PDF page 43

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=43)

```text
5.1.1     Data Transfer Instructions

Tables 5.3 to 5.8 list the minimum number of clock states required for execution.




























































                                                                                                   HITACHI  39
```

## PDF page 44

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=44)

```text
Table 5.3   Data Transfer Instructions

                                                                                                  Execu-
                                                                                                  tion         T
Instruction              Instruction Code     Operation                                           State        Bit
MOV   #imm,Rn      1110nnnniiiiiiii   imm ﬁ   Sign extension ﬁ                                    1—
                                                               Rn
MOV.W  @(disp,PC),Rn  1001nnnndddddddd     (disp · 2  + PC) ﬁ   Sign                              1—
                                                               extension ﬁ   Rn
MOV.L  @(disp,PC),Rn  1101nnnndddddddd     (disp · 4 + PC) ﬁ   Rn                1                    —
MOV    Rm,Rn          0110nnnnmmmm0011        Rm  ﬁ    Rn                                        1                    —
MOV.B  Rm,@Rn         0010nnnnmmmm0000        Rm  ﬁ   (Rn)                                 1                  —
MOV.W  Rm,@Rn         0010nnnnmmmm0001        Rm  ﬁ   (Rn)                                 1                  —
MOV.L  Rm,@Rn         0010nnnnmmmm0010        Rm  ﬁ   (Rn)                                 1                  —
MOV.B  @Rm,Rn         0110nnnnmmmm0000      (Rm)  ﬁ   Sign extension ﬁ                            1—
                                                               Rn
MOV.W  @Rm,Rn        0110nnnnmmmm0001     (Rm) ﬁ   Sign extension ﬁ                               1—
                                                               Rn
MOV.L  @Rm,Rn        0110nnnnmmmm0010     (Rm) ﬁ   Rn                                     1                    —
MOV.B  Rm,@–Rn        0010nnnnmmmm0100      Rn–1  ﬁ   Rn, Rm ﬁ   (Rn)          1                  —
MOV.W  Rm,@–Rn        0010nnnnmmmm0101      Rn–2  ﬁ   Rn, Rm ﬁ   (Rn)          1                  —
MOV.L  Rm,@–Rn        0010nnnnmmmm0110      Rn–4  ﬁ   Rn, Rm ﬁ   (Rn)          1                  —
MOV.B  @Rm+,Rn        0110nnnnmmmm0100      (Rm)  ﬁ   Sign extension ﬁ                            1—
                                                               Rn,Rm + 1 ﬁ    Rm
MOV.W  @Rm+,Rn        0110nnnnmmmm0101      (Rm)  ﬁ   Sign extension ﬁ                            1—
                                                               Rn,Rm + 2 ﬁ    Rm
MOV.L  @Rm+,Rn        0110nnnnmmmm0110      (Rm)  ﬁ   Rn,Rm + 4 ﬁ    Rm         1                       —
MOV.B  R0,@(disp,Rn)  10000000nnnndddd       R0  ﬁ   (disp  + Rn)                  1                —
MOV.W  R0,@(disp,Rn)  10000001nnnndddd       R0  ﬁ   (disp  · 2 + Rn)           1               —
MOV.L  Rm,@(disp,Rn)  0001nnnnmmmmdddd       Rm  ﬁ   (disp · 4 + Rn)            1                —
MOV.B  @(disp,Rm),R0  10000100mmmmdddd      (disp + Rm) ﬁ   Sign                                  1—
                                                               extension ﬁ   R0
MOV.W  @(disp,Rm),R0  10000101mmmmdddd     (disp · 2 + Rm) ﬁ   Sign                               1—
                                                               extension ﬁ   R0
MOV.L  @(disp,Rm),Rn  0101nnnnmmmmdddd     (disp · 4 + Rm) ﬁ   Rn                1                    —
MOV.B  Rm,@(R0,Rn)    0000nnnnmmmm0100        Rm  ﬁ   (R0 + Rn)                       1                 —
MOV.W  Rm,@(R0,Rn)    0000nnnnmmmm0101        Rm  ﬁ   (R0 + Rn)                       1                 —








40 HITACHI
```

## PDF page 45

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=45)

```text
Table 5.3   Data Transfer Instructions (cont)

                                                                                           Execu-
                                                                                           tion       T
Instruction               Instruction Code    Operation                                    State      Bit
MOV.L Rm,@(R0,Rn)    0000nnnnmmmm0110   Rm ﬁ   (R0 + Rn)                     1                —
MOV.B  @(R0,Rm),Rn     0000nnnnmmmm1100     (R0 + Rm)  ﬁ   Sign                            1—
                                                            extension ﬁ   Rn
MOV.W  @(R0,Rm),Rn     0000nnnnmmmm1101     (R0 + Rm)  ﬁ   Sign                            1—
                                                            extension ﬁ   Rn
MOV.L  @(R0,Rm),Rn     0000nnnnmmmm1110     (R0 + Rm)  ﬁ   Rn                         1                   —
MOV.B  R0,@(disp,GBR)  11000000dddddddd      R0  ﬁ   (disp + GBR)              1              —
MOV.W  R0,@(disp,GBR)  11000001dddddddd      R0  ﬁ   (disp · 2 + GBR)        1               —
MOV.L  R0,@(disp,GBR)  11000010dddddddd      R0  ﬁ   (disp · 4+ GBR)        1              —
MOV.B  @(disp,GBR),R0  11000100dddddddd     (disp + GBR) ﬁ   Sign                          1—
                                                            extension ﬁ   R0
MOV.W  @(disp,GBR),R0  11000101dddddddd     (disp · 2 + GBR) ﬁ   Sign                      1—
                                                            extension ﬁ   R0
MOV.L  @(disp,GBR),R0  11000110dddddddd     (disp · 4 + GBR) ﬁ   R0        1              —
MOVA   @(disp,PC),R0   11000111dddddddd     disp · 4 + PC ﬁ   R0                 1                   —
MOVT   Rn              0000nnnn00101001     T ﬁ   Rn                                         1                   —
SWAP.B Rm,Rn           0110nnnnmmmm1000       Rm  ﬁ   Swap upper and                       1—
                                                            lower 2 bytesﬁ   Rn
SWAP.W Rm,Rn           0110nnnnmmmm1001       Rm  ﬁ   Swap upper and                       1—
                                                            lower word ﬁ   Rn
XTRCT  Rm,Rn           0010nnnnmmmm1101     Center 32 bits of Rm and                       1—
                                                            Rnﬁ    Rn
























                                                                                             HITACHI  41
```

## PDF page 46

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=46)

```text
5. 1.2    Arithmetic Instructions

Table 5.4   Arithmetic Instructions

                                                                                      Execution
Instruction          Instruction Code   Operation                                     State       T Bit
ADD   Rm,Rn      0011nnnnmmmm110                         Rn + Rm  ﬁ   Rn                   1                         —
                             0
ADD    #imm,Rn     0111nnnniiiiiii                       Rn + imm  ﬁ   Rn                 1                         —
                             i
ADDC   Rm,Rn       0011nnnnmmmm111                       Rn + Rm + T ﬁ   Rn,          1                    Carry
                             0                           Carry ﬁ   T
ADDV   Rm,Rn       0011nnnnmmmm111                       Rn + Rm  ﬁ   Rn,             1                 Overflow
                             1                           Overflow  ﬁ   T
CMP/EQ #imm,R0     10001000iiiiiii                       If R0 = imm, 1 ﬁ   T        1                    Compariso
                             i                                                                        n result
CMP/EQ Rm,Rn       0011nnnnmmmm000                       If Rn = Rm, 1 ﬁ   T         1                    Compariso
                             0                                                                        n result
CMP/HS Rm,Rn       0011nnnnmmmm001                       If Rn‡Rm with                1                   Compariso
                             0                           unsigned data, 1 ﬁ   T                       n result
CMP/GE Rm,Rn       0011nnnnmmmm001                       If Rn ‡  Rm with             1                   Compariso
                             1                           signed data, 1 ﬁ   T                         n result
CMP/HI Rm,Rn       0011nnnnmmmm011                       If Rn > Rm with              1                   Compariso
                             0                           unsigned data, 1 ﬁ   T                       n result
CMP/GT Rm,Rn       0011nnnnmmmm011                       If Rn > Rm with              1                   Compariso
                             1                           signed data, 1 ﬁ   T                         n result
CMP/PL R n         0100nnnn0001010                       If Rn > 0, 1 ﬁ   T             1                    Compariso
                             1                                                                        n result
CMP/PZ R n         0100nnnn0001000                       If Rn ‡ 0, 1 ﬁ   T             1                    Compariso
                             1                                                                        n result
CMP/STR                      0010nnnnmmmm110             If Rn and Rm have an         1                Compariso
          Rm,Rn              0                           equivalent byte, 1 ﬁ                         n result
                                                         T
DIV1   Rm,Rn       0011nnnnmmmm010                       Single-step division         1             Calculation
                             0                           (Rn/Rm)                                      result
DIV0S  Rm,Rn      0010nnnnmmmm011                        MSB of Rn ﬁ   Q,             1                   Calculation
                             1                           MSB of Rm ﬁ   M, M                           result
                                                         ^ Q ﬁ   T
DIV0U              000000000001100                       0  ﬁ  M/Q/T              1              0
                             1







42 HITACHI
```

## PDF page 47

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=47)

```text
Table 5.4   Arithmetic Instructions (cont)

                                                                             Execution
Instruction          Instruction Code    Operation                           State       T Bit
DMULS.L                                                                      2 to 4*1        —
         Rm,Rn*2    0011nnnnmmmm1101   Signed operation ofRn x Rm  ﬁ   MACH,
                                                   MACL
                                                   32 x 32 ﬁ   64 bits
DMULU.L                                                                      2 to 4*1        —
         Rm,Rn*2     0011nnnnmmmm0101   Unsigned operationof Rn x Rm ﬁ
                                                   MACH, MACL
                                                   32 x 32 ﬁ   64 bits
DT     Rn*2       0100nnnn00010000    Rn - 1  ﬁ   Rn, when                   1                   Compariso
                                                   Rn is 0, 1 ﬁ   T. When                  n result
                                                   Rn is nonzero, 0 ﬁ   T
EXTS.B Rm,Rn       0110nnnnmmmm1110    A byte in Rm is sign-                 1—
                                                   extended  ﬁ   Rn
EXTS.W Rm,Rn       0110nnnnmmmm1111    A word in Rm is sign-                 1—
                                                   extended  ﬁ   Rn
EXTU.B Rm,Rn       0110nnnnmmmm1100    A byte in Rm is zero-                 1—
                                                   extended  ﬁ   Rn
EXTU.W Rm,Rn       0110nnnnmmmm1101    A word in Rm is zero-                 1—
                                                   extended  ﬁ   Rn
MAC.L  @Rm+,@Rn+          0000nnnnmmmm1111   Signed operation of             3/(2 to 4)*1   —
                *2                                 (Rn) x (Rm) + MAC
                                                   ﬁ   MAC
                                                   32 x 32 + 64ﬁ   64
                                                   bits
MAC.W  @Rm+,@Rn+   0100nnnnmmmm1111   Signed operation of                    3/(2)*1          —
                                                   (Rn) · (Rm) + MAC
                                                   ﬁ   MAC
                                                   (SH-2 CPU) 16 x 16
                                                   + 64 ﬁ   64 bits
                                                   (SH-1 CPU) 16 x 16
                                                   + 42 ﬁ   42 bits
MUL.L  Rm,Rn*2     0000nnnnmmmm0111    Rn x Rm  ﬁ  MACL,                     2 to 4*1        —
                                                   32 x 32 ﬁ   32 bits
MULS.W Rm,Rn      0010nnnnmmmm1111   Signed operation of                     1 to 3*1        —
                                                   Rn · Rm ﬁ   MAC
                                                   16 x 16 ﬁ   32 bits
Notes:  1.   The normal minimum number of execution states  (The number in parentheses is the
            number of states when there is contention with preceding/following instructions)
        2.  SH-2 CPU instructions



                                                                                           HITACHI  43
```

## PDF page 48

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=48)

```text
Table 5.4   Arithmetic Instructions (cont)

                                                                                   Execution
Instruction          Instruction Code    Operation                                 State       T Bit
MULU.W Rm,Rn      0010nnnnmmmm1110   Unsigned operation                            1 to 3*1        —
                                                        of Rn · Rm ﬁ   MAC
                                                        16 x 16 ﬁ   32 bits
NEG    Rm,Rn      0110nnnnmmmm1011    0–Rm ﬁ   Rn                        1                         —
NEGC   Rm,Rn       0110nnnnmmmm1010    0–Rm–T  ﬁ   Rn,                             1                 Borrow
                                                        Borrow ﬁ   T
SUB    Rm,Rn       0011nnnnmmmm1000     Rn–Rm  ﬁ   Rn                     1                         —
SUBC   Rm,Rn       0011nnnnmmmm1010    Rn–Rm–T  ﬁ   Rn,                            1                 Borrow
                                                        Borrow ﬁ   T
SUBV   Rm,Rn       0011nnnnmmmm1011     Rn–Rm  ﬁ   Rn,                             1                  Underflow
                                                        Underflow ﬁ   T
Notes:  1.   The normal minimum number of execution states  (The number in parentheses is the
             number of states when there is contention with preceding/following instructions)

5.1.3     Logic Operation Instructions

Table 5.5   Logic Operation Instructions

                                                                                           Executio
Instruction              Instruction Code    Operation                                     n State    T Bit
AND   Rm,Rn        0010nnnnmmmm1001    Rn & Rm ﬁ    Rn                      1                        —
AND    #imm,R0         11001001iiiiiiii    R0 & imm ﬁ    R0                    1                        —
AND.B  #imm,@(R0,GBR)  11001101iiiiiiii    (R0 + GBR) & imm ﬁ                              3—
                                                              (R0 + GBR)
NOT    Rm,Rn           0110nnnnmmmm0111     ~Rm  ﬁ    Rn                              1                        —
OR     Rm,Rn           0010nnnnmmmm1011    Rn | Rm ﬁ    Rn1—
OR     #imm,R0         11001011iiiiiiii    R0 | imm ﬁ    R0                      1                        —
OR.B   #imm,@(R0,GBR)  11001111iiiiiiii    (R0 + GBR) | imm ﬁ                              3—
                                                              (R0 + GBR)
TAS.B  @Rn             0100nnnn00011011    If (Rn) is 0, 1 ﬁ   T; 1 ﬁ                      4               Test
                                                              MSB of (Rn)                                 result
TST    Rm,Rn          0010nnnnmmmm1000    Rn & Rm; if the result is                        1                  Test
                                                              0, 1 ﬁ   T                                  result
TST    #imm,R0         11001000iiiiiiii    R0 & imm; if the result                         1                  Test
                                                              is 0, 1 ﬁ   T                               result






44 HITACHI
```

## PDF page 49

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=49)

```text
Table 5.5   Logic Operation Instructions (cont)

                                                                                                          Executio
Instruction              Instruction Code    Operation                                                    n State    T Bit
TST.B #imm,@(R0,GBR) 11001100iiiiiiii    (R0 + GBR) & imm; if                                             3                  Test
                                                                        the result is 0, 1 ﬁ   T                           result
XOR    Rm,Rn           0010nnnnmmmm1010    Rn ^ Rm ﬁ    Rn                       1                        —
XOR    #imm,R0         11001010iiiiiiii    R0 ^ imm ﬁ    R0                     1                        —
XOR.B  #imm,@(R0,GBR)  11001110iiiiiiii    (R0 + GBR) ^ imm ﬁ                                             3—
                                                                        (R0 + GBR)


5.1.4     Shift Instructions

Table 5.6   Shift Instructions

Instruction     Instruction Code    Operation           Execution State     T Bit
ROTL  Rn    0100nnnn00000100  T ‹  Rn ‹  MSB      1                 MSB
ROTR  Rn    0100nnnn00000101  LSB ﬁ   Rn ﬁ   T               1                                     LSB
ROTCL  Rn      0100nnnn00100100     T  ‹   Rn  ‹    T                 1                                  MSB
ROTCR  Rn      0100nnnn00100101     T  ﬁ   Rn ﬁ   T                    1                                     LSB
SHAL   Rn      0100nnnn00100000     T  ‹   Rn  ‹    0                  1                                  MSB
SHAR   Rn      0100nnnn00100001     MSB  ﬁ   Rn ﬁ   T              1                                     LSB
SHLL   Rn      0100nnnn00000000     T  ‹   Rn  ‹    0                  1                                  MSB
SHLR   Rn      0100nnnn00000001       0   ﬁ   Rn ﬁ   T                    1                                     LSB
SHLL2  Rn      0100nnnn00001000      Rn<<2  ﬁ   Rn                         1                                               —
SHLR2  Rn      0100nnnn00001001    Rn>>2  ﬁ   Rn                         1                                               —
SHLL8  Rn      0100nnnn00011000    Rn<<8  ﬁ   Rn                         1                                               —
SHLR8  Rn      0100nnnn00011001    Rn>>8  ﬁ   Rn                         1                                               —
SHLL16 Rn      0100nnnn00101000    Rn<<16  ﬁ   Rn                       1                                               —
SHLR16 Rn      0100nnnn00101001    Rn>>16  ﬁ   Rn                       1                                               —
















                                                                                                                  HITACHI  45
```

## PDF page 50

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=50)

```text
5.1.5    Branch Instructions

Table 5.7   Branch Instructions

                                                                                     Executio
Instruction    Instruction Code    Operation                                         n State    T Bit
B F label 10001011dddddddd    If T = 0, disp · 2 + PC ﬁ   PC;  if T                  3/1*3             —
                                            = 1, nop (where label is disp · 2 +
                                            PC)
BF/S label*2  10001111dddddddd    Delayed branch, if T = 0, disp · 2 +               2/1*3            —
                                            PC ﬁ   PC;  if T = 1, nop
B T label 10001001dddddddd    If T = 1, disp · 2 + PC ﬁ   PC;  if T                  3/1*3            —
                                            = 0, nop (where label is disp + PC)
BT/S label*2  10001101dddddddd    Delayed branch, if T = 1, disp · 2 +               2/1*3            —
                                            PC ﬁ   PC;  if T = 0, nop
BRA  label   1010dddddddddddd    Delayed branch, disp  · 2 + PC ﬁ                    2—
                                            PC
BRAF Rm*2     0000mmmm00100011    Delayed branch, Rm + PC ﬁ   PC        2                   —
BSR  label    1011dddddddddddd    Delayed branch, PC ﬁ   PR, disp ·                  2—
                                            2 + PC ﬁ   PC
BSRF Rm*2     0000mmmm00000011    Delayed branch, PC ﬁ   PR, Rm +                    2—
                                            PC ﬁ   PC
JMP    @Rm   0100mmmm00101011    Delayed branch, Rm ﬁ   PC                 2                   —
JSR    @Rm   0100mmmm00001011    Delayed branch, PC ﬁ   PR, Rm ﬁ                     2—
                                            PC
RTS          0000000000001011    Delayed branch, PR ﬁ   PC                  2                   —
Notes:  2.  SH-2 CPU instruction
        3.  One state when it does not branch
























46 HITACHI
```

## PDF page 51

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=51)

```text
5.1.6     System Control Instructions

Table 5.8   System Control Instructions

                                                                                                         Executio         T
Instruction          Instruction Code    Operation                                                       n State          Bit
CLRT           0000000000001000   0 ﬁ   T10
CLRMAC            0000000000101000    0                                                 ﬁ   MACH, MACL                1              —
LDC   Rm,SR      0100mmmm00001110    Rm ﬁ   SR                                    1                   LSB
LDC    Rm,GBR      0100mmmm00011110     Rm  ﬁ   GBR                                  1                   —
LDC    Rm,VBR      0100mmmm00101110     Rm  ﬁ   VBR                         1              —
LDC.L  @Rm+,SR    0100mmmm00000111   (Rm) ﬁ   SR,  Rm + 4 ﬁ    Rm             3                           L S B
LDC.L  @Rm+,GBR    0100mmmm00010111    (Rm)  ﬁ   GBR,  Rm + 4 ﬁ    Rm         3                           —
LDC.L  @Rm+,VBR    0100mmmm00100111    (Rm)  ﬁ   VBR,  Rm + 4 ﬁ    Rm         3                           —
LDS    Rm,MACH     0100mmmm00001010      Rm  ﬁ   MACH                       1              —
LDS   Rm,MACL    0100mmmm00011010    Rm ﬁ   MACL                           1                —
LDS    Rm,PR      0100mmmm00101010     Rm  ﬁ    PR                                              1                        —
LDS.L  @Rm+,MACH   0100mmmm00000110     (Rm)  ﬁ   MACH,  Rm + 4 ﬁ    Rm     1                           —
LDS.L  @Rm+,MACL   0100mmmm00010110     (Rm)  ﬁ   MACL,  Rm + 4 ﬁ    Rm     1                           —
LDS.L  @Rm+,PR     0100mmmm00100110     (Rm)  ﬁ   PR, Rm + 4 ﬁ    Rm               1                           —
NOP               0000000000001001   No operation                        1              —
RTE              0000000000101011    Delayed branch, stack area ﬁ                                        4                LSB
                                                               PC/SR
SETT               0000000000011000     1  ﬁ   T11
SLEEP              0000000000011011    Sleep                                           3*4                —
STC    SR,Rn       0000nnnn00000010    SR                                                ﬁ    Rn                                               1                       —
STC    GBR,Rn      0000nnnn00010010     GBR ﬁ    Rn                                           1                       —
STC    VBR,Rn      0000nnnn00100010    VBR ﬁ    Rn                                           1                       —
STC.L  SR,@–Rn     0100nnnn00000011     Rn–4  ﬁ   Rn,  SR ﬁ   (Rn)               2                    —
STC.L  GBR,@–Rn    0100nnnn00010011     Rn–4  ﬁ   Rn,  GBR ﬁ   (Rn)            2                    —
STC.L  VBR,@–Rn    0100nnnn00100011     Rn–4  ﬁ   Rn,  VBR ﬁ   (Rn)            2                    —
STS    MACH,Rn     0000nnnn00001010    MACH ﬁ    Rn                                       1                       —
STS    MACL,Rn     0000nnnn00011010    MACL  ﬁ   Rn                                        1                       —
STS    PR,Rn       0000nnnn00101010     PR  ﬁ   Rn                                               1                       —








                                                                                                               HITACHI  47
```

## PDF page 52

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=52)

```text
Table 5.8   System Control Instructions (cont)

                                                                                            Execution       T
Instruction          Instruction Code    Operation                                          State           Bit
STS.L MACH,@–Rn  0100nnnn00000010  Rn–4 ﬁ   Rn,  MACH ﬁ   (Rn)      1                      —
STS.L  MACL,@–Rn   0100nnnn00010010    Rn–4  ﬁ   Rn,  MACL ﬁ   (Rn)      1                      —
STS.L  PR,@–Rn     0100nnnn00100010    Rn–4  ﬁ   Rn,  PR ﬁ   (Rn)            1                      —
TRAPA  #imm        11000011iiiiiiii    PC/SR ﬁ   stack area, (imm ·                         8—
                                                        4 + VBR) ﬁ   PC
Notes:  4.  The number of execution states before the chip enters the sleep state
         5.  The above table lists the minimum execution cycles. In practice, the number of
             execution cycles increases when the instruction fetch is in contention with data
             access or when the destination register of a load instruction (memory ﬁ   register) is
             the same as the register used by the next instruction.


5.2        Instruction Set in Alphabetical Order

Table 5.9 alphabetically lists instruction codes and number of execution cycles for each
instruction.





































48 HITACHI
```

## PDF page 53

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=53)

```text
Table 5.9   Instruction Set

                                                                                     Execu-
                                                                                     tion
Instruction               Instruction Code   Operation                               State    T Bit
ADD   #imm,Rn        0111nnnniiiiiii                        Rn + imm  ﬁ   Rn             1                  —
                                  i
ADD    Rm,Rn           0011nnnnmmmm110                      Rn + Rm  ﬁ   Rn               1                  —
                                  0
ADDC   Rm,Rn           0011nnnnmmmm111                      Rn + Rm + T ﬁ   Rn,      1              Carry
                                  0                         Carry ﬁ   T
ADDV   Rm,Rn           0011nnnnmmmm111                      Rn + Rm  ﬁ   Rn,         1            Overflow
                                  1                         Overflow  ﬁ   T
AND    #imm,R0         11001001iiiiiii                      R0 & imm ﬁ    R0             1                  —
                                  i
AND    Rm,Rn           0010nnnnmmmm100                      Rn & Rm ﬁ    Rn              1                  —
                                  1
AND.B  #imm,@(R0,GBR)  11001101iiiiiii                      (R0 + GBR) & imm         3—
                                  i                         ﬁ   (R0 + GBR)
B F label  10001011ddddddd                                  If T = 0, disp · 2 +     3/1*3       —
                                  d                         PC ﬁ   PC;  if T = 1,
                                                            nop
BF/S   label*2         10001111ddddddd                      If T = 0, disp  · 2+     2/1*3       —
                                  d                         PC ﬁ   PC;  if T = 1,
                                                            nop






























                                                                                                HITACHI  49
```

## PDF page 54

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=54)

```text
Table 5.9   Instruction Set (cont)

                                                                               Execu-
                                                                               tion
Instruction         Instruction Code     Operation                             State    T Bit
BRA   label     1010dddddddddddd     Delayed branch, disp                      2—
                                                    · 2 + PC ﬁ   PC
BRAF   Rm*2       0000mmmm00100011      Delayed branch, Rm +                   2—
                                                    PC ﬁ   PC
BSR    label      1011dddddddddddd     Delayed branch, PC ﬁ                    2—
                                                    PR, disp · 2 + PC ﬁ
                                                    PC
BSRF   Rm*2       0000mmmm00000011     Delayed branch, PC ﬁ                    2—
                                                    PR, Rm + PC ﬁ   PC
BT     label      10001001dddddddd     If T = 1, disp  · 2+ PC                 3/1*3        —
                                                    ﬁ   PC;  if T = 0, nop
BT/S   label*2    10001101dddddddd     If T = 1, disp · 2 + PC                 2/1*3        —
                                                    ﬁ   PC;  if T = 0, nop
CLRMAC           0000000000101000      0  ﬁ   MACH, MACL       1           —
CLRT            0000000000001000     0 ﬁ   T10
CMP/EQ #imm,R0    10001000iiiiiiii      If R0 = imm, 1 ﬁ   T          1               Comparison
                                                                                          result
CMP/EQ Rm,Rn      0011nnnnmmmm0000      If Rn = Rm, 1 ﬁ   T            1               Comparison
                                                                                          result
CMP/GE Rm,Rn      0011nnnnmmmm0011     If Rn ‡ Rm with signed                  1              Comparison
                                                    data, 1 ﬁ   T                         result
CMP/GT Rm,Rn      0011nnnnmmmm0111     If Rn > Rm with signed                  1              Comparison
                                                    data, 1 ﬁ   T                         result
CMP/HI Rm,Rn      0011nnnnmmmm0110     If Rn > Rm with                         1              Comparison
                                                    unsigned data,                        result
                                                    1 ﬁ   T
CMP/HS Rm,Rn      0011nnnnmmmm0010     If Rn ‡ Rm with                         1              Comparison
                                                    unsigned data,                        result
                                                    1 ﬁ   T
CMP/PL Rn         0100nnnn00010101      If Rn>0, 1 ﬁ   T                 1               Comparison
                                                                                          result
CMP/PZ Rn         0100nnnn00010001      If Rn ‡ 0, 1 ﬁ   T               1               Comparison
                                                                                          result
Notes:  2.  SH-2 CPU instructions
        3.  One state when it does not branch






50 HITACHI
```

## PDF page 55

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=55)

```text
Table 5.9   Instruction Set (cont)

                                                                                 Execu-
                                                                                 tion
Instruction           Instruction Code       Operation                           State    T Bit
CMP/STR                    0010nnnnmmmm1100           If Rn and Rm have          1              Comparison
         Rm,Rn                                           an equivalent byte,               result
                                                         1  ﬁ   T
DIV0S  Rm,Rn        0010nnnnmmmm0111          MSB of Rn ﬁ   Q,                   1              Calculation
                                                         MSB of Rm ﬁ   M,                  result
                                                         M ^ Q ﬁ   T
DIV0U              0000000000011001             0  ﬁ  M/Q/T            1          0
DIV1  Rm,Rn      0011nnnnmmmm0100       Single-step                              1              Calculation
                                                         division (Rn/Rm)                  result
DMULS.L                                                                          2 to 4*1   —
         Rm,Rn*2      0011nnnnmmmm1101       Signed operationof Rn x Rm ﬁ
                                                         MACH, MACL
DMULU.L                                                                          2 to 4*1   —
         Rm,Rn*2     0011nnnnmmmm0101         Unsignedoperation of Rn x
                                                         Rm  ﬁ   MACH,
                                                         MACL
DT     Rn*2        0100nnnn00010000           Rn - 1  ﬁ   Rn, when               1              Comparison
                                                         Rn is 0, 1 ﬁ   T.                 result
                                                         When Rn is
                                                         nonzero, 0 ﬁ   T
EXTS.B Rm,Rn        0110nnnnmmmm1110          A byte in Rm is                    1—
                                                         sign-extended  ﬁ
                                                         Rn
EXTS.W Rm,Rn        0110nnnnmmmm1111           A word in Rm is                   1—
                                                         sign-extended  ﬁ
                                                         Rn
EXTU.B Rm,Rn        0110nnnnmmmm1100          A byte in Rm is                    1—
                                                         zero-extended  ﬁ
                                                         Rn
EXTU.W Rm,Rn       0110nnnnmmmm1101           A word in Rm is                    1—
                                                         zero-extended  ﬁ
                                                         Rn
JMP    @Rm         0100mmmm00101011          Delayed branch,                     2—
                                                         Rm  ﬁ   PC
Notes:  1.   The normal minimum number of execution states
        2.  SH-2 CPU instructions







                                                                                           HITACHI  51
```

## PDF page 56

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=56)

```text
Table 5.9   Instruction Set (cont)

                                                                                  Execu-
                                                                                  tion
Instruction             Instruction Code     Operation                            State    T Bit
JSR   @Rm         0100mmmm00001011      Delayed branch,                           2—
                                                          PC ﬁ   PR, Rm ﬁ
                                                          PC
LDC    Rm,GBR        0100mmmm00011110         Rm  ﬁ   GBR                1              —
LDC    Rm,SR         0100mmmm00001110          Rm  ﬁ   SR                   1              LSB
LDC    Rm,VBR        0100mmmm00101110         Rm  ﬁ   VBR            1          —
LDC.L  @Rm+,GBR     0100mmmm00010111      (Rm) ﬁ   GBR,  Rm                       3—
                                                          + 4 ﬁ    Rm
LDC.L  @Rm+,SR       0100mmmm00000111        (Rm)  ﬁ   SR,  Rm +                  3            LSB
                                                          4  ﬁ    Rm
LDC.L  @Rm+,VBR      0100mmmm00100111        (Rm)  ﬁ   VBR,  Rm                   3—
                                                          + 4 ﬁ    Rm
LDS    Rm,MACH       0100mmmm00001010          Rm  ﬁ   MACH          1           —
LDS   Rm,MACL      0100mmmm00011010       Rm ﬁ   MACL            1            —
LDS    Rm,PR         0100mmmm00101010        Rm  ﬁ    PR                         1                  —
LDS.L  @Rm+,MACH     0100mmmm00000110        (Rm)  ﬁ   MACH,                      1—
                                                          Rm + 4  ﬁ    Rm
LDS.L  @Rm+,MACL     0100mmmm00010110        (Rm)  ﬁ   MACL,  Rm                  1—
                                                          + 4 ﬁ    Rm
LDS.L  @Rm+,PR       0100mmmm00100110        (Rm)  ﬁ   PR, Rm + 4                 1—
                                                          ﬁ    Rm
MAC.L  @Rm+,@Rn+*2   0000nnnnmmmm1111    Signed operation                         3/(2 to
                                                          of (Rn) · (Rm) +        4)* 1         —
                                                          MAC  ﬁ   MAC
MAC.W  @Rm+,@Rn+     0100nnnnmmmm1111    Signed operation                         3/(2)*1     —
                                                          of (Rn) · (Rm) +
                                                          MAC  ﬁ   MAC
MOV    #imm,Rn       1110nnnniiiiiiii       imm ﬁ   Sign                          1—
                                                          extension ﬁ   Rn
MOV    Rm,Rn         0110nnnnmmmm0011          Rm  ﬁ    Rn                         1                  —
Notes:  1.   The normal minimum number of execution states  (the number in parentheses is the
            number of states when there is contention with preceding/following instructions)
        2.  SH-2 instructions








52 HITACHI
```

## PDF page 57

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=57)

```text
Table 5.9   Instruction Set (cont)

                                                                                        Execu-
                                                                                        tion
Instruction               Instruction Code    Operation                                 State     T Bit
MOV.B  @(disp,GBR),R0  11000100dddddddd    (disp + GBR) ﬁ   Sign                        1—
                                                           extension ﬁ   R0
MOV.B  @(disp,Rm),R0   10000100mmmmdddd     (disp + Rm) ﬁ   Sign                        1—
                                                           extension ﬁ   R0
MOV.B  @(R0,Rm),Rn     0000nnnnmmmm1100     (R0 + Rm)  ﬁ   Sign                         1—
                                                           extension ﬁ   Rn
MOV.B  @Rm+,Rn         0110nnnnmmmm0100      (Rm)  ﬁ  Sign extension                    1—
                                                           ﬁ   Rn, Rm + 1 ﬁ    Rm
MOV.B  @Rm,Rn          0110nnnnmmmm0000     (Rm)  ﬁ  Sign extension                     1—
                                                           ﬁ   Rn
MOV.B  R0,@(disp,GBR)  11000000dddddddd      R0  ﬁ   (disp + GBR)           1                —
MOV.B  R0,@(disp,Rn)   10000000nnnndddd      R0  ﬁ   (disp + Rn)              1                —
MOV.B  Rm,@(R0,Rn)     0000nnnnmmmm0100      Rm  ﬁ   (R0 + Rn)                 1                 —
MOV.B  Rm,@–Rn         0010nnnnmmmm0100     Rn–1  ﬁ   Rn,  Rm  ﬁ                        1—
                                                           (Rn)
MOV.B  Rm,@Rn          0010nnnnmmmm0000       Rm  ﬁ   (Rn)                           1                  —
MOV.L  @(disp,GBR),R0  11000110dddddddd     (disp · 4 + GBR) ﬁ   R0    1                —
MOV.L  @(disp,PC),Rn   1101nnnndddddddd     (disp · 4 + PC) ﬁ   Rn         1                    —
MOV.L  @(disp,Rm),Rn   0101nnnnmmmmdddd     (disp · 4 + Rm) ﬁ   Rn         1                    —
MOV.L  @(R0,Rm),Rn     0000nnnnmmmm1110     (R0 + Rm)  ﬁ   Rn1—
MOV.L  @Rm+,Rn         0110nnnnmmmm0110     (Rm)                    ﬁ   Rn, Rm + 4 ﬁ    1—
                                                           Rm
MOV.L  @Rm,Rn          0110nnnnmmmm0010     (Rm)  ﬁ   Rn                              1                    —
MOV.L  R0,@(disp,GBR)  11000010dddddddd      R0  ﬁ   (disp · 4 + GBR)    1                —
MOV.L  Rm,@(disp,Rn)   0001nnnnmmmmdddd       Rm  ﬁ   (disp · 4 + Rn)       1                —
MOV.L  Rm,@(R0,Rn)     0000nnnnmmmm0110      Rm  ﬁ   (R0 + Rn)                 1                 —
MOV.L  Rm,@–Rn         0010nnnnmmmm0110     Rn–4  ﬁ   Rn,  Rm  ﬁ                        1—
                                                           (Rn)
MOV.L  Rm,@Rn          0010nnnnmmmm0010       Rm  ﬁ   (Rn)                           1                  —
MOV.W  @(disp,GBR),R0  11000101dddddddd     (disp · 2 + GBR) ﬁ                          1—
                                                           Sign extension ﬁ
                                                           R0










                                                                                             HITACHI  53
```

## PDF page 58

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=58)

```text
Table 5.9   Instruction Set (cont)

                                                                                    Execu-
                                                                                    tion
Instruction           Instruction Code     Operation                                State     T Bit
MOV.W                       1001nnnndddddddd    (disp · 2 + PC) ﬁ                   1—
     @(disp,PC),Rn                                      Sign extension ﬁ   Rn
MOV.W                       10000101mmmmdddd     (disp · 2 + Rm) ﬁ                  1—
     @(disp,Rm),R0                                      Sign extension ﬁ    R0
MOV.W  @(R0,Rm),Rn  0000nnnnmmmm1101      (R0 + Rm)  ﬁ   Sign                       1—
                                                        extension ﬁ   Rn
MOV.W  @Rm+,Rn      0110nnnnmmmm0101      (Rm)  ﬁ   Sign                            1—
                                                        extension ﬁ   Rn, Rm +
                                                        2  ﬁ    Rm
MOV.W  @Rm,Rn       0110nnnnmmmm0001      (Rm)  ﬁ   Sign                            1—
                                                        extension  ﬁ   Rn
MOV.W                       11000001dddddddd       R0  ﬁ   (disp · 2+ GBR)   1               —
     R0,@(disp,GBR)
MOV.W                       10000001nnnndddd      R0  ﬁ   (disp  · 2 + Rn)    1               —
     R0,@(disp,Rn)
MOV.W  Rm,@(R0,Rn)  0000nnnnmmmm0101       Rm  ﬁ   (R0 + Rn)               1                  —
MOV.W  Rm,@–Rn      0010nnnnmmmm0101      Rn–2  ﬁ   Rn,  Rm  ﬁ                      1—
                                                        (Rn)
MOV.W  Rm,@Rn       0010nnnnmmmm0001        Rm  ﬁ   (Rn)                         1                  —
MOVA @(disp,PC),R0  11000111dddddddd     disp · 4 + PC ﬁ   R0          1                     —
MOVT   Rn           0000nnnn00101001      T ﬁ   Rn                                  1                     —
MUL.L  Rm,Rn*2      0000nnnnmmmm0111      Rn · Rm  ﬁ   MACL         2 to 4*1     —
MULS.W Rm,Rn        0010nnnnmmmm1111    Signed operation of                         1 to 3*1     —
                                                        Rn · Rm ﬁ   MAC
MULU.W Rm,Rn        0010nnnnmmmm1110    Unsigned operation of                       1 to 3*1     —
                                                        Rn · Rm ﬁ   MAC
NEG   Rm,Rn        0110nnnnmmmm1011      0–Rm ﬁ   Rn                          1                     —
NEGC   Rm,Rn        0110nnnnmmmm1010      0–Rm–T  ﬁ   Rn,                           1               Borrow
                                                        Borrow ﬁ   T
NOP                0000000000001001    No operation              1            —
NOT   Rm,Rn       0110nnnnmmmm0111     ~Rm ﬁ    Rn                             1                     —
OR     #imm,R0      11001011iiiiiiii      R0 | imm ﬁ    R01—
OR     Rm,Rn        0010nnnnmmmm1011      Rn | Rm ﬁ    Rn                      1                     —
Notes:  1.   The normal minimum number of execution states
        2.  SH-2 CPU instructions





54 HITACHI
```

## PDF page 59

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=59)

```text
Table 5.9   Instruction Set (cont)

                                                                                                  Execu-
                                                                                                  tion
Instruction                 Instruction Code    Operation                                         State    T Bit
OR.B  #imm,@(R0,GBR)   11001111iiiiiiii     (R0 + GBR) | imm ﬁ                                    3—
                                                                      (R0 + GBR)
ROTCL  Rn                0100nnnn00100100     T ‹   Rn  ‹   T            1           MSB
ROTCR  Rn                0100nnnn00100101     T ﬁ   Rn ﬁ   T                1              LSB
ROTL   Rn                 0100nnnn00000100      T  ‹   Rn  ‹    MSB        1            MSB
ROTR   Rn                 0100nnnn00000101      LSB  ﬁ   Rn ﬁ   T           1              LSB
RTE                       0000000000101011    Delayed branch,                                     4            LSB
                                                                      stack area ﬁ   PC/SR
RTS                     0000000000001011    Delayed branch, PR                                    2—
                                                                      ﬁ   PC
SETT                    0000000000011000      1  ﬁ   T11
SHAL   Rn                0100nnnn00100000     T                               ‹                                Rn                               ‹                                0                1              MSB
SHAR   Rn                0100nnnn00100001     MSB                                ﬁ   Rn ﬁ   T          1              LSB
SHLL   Rn                 0100nnnn00000000      T  ‹   Rn  ‹    0              1            MSB
SHLL2  Rn                 0100nnnn00001000       Rn<<2  ﬁ   Rn                     1                  —
SHLL8  Rn                0100nnnn00011000     Rn<<8  ﬁ   Rn                     1                  —
SHLL16 Rn                0100nnnn00101000     Rn<<16  ﬁ   Rn                  1                  —
SHLR   Rn                0100nnnn00000001      0  ﬁ   Rn ﬁ   T                 1              LSB
SHLR2  Rn                 0100nnnn00001001       Rn>>2  ﬁ   Rn                     1                  —
SHLR8  Rn                0100nnnn00011001     Rn>>8  ﬁ   Rn                     1                  —
SHLR16 Rn                0100nnnn00101001     Rn>>16  ﬁ   Rn                  1                  —
SLEEP                    0000000000011011    Sleep                         3             —
STC    GBR,Rn            0000nnnn00010010     GBR ﬁ    Rn                        1                  —
STC    SR,Rn             0000nnnn00000010     SR ﬁ    Rn                            1                  —
STC    VBR,Rn            0000nnnn00100010     VBR ﬁ    Rn                        1                  —
STC.L  GBR,@–Rn          0100nnnn00010011     Rn–4  ﬁ   Rn,  GBR                                  2—
                                                                      ﬁ   (Rn)
STC.L  SR,@–Rn           0100nnnn00000011     Rn–4  ﬁ   Rn,  SR ﬁ                                 2—
                                                                      (Rn)
STC.L  VBR,@–Rn          0100nnnn00100011     Rn–4  ﬁ   Rn,  VBR                                  2—
                                                                      ﬁ   (Rn)
STS    MACH,Rn           0000nnnn00001010     MACH ﬁ    Rn                    1                  —





                                                                                                         HITACHI  55
```

## PDF page 60

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=60)

```text
Table 5.9   Instruction Set (cont)

                                                                                          Execu
                                                                                          -tion
Instruction               Instruction Code        Operation                               State   T Bit
STS   MACL,Rn        0000nnnn00011010        MACL ﬁ   Rn                  1                 —
STS    PR,Rn           0000nnnn00101010               PR  ﬁ   Rn                         1                 —
STS.L  MACH,@–Rn       0100nnnn00000010             Rn–4  ﬁ   Rn,                         1—
                                                                 MACH ﬁ   (Rn)
STS.L  MACL,@–Rn       0100nnnn00010010             Rn–4  ﬁ   Rn,                         1—
                                                                 MACL  ﬁ   (Rn)
STS.L  PR,@–Rn         0100nnnn00100010             Rn–4  ﬁ   Rn,  PR ﬁ                   1—
                                                                 (Rn)
SUB    Rm,Rn           0011nnnnmmmm1000              Rn–Rm  ﬁ   Rn1—
SUBC   Rm,Rn           0011nnnnmmmm1010            Rn–Rm–T                ﬁ   Rn,         1            Borrow
                                                                 Borrow ﬁ   T
SUBV   Rm,Rn           0011nnnnmmmm1011              Rn–Rm  ﬁ   Rn,                       1             Under-
                                                                 Underflow ﬁ   T                    flow
SWAP.B Rm,Rn           0110nnnnmmmm1000                 Rm  ﬁ   Swap                      1—
                                                                 upper and lower 2
                                                                 bytesﬁ    Rn
SWAP.W  Rm,Rn           0110nnnnmmmm1001                 Rm  ﬁ   Swap upper               1—
                                                                 and lower wordﬁ
                                                                 Rn
TAS.B  @Rn            0100nnnn00011011            If (Rn) is 0, 1 ﬁ   T;                  4          Test
                                                                 1 ﬁ   MSB of (Rn)                  result
TRAPA  #imm           11000011iiiiiiii          PC/SR ﬁ   stack                           8—
                                                                 area, (imm  · 4 +
                                                                 VBR) ﬁ   PC
TST    #imm,R0         11001000iiiiiiii            R0 & imm; if the                       1            Test
                                                                 result is 0, 1 ﬁ   T               result
TST    Rm,Rn           0010nnnnmmmm1000            Rn & Rm; if the                        1            Test
                                                                 result is 0, 1 ﬁ   T               result
TST.B  #imm,@(R0,GBR)     11001100iiiiiiii            (R0 + GBR) & imm;                   3            Test
                                                                 if the result is 0, 1              result
                                                                 ﬁ   T
XOR    #imm,R0         11001010iiiiiiii             R0 ^ imm ﬁ    R0             1                 —
XOR    Rm,Rn           0010nnnnmmmm1010             Rn ^ Rm ﬁ    Rn               1                 —
XOR.B  #imm,@(R0,GBR)     11001110iiiiiiii            (R0 + GBR) ^ imm                    3—
                                                                 ﬁ   (R0 + GBR)
XTRCT  Rm,Rn           0010nnnnmmmm1101         Center 32 bits of                         1—
                                                                 Rm and Rn  ﬁ   Rn



56 HITACHI
```
