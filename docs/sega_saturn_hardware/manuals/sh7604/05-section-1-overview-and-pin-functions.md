# Section 1 Overview and Pin Functions

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=17)

Physical PDF pages 17–30. Text extraction; consult the PDF for figures.

## Bookmarks

- [1.1 SH7604 Features](#pdf-page-17)
- [1.1.1 Features of the SH7604](#pdf-page-17)
- [1.2 Block Diagram](#pdf-page-22)
- [1.3 Description of Pins](#pdf-page-23)
- [1.3.1 Pin Arrangement](#pdf-page-23)
- [1.3.2 Pin Functions](#pdf-page-25)

## PDF page 17

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=17)

```text
                     Section 1   Overview and Pin Functions


1.1        SH7604 Features

The SH7604 is a new-generation single-chip RISC microprocessor that integrates a Hitachi-
original CPU, a multiplier, cache memory, and peripheral functions required for system
configuration.

The CPU features a RISC-type instruction set. Most instructions can be executed in one clock
cycle, which greatly improves instruction execution speed. In addition, the on-chip 4-kbyte cache
memory and divider enhance data processing ability.

The SH7604 is also provided with on-chip peripheral functions including a direct memory access
controller (DMAC), timers, a serial communication interface (SCI), and an interrupt controller.
External memory access support functions (provided by the bus state controller) enable direct
connection to DRAM, synchronous DRAM, and pseudo-SRAM.

The high-speed CPU and comprehensive peripheral functions enable designers to construct high-
performance systems with advanced functionality at low cost, even in applications such as real-
time control that require very high speeds, impossible with conventional microprocessors.


1.1.1        Features of the SH7604

CPU:

•    Original Hitachi architecture
•    32-bit internal configuration
•    General-registers:
    ¾    Sixteen 32-bit general registers
    ¾    Three 32-bit control registers
    ¾    Four 32-bit system registers
•    RISC-type instruction set:
    ¾    Instruction length: 16-bit fixed length for improved code efficiency
    ¾    Load-store architecture (basic arithmetic and logic operations are executed between
        registers)
    ¾    Delayed conditional/unconditional branch instructions reduce pipeline disruption during
        branching
    ¾    Instruction set based on C language
•    Instruction execution time: one instruction/state (35 ns/instruction at 28.7 MHz operation)
•    Address space: 4 Gbytes available in the architecture (128-Mbyte memory space)


                                                                                                                       1
```

## PDF page 18

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=18)

```text
•    On-chip multiplier: multiply operations (32 bits · 32 bits ﬁ   64 bits) and multiply-and-
    accumulate operations (32 bits · 32 bits + 64 bits ﬁ   64 bits) executed in 2 to 4 states
•    Five-stage pipeline

Operating Modes:

•    Clock mode: selected from the combination of an on-chip oscillator module, a frequency
    multiplier, clock output, PLL synchronization, and 90° phase shifting (the range of choices
    depends on the package)
•    Slave/master mode
•    Processing states
    ¾    Power-on reset state
    ¾    Manual reset state
    ¾    Exception handling state
    ¾    Program execution state
    ¾    Power-down state
    ¾    Bus-released state
•    Power-down states
    ¾    Sleep mode
    ¾    Standby mode
    ¾    Module stop mode

Interrupt Controller (INTC):

•    Five external interrupt pins (NMI, IRL0 to IRL3), encoded input of 15 external interrupt
    sources via pins IRL0 to IRL3
•    Twelve internal interrupt sources (DMAC · 2, DIVU · 1, FRT · 3, WDT · 1, SCI · 4,
    REF · 1)
•    Sixteen programmable priority levels
•    Vector number settable for each internal interrupt source
•    Auto-vector or external vector selectable as vector for external interrupts via pins IRL0 to
    IRL3

User Break Controller (UBC):

•    Generates an interrupt when the CPU or DMAC generates an address, data, or bus cycle with
    the specified conditions (address, data, CPU cycle/non-CUP cycle, instruction fetch/data
    access, read/write, byte/word/longword access)
•    Simplifies configuration of a self-debugger




2
```

## PDF page 19

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=19)

```text
Clock Pulse Generator (CPG)/Phase Locked Loop (PLL):

•    On-chip clock pulse generator
•    Crystal clock source or external clock source can be selected
•    Clock multiplication (·1, ·2, ·4), PLL synchronization, or 90° phase shift can be selected
•    Supports clock pause function for frequency change of external clock

Bus State Controller (BSC):

•    Supports external memory access
    ¾    32-bit external data bus
•    Memory address space divided into four areas. It is possible to set the following characteristics
    for each area (32 Mbyte linear):
    ¾    Bus size (8, 16, or 32 bits)
    ¾    Number of wait cycles settable or not settable
    ¾    Setting the memory space type simplifies connection to DRAM, synchronous DRAM,
        pseudo-SRAM, and burst ROM
    ¾    Outputs signals RAS, CAS, CE, and OE corresponding to DRAM, synchronous DRAM,
        and pseudo-SRAM areas
    ¾    Tp cycles can be generated to assure RAS precharge time
    ¾    Address multiplexing is supported internally, so DRAM and synchronous DRAM can be
        connected directly
    ¾    Outputs chip select signals (CS0 to CS3) for each area
•    DRAM/synchronous DRAM/pseudo-SRAM refresh functions
    ¾    Programmable refresh interval
    ¾    Supports CAS-before-RAS refresh and self-refresh modes
•    DRAM/synchronous DRAM/pseudo-SRAM burst access function
    ¾    Supports high-speed access modes for DRAM/synchronous DRAM/pseudo-SRAM
•    Wait cycles can be inserted by an external WAIT signal

Cache Memory:

•    4 kbytes
•    64 entries, 4-way set associative, 16-byte line length
•    Write-through data writing method
•    LRU replacement algorithm
•    2 kbytes of the cache can be used as 2-kbyte internal RAM






                                                                                                                     3
```

## PDF page 20

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=20)

```text
Direct Memory Access Controller (DMAC) (2 Channels):

•    Permits DMA transfer between external memory, external I/O, on-chip peripheral modules
•    Enables DMA transfer request and auto-request from external pins, on-chip SCI, and on-chip
   timers
•    Cycle-steal mode or burst mode
•    Channel priority level is selectable (fixed mode or round-robin mode)
•    Dual or single address transfer mode is selectable
•    Transfer data width: 1/2/4/16 bytes
•    Address space: 4 Gbytes; maximum number of transfers: 16,777,216

Division Unit (DIVU):

•    Executes 64 ÷ 32 →   32… 32 and 32 ÷ 32   →   32… 32 divisions in 39 cycles
•    Overflow interrupt

16-Bit Free-Running Timer (FRT) (1 Channel):

•    Selects input from three internal/external clocks
•    Input capture and output compare
•    Counter overflow, compare match, and input capture interrupts

Watchdog Timer (WDT) (1 Channel):

•    Can be switched between watchdog timer and interval timer functions
•    Count overflow can generate an internal reset, external signal, or interrupt
•    Power-on reset or manual reset can be selected as the internal reset

Serial Communication Interface (SCI) (1 Channel):

•    Asynchronous or synchronous mode is selectable
•    Simultaneous transmission and reception (full duplex)
•    Dedicated baud rate generator
•    Multiprocessor communication function

Package:

•    144-pin plastic QFP (FP-144J)
•    176-pin plastic TFBGA (TBP-176)






4
```

## PDF page 21

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=21)

```text
Product Lineup:

Product Code             Package                  Operating Temperature     Frequency     Voltage
HD6417604SF28         QFP2020-144          - 20     °C to 75°C                      28 MHz           5 V
HD6417604SFI28        QFP2020-144          - 40    °C to 85°C                      28 MHz           5 V
HD6417604SVF20      QFP2020-144          - 20     °C to 75°C                      20 MHz           3.3 V
HD6417604SBP28      CSP-1313-176         - 20     °C to 75°C                      28 MHz           5 V
HD6417604SVBP20   CSP-1313-176         - 20     °C to 75°C                      20 MHz           3.3 V


















































                                                                                                                                          5
```

## PDF page 22

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=22)

```text
1.2        Block Diagram

Figure 1.1 shows a block diagram of the SH7604.




     MULT


     Cache              CPU

   address array

                      Exception   
              Cache address busCache data bushandling  
     Cache             interrupt 
    controller         controller         16-bit free 
                                           running 
                                           timer


     Cache            User break           Serial  
     data              controller         communi-  
     array                                 cation 
                                           interface


                     Vector address
     DIVU                       Internal interrupt signalsPeripheral address busWatchdog   
              Internal address busInternal data bustimer


     Direct            Bus state 
    memory             controller
     access                           Peripheral data busOperating- 
    controller                             mode    
   (× 2 channels)                         controller
                      Clock pulse    
                       generator

            External bus  
             interface




                  Figure 1.1   Block Diagram





6
```

## PDF page 23

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=23)

```text
1.3        Description of Pins


1.3.1        Pin Arrangement
        SSCC  SSCC      CCSS   SSCC/CKM
      D10D9D8VD7VD6D5D4D3VD2VD1D0IRL0IRL1IRL2IRL3NMIIVECFVRESVMD5MD4CKIOMD3XTALVEXTALVCKPREQCKRACKMD2CAP2
      144143142141140139138137136135134133132131122130129128127126125124123121120119118117116115114113112111110109
  D111                             108CAP1
  D122                             107MD1
  D133                             106V SS (PLL)
  V CC4                            105MD0
  D145                             104V CC (PLL)
  V SS6                            103SCK
  D157                             102TXD
  D168                             101RXD
  D179                             100FTCI
  D1810                             99FTI
  D1911                             98V SS
  V CC12                            97FTOA
  D2013                             96V CC
  V SS14                            95FTOB
  D2115           Top view          94WDTOVF
  D2216                             93BREQ/BGR
  D2317                             92BACK/BRLS
  V CC18          (FP-144)          91V SS
  D2419                             90NC*
  V SS20                            89WAIT
  D2521          HD6417604          88CKE
  D2622                             87RD
  D2723                             86V SS
  V CC24                            85CASLL/DQMLL/  WE0
  D2825                             84V CC
  V SS26                            83CASLH/DQMLU/   WE1
  D2927                             82CASHL/DQMUL/   WE2
  D3028                             81CASHH/DQMUU/   WE3
  D3129                             80CAS/OE
  A030                              79RAS/CE
  A131                              78V SS
  A232                              77RD/  WR
  V SS33                            76BS
  A334                              75CS3
  A435                              74CS2
  A536                              73CS1
      373839404142434445464748495051525354555657585960616263646566676869707172
      A6A7A8CCA9SSCCSS   A22CCSSCCSSCCSS
        VVA10A11A12A13A14VA15VA16A17A18VA19VA20A21VA23VA24A25A26DACK0VDACK1VDREQ0DREQ1CS0

 Note:  Do not connect anything to the pin abeled NC.
          Figure 1.2   Pin Arrangement (144-Pin Plastic QFP)











                                               7
```

## PDF page 24

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=24)

```text
              ABCDEFGHJKLMNPR

          15                                                            15 

          14                                                            14 

          13                                                            13 

          12                                                            12 

          11                                                            11 

          10                                                            10 

          9                                                             9 
          8                           TBP-176                           8 
                                     (Top View)
          7                                                             7 

          6                                                             6 

          5                     INDEX MARK                              5 

          4                                                             4 

          3                                                             3 

          2                                                             2 

          1                                                             1

              ABCDEFGHJKLMNPR

            Note: The part surrounded by the interrupt line is perspective.

                 Figure 1.3   Pin Arrangement (176-Pin Plastic TFBGA)

























8
```

## PDF page 25

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=25)

```text
1.3.2        Pin Functions

Table 1.2 shows the pin functions of the SH7604.

Table 1.1      Pin Functions

         Pin No.
FP-144  TBP-176Pin Name           I/O     Pin Description
—            A1            NC                                    —             Reserved pin (leave unconnected)
—            C3            NC                                    —             Reserved pin (leave unconnected)
—            B1            NC                                    —             Reserved pin (leave unconnected)
—            C2            NC                                    —             Reserved pin (leave unconnected)
1              D3            D11                                   I/O           Data bus
2              C1            D12                                   I/O           Data bus
3              D2            D13                                   I/O           Data bus
4E4VCC                                    I               Power
5              D1            D14                                   I/O           Data bus
6E3VSS                                    I               Ground
7              E2            D15                                   I/O           Data bus
8              E1            D16                                   I/O           Data bus
9              F4            D17                                   I/O           Data bus
10            F3            D18                                   I/O           Data bus
11            F1            D19                                   I/O           Data bus
12            F2            V CC                                    I               Power
13            G4           D20                                   I/O           Data bus
14            G3           V  SS                                    I               Ground
15            G1           D21                                   I/O           Data bus
16            G2           D22                                   I/O           Data bus
17            H4            D23                                   I/O           Data bus
18            H3            V  CC                                    I               Power
19            H1            D24                                   I/O           Data bus
20            H2            V  SS                                    I               Ground
21            J4            D25                                   I/O           Data bus
22            J3            D26                                   I/O           Data bus
23            J1            D27                                   I/O           Data bus
24            J2            VCC                                    I               Power


                                                                                                                                                                  9
```

## PDF page 26

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=26)

```text
Table 1.1      Pin Functions (cont)

         Pin No.
FP-144  TBP-176Pin Name           I/O     Pin Description
25            K4            D28                                   I/O           Data bus
26            K3            V  SS                                    I               Ground
27            K1            D29                                   I/O           Data bus
28            K2            D30                                   I/O           Data bus
29            L3            D31                                   I/O           Data bus
30            L1            A0                                     I/O           Address bus
31            L2            A1                                     I/O           Address bus
32            L4            A2                                     I/O           Address bus
33            M1           V   SS                                    I               Ground
34            M2           A3                                     I/O           Address bus
35            M3           A4                                     I/O           Address bus
36            N1            A5                                     I/O           Address bus
—            M4           NC                                    —             Reserved pin (leave unconnected)
—            N2            NC                                    —             Reserved pin (leave unconnected)
—            P1            NC                                    —             Reserved pin (leave unconnected)
—            P2            NC                                    —             Reserved pin (leave unconnected)
—            R1            NC                                    —             Reserved pin (leave unconnected)
—            N3            NC                                    —             Reserved pin (leave unconnected)
—            R2            NC                                    —             Reserved pin (leave unconnected)
—            P3            NC                                    —             Reserved pin (leave unconnected)
37            N4            A6                                     I/O           Address bus
38            R3            A7                                     I/O           Address bus
39            P4            A8                                     I/O           Address bus
40            M5           V   CC                                    I               Power
41            R4            A9                                     I/O           Address bus
42            N5            V  SS                                    I               Ground
43            P5            A10                                   I/O           Address bus
44            R5            A11                                   I/O           Address bus
45            M6           A12                                   I/O           Address bus
46            N6            A13                                   I/O           Address bus
47            R6            A14                                   I/O           Address bus


10
```

## PDF page 27

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=27)

```text
Table 1.1      Pin Functions (cont)

         Pin No.
FP-144  TBP-176Pin Name           I/O     Pin Description
48            P6            V  CC                                    I               Power
49            M7           A15                                   I/O           Address bus
50            N7            V  SS                                    I               Ground
51            R7            A16                                   I/O           Address bus
52            P7            A17                                   I/O           Address bus
53            M8           A18                                   I/O           Address bus
54            N8            V  CC                                    I               Power
55            R8            A19                                   I/O           Address bus
56            P8            V  SS                                    I               Ground
57            M9           A20                                   I/O           Address bus
58            N9            A21                                   I/O           Address bus
59            R9            A22                                   I/O           Address bus
60            P9            V  CC                                    I               Power
61            M10         A23                                   I/O           Address bus
62            N10         V  SS                                    I               Ground
63            R10         A24                                   I/O           Address bus
64            P10          A25                                   I/O           Address bus
65            N11         A26                                   I/O           Address bus
66            R11         DACK0                             O             DMAC0 acknowledge
67            P11          V  CC                                    I               Power
68            M11         DACK1                             O             DMAC1 acknowledge
69            R12         V   SS                                    I               Ground
70            P12          DREQ0                             I               DMAC0 request
71            N12         DREQ1                             I               DMAC1 request
72            R13         CS0                                  O             Chip select 0
—            M12         NC                                    —             Reserved pin (leave unconnected)
—            P13          NC                                    —             Reserved pin (leave unconnected)
—            R14         NC                                    —             Reserved pin (leave unconnected)
—            P14          NC                                    —             Reserved pin (leave unconnected)
—            R15         NC                                    —             Reserved pin (leave unconnected)
—            N13         NC                                    —             Reserved pin (leave unconnected)


                                                                                                                                                                     11
```

## PDF page 28

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=28)

```text
Table 1.1      Pin Functions (cont)

        Pin No.
FP-144  TBP-176Pin Name           I/O     Pin Description
—            P15          NC                                    —             Reserved pin (leave unconnected)
—            N14         NC                                    —             Reserved pin (leave unconnected)
73            M13         CS1                                  O             Chip select 1
74            N15        CS2                                  O             Chip select 2
75            M14         CS3                                  O             Chip select 3
76            L12         BS                                    I/O           Bus cycle start
77            M15         RD/   WR                             I/O           Read/write
78            L13          V SS                                    I               Ground
79            L14        RAS/CE                           O             RAS for DRAM and synchronous DRAM, CE
                                                                                   for pseudo-SRAM
80            L15         CAS/OE                           O             CAS for synchronous DRAM, OE for
                                                                                   pseudo-SRAM
81            K12        CASHH/DQMUU/   WE3    O             Most significant byte selection signal for
                                                                                   memory
82            K13        CASHL/DQMUL/   WE2     O             Second byte selection signal for memory
83            K15        CASLH/DQMLU/   WE1     O             Third byte selection signal for memory
84            K14          V  CC                                    I               Power
85            J12        CASLL/DQMLL/  WE0      O             Least significant byte selection signal for
                                                                                   memory
86            J13          VSS                                    I               Ground
87            J15        RD                                    O             Read pulse
88            J14          CKE                                  O             Synchronous DRAM clock enable control
89            H12        WAIT                                I               Hardware wait request
90            H13         NC                                    —             Reserved pin (leave unconnected)
91            H15         V  SS                                    I               Ground
92            H14        BACK/BRLS                    I               Bus acknowledge in slave mode, bus
                                                                                   request in master mode
93            G12        BREQ/BGR                     O             Bus request in slave mode, bus grant in
                                                                                   master mode
94            G13        WDTOVF                         O             Watchdog timer output
95            G15         FTOB                               O             Free-running timer output B
96            G14         V  CC                                    I               Power



12
```

## PDF page 29

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=29)

```text
Table 1.1      Pin Functions (cont)

         Pin No.
FP-144  TBP-176Pin Name           I/O     Pin Description
97            F12          FTOA                               O             Free-running timer output A
98            F13          V SS                                    I               Ground
99            F15          FTI                                    I               Free-running timer input
100          F14          FTCI                                 I               Free-running timer clock input
101          E13          RxD                                  I               Serial data input
102          E15          TxD                                   O             Serial data output
103          E14          SCK                                  I/O           Serial clock input/output
104          E12          V   CC  (PLL)                        I               Power for on-chip PLL
105          D15         MD0                                  I               Operating mode pin
106          D14         V  SS  (PLL)                         I               Ground for on-chip PLL
107          D13         MD1                                  I               Operating mode pin
108          C15         CAP1                                O             External capacitance pin for PLL
—            D12         NC                                    —             Reserved pin (leave unconnected)
—            C14         NC                                    —             Reserved pin (leave unconnected)
—            B15          NC                                    —             Reserved pin (leave unconnected)
—            B14          NC                                    —             Reserved pin (leave unconnected)
—            A15          NC                                    —             Reserved pin (leave unconnected)
—            C13         NC                                    —             Reserved pin (leave unconnected)
—            A14          NC                                    —             Reserved pin (leave unconnected)
—            B13          NC                                    —             Reserved pin (leave unconnected)
109          C12         CAP2                                O             External capacitance pin for PLL
110          A13          MD2                                  I               Operating mode pin
111          B12         CKPACK                          O             Clock pause acknowledge output
112          D11         CKPREQ/CKM                I               Clock pause request input
113          A12          V  CC                                    I               Power
114          C11         EXTAL                              I               Pin for connecting crystal resonator
115          B11          V  SS                                    I               Ground
116          A11          XTAL                                O             Pin for connecting crystal resonator
117          D10         MD3                                  I               Operating mode pin
118          C10         CKIO                                I/O           System clock input/output
119          A10          MD4                                  I               Operating mode pin


                                                                                                                                                             13
```

## PDF page 30

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=30)

```text
Table 1.1      Pin Functions (cont)

         Pin No.
FP-144  TBP-176Pin Name           I/O     Pin Description
120          B10          MD5                                  I               Operating mode pin
121          D9            V  SS                                    I               Ground
122          C9          RES                                  I               Reset
123          A9            V  CC                                    Power      Power
124          B9          IVECF                              O             Interrupt vector fetch cycle
125          D8            NMI                                   I               Nonmaskable interrupt request
126          C8          IRL3                                  I               External interrupt source input
127          A8          IRL2                                  I               External interrupt source input
128          B8          IRL1                                  I               External interrupt source input
129          D7          IRL0                                  I               External interrupt source input
130          C7            D0                                     I/O           Data bus
131          A7            D1                                     I/O           Data bus
132          B7            V  CC                                    I               Power
133          D6            D2                                     I/O           Data bus
134          C6            V  SS                                    I               Ground
135          A6            D3                                     I/O           Data bus
136          B6            D4                                     I/O           Data bus
137          C5            D5                                     I/O           Data bus
138          A5            D6                                     I/O           Data bus
139          B5            V  CC                                    I               Power
140          D5            D7                                     I/O           Data bus
141          A4            V  SS                                    I               Ground
142          B4            D8                                     I/O           Data bus
143          C4            D9                                     I/O           Data bus
144          A3            D10                                   I/O           Data bus
—            D4            NC                                    —             Reserved pin (leave unconnected)
—            B3            NC                                    —             Reserved pin (leave unconnected)
—            A2            NC                                    —             Reserved pin (leave unconnected)
—            B2            NC                                    —             Reserved pin (leave unconnected)





14
```
