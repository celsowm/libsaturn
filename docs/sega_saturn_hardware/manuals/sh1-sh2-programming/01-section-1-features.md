# Section 1 Features

SH-1/SH-2 Programming Manual — Hitachi, September 3, 1996.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=18)

Physical PDF pages 18–19. Text extraction; consult the PDF for figures.

## PDF page 18

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=18)

```text
                                      Section 1   Features


The SH-1 and SH-2 CPU have RISC-type instruction sets. Basic instructions are executed in
one clock cycle, which dramatically improves instruction execution speed. The CPU also has
an internal 32-bit architecture for enhanced data processing ability. Table 1.1 lists the SH-1
and SH-2 CPU features.
























































14 HITACHI
```

## PDF page 19

[Original page](https://antime.kapsi.fi/sega/files/h12p0.pdf#page=19)

```text
Table 1.1    SH-1  and SH-2 CPU Features

Item                           Feature
Architecture                       •   Original Hitachi architecture
                                   •    32-bit internal data paths
General-register machine     •    Sixteen 32-bit general registers

                                   •   Three 32-bit control registers
                                   •   Four 32-bit system registers
Instruction set                •   Instruction length: 16-bit fixed length for improved code
                                        efficiency
                                   •   Load-store architecture (basic arithmetic and logic operations are
                                        executed between registers)
                                   •   Delayed branch system used for reduced pipeline disruption
                                   •   Instruction set optimized for C language
Instruction execution
time                          •   One instruction/cycle for basic instructions
Address space                  •    Architecture makes 4 Gbytes available
On-chip multiplier
(SH-1 CPU)                        •   Multiplication operations (16 bits · 16 bits ﬁ  32 bits) executedin 1 to 3 cycles, and multiplication/accumulation operations (16

                                        bits · 16 bits + 42 bits ﬁ   42 bits) executed in 3/(2)* cycles
On-chip multiplier
(SH-2 CPU)                        •   Multiplication operations executed in 1 to 2 cycles (16 bits · 16bits  ﬁ   32 bits) or 2 to 4 cycles (32 bits · 32 bits ﬁ   64 bits), and

                                        multiplication/accumulation operations executed in 3/(2)* cycles
                                        (16 bits · 16 bits + 64 bits ﬁ   64 bits) or 3/(2 to 4)* cycles (32
                                        bits · 32 bits + 64 bits ﬁ   64 bits)
Pipeline                                •   Five-stage pipeline
Processing states              •    Reset state

                                   •   Exception processing state
                                   •   Program execution state
                                   •   Power-down state
                                   •    Bus release state
Power-down states           •   Sleep mode

                                   •   Standby mode
Note:   The normal minimum number of execution cycles (The number in parentheses in the
         number in contention with preceding/following instructions).










                                                                                                             HITACHI  15
```
