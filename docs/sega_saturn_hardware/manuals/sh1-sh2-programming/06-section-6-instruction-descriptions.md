# Section 6 Instruction Descriptions

SH-1/SH-2 Programming Manual — Hitachi, September 3, 1996.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=61)

Physical PDF pages 61–171. Text extraction; consult the PDF for figures.

## PDF page 61

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=61)

```text
                        Section 6   Instruction Descriptions


This section describes instructions in alphabetical order using the format shown below in
section 6.1. The actual descriptions begin at section 6.2.

6.1        Sample Description (Name): Classification

Class: Indicates if the instruction is a delayed branch instruction or interrupt disabled
instruction

Format                     Abstract               Code            State        T Bit
Assembler input              A brief description of     Displayed in          Number of       The value of
format; imm and disp         operation                  order MSB to          states when     T bit after
are numbers,                                            LSB                   there is no     the
expressions, or                                                               wait state      instruction is
symbols                                                                                       executed


Description: Description of operation

Notes: Notes on using the instruction

Operation: Operation written in C language. This part is just a reference to help
understanding of an operation. The following resources should be used.

•   Reads data of each length from address Addr. An address error will occur if word data is
   read from an address other than 2n or if longword data is read from an address other than
   4n:
  unsigned char  Read_Byte(unsigned long Addr);
  unsigned short Read_Word(unsigned long Addr);
  unsigned longRead_Long(unsigned long Addr);

•   Writes data of each length to address Addr. An address error will occur if word data is
   written to an address other than 2n or if longword data is written to an address other than
   4n:
  unsigned char  Write_Byte(unsigned long Addr, unsigned long Data);
  unsigned short Write_Word(unsigned long Addr, unsigned long Data);
  unsigned longWrite_Long(unsigned long Addr, unsigned long Data);

•   Starts execution from the slot instruction located at an address (Addr – 4). For Delay_Slot
   (4);, execution starts from an instruction at address 0 rather than address 4. The following
   instructions are detected before execution as illegal slot instruction (they become illegal
   slot instructions when used as delay slot instructions):
   BF, BT, BRA, BSR, JMP, JSR, RTS, RTE, TRAPA, BF/S, BT/S, BRAF, BSRF



                                                                                                HITACHI  57
```

## PDF page 62

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=62)

```text
  Delay_Slot(unsigned long Addr);

•   List registers:
  unsigned long R[16];
  unsigned long SR,GBR,VBR;
  unsigned long MACH,MACL,PR;
  unsigned long PC;

•   Definition of SR structures:
  struct SR0 {
       unsigned long dummy0:22;
       unsigned longM0:1;
       unsigned longQ0:1;
       unsigned longI0:4;
       unsigned long dummy1:2;
       unsigned longS0:1;
       unsigned longT0:1;
  };

•   Definition of bits in SR:
  #define M ((*(struct SR0 *)(&SR)).M0)
  #define Q ((*(struct SR0 *)(&SR)).Q0)
  #define S ((*(struct SR0 *)(&SR)).S0)
  #define T ((*(struct SR0 *)(&SR)).T0)

•   Error display function:
  Error( char *er );

The PC should point to the location four bytes (the second instruction) after the current
instruction. Therefore, PC = 4; means the instruction starts execution from address 0, not
address 4.

Examples: Examples are written in assembler mnemonics and describe state before and after
executing the instruction. Characters in italics such as .align are assembler control
instructions (listed below). For more information, see the Cross Assembler User's Manual.













58 HITACHI
```

## PDF page 63

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=63)

```text
  .org              Location counter set
  .data.w          Securing integer word data
  .data.l          Securing integer longword data
  .sdata           Securing string data
  .align 2        2-byte boundary alignment
  .align 4        2-byte boundary alignment
  .arepeat 16   16-repeat expansion
  .arepeat 32   32-repeat expansion
  .aendr            End of repeat expansion of specified number

   Note:   The SH-series cross assembler version 1.0 does not support the conditional
            assembler functions.

Notes: 1.  In the assembler descriptions in this manual for addressing modes that involve the
             following displacements (disp), the value prior to scaling (x1, x2, x4) according to
             the operand size is written. This is done to show clearly the operation of the LSI;
             see the assembler notation rules for the actual assembler descriptions.
                     @(disp:4, Rn):  Register indirect with displacement
                     @(disp:8, GBR):         GBR indirect with displacement
                     @(disp 8, PC):  PC relative with displacement
                     disp:8, disp:12:  PC relative
         2.  Among the 16 bits of the instruction code, a code not assigned as an instruction is
             treated as a general illegal instruction, and will result in illegal instruction
             exception processing, This includes the case where an instruction code for the SH-
             2 CPU only is executed on the SH-1 CPU.
             Example 1: H'FFF      [General illegal instruction in both SH-1 and SH-2 CPU]
             Example 2: H'3105      (=DMUL.L R0, R1)[Illegal instruction in SH-1 CPU]
         3.  If the instruction following a delayed branch instruction such as BRA, BT/S, etc.,
             is a general illegal instruction or a branch instruction (known as a slot illegal
             instruction), illegal instruction exception processing will be performed.
             Example 1      ....
                     BRA Label
                     . data. W H'FFFF  ‹   Slot illegal instruction
                                          ....             [H'FFF is fundamentally a general illegal
                                                             instruction]
             Example 2   RTE
                               BT/S Label ‹  Slot illegal instruction








                                                                                                    HITACHI  59
```

## PDF page 64

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=64)

```text
6.2        ADD (ADD Binary): Arithmetic Instruction

Format                     Abstract                            Code                   State   T Bit
ADD   Rm,Rn             Rm + Rn  ﬁ   Rn                     0011nnnnmmmm1100           1         —
ADD   #imm,Rn           Rn + imm  ﬁ   Rn                    0111nnnniiiiiiii           1         —


Description: Adds general register Rn data to Rm data, and stores the result in Rn. The
contents of Rn can also be added to 8-bit immediate data. Since the 8-bit immediate data is
sign-extended to 32 bits, this instruction can add and subtract immediate data.

Operation:

  ADD(long m,long n)  /* ADD Rm,Rn */
  {
       R[n]+=R[m];
       PC+=2;
  }

  ADDI(long i,long n) /* ADD #imm,Rn */
  {
       if ((i&0x80)==0) R[n]+=(0x000000FF & (long)i);
       else R[n]+=(0xFFFFFF00 | (long)i);
       PC+=2;
  }

Examples:

  ADD    R0,R1        Before execution   R0 = H'7FFFFFFF, R1 = H'00000001
                          After execution      R1 = H'80000000

  ADD   #H'01,R2    Before execution    R2 = H'00000000
                          After execution      R2 = H'00000001

  ADD   #H'FE,R3    Before execution    R3 = H'00000001
                          After execution     R3 = H'FFFFFFFF













60 HITACHI
```

## PDF page 65

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=65)

```text
6.3        ADDC (ADD with Carry): Arithmetic Instruction

Format                    Abstract                            Code                   State   T Bit
ADDC  Rm,Rn            Rn + Rm + T ﬁ   Rn, carry ﬁ   T   0011nnnnmmmm1110       1            Carry


Description: Adds general register Rm data and the T bit to Rn data, and stores the result in
Rn. The T bit changes according to the result. This instruction can add data that has more
than 32 bits.

Operation:

  ADDC (long m,long n)    /* ADDC Rm,Rn */
  {
       unsigned long tmp0,tmp1;


       tmp1=R[n]+R[m];
       tmp0=R[n];
       R[n]=tmp1+T;
       if (tmp0>tmp1) T=1;
       else T=0;
       if (tmp1>R[n]) T=1;
       PC+=2;
  }

Examples:

  CLRT                    R0:R1 (64 bits) + R2:R3 (64 bits) =  R0:R1 (64 bits)
  ADDC   R3,R1        Before execution       T = 0, R1 = H'00000001, R3 = H'FFFFFFFF
                           After execution          T = 1, R1 = H'0000000
  ADDC   R2,R0         Before execution       T = 1, R0 = H'00000000, R2 = H'00000000
                           After execution          T = 0, R0 = H'00000001

















                                                                                                HITACHI  61
```

## PDF page 66

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=66)

```text
6.4        ADDV (ADD with V Flag Overflow Check): Arithmetic Instruction

Format                 Abstract                           Code                   State   T Bit
ADDV  Rm,Rn         Rn + Rm  ﬁ   Rn, overflow ﬁ   T   0011nnnnmmmm1111      1          Overflo
                                                                                              w


Description: Adds general register Rn data to Rm data, and stores the result in Rn. If an
overflow occurs, the T bit is set to 1.

Operation:

  ADDV(long m,long n)    /*ADDV Rm,Rn */
  {
       long dest,src,ans;


       if ((long)R[n]>=0) dest=0;
       else dest=1;
       if ((long)R[m]>=0) src=0;
       else src=1;
       src+=dest;
       R[n]+=R[m];
       if ((long)R[n]>=0) ans=0;
       else ans=1;
       ans+=dest;
       if (src==0 || src==2) {
           if (ans==1) T=1;
           else T=0;
       }
       else T=0;
       PC+=2;
  }

Examples:

  ADDV   R0,R1       Before execution       R0 = H'00000001, R1 = H'7FFFFFFE, T = 0
                         After execution         R1 = H'7FFFFFFF, T = 0
  ADDV  R0,R1       Before execution       R0 = H'00000002, R1 = H'7FFFFFFE, T = 0
                         After execution          R1 = H'80000000, T = 1






62 HITACHI
```

## PDF page 67

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=67)

```text
6.5        AND (AND Logical): Logic Operation Instruction

Format                             Abstract                        Code                       State    T
                                                                                                 Bit
AND    Rm,Rn                 Rn & Rm ﬁ    Rn                 0010nnnnmmmm1001           1        —
AND    #imm,R0               R0 & imm ﬁ    R0                11001001iiiiiiii           1        —
AND.B                        (R0 + GBR) & imm ﬁ   (R0 +      11001101iiiiiiii           3        —
#imm,@(R0,GBR)               GBR)


Description: Logically ANDs the contents of general registers Rn and Rm, and stores the
result in Rn. The contents of general register R0 can be ANDed with zero-extended 8-bit
immediate data. 8-bit memory data pointed to by GBR relative addressing can be ANDed
with 8-bit immediate data.

Note: After AND #imm, R0 is executed and the upper 24 bits of R0 are always cleared to 0.

Operation:

  AND(long m,long n)  /* AND Rm,Rn */
  {
       R[n]&=R[m]
       PC+=2;
  }

  ANDI(long i) /* AND #imm,R0 */
  {
       R[0]&=(0x000000FF & (long)i);
       PC+=2;
  }

  ANDM(long i) /* AND.B #imm,@(R0,GBR) */
  {
       long temp;


       temp=(long)Read_Byte(GBR+R[0]);
       temp&=(0x000000FF & (long)i);
       Write_Byte(GBR+R[0],temp);
       PC+=2;
  }






                                                                                           HITACHI  63
```

## PDF page 68

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=68)

```text
Examples:

  ANDR0,R1                   Before execution    R0 = H'AAAAAAAA, R1 = H'55555555
                                  After execution        R1 = H'00000000
  AND#H'0F,R0              Before execution    R0 = H'FFFFFFFF
                                  After execution        R0 = H'0000000F
  AND.B  #H'80,@(R0,GBR) Before execution     @(R0,GBR) = H'A5
                                  After execution       @(R0,GBR) = H'80





















































64 HITACHI
```

## PDF page 69

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=69)

```text
6.6        BF (Branch if False): Branch Instruction

Format         Abstract                                       Code                  State   T Bit
B F             When T = 0, disp · 2 + PC ﬁ   PC;                 10001011dddddddd     3/1         —
          la    When T = 1, nop
bel


Description: Reads the T bit, and conditionally branches. If T = 1, BF executes the next
instruction. If T = 0, it branches. The branch destination is an address specified by PC +
displacement. The PC points to the starting address of the second instruction after the branch
instruction. The 8-bit displacement is sign-extended and doubled. Consequently, the relative
interval from the branch destination is –256 to +254 bytes. If the displacement is too short to
reach the branch destination, use BF with the BRA instruction or the like.

Note: When branching, three cycles; when not branching, one cycle.

Operation:

  BF(long d)/* BF disp */
  {
       long disp;


       if ((d&0x80)==0) disp=(0x000000FF & (long)d);
       else disp=(0xFFFFFF00 | (long)d);
       if (T==0) PC=PC+(disp<<1)+4;
       else PC+=2;
  }

Example:

                CLRT                      T is always cleared to 0
                B T TRGET_T    Does not branch, because T = 0
                B F TRGET_F    Branches to TRGET_F, because T = 0
                NOP
                NOP                             ‹   The PC location is used to calculate the
                                      branch destination address of the BF
                                      instruction
     TRGET_F:                              ‹  Branch destination of the BF instruction









                                                                                                   HITACHI  65
```

## PDF page 70

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=70)

```text
6.7        BF/S (Branch if False with Delay Slot): Branch Instruction (SH-2
           CPU)

Format         Abstract                                       Code                  State   T Bit
BF/S            When T = 0, disp · 2 + PC ﬁ   PC;                10001111dddddddd     2/1         —
label           When T = 1, nop


Description: Reads the T bit, and conditionally branches with delay slot. If T = 1, BF
executes the next instruction. If T = 0, it branches after executing the next instruction. The
branch destination is an address specified by PC + displacement. The PC points to the
starting address of the second instruction after the branch instruction. The 8-bit displacement
is sign-extended and doubled. Consequently, the relative interval from the branch destination
is –256 to +254 bytes. If the displacement is too short to reach the branch destination, use
BF/S with the BRA instruction or the like.

Note: Since this is a delayed branch instruction, the instruction immediately after is executed
before the branch. Between the time this instruction and the instruction immediately after are
executed, address errors or interrupts are not accepted. When the instruction immediately
after is a branch instruction, it is recognized as an illegal slot instruction.

When branching, this is a two-cycle instruction; when not branching, one cycle.

Operation:

  BFS(long d)  /* BFS disp */
  {
       long disp;
       unsigned long temp;


       temp=PC;
       if ((d&0x80)==0) disp=(0x000000FF & (long)d);
       else disp=(0xFFFFFF00 | (long)d);
       if (T==0) {
            PC=PC+(disp<<1)+4;
            Delay_Slot(temp+2);
       }
       else PC+=2;
  }







66 HITACHI
```

## PDF page 71

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=71)

```text
Example:

                CLRT                  T is always 0
                BT/S TRGET_T     Does not branch, because T = 0
                NOP
                BF/S TRGET_F     Branches to TRGET, because T = 0
                ADD  R0,R1        Executed before branch
                NOP                     ‹  The PC location is used to calculate the branch destination
                                       address of the BF/S instruction
        TRGET_F:                     ‹  Branch destination of the BF/S instruction

Note:   With delayed branching, branching occurs after execution of the slot instruction.
         However, instructions such as register changes etc. are executed in the order of
         delayed branch instruction, then delay slot instruction. For example, even if the
         register in which the branch destination address has been loaded is changed by the
         delay slot instruction, the branch will still be made using the value of the register
         prior to the change as the branch destination address.








































                                                                                                     HITACHI  67
```

## PDF page 72

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=72)

```text
6.8        BRA (Branch): Branch Instruction

Format                    Abstract                            Code                   State   T Bit
BRA   label        disp · 2  + PC ﬁ   PC         1010dddddddddddd 2—


Description: Branches unconditionally after executing the instruction following this BRA
instruction. The branch destination is an address specified by PC + displacement. The PC
points to the starting address of the second instruction after this BRA instruction. The 12-bit
displacement is sign-extended and doubled. Consequently, the relative interval from the
branch destination is –4096 to +4094 bytes. If the displacement is too short to reach the
branch destination, this instruction must be changed to the JMP instruction. Here, a MOV
instruction must be used to transfer the destination address to a register.

Note: Since this is a delayed branch instruction, the instruction after BRA is executed before
branching. No interrupts or address errors are accepted between this instruction and the next
instruction. If the next instruction is a branch instruction, it is acknowledged as an illegal slot
instruction.

Operation:

  BRA(long d)  /* BRA disp */
  {
       unsigned long temp;


       long disp;
       if ((d&0x800)==0) disp=(0x00000FFF & d);
       else disp=(0xFFFFF000 | d);
       temp=PC;
       PC=PC+(disp<<1)+4;
       Delay_Slot(temp+2);
  }

Example:

            BRA    TRGET   Branches to TRGET
            ADD   R0,R1   Executes ADD before branching
            NOP                    ‹  The PC location is used to calculate the branch destination
                                 address of the BRA instruction
     TRGET:                         ‹   Branch destination of the BRA instruction

       Note: With delayed branching, branching occurs after execution of the slot instruction.
       However, instructions such as register changes etc. are executed in the order of



68 HITACHI
```

## PDF page 73

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=73)

```text
delayed branch instruction, then delay slot instruction. For example, even if the
register in which the branch destination address has been loaded is changed by the
delay slot instruction, the branch will still be made using the value of the register prior
to the change as the branch destination address.



























































                                                                                              HITACHI  69
```

## PDF page 74

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=74)

```text
6.9       BRAF (Branch Far): Branch Instruction (SH-2 CPU)

Format                   Abstract                          Code                   State   T Bit
BRAF  Rm                   Rm + PC ﬁ   PC             0000mmmm00100011 2—


Description: Branches unconditionally. The branch destination is PC + the 32-bit contents of
the general register Rm. PC is the start address of the second instruction after this instruction.

Note: Since this is a delayed branch instruction, the instruction after BRAF is executed
before branching. No interrupts or address errors are accepted between this instruction and the
next instruction. If the next instruction is a branch instruction, it is acknowledged as an illegal
slot instruction.

Operation:

  BRAF(long m) /* BRAF Rm */
  {
       unsigned long temp;


       temp=PC;
       PC+=R[m];
       Delay_Slot(temp+2);
  }

Example:

            MOV.L  #(TRGET-BSRF_PC),R0   Sets displacement
            BRAF  @R0                             Branches to TRGET
            ADD   R0,R1                           Executes ADD before branching
     BRAF_PC:                                                         ‹   The PC location is used to calculate
                                                        the branch destination address of
                                                        the BRAF instruction
            NOP
     TRGET:                 ‹   Branch destination of the BRAF instruction














70 HITACHI
```

## PDF page 75

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=75)

```text
Note: With delayed branching, branching occurs after execution of the slot instruction.
However, instructions such as register changes etc. are executed in the order of
delayed branch instruction, then delay slot instruction. For example, even if the
register in which the branch destination address has been loaded is changed by the
delay slot instruction, the branch will still be made using the value of the register prior
to the change as the branch destination address.
























































                                                                                              HITACHI  71
```

## PDF page 76

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=76)

```text
6.10      BSR (Branch to Subroutine): Branch Instruction

Format                    Abstract                            Code                   State   T Bit
BSR   label        PC ﬁ   PR, disp · 2 + PC ﬁ   PC 1011dddddddddddd 2—


Description: Branches to the subroutine procedure at a specified address after executing the
instruction following this BSR instruction. The PC value is stored in the PR, and the program
branches to an address specified by PC + displacement. The PC points to the starting address
of the second instruction after this BSR instruction. The 12-bit displacement is sign-extended
and doubled. Consequently, the relative interval from the branch destination is –4096 to
+4094 bytes. If the displacement is too short to reach the branch destination, the JSR
instruction must be used instead. With JSR, the destination address must be transferred to a
register by using the MOV instruction. This BSR instruction and the RTS instruction are used
for a subroutine procedure call.

Note: Since this is a delayed branch instruction, the instruction after BSR is executed before
branching. No interrupts or address errors are accepted between this instruction and the next
instruction. If the next instruction is a branch instruction, it is acknowledged as an illegal slot
instruction.

Operation:

  BSR(long d)  /* BSR disp */
  {
       long disp;


       if ((d&0x800)==0) disp=(0x00000FFF & d);
       else disp=(0xFFFFF000 | d);
       PR=PC;
       PC=PC+(disp<<1)+4;
       Delay_Slot(PR+2);
  }
















72 HITACHI
```

## PDF page 77

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=77)

```text
Example:

             BSR   TRGET       Branches to TRGET
             MOV   R3,R4       Executes the MOV instruction before branching
             ADD   R0,R1         ‹  The PC location is used to calculate the branch destination
                                    address of the BSR instruction (return address for when the
                                    subroutine procedure is completed (PR data))
             .......
             .......
  TRGET:                          ‹  Procedure entrance
             MOV  R2,R3
             RTS                    Returns to the above ADD instruction
             MOV#1,R0           Executes MOV before branching

        Note: With delayed branching, branching occurs after execution of the slot instruction.
        However, instructions such as register changes etc. are executed in the order of
        delayed branch instruction, then delay slot instruction. For example, even if the
        register in which the branch destination address has been loaded is changed by the
        delay slot instruction, the branch will still be made using the value of the register prior
        to the change as the branch destination address.




































                                                                                                    HITACHI  73
```

## PDF page 78

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=78)

```text
6.11      BSRF (Branch to Subroutine Far): Branch Instruction (SH-2 CPU)

Format                    Abstract                           Code                   State   T Bit
BSRF  Rm            PC ﬁ   PR, Rm + PC ﬁ   PC     0000mmmm00000011 2—

Description: Branches to the subroutine procedure at a specified address after executing the
instruction following this BSRF instruction. The PC value is stored in the PR. The branch
destination is PC + the 32-bit contents of the general register Rm. PC is the start address of
the second instruction after this instruction. Used as a subroutine procedure call in
combination with RTS.
Note: Since this is a delayed branch instruction, the instruction after BSR is executed before
branching. No interrupts or address errors are accepted between this instruction and the next
instruction. If the next instruction is a branch instruction, it is acknowledged as an illegal slot
instruction.
Operation:
  BSRF(long m) /* BSRF Rm */
  {
       PR=PC;
       PC+=R[m];
       Delay_Slot(PR+2);
  }

Example:
             MOV.L  #(TRGET-BSRF_PC),R0       Sets displacement
             BRSF  @R0                                   Branches to TRGET
             MOV   R3,R4                                  Executes the MOV instruction before
                                                               branching
  BSRF_PC:                                                       ‹  The PC location is used to
                                                               calculate the branch destination
                                                               with BSRF
             ADD   R0,R1
             .....
             .....
  TRGET:                                                     ‹  Procedure entrance
             MOV  R2,R3
             RTS                                               Returns to the above ADD instruction
             MOV#1,R0                                      Executes MOV before branching

    Note: With delayed branching, branching occurs after execution of the slot instruction.
    However, instructions such as register changes etc. are executed in the order of delayed
    branch instruction, then delay slot instruction. For example, even if the register in which
    the branch destination address has been loaded is changed by the delay slot instruction,
    the branch will still be made using the value of the register prior to the change as the
    branch destination address.





74 HITACHI
```

## PDF page 79

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=79)

```text
6.12      BT (Branch if True): Branch Instruction

Format                      Abstract                             Code                   State   T Bit
B T label          When T = 1, disp · 2 + PC ﬁ                  10001001dddddddd      3/1         —
                         PC;
                         When T = 0, nop


Description: Reads the T bit, and conditionally branches. If T = 1, BT branches. If T = 0, BT
executes the next instruction. The branch destination is an address specified by PC +
displacement. The PC points to the starting address of the second instruction after the branch
instruction. The 8-bit displacement is sign-extended and doubled. Consequently, the relative
interval from the branch destination is –256 to +254 bytes. If the displacement is too short to
reach the branch destination, use BT with the BRA instruction or the like.

Note: When branching, requires three cycles; when not branching, one cycle.

Operation:

  BT(long d)/* BT disp */
  {
       long disp;


       if ((d&0x80)==0) disp=(0x000000FF & (long)d);
       else disp=(0xFFFFFF00 | (long)d);
       if (T==1) PC=PC+(disp<<1)+4;
       else PC+=2;
  }

Example:

               SETT                  T is always 1
               B F TRGET_F   Does not branch, because T = 1
               B T TRGET_T   Branches to TRGET_T, because T = 1
               NOP
               NOP                      ‹  The PC location is used to calculate the branch destination
                                     address of the BT instruction
       TRGET_T:                    ‹   Branch destination of the BT instruction











                                                                                                 HITACHI  75
```

## PDF page 80

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=80)

```text
6.13      BT/S (Branch if True with Delay Slot): Branch Instruction (SH-2
           CPU)

Format                     Abstract                             Code                   State   T Bit
BT/S  label          When T = 1, disp · 2 + PC ﬁ                10001101dddddddd      2/1         —
                         PC;
                         When T = 0, nop


Description: Reads the T bit, and conditionally branches with delay slot. If T = 1, BT/S
branches after the following instruction executes. If T = 0, BT/S executes the next instruction.
The branch destination is an address specified by PC + displacement. The PC points to the
starting address of the second instruction after the branch instruction. The 8-bit displacement
is sign-extended and doubled. Consequently, the relative interval from the branch destination
is –256 to +254 bytes. If the displacement is too short to reach the branch destination, use
BT/S with the BRA instruction or the like.

Note: Since this is a delay branch instruction, the instruction immediately after is executed
before the branch. Between the time this instruction and the immediately after instruction are
executed, address errors or interrupts are not accepted. When the immediately after
instruction is a branch instruction, it is recognized as an illegal slot instruction. When
branching, requires two cycles; when not branching, one cycle.

Operation:

  BTS(long d)  /* BTS disp */
  {
       long disp;
       unsigned  long temp;


       temp=PC;
       if ((d&0x80)==0) disp=(0x000000FF & (long)d);
       else disp=(0xFFFFFF00 | (long)d);
       if (T==1) {
            PC=PC+(disp<<1)+4;
            Delay_Slot(temp+2);
       }
       else PC+=2;
  }








76 HITACHI
```

## PDF page 81

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=81)

```text
Example:

               SETT                T is always 1
               BF/S TRGET_F  Does not branch, because T = 1
               NOP
               BT/S TRGET_T  Branches to TRGET, because T = 1
               ADD  R0,R1      Executes before branching.
               NOP                   ‹  The PC location is used to calculate the branch destination
                                     address of the BT/S instruction
       TRGET_T:                 ‹   Branch destination of the BT/S instruction

Note:   With delayed branching, branching occurs after execution of the slot instruction.
         However, instructions such as register changes etc. are executed in the order of
         delayed branch instruction, then delay slot instruction. For example, even if the
         register in which the branch destination address has been loaded is changed by the
         delay slot instruction, the branch will still be made using the value of the register
         prior to the change as the branch destination address.








































                                                                                                     HITACHI  77
```

## PDF page 82

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=82)

```text
6.14      CLRMAC (Clear MAC Register): System Control Instruction

Format                    Abstract                           Code                   State   T Bit
CLRMAC                   0 ﬁ   MACH, MACL         0000000000101000 1—


Operation:

  CLRMAC()  /* CLRMAC */
  {
      MACH=0;
      MACL=0;
      PC+=2;
  }

Example:

  CLRMAC                          Initializes the MAC register
  MAC.W   @R0+,@R1+         Multiply and accumulate operation
  MAC.W  @R0+,@R1+




































78 HITACHI
```

## PDF page 83

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=83)

```text
6.15      CLRT (Clear T Bit): System Control Instruction

Format                     Abstract                           Code                   State    T Bit
CLRT                      0 ﬁ   T                    000000000000100010


Description: Clears the T bit.

Operation:

  CLRT() /* CLRT */
  {
       T=0;
       PC+=2;
  }

Example:

  CLRT    Before execution     T = 1
           After execution      T = 0





































                                                                                           HITACHI  79
```

## PDF page 84

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=84)

```text
6.16      CMP/cond (Compare Conditionally): Arithmetic Instruction

Format                    Abstract                       Code                   State  T Bit
CMP/EQ Rm,Rn            When Rn = Rm, 1 ﬁ   T            0011nnnnmmmm0000            1         Comparison
                                                                                               result
CMP/GE Rm,Rn            When signed and Rn ‡             0011nnnnmmmm0011            1         Comparison
                        Rm, 1  ﬁ   T                                                           result
CMP/GT Rm,Rn            When signed and Rn >             0011nnnnmmmm0111            1         Comparison
                        Rm, 1  ﬁ   T                                                           result
CMP/HI Rm,Rn            When unsigned and Rn >           0011nnnnmmmm0110            1         Comparison
                        Rm, 1  ﬁ   T                                                           result
CMP/HS Rm,Rn            When unsigned and Rn ‡           0011nnnnmmmm0010            1         Comparison
                        Rm, 1  ﬁ   T                                                           result
CMP/PL R n              When Rn > 0, 1 ﬁ   T             0100nnnn00010101            1         Comparison
                                                                                               result
CMP/PZ R n              When Rn ‡ 0, 1 ﬁ   T             0100nnnn00010001            1         Comparison
                                                                                               result
CMP/STR                 When a byte in Rn equals         0010nnnnmmmm1100            1         Comparison
         Rm,Rn          a byte in Rm, 1 ﬁ   T                                                  result
                        When R0 = imm, 1 ﬁ   T           10001000iiiiiiii            1         Comparison
CMP/EQ #imm,R0                                                                                 result


Description: Compares general register Rn data with Rm data, and sets the T bit to 1 if a
specified condition (cond) is satisfied. The T bit is cleared to 0 if the condition is not
satisfied. The Rn data does not change. The following eight conditions can be specified.
Conditions PZ and PL are the results of comparisons between Rn and 0. Sign-extended 8-bit
immediate data can also be compared with R0 by using condition EQ. Here, R0 data does not
change. Table 6.1 shows the mnemonics for the conditions.























80 HITACHI
```

## PDF page 85

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=85)

```text
Table 6.1   CMP Mnemonics

Mnemonics                Condition
CMP/EQ Rm,Rn                     If Rn = Rm, T = 1
CMP/GE Rm,Rn                    If Rn ‡ Rm with signed data, T = 1
CMP/GT Rm,Rn                    If Rn > Rm with signed data, T = 1
CMP/HI Rm,Rn                    If Rn > Rm with unsigned data, T = 1
CMP/HS Rm,Rn                    If Rn ‡ Rm with unsigned data, T = 1
CMP/PL Rn                          If Rn > 0, T = 1
CMP/PZ Rn                          If Rn ‡ 0, T = 1
CMP/STR Rm,Rn                  If a byte in Rn equals a byte in Rm, T = 1
CMP/EQ #imm,R0                 If R0 = imm, T = 1


Operation:

  CMPEQ(long m,long n)    /* CMP_EQ Rm,Rn */
  {
       if (R[n]==R[m]) T=1;
       else T=0;
       PC+=2;
  }

  CMPGE(long m,long n)    /* CMP_GE Rm,Rn */
  {
       if ((long)R[n]>=(long)R[m]) T=1;
       else T=0;
       PC+=2;
  }

  CMPGT(long m,long n)    /* CMP_GT Rm,Rn */
  {
       if ((long)R[n]>(long)R[m]) T=1;
       else T=0;
       PC+=2;
  }









                                                                                                  HITACHI  81
```

## PDF page 86

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=86)

```text
  CMPHI(long m,long n)    /* CMP_HI Rm,Rn */
  {
      if ((unsigned long)R[n]>(unsigned long)R[m]) T=1;
      else T=0;
      PC+=2;
  }

 CMPHS(long m,long n)    /* CMP_HS Rm,Rn */
 {
      if ((unsigned long)R[n]>=(unsigned long)R[m]) T=1;
      else T=0;
      PC+=2;
  }

 CMPPL(long n)           /* CMP_PL Rn */
 {
      if ((long)R[n]>0) T=1;
      else T=0;
      PC+=2;
  }

 CMPPZ(long n) /* CMP_PZ Rn */
 {
      if ((long)R[n]>=0) T=1;
      else T=0;
      PC+=2;
  }























82 HITACHI
```

## PDF page 87

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=87)

```text
 CMPSTR(long m,long n)   /* CMP_STR Rm,Rn */
 {
      unsigned long temp;
      long HH,HL,LH,LL;


      temp=R[n]^R[m];
      HH=(temp>>12)&0x000000FF;
      HH=(temp>>8)&0x000000FF;
      HH=(temp>>4)&0x000000FF;
      LL=temp&0x000000FF;
      HH=HH&&HL&&LH&&LL;
      if (HH==0) T=1;
      else T=0;
      PC+=2;
  }

 CMPIM(long i)           /* CMP_EQ #imm,R0 */
 {
      long imm;


      if ((i&0x80)==0) imm=(0x000000FF & (long i));
      else imm=(0xFFFFFF00 | (long i));
      if (R[0]==imm) T=1;
      else T=0;
      PC+=2;
  }

Example:

      CMP/GE    R0,R1         R0 = H'7FFFFFFF, R1 = H'80000000
      B T TRGET_T      Does not branch because T = 0
      CMP/HS   R0,R1         R0 = H'7FFFFFFF, R1 = H'80000000
      B T TRGET_T     Branches because T = 1
      CMP/STR  R2,R3         R2 = “ABCD”, R3 = “XYCZ”
      B T TRGET_T              Branches because T = 1










                                                                                  HITACHI  83
```

## PDF page 88

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=88)

```text
6.17      DIV0S (Divide Step 0 as Signed): Arithmetic Instruction

Format              Abstract                            Code                   State   T Bit
DIV0S  Rm,Rn    MSB of Rn ﬁ   Q, MSB of Rm               0010nnnnmmmm0111       1            Calculation
                    ﬁ   M, M^Q ﬁ   T                                                            result


Description: DIV0S is an initialization instruction for signed division. It finds the quotient by
repeatedly dividing in combination with the DIV1 or another instruction that divides for each
bit after this instruction. See the description given with DIV1 for more information.

Operation:

  DIV0S(long m,long n)    /* DIV0S Rm,Rn */
  {
       if ((R[n]&0x80000000)==0) Q=0;
       else Q=1;
       if ((R[m]&0x80000000)==0) M=0;
       else M=1;
       T=!(M==Q);
       PC+=2;
  }

Example: See DIV1.































84 HITACHI
```

## PDF page 89

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=89)

```text
6.18      DIV0U (Divide Step 0 as Unsigned): Arithmetic Instruction

Format                     Abstract                            Code                   State   T Bit
DIV0U                     0 ﬁ  M/Q/T              0000000000011001 10

Description: DIV0U is an initialization instruction for unsigned division. It finds the quotient
by repeatedly dividing in combination with the DIV1 or another instruction that divides for
each bit after this instruction. See the description given with DIV1 for more information.

Operation:

  DIV0U()   /* DIV0U */
  {
       M=Q=T=0;
       PC+=2;
  }

Example: See DIV1.






































                                                                                              HITACHI  85
```

## PDF page 90

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=90)

```text
6.19      DIV1 (Divide Step 1): Arithmetic Instruction

Format                   Abstract                         Code                   State   T Bit
DIV1  Rm,Rn         1-step division (Rn ‚ Rm)    0011nnnnmmmm0100      1           Calculation
                                                                                                       result

Description: Uses single-step division to divide one bit of the 32-bit data in general register
Rn (dividend) by Rm data (divisor). It finds a quotient through repetition either independently
or used in combination with other instructions. During this repetition, do not rewrite the
specified register or the M, Q, and T bits.

In one-step division, the dividend is shifted one bit left, the divisor is subtracted and the
quotient bit reflected in the Q bit according to the status (positive or negative). To find the
remainder in a division, first find the quotient using a DIV1 instruction, then find the
remainder as follows:

          (Dividend) – (divisor) · (quotient) = (remainder)

with the SH-2 CPU in which a divider is installed as a peripheral function, the remainder can
be found as a function of the divider.

Zero division, overflow detection, and remainder operation are not supported. Check for zero
division and overflow division before dividing.

Find the remainder by first finding the sum of the divisor and the quotient obtained and then
subtracting it from the dividend. That is, first initialize with DIV0S or DIV0U. Repeat DIV1
for each bit of the divisor to obtain the quotient. When the quotient requires 17 or more bits,
place ROTCL before DIV1. For the division sequence, see the following examples.


























86 HITACHI
```

## PDF page 91

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=91)

```text
Operation:

  DIV1(long m,long n)    /* DIV1 Rm,Rn */
  {
      unsigned long tmp0;
      unsigned char    old_q,tmp1;


      old_q=Q;
      Q=(unsigned char)((0x80000000 & R[n])!=0);
      R[n]<<=1;
      R[n]|=(unsigned long)T;
          switch(old_q){
          case 0:switch(M){
              case 0:tmp0=R[n];
                  R[n]-=R[m];
                  tmp1=(R[n]>tmp0);
                  switch(Q){
                  case 0:Q=tmp1;
                       break;
                  case 1:Q=(unsigned char)(tmp1==0);
                       break;
                  }
                  break;
              case 1:tmp0=R[n];
                  R[n]+=R[m];
                  tmp1=(R[n]<tmp0);
                  switch(Q){
                  case 0:Q=(unsigned char)(tmp1==0);
                       break;
                  case 1:Q=tmp1;
                       break;
              }
              break;
          }
          break;








                                                                                HITACHI  87
```

## PDF page 92

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=92)

```text
      case 1:switch(M){
          case 0:tmp0=R[n];
              R[n]+=R[m];
              tmp1=(R[n]<tmp0);
              switch(Q){
              case 0:Q=tmp1;
                  break;
              case 1:Q=(unsigned char)(tmp1==0);
                  break;
              }
              break;
          case 1:tmp0=R[n];
              R[n]-=R[m];
              tmp1=(R[n]>tmp0);
              switch(Q){
              case 0:Q=(unsigned char)(tmp1==0);
                  break;
          case 1:Q=tmp1;
                  break;
              }
              break;
          }
          break;
      }
      T=(Q==M);
      PC+=2;
  }




















88 HITACHI
```

## PDF page 93

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=93)

```text
Example 1:

                                R1 (32 bits) / R0 (16 bits) = R1 (16 bits):Unsigned
  SHLL16    R0               Upper 16 bits = divisor, lower 16 bits = 0
  TST       R0,R0         Zero division check
  BT       ZERO_DIV
  CMP/HS    R0,R1         Overflow check
  BT       OVER_DIV
  DIV0U                   Flag initialization
  .arepeat 16
  DIV1   R0,R1        Repeat 16 times
  .aendr
  ROTCL    R1
  EXTU.W R1,R2               R1 = Quotient

Example 2:

                                 R1:R2 (64 bits)/R0 (32 bits) = R2 (32 bits):Unsigned
  TST      R0,R0          Zero division check
  BT       ZERO_DIV
  CMP/HS    R0,R1           Overflow check
  BT       OVER_DIV
  DIV0U                    Flag initialization
  .arepeat 32
  ROTCL  R2             Repeat 32 times
  DIV1     R0,R1
  .aendr
  ROTCL  R2                     R2 = Quotient





















                                                                                           HITACHI  89
```

## PDF page 94

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=94)

```text
Example 3:

                                  R1 (16 bits)/R0 (16 bits) = R1 (16 bits):Signed
  SHLL16    R0                Upper 16 bits = divisor, lower 16 bits = 0
  EXTS.W     R1,R1          Sign-extends the dividend to 32 bits
  XOR       R2,R2          R2 = 0
  MOV       R1,R3
  ROTCL      R3
  SUBC      R2,R1         Decrements if the dividend is negative
  DIV0S     R0,R1       Flag initialization
  .arepeat 16
  DIV1  R0,R1        Repeat 16 times
  .aendr
  EXTS.W    R1,R1
  ROTCL     R1               R1 = quotient (one’s complement)
  ADDC     R2,R1          Increments and takes the two’s complement if the MSB of the
                                  quotient is 1
  EXTS.W R1,R1               R1 = quotient (two’s complement)

Example 4:

                                   R2 (32 bits) / R0 (32 bits) = R2 (32 bits):Signed
  MOV      R2,R3
  ROTCL     R3
  SUBC      R1,R1            Sign-extends the dividend to 64 bits (R1:R2)
  XOR      R3,R3             R3 = 0
  SUBC      R3,R2            Decrements and takes the one’s complement if the dividend is
                                   negative
  DIV0S     R0,R1         Flag initialization
  .arepeat 32
  ROTCL  R2              Repeat 32 times
  DIV1     R0,R1
  .aendr
  ROTCL    R2               R2 = Quotient (one’s complement)
  ADDC  R3,R2                 Increments and takes the two’s complement if the MSB of the
                                   quotient is 1. R2 = Quotient (two’s complement)









90 HITACHI
```

## PDF page 95

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=95)

```text
6.20      DMULS.L (Double-Length Multiply as Signed): Arithmetic
          Instruction (SH-2 CPU)

Format                  Abstract                         Code                   State   T Bit
DMULS.L               With signed, Rn · Rm ﬁ            0011nnnnmmmm1101      2 to 4    —
         Rm,Rn        MACH, MACL


Description: Performs 32-bit multiplication of the contents of general registers Rn and Rm,
and stores the 64-bit results in the MACL and MACH registers. The operation is a signed
arithmetic operation.

Operation:

  DMULS(long m,long n)/* DMULS.L Rm,Rn */
  {
      unsigned  long RnL,RnH,RmL,RmH,Res0,Res1,Res2;
      unsigned  long temp0,temp1,temp2,temp3;
      long tempm,tempn,fnLmL;


      tempn=(long)R[n];
      tempm=(long)R[m];
      if (tempn<0) tempn=0-tempn;
      if (tempm<0) tempm=0-tempm;
      if ((long)(R[n]^R[m])<0) fnLmL=-1;
      else fnLmL=0;


      temp1=(unsigned long)tempn;
      temp2=(unsigned long)tempm;


      RnL=temp1&0x0000FFFF;
      RnH=(temp1>>16)&0x0000FFFF;
      RmL=temp2&0x0000FFFF;
      RmH=(temp2>>16)&0x0000FFFF;


      temp0=RmL*RnL;
      temp1=RmH*RnL;
      temp2=RmL*RnH;
      temp3=RmH*RnH;





                                                                                      HITACHI  91
```

## PDF page 96

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=96)

```text
      Res2=0
      Res1=temp1+temp2;
      if (Res1<temp1) Res2+=0x00010000;


      temp1=(Res1<<16)&0xFFFF0000;
      Res0=temp0+temp1;
      if (Res0<temp0) Res2++;


      Res2=Res2+((Res1>>16)&0x0000FFFF)+temp3;


      if (fnLmL<0) {
           Res2=~Res2;
           if (Res0==0)
               Res2++;
           else
               Res0=(~Res0)+1;
      }
      MACH=Res2;
      MACL=Res0;
      PC+=2;
  }

Example:

  DMULS  R0,R1         Before execution R0 = H'FFFFFFFE, R1 = H'00005555
                         After execution   MACH = H'FFFFFFFF, MACL = H'FFFF5556
  STS   MACH,R0     Operation result (top)
  STSMACL,R0           Operation result (bottom)





















92 HITACHI
```

## PDF page 97

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=97)

```text
6.21      DMULU.L (Double-Length Multiply as Unsigned): Arithmetic
          Instruction (SH-2 CPU)

Format                  Abstract                         Code                   State   T Bit
DMULU.L                Without signed, Rn · Rm ﬁ         0011nnnnmmmm0101      2 to 4    —
         Rm,Rn         MACH, MACL


Description: Performs 32-bit multiplication of the contents of general registers Rn and Rm,
and stores the 64-bit results in the MACL and MACH registers. The operation is an unsigned
arithmetic operation.

Operation:

  DMULU(long m,long n)/* DMULU.L Rm,Rn */
  {
      unsigned  long RnL,RnH,RmL,RmH,Res0,Res1,Res2;
      unsigned  long temp0,temp1,temp2,temp3;


      RnL=R[n]&0x0000FFFF;
      RnH=(R[n]>>16)&0x0000FFFF;


      RmL=R[m]&0x0000FFFF;
      RmH=(R[m]>>16)&0x0000FFFF;


      temp0=RmL*RnL;
      temp1=RmH*RnL;
      temp2=RmL*RnH;
      temp3=RmH*RnH;





















                                                                                        HITACHI  93
```

## PDF page 98

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=98)

```text
      Res2=0
      Res1=temp1+temp2;
      if (Res1<temp1) Res2+=0x00010000;


      temp1=(Res1<<16)&0xFFFF0000;
      Res0=temp0+temp1;
      if (Res0<temp0) Res2++;


      Res2=Res2+((Res1>>16)&0x0000FFFF)+temp3;


      MACH=Res2;
      MACL=Res0;
      PC+=2;
  }

Example:

  DMULU   R0,R1        Before execution R0 = H'FFFFFFFE, R1 = H'00005555
                         After execution    MACH = H'00005554, MACL = H'FFFF5556
  STS    MACH,R0     Operation result (top)
  STS    MACL,R0     Operation result (bottom)
































94 HITACHI
```

## PDF page 99

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=99)

```text
6.22      DT (Decrement and Test): Arithmetic Instruction (SH-2 CPU)

Format                  Abstract                          Code                 State   T Bit
DT     Rn               Rn - 1  ﬁ   Rn;                    0100nnnn00010000    1            Comparison
                      When Rn is 0, 1 ﬁ   T,                                                    result
                      when Rn is nonzero, 0 ﬁ   T


Description: The contents of general register Rn is decremented by 1 and the result is
compared to 0 (zero). When the result is 0, the T bit is set to 1. When the result is not zero,
the T bit is set to 0.

Operation:

  DT(long n)/* DT Rn */
  {
       R[n]--;
       if (R[n]==0) T=1;
       else T=0;
       PC+=2;
  }

Example:

       MOV    #4,R5   Sets the number of loops.
   LOOP:
       ADD    R0,R1
       DT     R S      Decrements the R5 value and checks whether it has become 0.
       B F LOOP    Branches to LOOP if T=0. (In this example, loops 4 times.)























                                                                                                 HITACHI  95
```

## PDF page 100

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=100)

```text
6.23      EXTS (Extend as Signed): Arithmetic Instruction

Format                  Abstract                               Code                   State   T Bit
EXTS.B  Rm,Rn        Sign-extended Rm from byte ﬁ          0110nnnnmmmm1110 1—
                     Rn
EXTS.W  Rm,Rn       Sign-extended Rm from word ﬁ           0110nnnnmmmm1111 1—
                     Rn


Description: Sign-extends general register Rm data, and stores the result in Rn. If byte length
is specified, the bit 7 value of Rm is transferred to bits 8 to 31 of Rn. If word length is
specified, the bit 15 value of Rm is transferred to bits 16 to 31 of Rn.

Operation:

  EXTSB(long m,long n)    /* EXTS.B Rm,Rn */
  {
       R[n]=R[m];
       if ((R[m]&0x00000080)==0) R[n]&=0x000000FF;
       else R[n]|=0xFFFFFF00;
       PC+=2;
  }

  EXTSW(long m,long n)    /* EXTS.W Rm,Rn */
  {
       R[n]=R[m];
       if ((R[m]&0x00008000)==0) R[n]&=0x0000FFFF;
       else R[n]|=0xFFFF0000;
       PC+=2;
  }

Examples:

  EXTS.B  R0,R1     Before execution       R0 = H'00000080
                      After execution       R1 = H'FFFFFF80
  EXTS.W R0,R1     Before execution       R0 = H'00008000
                      After execution        R1 = H'FFFF8000











96 HITACHI
```

## PDF page 101

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=101)

```text
6.24      EXTU (Extend as Unsigned): Arithmetic Instruction

Format                 Abstract                              Code                   State   T Bit
EXTU.B Rm,Rn          Zero-extend Rm from byte ﬁ    Rn       0110nnnnmmmm1100            1        —
EXTU.W Rm,Rn          Zero-extend Rm from word ﬁ   Rn        0110nnnnmmmm1101            1        —


Description: Zero-extends general register Rm data, and stores the result in Rn. If byte length
is specified, 0 is transferred to bits 8 to 31 of Rn. If word length is specified, 0 is transferred to
bits 16 to 31 of Rn.

Operation:

  EXTUB(long m,long n)/* EXTU.B Rm,Rn */
  {
       R[n]=R[m];
       R[n]&=0x000000FF;
       PC+=2;
  }

  EXTUW(long m,long n)/* EXTU.W Rm,Rn */
  {
       R[n]=R[m];
       R[n]&=0x0000FFFF;
       PC+=2;
  }

Examples:

  EXTU.B  R0,R1     Before execution   R0 = H'FFFFFF80
                        After execution       R1 = H'00000080
  EXTU.W  R0,R1     Before execution    R0 = H'FFFF8000
                        After execution        R1 = H'00008000
















                                                                                             HITACHI  97
```

## PDF page 102

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=102)

```text
6.25     JMP (Jump): Branch Instruction

Class: Delayed branch instruction

Format                 Abstract                       Code                   State   T Bit
JMP   @Rm             Rm ﬁ   PC                  0100mmmm00101011 2—


Description: Delayed-branches unconditionally to the address specified with register indirect.
The branch destination is an address specified by the 32-bit data in general register Rm.

Note: Since this is a delayed branch instruction, the instruction after JMP is executed before
branching. No interrupts or address errors are accepted between this instruction and the next
instruction. If the next instruction is a branch instruction, it is acknowledged as an illegal slot
instruction.

Operation:

  JMP(long m)  /* JMP @Rm */
  {
       unsigned long temp;


       temp=PC;
       PC=R[m]+4;
       Delay_Slot(temp+2);
  }

Example:

                    MOV.L    JMP_TABLE,R0    Address of R0 = TRGET
                    JMP      @R0                  Branches to TRGET
                    MOV      R0,R1               Executes MOV before branching
                    .align 4
  JMP_TABLE:  .data.l  TRGET             Jump table
                    .................
  TRGET:     ADD     #1,R1               ‹   Branch destination

Note:  With delayed branching, branching occurs after execution of the slot instruction.
        However, instructions such as register changes etc. are executed in the order of
        delayed branch instruction, then delay slot instruction. For example, even if the
        register in which the branch destination address has been loaded is changed by the
        delay slot instruction, the branch will still be made using the value of the register
        prior to the change as the branch destination address.



98 HITACHI
```

## PDF page 103

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=103)

```text
6.26      JSR (Jump to Subroutine): Branch Instruction

Class: Delayed branch instruction

Format                 Abstract                       Code                   State   T Bit
JSR   @Rm           PC ﬁ   PR, Rm ﬁ   PC         0100mmmm00001011 2—


Description: Delayed-branches to the subroutine procedure at a specified address after
executing the instruction following this JSR instruction. The PC value is stored in the PR. The
jump destination is an address specified by the 32-bit data in general register Rm. The PC
points to the starting address of the second instruction after JSR. The JSR instruction and RTS
instruction are used for subroutine procedure calls.

Note: Since this is a delayed branch instruction, the instruction after JSR is executed before
branching. No interrupts and address errors are accepted between this instruction and the next
instruction. If the next instruction is a branch instruction, it is acknowledged as an illegal slot
instruction.

Operation:

  JSR(long m)  /* JSR @Rm */
  {
       PR=PC;
       PC=R[m]+4;
       Delay_Slot(PR+2);
  }

Example:

                   MOV.L    JSR_TABLE,R0       R0 = Address of TRGET
                   JSR     @R0                    Branches to TRGET
                   XOR     R1,R1                 Executes XOR before branching
                   ADD     R0,R1                     ‹  Return address for when the
                                                       subroutine procedure is completed
                                                       (PR data)
                   ...........
                   .align 4
  JSR_TABLE: .data.l  TRGET               Jump table
  TRGET:    NOP                                  ‹  Procedure entrance
                   MOV    R2,R3
                   RTS                                  Returns to the above ADD instruction
                   MOV     #70,R1               Executes MOV before RTS


                                                                                                HITACHI  99
```

## PDF page 104

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=104)

```text
Note:   With delayed branching, branching occurs after execution of the slot instruction.
         However, instructions such as register changes etc. are executed in the order of
         delayed branch instruction, then delay slot instruction. For example, even if the
         register in which the branch destination address has been loaded is changed by the
         delay slot instruction, the branch will still be made using the value of the register
         prior to the change as the branch destination address.
























































100 HITACHI
```

## PDF page 105

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=105)

```text
6.27      LDC (Load to Control Register): System Control Instruction

Class: Interrupt disabled instruction

Format                  Abstract                        Code                   State   T Bit
LDC   Rm,SR            Rm ﬁ   SR                           0100mmmm00001110           1        LSB
LDC   Rm,GBR           Rm  ﬁ   GBR                         0100mmmm00011110           1        —
LDC   Rm,VBR           Rm  ﬁ   VBR                         0100mmmm00101110           1        —
LDC.L  @Rm+,SR         (Rm) ﬁ   SR, Rm + 4 ﬁ    Rm         0100mmmm00000111           3        LSB
LDC.L  @Rm+,GBR        (Rm)  ﬁ   GBR, Rm + 4 ﬁ    Rm       0100mmmm00010111           3        —
LDC.L  @Rm+,VBR        (Rm)  ﬁ   VBR, Rm + 4 ﬁ    Rm       0100mmmm00100111           3        —


Description: Stores the source operand into control registers SR, GBR, or VBR.

Note: No interrupts are accepted between this instruction and the next instruction. Address
errors are accepted.

Operation:

  LDCSR(long m)    /* LDC Rm,SR */
  {
       SR=R[m]&0x000003F3;
       PC+=2;
  }

  LDCGBR(long m)   /* LDC Rm,GBR */
  {
       GBR=R[m];
       PC+=2;
  }

  LDCVBR(long m)   /* LDC Rm,VBR */
  {
       VBR=R[m];
       PC+=2;
  }










                                                                                        HITACHI  101
```

## PDF page 106

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=106)

```text
  LDCMSR(long m)   /* LDC.L @Rm+,SR */
  {
      SR=Read_Long(R[m])&0x000003F3;
      R[m]+=4;
      PC+=2;
  }

  LDCMGBR(long m)  /* LDC.L @Rm+,GBR */
  {
      GBR=Read_Long(R[m]);
      R[m]+=4;
      PC+=2;
  }

  LDCMVBR(long m)  /* LDC.L @Rm+,VBR */
  {
      VBR=Read_Long(R[m]);
      R[m]+=4;
      PC+=2;
  }

Examples:

  LDC    R0,SR           Before execution    R0 = H'FFFFFFFF, SR = H'00000000
                            After execution      SR = H'000003F3

  LDC.L  @R15+,GBR      Before execution     R15 = H'10000000
                            After execution       R15 = H'10000004, GBR = @H'10000000























102 HITACHI
```

## PDF page 107

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=107)

```text
6.28      LDS (Load to System Register): System Control Instruction

Class: Interrupt disabled instruction

Format                   Abstract                          Code                  State   T Bit
LDS   Rm,MACH           Rm ﬁ   MACH                          0100mmmm00001010          1         —
LDS   Rm,MACL           Rm ﬁ   MACL                          0100mmmm00011010          1         —
LDS   Rm,PR             Rm  ﬁ    PR                          0100mmmm00101010          1         —
LDS.L                   (Rm)  ﬁ   MACH, Rm + 4 ﬁ    Rm       0100mmmm00000110          1         —
         @Rm+,MAC       (Rm)  ﬁ   MACL, Rm + 4 ﬁ    Rm       0100mmmm00010110          1         —
H                       (Rm)  ﬁ   PR, Rm + 4 ﬁ    Rm         0100mmmm00100110          1         —
LDS.L
         @Rm+,MAC
L
LDS.L  @Rm+,PR


Description: Stores the source operand into the system registers MACH, MACL, or PR.

Note: No interrupts are accepted between this instruction and the next instruction. Address
errors are accepted.

For the SH-1 CPU, the lower 10 bits are stored in MACH. For the SH-2 CPU, 32 bits are
stored in MACH.

Operation:

  LDSMACH(long m)         /* LDS Rm,MACH */
  {
       MACH=R[m];
       if ((MACH&0x00000200)==0) MACH&=0x000003FF;    For SH-1 CPU (these 2 lines
       else MACH|=0xFFFFFC00;                                       not needed for SH-2 CPU)
       PC+=2;
  }
  LDSMACL(long m)         /* LDS Rm,MACL */
  {
       MACL=R[m];
       PC+=2;
  }








                                                                                          HITACHI  103
```

## PDF page 108

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=108)

```text
  LDSPR(long m)           /* LDS Rm,PR */
  {
      PR=R[m];
      PC+=2;
  }

  LDSMMACH(long m)       /* LDS.L @Rm+,MACH */
  {
      MACH=Read_Long(R[m]);
      if ((MACH&0x00000200)==0) MACH&=0x000003FF;    For SH-1 CPU (these 2 lines
      else MACH|=0xFFFFFC00;                                       not needed for SH-2 CPU)
      R[m]+=4;
      PC+=2;
  }
  LDSMMACL(long m)       /* LDS.L @Rm+,MACL */
  {
      MACL=Read_Long(R[m]);
      R[m]+=4;
      PC+=2;
  }

  LDSMPR(long m)   /* LDS.L @Rm+,PR */
  {
      PR=Read_Long(R[m]);
      R[m]+=4;
      PC+=2;
  }

Examples:

  LDS    R0,PR                Before execution    R0 = H'12345678, PR = H'00000000
                              After execution      PR = H'12345678
  LDS.L  @R15+,MACL     Before execution   R15 = H'10000000
                              After execution       R15 = H'10000004, MACL = @H'10000000












104 HITACHI
```

## PDF page 109

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=109)

```text
6.29      MAC.L (Multiply and Accumulate Long): Arithmetic Instruction
          (SH-2 CPU)

Format                 Abstract                       Code                   State   T Bit
MAC.L  @Rm+,@Rn+   Signed operation, (Rn) · (Rm)             0000nnnnmmmm1111      3/(2 to       —
                        + MAC ﬁ   MAC                                                   4)


Description: Signed-multiplies 32-bit operands obtained using the contents of general
registers Rm and Rn as addresses. The 64-bit result is added to contents of the MAC register,
and the final result is stored in the MAC register. Every time an operand is read, they
increment Rm and Rn by four.

When the S bit is cleared to 0, the 64-bit result is stored in the coupled MACH and MACL
registers. When bit S is set to 1, addition to the MAC register is a saturation operation at the
48th bit starting from the LSB. For the saturation operation, only the lower 48 bits of the
MACL registers are enabled and the result is limited to a range of H'FFFF800000000000
(minimum) to H'00007FFFFFFFFFFF (maximum).

Operation:

  MACL(long m,long n) /* MAC.L @Rm+,@Rn+*/
  {
       unsigned long RnL,RnH,RmL,RmH,Res0,Res1,Res2;
       unsigned long temp0,templ,temp2,temp3;
       long tempm,tempn,fnLmL;


       tempn=(long)Read_Long(R[n]);
       R[n]+=4;
       tempm=(long)Read_Long(R[m]);
       R[m]+=4;


       if ((long)(tempn^tempm)<0) fnLmL=-1;
       else fnLmL=0;
       if (tempn<0) tempn=0-tempn;
       if (tempm<0) tempm=0-tempm;


       temp1=(unsigned long)tempn;
       temp2=(unsigned long)tempm;






                                                                                          HITACHI  105
```

## PDF page 110

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=110)

```text
      RnL=temp1&0x0000FFFF;
      RnH=(temp1>>16)&0x0000FFFF;
      RmL=temp2&0x0000FFFF;
      RmH=(temp2>>16)&0x0000FFFF;


      temp0=RmL*RnL;
      temp1=RmH*RnL;
      temp2=RmL*RnH;
      temp3=RmH*RnH;


      Res2=0;
      Res1=temp1+temp2;
      if (Res1<temp1) Res2+=0x00010000;


      temp1=(Res1<<16)&0xFFFF0000;
      Res0=temp0+temp1;
      if (Res0<temp0) Res2++;


      Res2=Res2+((Res1>>16)&0x0000FFFF)+temp3;


      if(fnLm<0){
          Res2=~Res2;
          if (Res0==0) Res2++;
          else Res0=(~Res0)+1;
      }
      if(S==1){
          Res0=MACL+Res0;
          if (MACL>Res0) Res2++;
          Res2+=(MACH&0x0000FFFF);


          if(((long)Res2<0)&&(Res2<0xFFFF8000)){
              Res2=0x00008000;
              Res0=0x00000000;
          }
          if(((long)Res2>0)&&(Res2>0x00007FFF)){
              Res2=0x00007FFF;
              Res0=0xFFFFFFFF;
          };


106 HITACHI
```

## PDF page 111

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=111)

```text
           MACH=Res2;
           MACL=Res0;
      }
      else {
           Res0=MACL+Res0;
           if (MACL>Res0) Res2++;
           Res2+=MACH


           MACH=Res2;
           MACL=Res0;
      }
      PC+=2;
  }

Example:

           MOVA      TBLM,R0          Table address
           MOV      R0,R1
           MOVA     TBLN,R0          Table address
           CLRMAC                         MAC register initialization
           MAC.L    @R0+,@R1+
           MAC.L    @R0+,@R1+
           STS      MACL,R0           Store result into R0
           ...............
           .align    2
  TBLM   .data.l   H'1234ABCD
           .data.l   H'5678EF01
  TBLN   .data.l   H'0123ABCD
           .data.l   H'4567DEF0

















                                                                                   HITACHI  107
```

## PDF page 112

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=112)

```text
6.30      MAC (Multiply and Accumulate): Arithmetic Instruction (SH-1
           CPU)

Format                     Abstract                             Code                   State   T Bit
MAC.W                     With signed, (Rn) · (Rm) +              0100nnnnmmmm1111      3/(2)      —
          @Rm+,@Rn        MAC ﬁ   MAC
+


Description: Multiplies 16-bit operands obtained using the contents of general registers Rm
and Rn as addresses. The 32-bit result is added to contents of the MAC register, and the final
result is stored in the MAC register. Every time an operand is read, they increment Rm and
Rn by two.

When the S bit is cleared to 0, the 42-bit result is stored in the coupled MACH and MACL
registers. Bit 9 data is transferred to the upper 22 bits (bits 31 to 10) of the MACH register.

When the S bit is set to 1, addition to the MAC register is a saturation operation. For the
saturation operation, only the MACL register is enabled and the result is limited to a range of
H'80000000 (minimum) to H'7FFFFFFF (maximum).

If an overflow occurs, the LSB of the MACH register is set to 1. The result is stored in the
MACL register, and the result is limited to a value between H'80000000 (minimum) for
overflows in the negative direction and H'7FFFFFFF (maximum) for overflows in the positive
direction.

Note:   The normal number of cycles for execution is 3; however, this instruction can be
         executed in two cycles according to the succeeding instruction.


























108 HITACHI
```

## PDF page 113

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=113)

```text
6.31      MAC.W (Multiply and Accumulate Word): Arithmetic Instruction

Format                      Abstract                             Code                   State   T Bit
MAC.W  @Rm+,@Rn+         Signed operation,                      0100nnnnmmmm1111      3/(2)      —
MAC   @Rm+,@Rn+          (Rn) · (Rm) + MAC ﬁ   MAC

Description: Signed-multiplies 16-bit operands obtained using the contents of general
registers Rm and Rn as addresses. The 32-bit result is added to contents of the MAC register,
and the final result is stored in the MAC register. Every time an operand is read, they
increment Rm and Rn by two.

When the S bit is cleared to 0, the operation is 16 · 16 + 64 ﬁ   64-bit multiply and
accumulate and the 64-bit result is stored in the coupled MACH and MACL registers.

When the S bit is set to 1, the operation is 16 · 16 + 32 ﬁ  32-bit multiply and accumulate
and addition to the MAC register is a saturation operation. For the saturation operation, only
the MACL register is enabled and the result is limited to a range of H'80000000 (minimum)
to H'7FFFFFFF (maximum).

If an overflow occurs, the LSB of the MACH register is set to 1. The result is stored in the
MACL register, and the result is limited to a value between H'80000000 (minimum) for
overflows in the negative direction and H'7FFFFFFF (maximum) for overflows in the positive
direction.

Note: When the S bit is 0, the SH-2 CPU performs a 16 · 16 + 64 ﬁ   64 bit multiply and
accumulate operation and the SH-1 CPU performs a 16 · 16 + 42 ﬁ   42 bit multiply and
accumulate operation.

Operation:

  MACW(long m,long n) /* MAC.W @Rm+,@Rn+*/
  {
       long tempm,tempn,dest,src,ans;
       unsigned long templ;
       tempn=(long)Read_Word(R[n]);
       R[n]+=2;
       tempm=(long)Read_Word(R[m]);
       R[m]+=2;
       templ=MACL;
       tempm=((long)(short)tempn*(long)(short)tempm);






                                                                                              HITACHI  109
```

## PDF page 114

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=114)

```text
      if ((long)MACL>=0) dest=0;
      else dest=1;
      if ((long)tempm>=0 {
           src=0;
           tempn=0;
      }
      else {
           src=1;
           tempn=0xFFFFFFFF;
      }
      src+=dest;
      MACL+=tempm;
      if ((long)MACL>=0) ans=0;
      else ans=1;
      ans+=dest;
      if (S==1) {
           if (ans==1) {
                if (src==0 || src==2)                               For SH-1 CPU (these 2
                                                                        lines
                    MACH|=0x00000001;                                not needed for SH-2 CPU)
                if (src==0) MACL=0x7FFFFFFF;
                if (src==2) MACL=0x80000000;
           }
      }
      else {
           MACH+=tempn;
           if (templ>MACL) MACH+=1;
           if ((MACH&0x00000200)==0)                              For SH-1 CPU (these 3
                                                                        lines
                MACH&=0x000003FF;                                    not needed for SH-2 CPU)
           else MACH|=0xFFFFFC00;
      }
      PC+=2;
  }









110 HITACHI
```

## PDF page 115

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=115)

```text
Example:

           MOVA      TBLM,R0          Table address
           MOV      R0,R1
           MOVA     TBLN,R0          Table address
           CLRMAC                         MAC register initialization
           MAC.W    @R0+,@R1+
           MAC.W    @R0+,@R1+
           STS      MACL,R0           Store result into R0
           ...............
           .align    2
  TBLM   .data.w   H'1234
           .data.w   H'5678
  TBLN   .data.w   H'0123
           .data.w   H'4567









































                                                                                     HITACHI  111
```

## PDF page 116

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=116)

```text
6.32      MOV (Move Data): Data Transfer Instruction

Format                         Abstract                          Code                   State  T Bit
MOV   Rm,Rn                 Rm ﬁ    Rn                         0110nnnnmmmm0011           1         —
MOV.B  Rm,@Rn               Rm  ﬁ   (Rn)                       0010nnnnmmmm0000           1         —
MOV.W  Rm,@Rn               Rm  ﬁ   (Rn)                       0010nnnnmmmm0001           1         —
MOV.L  Rm,@Rn               Rm  ﬁ   (Rn)                       0010nnnnmmmm0010           1         —
MOV.B  @Rm,Rn               (Rm)  ﬁ   sign extension ﬁ   Rn    0110nnnnmmmm0000           1         —
MOV.W  @Rm,Rn               (Rm)  ﬁ   sign extension ﬁ   Rn    0110nnnnmmmm0001           1         —
MOV.L  @Rm,Rn               (Rm)  ﬁ   Rn                       0110nnnnmmmm0010           1         —
MOV.B  Rm,@–Rn              Rn – 1  ﬁ   Rn, Rm ﬁ   (Rn)        0010nnnnmmmm0100           1         —
MOV.W  Rm,@–Rn              Rn – 2  ﬁ   Rn, Rm ﬁ   (Rn)        0010nnnnmmmm0101           1         —
MOV.L  Rm,@–Rn              Rn – 4  ﬁ   Rn, Rm ﬁ   (Rn)        0010nnnnmmmm0110           1         —
MOV.B  @Rm+,Rn              (Rm)  ﬁ   sign extension ﬁ   Rn,   0110nnnnmmmm0100           1         —
                            Rm + 1  ﬁ    Rm
MOV.W  @Rm+,Rn              (Rm)  ﬁ   sign extension ﬁ   Rn,   0110nnnnmmmm0101           1         —
                            Rm + 2  ﬁ    Rm
MOV.L  @Rm+,Rn              (Rm)  ﬁ   Rn, Rm + 4 ﬁ    Rm       0110nnnnmmmm0110           1         —
MOV.B  Rm,@(R0,Rn)          Rm  ﬁ   (R0 + Rn)                  0000nnnnmmmm0100           1         —
MOV.W  Rm,@(R0,Rn)          Rm  ﬁ   (R0 + Rn)                  0000nnnnmmmm0101           1         —
MOV.L  Rm,@(R0,Rn)          Rm  ﬁ   (R0 + Rn)                  0000nnnnmmmm0110           1         —
MOV.B  @(R0,Rm),Rn          (R0 + Rm)  ﬁ   sign extension      0000nnnnmmmm1100           1         —
                            ﬁ   Rn                             0000nnnnmmmm1101           1         —
MOV.W  @(R0,Rm),Rn          (R0 + Rm)  ﬁ   sign extension      0000nnnnmmmm1110           1         —
                            ﬁ   Rn
MOV.L  @(R0,Rm),Rn          (R0 + Rm)  ﬁ   Rn


Description: Transfers the source operand to the destination. When the operand is stored in
memory, the transferred data can be a byte, word, or longword. When the source operand is in
memory, loaded data from memory is stored in a register after it is sign-extended to a
longword.

Operation:

  MOV(long m,long n)     /* MOV Rm,Rn */
  {
       R[n]=R[m];
       PC+=2;
  }




112 HITACHI
```

## PDF page 117

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=117)

```text
MOVBS(long m,long n)    /* MOV.B Rm,@Rn */
{
    Write_Byte(R[n],R[m]);
    PC+=2;
}

MOVWS(long m,long n)    /* MOV.W Rm,@Rn */
{
    Write_Word(R[n],R[m]);
    PC+=2;
}

MOVLS(long m,long n)    /* MOV.L Rm,@Rn */
{
    Write_Long(R[n],R[m]);
    PC+=2;
}

MOVBL(long m,long n)    /* MOV.B @Rm,Rn */
{
    R[n]=(long)Read_Byte(R[m]);
    if ((R[n]&0x80)==0) R[n]&0x000000FF;
    else R[n]|=0xFFFFFF00;
    PC+=2;
}

MOVWL(long m,long n)    /* MOV.W @Rm,Rn */
{
    R[n]=(long)Read_Word(R[m]);
    if ((R[n]&0x8000)==0) R[n]&0x0000FFFF;
    else R[n]|=0xFFFF0000;
    PC+=2;
}

MOVLL(long m,long n)    /* MOV.L @Rm,Rn */
{
    R[n]=Read_Long(R[m]);
    PC+=2;
}





                                                                             HITACHI  113
```

## PDF page 118

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=118)

```text
  MOVBM(long m,long n)    /* MOV.B Rm,@–Rn */
  {
      Write_Byte(R[n]–1,R[m]);
      R[n]–=1;
      PC+=2;
  }

  MOVWM(long m,long n)    /* MOV.W Rm,@–Rn */
  {
      Write_Word(R[n]–2,R[m]);
      R[n]–=2;
      PC+=2;
  }

  MOVLM(long m,long n)    /* MOV.L Rm,@–Rn */
  {
      Write_Long(R[n]–4,R[m]);
      R[n]–=4;
      PC+=2;
  }

  MOVBP(long m,long n)/* MOV.B @Rm+,Rn */
  {
      R[n]=(long)Read_Byte(R[m]);
      if ((R[n]&0x80)==0) R[n]&0x000000FF;
      else R[n]|=0xFFFFFF00;
      if (n!=m) R[m]+=1;
      PC+=2;
  }

  MOVWP(long m,long n)    /* MOV.W @Rm+,Rn */
  {
      R[n]=(long)Read_Word(R[m]);
      if ((R[n]&0x8000)==0) R[n]&0x0000FFFF;
      else R[n]|=0xFFFF0000;
      if (n!=m) R[m]+=2;
      PC+=2;
  }






114 HITACHI
```

## PDF page 119

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=119)

```text
MOVLP(long m,long n)    /* MOV.L @Rm+,Rn */
{
    R[n]=Read_Long(R[m]);
    if (n!=m) R[m]+=4;
    PC+=2;
}

MOVBS0(long m,long n)    /* MOV.B Rm,@(R0,Rn) */
{
    Write_Byte(R[n]+R[0],R[m]);
    PC+=2;
}

MOVWS0(long m,long n)    /* MOV.W Rm,@(R0,Rn) */
{
    Write_Word(R[n]+R[0],R[m]);
    PC+=2;
}

MOVLS0(long m,long n)   /* MOV.L Rm,@(R0,Rn) */
{
    Write_Long(R[n]+R[0],R[m]);
    PC+=2;
}

MOVBL0(long m,long n)   /* MOV.B @(R0,Rm),Rn */
{
    R[n]=(long)Read_Byte(R[m]+R[0]);
    if ((R[n]&0x80)==0) R[n]&0x000000FF;
    else R[n]|=0xFFFFFF00;
    PC+=2;
}

MOVWL0(long m,long n)   /* MOV.W @(R0,Rm),Rn */
{
    R[n]=(long)Read_Word(R[m]+R[0]);
    if ((R[n]&0x8000)==0) R[n]&0x0000FFFF;
    else R[n]|=0xFFFF0000;
    PC+=2;
}



                                                                            HITACHI  115
```

## PDF page 120

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=120)

```text
  MOVLL0(long m,long n)   /* MOV.L @(R0,Rm),Rn */
  {
       R[n]=Read_Long(R[m]+R[0]);
       PC+=2;
  }

Example:

  MOV    R0,R1           Before execution  R0 = H'FFFFFFFF, R1 = H'00000000
                             After execution    R1 = H'FFFFFFFF

  MOV.W  R0,@R1         Before execution  R0 = H'FFFF7F80
                             After execution    @R1 = H'7F80

  MOV.B  @R0,R1           Before execution   @R0 = H'80, R1 = H'00000000
                             After execution    R1 = H'FFFFFF80

  MOV.W  R0,@–R1         Before execution   R0 = H'AAAAAAAA, R1 = H'FFFF7F80
                             After execution     R1 = H'FFFF7F7E, @R1 = H'AAAA

  MOV.L  @R0+,R1         Before execution   R0 = H'12345670
                             After execution     R0 = H'12345674, R1 = @H'12345670

  MOV.B  R1,@(R0,R2)  Before execution   R2 = H'00000004, R0 = H'10000000
                             After execution     R1 = @H'10000004

  MOV.W  @(R0,R2),R1  Before execution   R2 = H'00000004, R0 = H'10000000
                             After execution     R1 = @H'10000004


























116 HITACHI
```

## PDF page 121

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=121)

```text
6.33      MOV (Move Immediate Data): Data Transfer Instruction

Format                              Abstract                       Code                 State   T Bit
MOV   #imm,Rn                    imm ﬁ   sign extension ﬁ          1110nnnniiiiiiii           1         —
                                 Rn                                1001nnnndddddddd           1         —
MOV.W  @(disp,PC),Rn             (disp · 2 + PC) ﬁ   sign
                                 extension  ﬁ   Rn                 1101nnnndddddddd           1         —
MOV.L  @(disp,PC),Rn             (disp · 4 + PC) ﬁ    Rn


Description: Stores immediate data, which has been sign-extended to a longword, into
general register Rn.

If the data is a word or longword, table data stored in the address specified by PC +
displacement is accessed. If the data is a word, the 8-bit displacement is zero-extended and
doubled. Consequently, the relative interval from the table is up to PC + 510 bytes. The PC
points to the starting address of the second instruction after this MOV instruction. If the data
is a longword, the 8-bit displacement is zero-extended and quadrupled. Consequently, the
relative interval from the table is up to PC + 1020 bytes. The PC points to the starting address
of the second instruction after this MOV instruction, but the lowest two bits of the PC are
corrected to B’00.

Note: The end address of the program area (module) or the second address after an
unconditional branch instruction are suitable for the start address of the table. If suitable table
assignment is impossible (for example, if there are no unconditional branch instructions
within the area specified by PC + 510 bytes or PC + 1020 bytes), the BRA instruction must
be used to jump past the table. When this MOV instruction is placed immediately after a
delayed branch instruction, the PC points to an address specified by (the starting address of
the branch destination) + 2.

Operation:

  MOVI(long i,long n)     /* MOV #imm,Rn */
  {
       if ((i&0x80)==0) R[n]=(0x000000FF & (long)i);
       else R[n]=(0xFFFFFF00 | (long)i);
       PC+=2;
  }

  MOVWI(long d,long n)    /* MOV.W @(disp,PC),Rn */
  {
       long disp;





                                                                                                HITACHI  117
```

## PDF page 122

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=122)

```text
      disp=(0x000000FF & (long)d);
      R[n]=(long)Read_Word(PC+(disp<<1));
      if ((R[n]&0x8000)==0) R[n]&=0x0000FFFF;
      else R[n]|=0xFFFF0000;
      PC+=2;
  }

 MOVLI(long d,long n)    /* MOV.L @(disp,PC),Rn */
 {
      long disp;


      disp=(0x000000FF & (long)d);
      R[n]=Read_Long((PC&0xFFFFFFFC)+(disp<<2));
      PC+=2;
  }

Example:

  Address
  1000       MOV      #H'80,R1       R1 = H'FFFFFF80
  1002  MOV.W IMM,R2           R2 = H'FFFF9ABC, IMM means @(H'08,PC)
  1004      ADD     #–1,R0
  1006      TST      R0,R0               ‹  PC location used for address calculation for
                                              the MOV.W instruction
  1008      MOVT     R13
  100A      BRA      NEXT              Delayed branch instruction
  100C      MOV.L    @(4,PC),R3      R3 = H'12345678
  100E IMM   .data.w  H'9ABC
  1010       .data.w  H'1234
  1012 NEXT  JMP      @R3                Branch destination of the BRA instruction
  1014      CMP/EQ   #0,R0                 ‹  PC location used for address calculation for
                                              the MOV.L instruction
                .align   4
  1018      .data.l   H'12345678












118 HITACHI
```

## PDF page 123

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=123)

```text
6.34      MOV (Move Peripheral Data): Data Transfer Instruction

Format                               Abstract                   Code                   State   T Bit
MOV.B  @(disp,GBR),R0             (disp + GBR) ﬁ   sign        11000100dddddddd              1         —
                                  extension  ﬁ   R0
MOV.W  @(disp,GBR),R0             (disp  · 2  + GBR) ﬁ         11000101dddddddd              1         —
                                  sign extension ﬁ    R0
MOV.L  @(disp,GBR),R0             (disp  · 4+ GBR) ﬁ   R0      11000110dddddddd              1         —
MOV.B  R0,@(disp,GBR)             R0  ﬁ   (disp + GBR)         11000000dddddddd              1         —
MOV.W  R0,@(disp,GBR)             R0  ﬁ   (disp  · 2 +         11000001dddddddd              1         —
MOV.L  R0,@(disp,GBR)             GBR)                         11000010dddddddd              1         —
                                  R0  ﬁ   (disp  · 4 +
                                  GBR)


Description: Transfers the source operand to the destination. This instruction is suitable for
accessing data in the peripheral module area. The data can be a byte, word, or longword, but
the register is fixed to R0.

A peripheral module base address is set to the GBR. When the peripheral module data is a
byte, the 8-bit displacement is zero-extended. Consequently, an address within +255 bytes
can be specified. When the peripheral module data is a word, the 8-bit displacement is zero-
extended and doubled. Consequently, an address within +510 bytes can be specified. When
the peripheral module data is a longword, the 8-bit displacement is zero-extended and is
quadrupled. Consequently, an address within +1020 bytes can be specified. If the
displacement is too short to reach the memory operand, the above @(R0,Rn) mode must be
used after the GBR data is transferred to a general register. When the source operand is in
memory, the loaded data is stored in the register after it is sign-extended to a longword.

Note: The destination register of a data load is always R0. R0 cannot be accessed by the next
instruction until the load instruction is finished. Changing the instruction order shown in figure
6.1 will give better results.


                    MOV.B      @(12, GBR), R0                 MOV.B      @(12, GBR), R0
                    AND        #80, R0                        ADD        #20, R1
                    ADD        #20, R1                        AND        #80, R0

                                   Figure 6.1   Using R0 after MOV










                                                                                               HITACHI  119
```

## PDF page 124

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=124)

```text
Operation:

 MOVBLG(long d)   /* MOV.B @(disp,GBR),R0 */
 {
      long disp;


      disp=(0x000000FF & (long)d);
      R[0]=(long)Read_Byte(GBR+disp);
      if ((R[0]&0x80)==0) R[0]&=0x000000FF;
      else R[0]|=0xFFFFFF00;
      PC+=2;
  }

 MOVWLG(long d)   /* MOV.W @(disp,GBR),R0 */
 {
      long disp;


      disp=(0x000000FF & (long)d);
      R[0]=(long)Read_Word(GBR+(disp<<1));
      if ((R[0]&0x8000)==0) R[0]&=0x0000FFFF;
      else R[0]|=0xFFFF0000;
      PC+=2;
  }

 MOVLLG(long d)   /* MOV.L @(disp,GBR),R0 */
 {
      long disp;


      disp=(0x000000FF & (long)d);
      R[0]=Read_Long(GBR+(disp<<2));
      PC+=2;
  }

 MOVBSG(long d)   /* MOV.B R0,@(disp,GBR) */
 {
      long disp;









120 HITACHI
```

## PDF page 125

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=125)

```text
      disp=(0x000000FF & (long)d);
      Write_Byte(GBR+disp,R[0]);
      PC+=2;
  }

 MOVWSG(long d)   /* MOV.W R0,@(disp,GBR) */
 {
      long disp;


      disp=(0x000000FF & (long)d);
      Write_Word(GBR+(disp<<1),R[0]);
      PC+=2;
  }

 MOVLSG(long d)   /* MOV.L R0,@(disp,GBR) */
 {
      long disp;


      disp=(0x000000FF & (long)d);
      Write_Long(GBR+(disp<<2),R[0]);
      PC+=2;
  }

Examples:

  MOV.L  @(2,GBR),R0      Before execution      @(GBR + 8) = H'12345670
                              After execution         R0 = @H'12345670

  MOV.B  R0,@(1,GBR)      Before execution     R0 = H'FFFF7F80
                              After execution       @(GBR + 1) = H'FFFF7F80




















                                                                                   HITACHI  121
```

## PDF page 126

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=126)

```text
6.35      MOV (Move Structure Data): Data Transfer Instruction

Format                              Abstract                   Code                   State   T Bit
MOV.B  R0,@(disp,Rn)             R0 ﬁ   (disp + Rn)            10000000nnnndddd              1         —
MOV.W  R0,@(disp,Rn)             R0  ﬁ   (disp  · 2 + Rn)      10000001nnnndddd              1         —
MOV.L  Rm,@(disp,Rn)             Rm  ﬁ   (disp  · 4 + Rn)      0001nnnnmmmmdddd              1         —
MOV.B  @(disp,Rm),R0             (disp + Rm) ﬁ   sign          10000100mmmmdddd              1         —
                                 extension  ﬁ   R0
MOV.W  @(disp,Rm),R0             (disp · 2 + Rm) ﬁ   sign      10000101mmmmdddd              1         —
                                 extension  ﬁ   R0
MOV.L  @(disp,Rm),Rn             (disp · 4 + Rm) ﬁ    Rn       0101nnnnmmmmdddd              1         —


Description: Transfers the source operand to the destination. This instruction is suitable for
accessing data in a structure or a stack. The data can be a byte, word, or longword, but when
a byte or word is selected, only the R0 register is fixed. When the data is a byte, the 4-bit
displacement is zero-extend. Consequently, an address within +15 bytes can be specified.
When the data is a word, the 4-bit displacement is zero-extended and doubled. Consequently,
an address within +30 bytes can be specified. When the data is a longword, the 4-bit
displacement is zero-extended and quadrupled. Consequently, an address within +60 bytes
can be specified. If the displacement is too short to reach the memory operand, the
aforementioned @(R0,Rn) mode must be used. When the source operand is in memory, the
loaded data is stored in the register after it is sign-extended to a longword.

Note: When byte or word data is loaded, the destination register is always R0. R0 cannot be
accessed by the next instruction until the load instruction is finished. Changing the instruction
order in figure 6.2 will give better results.


                      MOV.B      @(2, R1), R0                   MOV.B     @(2, R1), R0
                      AND        #80, R0                        ADD       #20, R1
                      ADD        #20, R1                        AND       #80, R0

                                   Figure 6.2   Using R0 after MOV
















122 HITACHI
```

## PDF page 127

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=127)

```text
Operation:

 MOVBS4(long d,long n)   /* MOV.B R0,@(disp,Rn) */
 {
      long disp;


      disp=(0x0000000F & (long)d);
      Write_Byte(R[n]+disp,R[0]);
      PC+=2;
  }

 MOVWS4(long d,long n)   /* MOV.W R0,@(disp,Rn) */
 {
      long disp;


      disp=(0x0000000F & (long)d);
      Write_Word(R[n]+(disp<<1),R[0]);
      PC+=2;
  }

 MOVLS4(long m,long d,long n)
      /* MOV.L Rm,@(disp,Rn) */
 {
      long disp;


      disp=(0x0000000F & (long)d);
      Write_Long(R[n]+(disp<<2),R[m]);
      PC+=2;
  }

  MOVBL4(long m,long d)  /* MOV.B @(disp,Rm),R0 */
 {
      long disp;


      disp=(0x0000000F & (long)d);
      R[0]=Read_Byte(R[m]+disp);
      if ((R[0]&0x80)==0) R[0]&=0x000000FF;
      else R[0]|=0xFFFFFF00;
      PC+=2;
  }


                                                                              HITACHI  123
```

## PDF page 128

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=128)

```text
  MOVWL4(long m,long d)  /* MOV.W @(disp,Rm),R0 */
 {
      long disp;


      disp=(0x0000000F & (long)d);
      R[0]=Read_Word(R[m]+(disp<<1));
      if ((R[0]&0x8000)==0) R[0]&=0x0000FFFF;
      else R[0]|=0xFFFF0000;
      PC+=2;
  }

  MOVLL4(long m,long d,long n)
      /* MOV.L @(disp,Rm),Rn */
 {
      long disp;


      disp=(0x0000000F & (long)d);
      R[n]=Read_Long(R[m]+(disp<<2));
      PC+=2;
  }

Examples:

  MOV.L  @(2,R0),R1        Before execution @(R0 + 8) = H'12345670
                              After execution R1 = @H'12345670

  MOV.L  R0,@(H'F,R1)    Before execution R0 = H'FFFF7F80
                              After execution @(R1 + 60) = H'FFFF7F80






















124 HITACHI
```

## PDF page 129

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=129)

```text
6.36      MOVA (Move Effective Address): Data Transfer Instruction

Format                               Abstract                   Code                   State   T Bit
MOVA  @(disp,PC),R0     disp  · 4 + PC ﬁ   R0    11000111dddddddd 1—


Description: Stores the effective address of the source operand into general register R0. The
8-bit displacement is zero-extended and quadrupled. Consequently, the relative interval from
the operand is PC + 1020 bytes. The PC points to the starting address of the second
instruction after this MOVA instruction, but the lowest two bits of the PC are corrected to
B’00.

Note: If this instruction is placed immediately after a delayed branch instruction, the PC must
point to an address specified by (the starting address of the branch destination) + 2.

Operation:

  MOVA(long d)  /* MOVA @(disp,PC),R0 */
  {
       long disp;


       disp=(0x000000FF & (long)d);
       R[0]=(PC&0xFFFFFFFC)+(disp<<2);
       PC+=2;
  }

Example:

  Address     .org    H'1006
  1006         MOVA    STR,R0            Address of STR ﬁ   R0
  1008        MOV.B   @R0,R1         R1 = “X”        ‹  PC location after correcting
                                               the lowest  two bits
  100A        ADD     R4,R5             ‹  Original PC location for address calculation for
                                                  the MOVA instruction
                    .align  4
  100C   STR:  .sdata    “XYZP12”
  ...............
  2002        BRA     TRGET          Delayed branch instruction
  2004       MOVA    @(0,PC),R0    Address of TRGET + 2 ﬁ   R0
  2006         NOP






                                                                                            HITACHI  125
```

## PDF page 130

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=130)

```text
6.37      MOVT (Move T Bit): Data Transfer Instruction

Format                    Abstract                            Code                   State   T Bit
MOVT  Rn            T ﬁ   Rn                   0000nnnn00101001 1—


Description: Stores the T bit value into general register Rn. When T = 1, 1 is stored in Rn,
and when T = 0, 0 is stored in Rn.

Operation:

  MOVT(long n)  /* MOVT Rn */
  {
       R[n]=(0x00000001 & SR);
       PC+=2;
  }

Example:

  XOR     R2,R2    R2 = 0
  CMP/PZ R2          T = 1
  MOVT   R0         R0 = 1
  CLRT                   T = 0
  MOVT   R1         R1 = 0






























126 HITACHI
```

## PDF page 131

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=131)

```text
6.38      MUL.L (Multiply Long): Arithmetic Instruction (SH-2 CPU)

Format                    Abstract                           Code                   State   T Bit
MUL.L  Rm,Rn        Rn · Rm ﬁ   MACL          0000nnnnmmmm0111      2 to 4    —


Description: Performs 32-bit multiplication of the contents of general registers Rn and Rm,
and stores the lower 32 bits of the result in the MACL register. The MACH register data does
not change.

Operation:

  MULL(long m,long n) /* MUL.L Rm,Rn */
  {
       MACL=R[n]*R[m];
       PC+=2;
  }

Example:

  MUL.L  R0,R1       Before execution    R0 = H'FFFFFFFE, R1 = H'00005555
                          After execution      MACL = H'FFFF5556
  STS                    MACL,R0            Operation result
































                                                                                            HITACHI  127
```

## PDF page 132

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=132)

```text
6.39      MULS.W (Multiply as Signed Word): Arithmetic Instruction

Format                     Abstract                            Code                   State   T Bit
MULS.W Rm,Rn            Signed operation, Rn · Rm ﬁ          0010nnnnmmmm1111      1 to 3    —
MULS  Rm,Rn             MACL


Description: Performs 16-bit multiplication of the contents of general registers Rn and Rm,
and stores the 32-bit result in the MACL register. The operation is signed and the MACH
register data does not change.

Operation:

  MULS(long m,long n) /* MULS Rm,Rn */
  {
       MACL=((long)(short)R[n]*(long)(short)R[m]);
       PC+=2;
  }

Example:

  MULS R0,R1       Before execution    R0 = H'FFFFFFFE, R1 = H'00005555
                       After execution      MACL = H'FFFF5556
  STS                 MACL,R0            Operation result































128 HITACHI
```

## PDF page 133

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=133)

```text
6.40      MULU.W (Multiply as Unsigned Word): Arithmetic Instruction

Format                    Abstract                           Code                   State   T Bit
MULU.W Rm,Rn            Unsigned, Rn · Rm ﬁ   MAC    0010nnnnmmmm1110      1 to 3    —
MULU  Rm,Rn


Description: Performs 16-bit multiplication of the contents of general registers Rn and Rm,
and stores the 32-bit result in the MACL register. The operation is unsigned and the MACH
register data does not change.

Operation:

  MULU(long m,long n) /* MULU Rm,Rn */
  {
       MACL=((unsigned long)(unsigned short)R[n]
            *(unsigned long)(unsigned short)R[m]);
       PC+=2;
  }

Example:

  MULU  R0,R1               Before execution       R0 = H'00000002, R1 = H'FFFFAAAA
                          After execution          MACL = H'00015554
  STSMACL,R0          Operation result





























                                                                                           HITACHI  129
```

## PDF page 134

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=134)

```text
6.41      NEG (Negate): Arithmetic Instruction

Format                     Abstract                             Code                   State   T Bit
NEG   Rm,Rn            0 – Rm ﬁ   Rn               0110nnnnmmmm1011 1—


Description: Takes the two’s complement of data in general register Rm, and stores the
result in Rn. This effectively subtracts Rm data from 0, and stores the result in Rn.

Operation:

  NEG(long m,long n)  /* NEG Rm,Rn */
  {
       R[n]=0-R[m];
       PC+=2;
  }

Example:

  NEG  R0,R1      Before execution          R0 = H'00000001
                     After execution         R1 = H'FFFFFFFF



































130 HITACHI
```

## PDF page 135

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=135)

```text
6.42      NEGC (Negate with Carry): Arithmetic Instruction

Format                     Abstract                             Code                  State   T Bit
NEGC  Rm,Rn            0 – Rm – T ﬁ   Rn, Borrow ﬁ   T  0110nnnnmmmm1010    1            Borrow


Description: Subtracts general register Rm data and the T bit from 0, and stores the result in
Rn. If a borrow is generated, T bit changes accordingly. This instruction is used for inverting
the sign of a value that has more than 32 bits.

Operation:

  NEGC(long m,long n) /* NEGC Rm,Rn */
  {
       unsigned long temp;


       temp=0-R[m];
       R[n]=temp-T;
       if (0<temp)  T=1;
       else T=0;
       if (temp<R[n])   T=1;
       PC+=2;
  }

Examples:

  CLRT                 Sign inversion of R1 and R0 (64 bits)
  NEGC   R1,R1     Before execution    R1 = H'00000001, T = 0
                       After execution      R1 = H'FFFFFFFF, T = 1
  NEGC  R0,R0     Before execution    R0 = H'00000000, T = 1
                       After execution      R0 = H'FFFFFFFF, T = 1



















                                                                                              HITACHI  131
```

## PDF page 136

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=136)

```text
6.43      NOP (No Operation): System Control Instruction

Format                    Abstract                           Code                   State   T Bit
NOP                       No operation             0000000000001001 1—


Description: Increments the PC to execute the next instruction.

Operation:

  NOP()  /* NOP */
  {
       PC+=2;
  }

Example:

  NOP   Executes in one cycle








































132 HITACHI
```

## PDF page 137

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=137)

```text
6.44      NOT (NOT—Logical Complement): Logic Operation Instruction

Format                     Abstract                            Code                   State   T Bit
NOT Rm,Rn                      ~Rm  ﬁ    Rn                 0110nnnnmmmm0111 1—


Description: Takes the one’s complement of general register Rm data, and stores the result
in Rn. This effectively inverts each bit of Rm data and stores the result in Rn.

Operation:

  NOT(long m,long n)  /* NOT Rm,Rn */
  {
       R[n]=~R[m];
       PC+=2;
  }

Example:

  NOT    R0,R1   Before execution  R0 = H'AAAAAAAA
                      After execution     R1 = H'55555555



































                                                                                          HITACHI  133
```

## PDF page 138

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=138)

```text
6.45      OR (OR Logical) Logic Operation Instruction

Format                            Abstract                         Code                 State   T Bit
OR    Rm,Rn                 Rn | Rm ﬁ    Rn                  0010nnnnmmmm1011         1        —
OR    #imm,R0               R0 | imm ﬁ    R0                 11001011iiiiiiii         1        —
OR.B                        (R0 + GBR) | imm ﬁ   (R0 +       11001111iiiiiiii         3        —
         #imm,@(R0,GB       GBR)
R)


Description: Logically ORs the contents of general registers Rn and Rm, and stores the
result in Rn. The contents of general register R0 can also be ORed with zero-extended 8-bit
immediate data, or 8-bit memory data accessed by using indirect indexed GBR addressing
can be ORed with 8-bit immediate data.

Operation:

  OR(long m,long n)   /* OR Rm,Rn */
  {
       R[n]|=R[m];
       PC+=2;
  }

  ORI(long i)   /* OR #imm,R0 */
  {
       R[0]|=(0x000000FF & (long)i);
       PC+=2;
  }

  ORM(long i)   /* OR.B #imm,@(R0,GBR) */
  {
       long temp;


       temp=(long)Read_Byte(GBR+R[0]);
       temp|=(0x000000FF & (long)i);
       Write_Byte(GBR+R[0],temp);
       PC+=2;
  }








134 HITACHI
```

## PDF page 139

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=139)

```text
Examples:

  OR    R0,R1                      Before execution       R0 = H'AAAA5555, R1 = H'55550000
                                     After execution    R1 = H'FFFF5555

  OR    #H'F0,R0                Before execution       R0 = H'00000008
                                     After execution    R0 = H'000000F8

  OR.B   #H'50,@(R0,GBR)   Before execution      @(R0,GBR) = H'A5
                                     After execution    @(R0,GBR) = H'F5



















































                                                                                            HITACHI  135
```

## PDF page 140

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=140)

```text
6.46      ROTCL (Rotate with Carry Left): Shift Instruction

Format                     Abstract                            Code                   State   T Bit
ROTCL  Rn            T ‹   Rn ‹   T                0100nnnn00100100     1         MSB


Description: Rotates the contents of general register Rn and the T bit to the left by one bit,
and stores the result in Rn. The bit that is shifted out of the operand is transferred to the T bit
(figure 6.3).


                                              MSB                              LSB
                      ROTCL               T


                                Figure 6.3   Rotate with Carry Left

Operation:

  ROTCL(long n)/* ROTCL Rn */
  {
       long temp;


       if ((R[n]&0x80000000)==0) temp=0;
       else temp=1;
       R[n]<<=1;
       if (T==1) R[n]|=0x00000001;
       else R[n]&=0xFFFFFFFE;
       if (temp==1) T=1;
       else T=0;
       PC+=2;
  }

Example:

  ROTCL  R0        Before execution        R0 = H'80000000, T = 0
                     After execution          R0 = H'00000000, T = 1












136 HITACHI
```

## PDF page 141

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=141)

```text
6.47      ROTCR (Rotate with Carry Right): Shift Instruction

Format                    Abstract                           Code                   State   T Bit
ROTCR  Rn            T ﬁ   Rn ﬁ   T                0100nnnn00100101      1          LSB


Description: Rotates the contents of general register Rn and the T bit to the  right by one bit,
and stores the result in Rn. The bit that is shifted out of the operand is transferred to the T bit
(figure 6.4).


                                     MSB                              LSB
                      ROTCR                                                   T


                                Figure 6.4   Rotate with Carry Right

Operation:

  ROTCR(long n)/* ROTCR Rn */
  {
       long temp;


       if ((R[n]&0x00000001)==0) temp=0;
       else temp=1;
       R[n]>>=1;
       if (T==1) R[n]|=0x80000000;
       else R[n]&=0x7FFFFFFF;
       if (temp==1) T=1;
       else T=0;
       PC+=2;
  }

Examples:

  ROTCR  R0      Before execution       R0 = H'00000001, T = 1
                    After execution          R0 = H'80000000, T = 1












                                                                                            HITACHI  137
```

## PDF page 142

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=142)

```text
6.48      ROTL (Rotate Left): Shift Instruction

Format                    Abstract                            Code                   State   T Bit
ROTL  Rn            T ‹   Rn ‹   MSB           0100nnnn00000100    1       MSB


Description: Rotates the contents of general register Rn to the left by one bit, and stores the
result in Rn (figure 6.5). The bit that is shifted out of the operand is transferred to the T bit.


                                              MSB                              LSB
                      ROTL           T


                                        Figure 6.5   Rotate Left

Operation:

  ROTL(long n) /* ROTL Rn */
  {
       if ((R[n]&0x80000000)==0) T=0;
       else T=1;
       R[n]<<=1;
       if (T==1) R[n]|=0x00000001;
       else R[n]&=0xFFFFFFFE;
       PC+=2;
  }

Examples:

  ROTL   R0        Before execution      R0 = H'80000000, T = 0
                      After execution        R0 = H'00000001, T = 1




















138 HITACHI
```

## PDF page 143

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=143)

```text
6.49      ROTR (Rotate Right): Shift Instruction

Format                    Abstract                           Code                   State   T Bit
ROTR  Rn            LSB ﬁ   Rn ﬁ   T             0100nnnn00000101      1          LSB


Description: Rotates the contents of general register Rn to the right by one bit, and stores the
result in Rn (figure 6.6). The bit that is shifted out of the operand is transferred to the T bit.


                                    MSB                               LSB
                       ROTR                                                       T


                                       Figure 6.6   Rotate Right

Operation:

  ROTR(long n) /* ROTR Rn */
  {
       if ((R[n]&0x00000001)==0) T=0;
       else T=1;
       R[n]>>=1;
       if (T==1) R[n]|=0x80000000;
       else R[n]&=0x7FFFFFFF;
       PC+=2;
  }

Examples:

  ROTR   R0        Before execution         R0 = H'00000001, T = 0
                      After execution           R0 = H'80000000, T = 1




















                                                                                             HITACHI  139
```

## PDF page 144

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=144)

```text
6.50      RTE (Return from Exception): System Control Instruction

Class: Delayed branch instruction

Format                 Abstract                        Code                   State   T Bit
RTE                              Stack area ﬁ   PC/SR         0000000000101011      4          LSB


Description: Returns from an interrupt routine. The PC and SR values are restored from the
stack, and the program continues from the address specified by the restored PC value.

Note: Since this is a delayed branch instruction, the instruction after this RTE is executed
before branching. No address errors and interrupts are accepted between this instruction and
the next instruction. If the next instruction is a branch instruction, it is acknowledged as an
illegal slot instruction.

Operation:

  RTE()  /* RTE */
  {
       unsigned long temp;


       temp=PC;
       PC=Read_Long(R[15])+4;
       R[15]+=4;
       SR=Read_Long(R[15])&0x000003F3;
       R[15]+=4;
       Delay_Slot(temp+2);
  }

Example:

  RTE                    Returns to the original routine
  ADD#8,R14          Executes ADD before branching

Note:   With delayed branching, branching occurs after execution of the slot instruction.
         However, instructions such as register changes etc. are executed in the order of
         delayed branch instruction, then delay slot instruction. For example, even if the
         register in which the branch destination address has been loaded is changed by the
         delay slot instruction, the branch will still be made using the value of the register
         prior to the change as the branch destination address.






140 HITACHI
```

## PDF page 145

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=145)

```text
6.51      RTS (Return from Subroutine): Branch Instruction

Class: Delayed branch instruction

Format                 Abstract                        Code                   State   T Bit
RTS                        PR ﬁ   PC                  0000000000001011 2—

Description: Returns from a subroutine procedure. The PC values are restored from the PR,
and the program continues from the address specified by the restored PC value. This
instruction is used to return to the program from a subroutine program called by a BSR or JSR
instruction.
Note: Since this is a delayed branch instruction, the instruction after this RTS is executed
before branching. No address errors and interrupts are accepted between this instruction and
the next instruction. If the next instruction is a branch instruction, it is acknowledged as an
illegal slot instruction.
Operation:
  RTS()  /* RTS */
  {
       unsigned long temp;

       temp=PC;
       PC=PR+4;
       Delay_Slot(temp+2);
  }

Example:
              MOV.L   TABLE,R3           R3 = Address of TRGET
              JSR     @R3                 Branches to TRGET
              NOP                             Executes NOP before JSR
              ADD     R0,R1                 ‹  Return address for when the subroutine procedure is
                                               completed (PR data)
     .............
  TABLE:  .data.l TRGET            Jump table
     .............
  TRGET:  MOV     R1,R0                        ‹   Procedure entrance
              RTS                                PR data ﬁ   PC
              MOV                              #12,R0      Executes MOV before branching

Note:   With delayed branching, branching occurs after execution of the slot instruction.
         However, instructions such as register changes etc. are executed in the order of
         delayed branch instruction, then delay slot instruction. For example, even if the
         register in which the branch destination address has been loaded is changed by the





                                                                                                 HITACHI  141
```

## PDF page 146

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=146)

```text
         delay slot instruction, the branch will still be made using the value of the register
         prior to the change as the branch destination address.






























































142 HITACHI
```

## PDF page 147

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=147)

```text
6.52      SETT (Set T Bit): System Control Instruction

Format                     Abstract                            Code                   State   T Bit
SETT                      1 ﬁ   T                    0000000000011000 11


Description: Sets the T bit to 1.

Operation:

  SETT() /* SETT */
  {
       T=1;
       PC+=2;
  }

Example:

  SETT     Before execution   T = 0
            After execution     T = 1





































                                                                                          HITACHI  143
```

## PDF page 148

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=148)

```text
6.53      SHAL (Shift Arithmetic Left): Shift Instruction

Format                     Abstract                            Code                   State   T Bit
SHAL  Rn            T ‹   Rn ‹    0                0100nnnn00100000     1         MSB


Description: Arithmetically shifts the contents of general register Rn to the left by one bit,
and stores the result in Rn. The bit that is shifted out of the operand is transferred to the T bit
(figure 6.7).


                                      MSB                            LSB
                      SHAL       T0

                               Figure 6.7   Shift Arithmetic Left

Operation:

  SHAL(long n) /* SHAL Rn(Same as SHLL) */
  {
      if ((R[n]&0x80000000)==0) T=0;
      else T=1;
      R[n]<<=1;
      PC+=2;
  }

Example:

  SHAL   R0        Before execution    R0 = H'80000001, T = 0
                    After execution      R0 = H'00000002, T = 1






















144 HITACHI
```

## PDF page 149

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=149)

```text
6.54      SHAR (Shift Arithmetic Right): Shift Instruction

Format                    Abstract                           Code                   State   T Bit
SHAR  Rn            MSB ﬁ   Rn ﬁ   T             0100nnnn00100001      1          LSB


Description: Arithmetically shifts the contents of general register Rn to the right by one bit,
and stores the result in Rn. The bit that is shifted out of the operand is transferred to the T bit
(figure 6.8).


                                      MSB                              LSB
                         SHAR                                                  T


                                Figure 6.8   Shift Arithmetic Right

Operation:

  SHAR(long n) /* SHAR Rn */
  {
       long temp;


       if ((R[n]&0x00000001)==0) T=0;
       else T=1;
       if ((R[n]&0x80000000)==0) temp=0;
       else temp=1;
       R[n]>>=1;
       if (temp==1) R[n]|=0x80000000;
       else R[n]&=0x7FFFFFFF;
       PC+=2;
  }

Example:

  SHAR   R0        Before execution         R0 = H'80000001, T = 0
                      After execution           R0 = H'C0000000, T = 1











                                                                                            HITACHI  145
```

## PDF page 150

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=150)

```text
6.55      SHLL (Shift Logical Left): Shift Instruction

Format                     Abstract                            Code                   State   T Bit
SHLL  Rn            T ‹   Rn ‹    0                0100nnnn00000000     1         MSB


Description: Logically shifts the contents of general register Rn to the left by one bit, and
stores the result in Rn. The bit that is shifted out of the operand is transferred to the T bit
(figure 6.9).


                                      MSB                            LSB
                      SHLL       T0

                                 Figure 6.9   Shift Logical Left

Operation:

  SHLL(long n) /* SHLL Rn(Same as SHAL) */
  {
      if ((R[n]&0x80000000)==0) T=0;
      else T=1;
      R[n]<<=1;
      PC+=2;
  }

Examples:

  SHLL   R0        Before execution     R0 = H'80000001, T = 0
                    After execution        R0 = H'00000002, T = 1






















146 HITACHI
```

## PDF page 151

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=151)

```text
6.56      SHLLn (Shift Logical Left n Bits): Shift Instruction

Format                     Abstract                            Code                   State   T Bit
SHLL2  Rn                 Rn << 2  ﬁ   Rn                        0100nnnn00001000             1          —
SHLL8  Rn                 Rn << 8  ﬁ   Rn                        0100nnnn00011000             1          —
SHLL16 Rn                 Rn << 16 ﬁ   Rn                        0100nnnn00101000             1          —


Description: Logically shifts the contents of general register Rn to the left by 2, 8, or 16 bits,
and stores the result in Rn. Bits that are shifted out of the operand are not stored (figure 6.10).


                                         MSB                                LSB
                             SHLL2


                                                                                   0


                                         MSB                                LSB
                             SHLL8


                                                                                   0


                                         MSB                                LSB
                             SHLL16


                                                                                   0

                                 Figure 6.10   Shift Logical Left n Bits

Operation:

       SHLL2(long n)/* SHLL2 Rn */
  {
       R[n]<<=2;
       PC+=2;
  }










                                                                                                 HITACHI  147
```

## PDF page 152

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=152)

```text
  SHLL8(long n)/* SHLL8 Rn */
  {
       R[n]<<=8;
       PC+=2;
  }

  SHLL16(long n)   /* SHLL16 Rn */
  {
       R[n]<<=16;
       PC+=2;
  }

Examples:

  SHLL2   R 0      Before execution           R0 = H'12345678
                     After execution           R0 = H'48D159E0

  SHLL8  R 0      Before execution           R0 = H'12345678
                     After execution             R0 = H'34567800

  SHLL16 R0       Before execution           R0 = H'12345678
                     After execution             R0 = H'56780000

































148 HITACHI
```

## PDF page 153

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=153)

```text
6.57      SHLR (Shift Logical Right): Shift Instruction

Format                    Abstract                            Code                   State   T Bit
SHLR  Rn              0 ﬁ   Rn ﬁ   T                0100nnnn00000001      1          LSB


Description: Logically shifts the contents of general register Rn to the right by one bit, and
stores the result in Rn. The bit that is shifted out of the operand is transferred to the T bit
(figure 6.11).


                                         MSB                               LSB
                        SHLR         0                                              T

                                   Figure 6.11   Shift Logical Right

Operation:

  SHLR(long n) /* SHLR Rn */
  {
       if ((R[n]&0x00000001)==0) T=0;
       else T=1;
       R[n]>>=1;
       R[n]&=0x7FFFFFFF;
       PC+=2;
  }

Examples

  SHLR   R0        Before execution         R0 = H'80000001, T = 0
                      After execution           R0 = H'40000000, T = 1





















                                                                                               HITACHI  149
```

## PDF page 154

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=154)

```text
6.58      SHLRn (Shift Logical Right n Bits): Shift Instruction

Format                    Abstract                            Code                   State   T Bit
SHLR2  Rn                Rn>>2 ﬁ   Rn                           0100nnnn00001001             1         —
SHLR8  Rn                Rn>>8  ﬁ   Rn                          0100nnnn00011001             1         —
SHLR16 Rn                Rn>>16  ﬁ   Rn                         0100nnnn00101001             1         —


Description: Logically shifts the contents of general register Rn to the right by 2, 8, or 16
bits, and stores the result in Rn. Bits that are shifted out of the operand are not stored
(figure 6.12).


                                              MSB                                LSB
                          SHLR2


                                          0


                                              MSB                                LSB
                          SHLR8


                                          0


                                              MSB                                LSB
                          SHLR16


                                          0

                               Figure 6.12   Shift Logical Right n Bits

Operation:

  SHLR2(long n)/* SHLR2 Rn */
  {
       R[n]>>=2;
       R[n]&=0x3FFFFFFF;
       PC+=2;
  }







150 HITACHI
```

## PDF page 155

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=155)

```text
  SHLR8(long n) /* SHLR8 Rn */
  {
        R[n]>>=8;
        R[n]&=0x00FFFFFF;
        PC+=2;
  }

  SHLR16(long n)   /* SHLR16 Rn */
  {
        R[n]>>=16;
        R[n]&=0x0000FFFF;
        PC+=2;
  }

Examples:

  SHLR2    R0             Before execution           R0 = H'12345678
                             After execution              R0 = H'048D159E
  SHLR8    R0             Before execution           R0 = H'12345678
                             After execution              R0 = H'00123456
  SHLR16   R0             Before execution           R0 = H'12345678
                             After execution              R0 = H'00001234































                                                                                                    HITACHI  151
```

## PDF page 156

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=156)

```text
6.59      SLEEP (Sleep): System Control Instruction

Format                    Abstract                            Code                   State   T Bit
SLEEP                 Sleep                 0000000000011011 3—


Description: Sets the CPU into power-down mode. In power-down mode, instruction
execution stops, but the CPU module state is maintained, and the CPU waits for an interrupt
request. If an interrupt is requested, the CPU exits the power-down mode and begins
exception processing.

Note: The number of cycles given is for the transition to sleep mode.

Operation:

  SLEEP()   /* SLEEP */
  {
       PC-=2;
       Wait_for_exception;
  }

Example:

  SLEEP      Transits power-down mode































152 HITACHI
```

## PDF page 157

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=157)

```text
6.60      STC (Store Control Register): System Control Instruction

Class: Interrupt disabled instruction

Format                    Abstract                      Code                   State   T Bit
STC   SR,Rn               SR ﬁ    Rn                       0000nnnn00000010            1        —
STC   GBR,Rn              GBR ﬁ    Rn                      0000nnnn00010010            1        —
STC   VBR,Rn              VBR ﬁ    Rn                      0000nnnn00100010            1        —
STC.L  SR,@-Rn            Rn – 4  ﬁ   Rn, SR ﬁ   (Rn)      0100nnnn00000011            2        —
STC.L  GBR,@-Rn           Rn – 4  ﬁ   Rn, GBR ﬁ   (Rn)     0100nnnn00010011            2        —
STC.L  VBR,@-Rn           Rn – 4  ﬁ   Rn, VBR ﬁ   (Rn)     0100nnnn00100011            2        —


Description: Stores control registers SR, GBR, or VBR data into a specified destination.

Note: No interrupts are accepted between this instruction and the next instruction. Address
errors are accepted.

Operation:

  STCSR(long n)    /* STC SR,Rn */
  {
       R[n]=SR;
       PC+=2;
  }

  STCGBR(long n)   /* STC GBR,Rn */
  {
       R[n]=GBR;
       PC+=2;
  }

  STCVBR(long n)   /* STC VBR,Rn */
  {
       R[n]=VBR;
       PC+=2;
  }










                                                                                         HITACHI  153
```

## PDF page 158

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=158)

```text
  STCMSR(long n)   /* STC.L SR,@-Rn */
  {
      R[n]-=4;
      Write_Long(R[n],SR);
      PC+=2;
  }

  STCMGBR(long n)  /* STC.L GBR,@-Rn */
  {
      R[n]-=4;
      Write_Long(R[n],GBR);
      PC+=2;
  }

  STCMVBR(long n)  /* STC.L VBR,@-Rn */
  {
      R[n]-=4;
      Write_Long(R[n],VBR);
      PC+=2;
  }

Examples:

  STC    SR,R0         Before execution      R0 = H'FFFFFFFF, SR = H'00000000
                         After execution          R0 = H'00000000
  STC.L  GBR,@-R15   Before execution        R15 = H'10000004
                         After execution         R15 = H'10000000, @R15 = GBR
























154 HITACHI
```

## PDF page 159

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=159)

```text
6.61      STS (Store System Register): System Control Instruction

Class: Interrupt disabled instruction

Format                   Abstract                       Code                   State   T Bit
STS   MACH,Rn            MACH ﬁ    Rn                         0000nnnn00001010            1         —
STS   MACL,Rn            MACL  ﬁ   Rn                         0000nnnn00011010            1         —
STS   PR,Rn              PR  ﬁ   Rn                           0000nnnn00101010            1         —
STS.L  MACH,@–Rn         Rn – 4  ﬁ   Rn, MACH ﬁ   (Rn)        0100nnnn00000010            1         —
STS.L  MACL,@–Rn         Rn – 4  ﬁ   Rn, MACL ﬁ   (Rn)        0100nnnn00010010            1         —
STS.L  PR,@–Rn           Rn – 4  ﬁ   Rn, PR ﬁ   (Rn)          0100nnnn00100010            1         —


Description: Stores system registers MACH, MACL and PR data into a specified destination.

Note: No interrupts are accepted between this instruction and the next instruction. Address
errors are accepted.

If the system register is MACH in the SH-1 series, the value of bit 9 is transferred to and
stored in the higher 22 bits (bits 31 to 10) of the destination. With the SH-2 series, the 32 bits
of MACH are stored directly.

Operation:

  STSMACH(long n)  /* STS MACH,Rn */
  {
       R[n]=MACH;
  if ((R[n]&0x00000200)==0)                     For SH-1 CPU (these 2 lines not
  R[n]&=0x000003FF;                                     needed for SH-2 CPU)
  else R[n]|=0xFFFFFC00;
       PC+=2;
  }

  STSMACL(long n)  /* STS MACL,Rn */
  {
       R[n]=MACL;
       PC+=2;
  }









                                                                                             HITACHI  155
```

## PDF page 160

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=160)

```text
  STSPR(long n)    /* STS PR,Rn */
 {
      R[n]=PR;
      PC+=2;
  }

  STSMMACH(long n) /* STS.L MACH,@–Rn */
  {
      R[n]–=4;


  if ((MACH&0x00000200)==0)
  Write_Long(R[n],MACH&0x000003FF);       For SH-1 CPU
  else Write_Long
  (R[n],MACH|0xFFFFFC00)

  Write_Long(R[n], MACH);        For SH-2 CPU


      PC+=2;
  }

 STSMMACL(long n) /* STS.L MACL,@–Rn */
 {
      R[n]–=4;
      Write_Long(R[n],MACL);
      PC+=2;
  }

 STSMPR(long n)   /* STS.L PR,@–Rn */
 {
      R[n]–=4;
      Write_Long(R[n],PR);
      PC+=2;
  }

Example:

  STS    MACH,R0    Before execution      R0 = H'FFFFFFFF, MACH = H'00000000
                        After execution         R0 = H'00000000

  STS.L  PR,@–R15    Before execution       R15 = H'10000004
                        After execution        R15 = H'10000000, @R15 = PR




156 HITACHI
```

## PDF page 161

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=161)

```text
6.62      SUB (Subtract Binary): Arithmetic Instruction

Format                     Abstract                             Code                   State   T Bit
SUB   Rm,Rn             Rn – Rm  ﬁ   Rn              0011nnnnmmmm1000 1—


Description: Subtracts general register Rm data from Rn data, and stores the result in Rn. To
subtract immediate data, use ADD #imm,Rn.

Operation:

  SUB(long m,long n)  /* SUB Rm,Rn */
  {
       R[n]-=R[m];
       PC+=2;
  }

Example:

  SUB  R0,R1   Before execution    R0 = H'00000001, R1 = H'80000000
                   After execution     R1 = H'7FFFFFFF




































                                                                                         HITACHI  157
```

## PDF page 162

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=162)

```text
 6.63     SUBC (Subtract with Carry): Arithmetic Instruction

Format                     Abstract                            Code                  State   T Bit
SUBC  Rm,Rn            Rn – Rm– T ﬁ   Rn, Borrow ﬁ            0011nnnnmmmm1010    1           Borrow
                        T


Description: Subtracts Rm data and the T bit value from general register Rn, and stores the
result in Rn. The T bit changes according to the result. This instruction is used for subtraction
of data that has more than 32 bits.

Operation:

  SUBC(long m,long n) /* SUBC Rm,Rn */
  {
       unsigned long tmp0,tmp1;


       tmp1=R[n]-R[m];
       tmp0=R[n];
       R[n]=tmp1-T;
       if (tmp0<tmp1) T=1;
       else T=0;
       if (tmp1<R[n]) T=1;
       PC+=2;
  }

Examples:

  CLRT               R0:R1(64 bits) – R2:R3(64 bits) = R0:R1(64 bits)
  SUBC  R3,R1     Before execution       T = 0, R1 = H'00000000, R3 = H'00000001
                       After execution        T = 1, R1 = H'FFFFFFFF
  SUBC  R2,R0     Before execution       T = 1, R0 = H'00000000, R2 = H'00000000
                       After execution        T = 1, R0 = H'FFFFFFFF
















158 HITACHI
```

## PDF page 163

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=163)

```text
6.64      SUBV (Subtract with V Flag Underflow Check): Arithmetic
          Instruction

Format                Abstract                            Code                  State   T Bit
SUBV  Rm,Rn      Rn – Rm  ﬁ   Rn, Underflow ﬁ   T 0011nnnnmmmm1011     1           Underflow


Description: Subtracts Rm data from general register Rn data, and stores the result in Rn. If
an underflow occurs, the T bit is set to 1.

Operation:

  SUBV(long m,long n) /* SUBV Rm,Rn */
  {
       long dest,src,ans;


       if ((long)R[n]>=0) dest=0;
       else dest=1;
       if ((long)R[m]>=0) src=0;
       else src=1;
       src+=dest;
       R[n]-=R[m];
       if ((long)R[n]>=0) ans=0;
       else ans=1;
       ans+=dest;
       if (src==1) {
           if (ans==1) T=1;
           else T=0;
       }
       else T=0;
       PC+=2;
  }

Examples:

  SUBV  R0,R1     Before execution     R0 = H'00000002, R1 = H'80000001
                     After execution       R1 = H'7FFFFFFF, T = 1

  SUBV  R2,R3    Before execution    R2 = H'FFFFFFFE, R3 = H'7FFFFFFE
                     After execution        R3 = H'80000000, T = 1




                                                                                         HITACHI  159
```

## PDF page 164

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=164)

```text
6.65      SWAP (Swap Register Halves): Data Transfer Instruction

Format                   Abstract                             Code                   State   T Bit
SWAP.B Rm,Rn          Rm ﬁ   Swap upper and lower           0110nnnnmmmm1000            1        —
                      halves of lower 2 bytes ﬁ   Rn
SWAP.W Rm,Rn          Rm  ﬁ   Swap upper and lower          0110nnnnmmmm1001            1        —
                      word ﬁ   Rn


Description: Swaps the upper and lower bytes of the general register Rm data, and stores the
result in Rn. If a byte is specified, bits 0 to 7 of Rm are swapped for bits 8 to 15. The upper 16
bits of Rm are transferred to the upper 16 bits of Rn. If a word is specified, bits 0 to 15 of Rm
are swapped for bits 16 to 31.

Operation:

  SWAPB(long m,long n)/* SWAP.B Rm,Rn */
  {
       unsigned long temp0,temp1;


       temp0=R[m]&0xffff0000;
       temp1=(R[m]&0x000000ff)<<8;
       R[n]=(R[m]>>8)&0x000000ff;
       R[n]=R[n]|temp1|temp0;
       PC+=2;
  }

  SWAPW(long m,long n)/* SWAP.W Rm,Rn */
  {
       unsigned long temp;
       temp=(R[m]>>16)&0x0000FFFF;
       R[n]=R[m]<<16;
       R[n]|=temp;
       PC+=2;
  }

Examples

  SWAP.B  R0,R1    Before execution    R0 = H'12345678
                       After execution       R1 = H'12347856

  SWAP.W  R0,R1   Before execution    R0 = H'12345678
                       After execution       R1 = H'56781234




160 HITACHI
```

## PDF page 165

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=165)

```text
6.66      TAS (Test and Set): Logic Operation Instruction

Format            Abstract                                      Code                 State   T Bit
TAS.B  @Rn     When (Rn) is 0, 1 ﬁ   T, 1 ﬁ   MSB of             0100nnnn00011011   4         Test
                 (Rn)                                                                                results


Description: Reads byte data from the address specified by general register Rn, and sets the
T bit to 1 if the data is 0, or clears the T bit to 0 if the data is not 0. Then, data bit 7 is set to
1, and the data is written to the address specified by Rn. During this operation, the bus is not
released.

Operation:

  TAS(long n)  /* TAS.B @Rn */
  {
       long temp;


       temp=(long)Read_Byte(R[n]); /* Bus Lock enable */
       if (temp==0) T=1;
       else T=0;
       temp|=0x00000080;
       Write_Byte(R[n],temp);       /* Bus Lock disable */
       PC+=2;
  }

Example:

  _LOOP  TAS.B   @R7           R7 = 1000
            BF     _LOOP       Loops until data in address 1000 is 0





















                                                                                              HITACHI  161
```

## PDF page 166

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=166)

```text
6.67      TRAPA (Trap Always): System Control Instruction

Format                 Abstract                              Code                  State   T Bit
TRAPA  #imm       PC/SR ﬁ   Stack area, (imm · 4 +           11000011iiiiiiii8—
                      VBR) ﬁ   PC


Description: Starts the trap exception processing. The PC and SR values are stored on the
stack, and the program branches to an address specified by the vector. The vector is a
memory address obtained by zero-extending the 8-bit immediate data and then quadrupling it.
The PC points the starting address of the next instruction. TRAPA and RTE are both used for
system calls.

Operation:

  TRAPA(long i)/* TRAPA #imm */
  {
       long imm;


       imm=(0x000000FF & i);
       R[15]-=4;
       Write_Long(R[15],SR);
       R[15]-=4;
       Write_Long(R[15],PC–2);
       PC=Read_Long(VBR+(imm<<2))+4;
  }

Example:

  Address
  VBR+H'80   .data.l         10000000
     ..........
                  TRAPA   #H'20      Branches to an address specified by data in address
                                          VBR + H'80
                  TST    #0,R0        ‹   Return address from the trap routine (stacked PC
                                          value)
     ...........
     ..........
  100000000  XOR     R0,R0           ‹  Trap routine entrance
  100000002  RTE                     Returns to the TST instruction
  100000004  NOP                     Executes NOP before RTE




162 HITACHI
```

## PDF page 167

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=167)

```text
6.68      TST (Test Logical): Logic Operation Instruction

Format                                Abstract                     Code                  State   T Bit
TST   Rm,Rn                     Rn & Rm, when result is       0010nnnnmmmm1000           1         Test
                                0, 1 ﬁ   T                                                         results
TST   #imm,R0                   R0 & imm, when result is      11001000iiiiiiii           1         Test
                                0, 1 ﬁ   T                                                         results
TST.B                           (R0 + GBR) & imm, when        11001100iiiiiiii           3         Test
         #imm,@(R0,GBR          result is 0, 1 ﬁ   T                                               results
)


Description: Logically ANDs the contents of general registers Rn and Rm, and sets the T bit
to 1 if the result is 0 or clears the T bit to 0 if the result is not 0. The Rn data does not
change. The contents of general register R0 can also be ANDed with zero-extended 8-bit
immediate data, or the contents of 8-bit memory accessed by indirect indexed GBR
addressing can be ANDed with 8-bit immediate data. The R0 and memory data do not
change.

Operation:

  TST(long m,long n)  /* TST Rm,Rn */
  {
       if ((R[n]&R[m])==0) T=1;
       else T=0;
       PC+=2;
  }

  TSTI(long i)  /* TEST #imm,R0 */
  {
       long temp;


       temp=R[0]&(0x000000FF & (long)i);
       if (temp==0) T=1;
       else T=0;
       PC+=2;
  }

  TSTM(long i)  /* TST.B #imm,@(R0,GBR) */
  {
       long temp;





                                                                                            HITACHI  163
```

## PDF page 168

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=168)

```text
       temp=(long)Read_Byte(GBR+R[0]);
       temp&=(0x000000FF & (long)i);
       if (temp==0) T=1;
       else T=0;
       PC+=2;
  }

Examples:

  TST    R0,R0                    Before execution    R0 = H'00000000
                                  After execution      T = 1

  TST   #H'80,R0           Before execution   R0 = H'FFFFFF7F
                                  After execution      T = 1

  TST.B  #H'A5,@(R0,GBR)  Before execution    @(R0,GBR) = H'A5
                                  After execution      T = 0








































164 HITACHI
```

## PDF page 169

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=169)

```text
6.69      XOR (Exclusive OR Logical): Logic Operation Instruction

Format                                Abstract                     Code                  State   T Bit
XOR   Rm,Rn                    Rn ^ Rm ﬁ    Rn              0010nnnnmmmm1010          1        —
XOR   #imm,R0                  R0 ^ imm ﬁ    R0             11001010iiiiiiii          1        —
XOR.B  #imm,@(R0,GBR)          (R0 + GBR) ^ imm ﬁ   (R0     11001110iiiiiiii          3        —
                               + GBR)


Description: Exclusive ORs the contents of general registers Rn and Rm, and stores the
result in Rn. The contents of general register R0 can also be exclusive ORed with zero-
extended 8-bit immediate data, or 8-bit memory accessed by indirect indexed GBR
addressing can be exclusive ORed with 8-bit immediate data.

Operation:

  XOR(long m,long n)  /* XOR Rm,Rn */
  {
       R[n]^=R[m];
       PC+=2;
  }

  XORI(long i)  /* XOR #imm,R0 */
  {
       R[0]^=(0x000000FF & (long)i);
       PC+=2;
  }

  XORM(long i)  /* XOR.B #imm,@(R0,GBR) */
  {
       long temp;


       temp=(long)Read_Byte(GBR+R[0]);
       temp^=(0x000000FF & (long)i);
       Write_Byte(GBR+R[0],temp);
       PC+=2;
  }









                                                                                        HITACHI  165
```

## PDF page 170

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=170)

```text
Examples:

  XOR   R0,R1                 Before execution  R0 = H'AAAAAAAA, R1 = H'55555555
                                   After execution   R1 = H'FFFFFFFF

  XOR  #H'F0,R0            Before execution  R0 = H'FFFFFFFF
                                   After execution   R0 = H'FFFFFF0F

  XOR.B #H'A5,@(R0,GBR)    Before execution  @(R0,GBR) = H'A5
                                   After execution    @(R0,GBR) = H'00



















































166 HITACHI
```

## PDF page 171

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=171)

```text
6.70      XTRCT (Extract): Data Transfer Instruction

Format                     Abstract                            Code                   State   T Bit
XTRCT  Rm,Rn           Center 32 bits of Rm and Rn ﬁ           0010nnnnmmmm1101 1—
                         Rn


Description: Extracts the middle 32 bits from the 64 bits of general registers Rm and Rn, and
stores the 32 bits in Rn (figure 6.13).


               MSB                                           MSBLSB                                            LSB
                        Rm                                                                 Rn



                                                      Rn

                                          Figure 6.13   Extract

Operation:

  XTRCT(long m,long n)    /* XTRCT Rm,Rn */
  {
       unsigned long temp;


       temp=(R[m]<<16)&0xFFFF0000;
       R[n]=(R[n]>>16)&0x0000FFFF;
       R[n]|=temp;
       PC+=2;
  }

Example:

  XTRCT  R0,R1    Before execution   R0 = H'01234567, R1 = H'89ABCDEF
                       After execution      R1 = H'456789AB















                                                                                               HITACHI  167
```
