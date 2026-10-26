# Section 3 Data Formats

SH-1/SH-2 Programming Manual — Hitachi, September 3, 1996.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=25)

Physical PDF pages 25–26. Text extraction; consult the PDF for figures.

## PDF page 25

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=25)

```text
                                   Section 3   Data Formats


3.1        Data Format in Registers

Register operands are always longwords (32 bits) (figure 3.1). When the memory operand is
only a byte (8 bits) or a word (16 bits), it is sign-extended into a longword when loaded into a
register.


                              31                                                       0
                                                    Longword

                                    Figure 3.1   Longword Operand

3.2        Data Format in Memory

Memory data formats are classified into bytes, words, and longwords. Byte data can be
accessed from any address, but an address error will occur if you try to access word data
starting from an address other than 2n or longword data starting from an address other than 4n.
In such cases, the data accessed cannot be guaranteed (figure 3.2). The hardware stack area,
which is referred to by the hardware stack pointer (SP, R15), uses only longword data starting
from address 4n because this area holds the program counter and status register. See the SH
Hardware Manual for more information on address errors.


                                                    Address m + 1  Address m + 3

                                             Address m       Address m + 2
                                                31                                     023                     715
                                                 Byte     Byte     Byte     Byte
                              Address 2n             Word              Word
                              Address 4n                   Longword
                                                           Big endian

                        Figure 3.2   Byte, Word, and Longword Alignment

SH7604 has a function that allows access of CS2 space (area 2) in little endian format,
which enables memory to be shared with processors that access memory in little endian
format  (figure 3.3). Byte data is arranged differently for little endian and the usual big
endian.








                                                                                                     HITACHI  21
```

## PDF page 26

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=26)

```text
                                          Address m + 2     Address m

                                 Address m + 3    Address m + 1
                                     31      23                       7150
                                      Byte      Byte      Byte      Byte
                                          Word              Word            Address 2n
                                                 Longword                   Address 4n
                                                Little endian*

                          Note : Only CS2 space of SH7604 can be set.

Figure 3.3   Byte, Word, and Longword Alignment in little endian format (SH7604 only)

3.3       Immediate Data Format

Byte immediate data is located in an instruction code. Immediate data accessed by the
MOV, ADD, and CMP/EQ instructions is sign-extended and calculated with registers and
longword data. Immediate data accessed by the TST, AND, OR, and XOR instructions is
zero-extended and calculated with longword data. Consequently, AND instructions with
immediate data always clear the upper 24 bits of the destination register.

Word or longword immediate data is not located in the instruction code. Rather, it is stored in
a memory table. The memory table is accessed by an immediate data transfer instruction
(MOV) using the PC relative addressing mode with displacement. Specific examples are
given in section 4.1.8, Immediate Data.






























22 HITACHI
```
