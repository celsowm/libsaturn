# Appendix B List of Registers

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=581)

Physical PDF pages 581–630. Text extraction; consult the PDF for figures.

## Bookmarks

- [B.1 List of I/O Registers](#pdf-page-581)
- [B.2 Register Chart](#pdf-page-589)

## PDF page 581

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=581)

```text
                              Appendix B   List of Registers


B.1    List of I/O Registers


                 Abbrevia-
                 tion of                                                          Bit Name
Address           Register     Bit 7       Bit 6       Bit 5       Bit 4       Bit 3       Bit 2       Bit 1       Bit 0       Module

H'FFFFFE00     SMR           C/A        CHR       PE          O/E        STOP     MP         CKS1     CKS0      SCI

H'FFFFFE01     BRR

H'FFFFFE02     SCR            TIE         RIE        TE          RE         MPIE      TEIE      CKE1     CKE0

H'FFFFFE03     TDR

H'FFFFFE04     SSR            TDRE     RDRF    ORER    FER       PER       TEND     MPB       MPBT

H'FFFFFE05     RDR

H'FFFFFE06       ——————————
to
H'FFFFFE09

H'FFFFFE10     TIER           ICIE       —           —           —           OCIAE   OCIBE   OVIE      —            FRT

H'FFFFFE11     FTCSR       ICF         —           —           —           OCFA    OCFB    OVF       CCLRA

H'FFFFFE12     FRC

H'FFFFFE13     OCRA/B

H'FFFFFE14

H'FFFFFE15     TCR

H'FFFFFE16                        IEDGA—————CKS1     CKS0

H'FFFFFE17     TOCR         —           —           —           OCRS    —           —           OLVLA   OLVLB

H'FFFFFE18     FICR

H'FFFFFE19

H'FFFFFE20       ——————————
to
H'FFFFFE59

H'FFFFFE60     IPRB           SCIIP3   SCIIP2   SCIIP1   SCIIP0   FRTIP3  FRTIP2  FRTIP1  FRTIP0   INTC

H'FFFFFE61  ————————

H'FFFFFE62     VCRA         —           SERV6   SERV5   SERV4   SERV3   SERV2   SERV1   SERV0

H'FFFFFE63                        —           SRXV6   SRXV5   SRXV4   SRXV3   SRXV2   SRXV1   SRXV0

H'FFFFFE64     VCRB         —           STXV6   STXV5   STXV4   STXV3   STXV2   STXV1   STXV0

H'FFFFFE65                        —           STEV6   STEV5   STEV4   STEV3   STEV2   STEV1   STEV0




                                                                                                                    565
```

## PDF page 582

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=582)

```text
            Abbrevia-
            tion of                                                          Bit Name
Address           Register     Bit 7       Bit 6       Bit 5       Bit 4       Bit 3       Bit 2       Bit 1       Bit 0       Module

H'FFFFFE66     VCRC         —           FICV6    FICV5    FICV4    FICV3    FICV2    FICV1    FICV0     INTC

H'FFFFFE67                        —           FOCV6  FOCV5  FOCV4  FOCV3  FOCV2  FOCV1  FOCV0

H'FFFFFE68     VCRD         —           FOVV6   FOVV5   FOVV4   FOVV3   FOVV2   FOVV1   FOVV0

H'FFFFFE69  ————————

H'FFFFFE6A  ——————————
to
H'FFFFFE70

H'FFFFFE71     DRCR0 ——————RS1RS0DMAC
                                                                                  (channel 0)

H'FFFFFE72     DRCR1 ——————RS1RS0DMAC
                                                                                  (channel 1)

H'FFFFFE73  ——————————
to
H'FFFFFE7F

H'FFFFFE80     WTCSR*     OVF       WT/IT    TME       —-          —           CKS2     CKS1     CKS0      WDT

H'FFFFFE81     WTCNT*

H'FFFFFE82     — ————————

H'FFFFFE83     RSTCSR*   WOVF    RSTE     RSTS—————

H'FFFFFE84  —————————
to
H'FFFFFE90

H'FFFFFE91     SBYCR       SBY       HIZ         —           MSTP4  MSTP3  MSTP2  MSTP1  MSTP0   Power-
                                                                                  down

H'FFFFFE92     CCR            W1         W0         —           CP         TW         OC         ID           CE          Cache

H'FFFFFE93  ——————————
to
H'FFFFFE9F
Note:   Address for reading. When writing, the address is H'FFFFFE80 for WTCSR and WTCNT,
      and H'FFFFFE82 for RSTCSR. See Section 12.2.4, Register Access, in Section 12,
      Watchdog Timer (WDT), for more information.












566
```

## PDF page 583

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=583)

```text
            Abbrevia-
            tion of                                                          Bit Name
Address           Register     Bit 7       Bit 6       Bit 5       Bit 4       Bit 3       Bit 2       Bit 1       Bit 0       Module

H'FFFFFEE0    ICR             NIMIL——————NIMIE     INTC

H'FFFFFEE1  ———————VECMD

H'FFFFFEE2           DIVUIP3  DIVUIP2  DIVUIP1  DIVUIP0  DMACI3   DMACI2   DMACI1   DMACI0
            IPRA

H'FFFFFEE3                           WDTIP3   WDTIP2   WDTIP1   WDTIP0————

H'FFFFFEE4    VCRWDT    —           WITV6   WITV5   WITV4   WITV3   WITV2   WITV1   WITV0

H'FFFFFEE5                       —           BCMV6  BCMV5  BCMV4  BCMV3  BCMV2  BCMV1  BCMV0

H'FFFFFEE6  ——————————
to
H'FFFFFEFF

H'FFFFFF00     DVSR                                                                                                                             DIVU

H'FFFFFF01

H'FFFFFF02

H'FFFFFF03

H'FFFFFF04     DVDNT

H'FFFFFF05

H'FFFFFF06

H'FFFFFF07

H'FFFFFF08     DVCR ————————

H'FFFFFF09  ————————

H'FFFFFF0A  ————————

H'FFFFFF0B  ——————OVFIE    OVF

H'FFFFFF0C    VCRDIV————————

H'FFFFFF0D  ————————

H'FFFFFF0E

H'FFFFFF0F

H'FFFFFF10     DVDNTH

H'FFFFFF11

H'FFFFFF12

H'FFFFFF13






                                                                                   567
```

## PDF page 584

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=584)

```text
                 Abbrevia-
                 tion of                                                          Bit Name
Address           Register     Bit 7       Bit 6       Bit 5       Bit 4       Bit 3       Bit 2       Bit 1       Bit 0       Module

H'FFFFFF14     DVDNTL                                                                                                                         DIVU

H'FFFFFF15

H'FFFFFF16

H'FFFFFF17

H'FFFFFF18       ——————————
to
H'FFFFFF3F

H'FFFFFF40     BARAH       BAA31   BAA30   BAA29   BAA28   BAA27   BAA26   BAA25   BAA24    UBC

H'FFFFFF41                        BAA23   BAA22   BAA21   BAA20   BAA19   BAA18   BAA17   BAA16    (channel A)

H'FFFFFF42     BARAL        BAA15   BAA14   BAA13   BAA12   BAA11   BAA10   BAA9     BAA8

H'FFFFFF43                        BAA7     BAA6     BAA5     BAA4     BAA3     BAA2     BAA1     BAA0

H'FFFFFF44     BAMRAH     BAMA31  BAMA30  BAMA29  BAMA28  BAMA27  BAMA26  BAMA25  BAMA24

H'FFFFFF45                           BAMA23  BAMA22  BAMA21  BAMA20  BAMA19  BAMA18  BAMA17  BAMA16

H'FFFFFF46     BAMRAL     BAMA15  BAMA14  BAMA13  BAMA12  BAMA11  BAMA10  BAMA9    BAMA8

H'FFFFFF47                           BAMA7    BAMA6    BAMA5    BAMA4    BAMA3    BAMA2    BAMA1    BAMA0

H'FFFFFF48     BBRA ————————

H'FFFFFF49                        CPA1     CPA0     IDA1      IDA0      RWA1    RWA0    SZA1     SZA0

H'FFFFFF4A       ——————————
to
H'FFFFFF5F

H'FFFFFF60     BARBH       BAB31   BAB30   BAB29   BAB28   BAB27   BAB26   BAB25   BAB24    UBC

H'FFFFFF61                        BAB23   BAB22   BAB21   BAB20   BAB19   BAB18   BAB17   BAB16    (channel B)

H'FFFFFF62     BARBL        BAB15   BAB14   BAB13   BAB12   BAB11   BAB10   BAB9     BAB8

H'FFFFFF63                        BAB7     BAB6     BAB5     BAB4     BAB3     BAB2     BAB1     BAB0

H'FFFFFF64     BAMRBH     BAMB31  BAMB30  BAMB29  BAMB28  BAMB27  BAMB26  BAMB25  BAMB24

H'FFFFFF65                           BAMB23  BAMB22  BAMB21  BAMB20  BAMB19  BAMB18  BAMB17  BAMB16














568
```

## PDF page 585

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=585)

```text
                 Abbrevia-
                 tion of                                                          Bit Name
Address           Register     Bit 7       Bit 6       Bit 5       Bit 4       Bit 3       Bit 2       Bit 1       Bit 0       Module

H'FFFFFF66     BAMRBL     BAMB15  BAMB14  BAMB13  BAMB12  BAMB11  BAMB10  BAMB9    BAMB8     UBC

H'FFFFFF67                        BAMB7  BAMB6  BAMB5  BAMB4  BAMB3  BAMB2  BAMB1  BAMB0   (channel B)

H'FFFFFF68     BBRB ————————

H'FFFFFF69                        CPB1     CPB0     IDB1      IDB0      RWB1    RWB0    SZB1     SZB0

H'FFFFFF6A       —————————
to
H'FFFFFF6F

H'FFFFFF70     BDRBH       BDB31   BDB30   BDB29   BDB28   BDB27   BDB26   BDB25   BDB24

H'FFFFFF71                        BDB23   BDB22   BDB21   BDB20   BDB19   BDB18   BDB17   BDB16

H'FFFFFF72     BDRBL       BDB15   BDB14   BDB13   BDB12   BDB11   BDB10   BDB9     BDB8

H'FFFFFF73                        BDB7     BDB6     BDB5     BDB4     BDB3     BDB2     BDB1     BDB0

H'FFFFFF74     BDMRBH    BDMB31  BDMB30  BDMB29  BDMB28  BDMB27  BDMB26  BDMB25  BDMB24

H'FFFFFF75                           BDMB23  BDMB22  BDMB21  BDMB20  BDMB19  BDMB18  BDMB17  BDMB16

H'FFFFFF76     BDMRBL     BDMB15  BDMB14  BDMB13  BDMB12  BDMB11  BDMB10  BDMB9    BDMB8

H'FFFFFF77                           BDMB7    BDMB6    BDMB5    BDMB4    BDMB3    BDMB2    BDMB1    BDMB0

H'FFFFFF78     BRCR          CMFCA    CMFPA   EBBE     UMD      —           PCBA     —           —

H'FFFFFF79                           CMFCB    CMFPB   —           SEQ       DBEB     PCBB     —           —

H'FFFFFF7A       ——————————
to
H'FFFFFF7F

























                                                                                                                    569
```

## PDF page 586

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=586)

```text
                  Abbrevia-
                  tion of                                                          Bit Name
Address           Register     Bit 7       Bit 6       Bit 5       Bit 4       Bit 3       Bit 2       Bit 1       Bit 0       Module

H'FFFFFF80     SAR0                                                                                                                              DMAC

H'FFFFFF81                                                                                                                                            (channel 0)

H'FFFFFF82

H'FFFFFF83

H'FFFFFF84     DAR0

H'FFFFFF85

H'FFFFFF86

H'FFFFFF87

H'FFFFFF88     TCR0 ————————

H'FFFFFF89

H'FFFFFF8A

H'FFFFFF8B

H'FFFFFF8C    CHCR0 ————————

H'FFFFFF8D  ————————

H'FFFFFF8E                        DM1       DM0       SM1       SM0       TS1        TS0        AR         AM

H'FFFFFF8F                        AL          DS         DL          TB          TA          IE           TE          DE

H'FFFFFF90     SAR1                                                                                                                              DMAC

H'FFFFFF91                                                                                                                                            (channel 1)

H'FFFFFF92

H'FFFFFF93

H'FFFFFF94     DAR1

H'FFFFFF95

H'FFFFFF96

H'FFFFFF97

H'FFFFFF98     TCR1 ————————

H'FFFFFF99

H'FFFFFF9A

H'FFFFFF9B








570
```

## PDF page 587

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=587)

```text
          Abbrevia-
          tion of                                                          Bit Name
Address           Register     Bit 7       Bit 6       Bit 5       Bit 4       Bit 3       Bit 2       Bit 1       Bit 0       Module

H'FFFFFF9C    CHCR1 ———————— DMAC

H'FFFFFF9D  ———————— (channel 1)

H'FFFFFF9E                        DM1       MD0       SM1       SM0       TS1        TS0        AR         AM

H'FFFFFF9F                        AL          DS         DL          TB          TA          IE           TE          DE

H'FFFFFFA0    VCRMA0———————— DMAC

H'FFFFFFA1  ———————— (channel 0)

H'FFFFFFA2  ————————

H'FFFFFFA3                        VC7       VC6       VC5       VC4       VC3       VC2       VC1       VC0

H'FFFFFFA4—————————
to
H'FFFFFFA7

H'FFFFFFA8    VCRDMA1———————— DMAC

H'FFFFFFA9  ———————— (channel 1)

H'FFFFFFAA  ————————

H'FFFFFFAB                       VC7       VC6       VC5       VC4       VC3       VC2       VC1       VC0

H'FFFFFFAC—————————
to
H'FFFFFFAF

H'FFFFFFB0    DMAOR ———————— DMAC

H'FFFFFFB1  ———————— (channels

H'FFFFFFB2  ———————— 0 and 1)

H'FFFFFFB3  ————PRAENMIF      DME

H'FFFFFFB4—————————
to
H'FFFFFFDF

















                                                                    571
```

## PDF page 588

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=588)

```text
          Abbrevia-
          tion of                                                          Bit Name
Address           Register     Bit 7       Bit 6       Bit 5       Bit 4       Bit 3       Bit 2       Bit 1       Bit 0       Module

H'FFFFFFE0  ————————BSC

H'FFFFFFE1  ————————

H'FFFFFFE2    BCR1           MASTR——ENDIAN   BSTROM   PSHR    AHLW1  AHLW0

H'FFFFFFE3                        A1LW1   A1LW0   A0LW1   A0LW0   —           DRAM2  DRAM1  DRAM0

H'FFFFFFE4    BCR2 ————————

H'FFFFFFE5  ————————

H'FFFFFFE6  ————————

H'FFFFFFE7                        A3SZ1   A3SZ0   A2SZ1   A2SZ0   A1SZ1   A1SZ0   —           —

H'FFFFFFE8    WCR ————————

H'FFFFFFE9  ————————

H'FFFFFFEA                       IW31      IW30      IW20      IW21      IW10      IW11      IW01      IW00

H'FFFFFFEB                       W31       W30       W20       W21       W10       W11       W01       W00

H'FFFFFFEC    MCR ————————

H'FFFFFFED  ————————

H'FFFFFFEE                       TRP       RCD       TRWL    TRAS1   TRS0     BE          RASD    —

H'FFFFFFEF                       AMX2     SZ          AMX1     AMX0     RFSH     RMD      —           —

H'FFFFFFF0     RTCSR ————————

H'FFFFFFF1  ————————

H'FFFFFFF2  ————————

H'FFFFFFF3                        CMF       CMIE     CKS2     CKS1     CKS0     —           —           —

H'FFFFFFF4     RTCNT ————————

H'FFFFFFF5

H'FFFFFFF6

H'FFFFFFF7

H'FFFFFFF8     RTCOR ————————

H'FFFFFFF9

H'FFFFFFFA

H'FFFFFFFB

H'FFFFFFFC——————————
to
H'FFFFFFFF




572
```

## PDF page 589

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=589)

```text
B.2    Register Chart

                         Register name (abbreviation)                              Access size                          Module
                                                                  Start address
                                                                                                                      SCI



                        Serial mode register (SMR)          H'FFFFFE00                        8


                          Item             7                                      Bit
   Register                                          6           5           4           3           2           1           0
   overview            Bit Name        C/A         CHR          PE         O/E        STOP          MP        CKS1        CKS0
                      Initial Value      0           0           0           0           0           0           0           0
                          R/W          R/W         R/W         R/W         R/W         R/W         R/W         R/W         R/W


                       Bit    Bit Name                        Value        Description
                        7    Communication mode                 0          Asynchronous mode (Initial value)
                             (C/A)                              1          Clocked synchronous mode
                        6    Character length                   0          Eight-bit data (Initial value)
                             (CHR)                              1          Seven-bit data
                        5    Parity enable                      0          Parity bit not added or checked (Initial value)
                             (PE)                               1          Parity bit added and checked
   Bit                  4    Parity mode                        0          Even parity (Initial value)
   function                  (OE)                               1          Odd parity
                        3    Stop bit length                    0          One stop bit (Initial value)
                             (STOP)                             1          Two stop bits
                        2    Multiprocessor mode                0          Multiprocessor function disabled (Initial value)
                             (MP)                               1          Multiprocessor format selected
                        1    Clock select 1 and 0            0        0    φ/4 (Initial value)
                             (CKS1, CKS0)                    0        1    φ/16
                        0                                    1        0    φ/64
                                                             1        1    φ/256

                  Bit number      Bit name                       Bit value                                      Bit description
                                       (abbreviation)            (When there is a set of bits,
                                                                 the upper bit is on the left,
                                                                 and the lower bit on the right.)



















                                                                                                                                   573
```

## PDF page 590

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=590)

```text
                SCI



    Serial mode register (SMR)                H'FFFFFE00                    8

                                                                                Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name          C/A          CHR           PE           O/E         STOP         MP         CKS1        CKS0
 Initial Value          0               0               0               0               0               0               0               0
       R/W             R/W          R/W          R/W          R/W          R/W          R/W          R/W          R/W


     Bit                  Bit Name             Value                                     Description
      7       Communication mode         0     Asynchronous mode (Initial value)
              (C/A)                                    1     Clocked synchronous mode
      6       Character length                 0     Eight-bit data (Initial value)
              (CHR)                                  1     Seven-bit data
      5       Parity enable                       0     Parity bit not added or checked (Initial value)
              (PE)                                     1     Parity bit added and checked
      4       Parity mode                         0     Even parity (Initial value)
              (OE)                                    1     Odd parity
      3       Stop bit length                     0     One stop bit (Initial value)
              (STOP)                                1     Two stop bits
      2       Multiprocessor mode           0     Multiprocessor function disabled (Initial value)
              (MP)                                    1     Multiprocessor format selected
      1       Clock select 1 and 0         0    0  f/4 (Initial value)
              (CKS1, CKS0)                  0    1  f/16
      010f/64
                                                    11f/256




Bit rate register (BRR)                     H'FFFFFE01                            8

                                                                                Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name
 Initial Value          1               1               1               1               1               1               1               1
       R/W             R/W          R/W          R/W          R/W          R/W          R/W          R/W          R/W


      Bit                    Bit Name                                                    Description
    7 to 0     (Bit rate setting)                 Sets serial transmit/receive bit rate








574
```

## PDF page 591

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=591)

```text
                                                                                                                   SCI


Serial control register (SCR)            H'FFFFFE02                            8

                                                                            Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name          TIE           RIE            TE            RE          MPIE        TEIE        CKE1        CKE0
 Initial Value          0               0               0               0               0               0               0               0
      R/W             R/W          R/W          R/W          R/W          R/W          R/W          R/W          R/W

    Bit                  Bit Name             Value                                     Description
     7       Transmit interrupt
             enable (TIE)                        0     Transmit-data-empty interrupt request (TXI) is disabled(Initial value)
                                                   1     Transmit-data-empty interrupt request (TXI) is enabled
     6       Receive interrupt enable
             (RIE)                                    0     Receive-data-full interrupt (RXI) and receive-errorinterrupt (ERI) requests are disabled (Initial value)
                                                   1     Receive-data-full interrupt (RXI) and receive-error
                                                         interrupt (ERI) requests are enabled
     5       Transmit enable (TE)          0     Transmitter disabled (Initial value)
                                                   1     Transmitter enabled
     4       Receive enable (RE)           0     Receiver disabled (Initial value)
                                                   1     Receiver enabled
     3       Multiprocessor interrupt
             enable (MPIE)                     0     Multiprocessor interrupts are disabled (nomal receiveoperation) (Initial value).
                                                         MPE is cleared to 0 when MPIE is cleared to 0, or the
                                                         multiprocessor bit (MPB) is set to 1 in receive data.
                                                   1     Multiprocessor interrupts are enabled. Receive-data-full
                                                         interrupt requests (RXI), receive-error interrupt requests
                                                         (ERI), and setting of the RDRF, FER, and ORER status
                                                         flags in the serial status register (SSR) are disabled until
                                                         the multiprocessor bit is set to 1.
     2       Transmit-end interrupt
             enable (TEIE)                      0     Transmit-end interrupt (TEI) requests are disabled(Initial value)
                                                   1     Transmit-end interrupt (TEI) requests are enabled
    1, 0     Clock enable 1 and 0
             (CKE1 and CKE2)            0    0  Asynchronousmode                         Internal clock, SCK pin used forinput pin (input signal is ignored or
                                                                                       output pin output level is undefined)
                                                         Clocked
                                                         synchronous mode    Internal clock, SCK pin used forsynchronous clock output
                                                 0    1  Asynchronous
                                                         mode                         Internal clock, SCK pin used forclock output
                                                         Clocked
                                                         synchronous mode    Internal clock, SCK pin used forsynchronous clock output
                                                 1    0  Asynchronous
                                                         mode                         Internal clock, SCK pin used forclock input
                                                         Clocked
                                                         synchronous mode    Internal clock, SCK pin used forsynchronous clock input
                                                 1    1  Asynchronous
                                                         mode                         Internal clock, SCK pin used forclock input
                                                         Clocked
                                                         synchronous mode    Internal clock, SCK pin used forsynchronous clock input






                                                                                                                                   575
```

## PDF page 592

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=592)

```text
                SCI



Transmit data register (TDR)           H'FFFFFE03                            8

                                                                               Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name
 Initial Value          1               1               1               1               1               1               1               1
       R/W             R/W          R/W          R/W          R/W          R/W          R/W          R/W          R/W


      Bit                    Bit Name                                                    Description
    7 to 0     (Stores transmit data)       Stores data for serial transmission




Serial status register (SSR)             H'FFFFFE04                            8

                                                                               Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name        TDRE       RDRF       ORER        FER          PER        TEND        MPB         MPBT
 Initial Value          1               0               0               0               0               1               0               0
       R/W           R(W)*       R(W)*       R(W)*       R(W)*       R(W)*           R               R             R/W
Note: Only 0 can be written to clear flags.


     Bit                  Bit Name             Value                                     Description
      7       Transmit data register                 0     TDR contains valid transmit data
              empty (TDRE)                                  TDRE is cleared to 0 when software reads TDRE after it
                                                            has been set to 1, then writes 0 in TDRE, or the DMAC
                                                            writes data in TDR.
                                                     1     TDR does not contain valid transmit data (Initial value)
                                                            TDRE is set to 1 when the chip is reset or enters standby
                                                            mode, the TE bit in the serial control register (SCR) is
                                                            cleared to 0, or TDR contents are loaded into TSR, so
                                                            new data can be written in TDR.
      6       Receive data register                  0     RDR does not contain valid received data (Initial value)
              full (RDRF)                                   RDRF is cleared to 0 when the chip is reset or enters
                                                            standby mode, software reads RDRF after it has been set
                                                            to 1, then writes 0 in RDRF, or the DMAC reads data
                                                            from RDR.
                                                     1     RDR contains valid received data
                                                            RDRF is set to 1 when serial data is received normally
                                                            and transferred from RSR to RDR.







576
```

## PDF page 593

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=593)

```text
                                                                                                          SCI

Bit                  Bit Name             Value                                     Description
5       Overrun error (ORER)         0     Receiving is in progress or has ended normally (Initial
                                                  value)
                                                  ORER is cleared to 0 when the chip is reset or enters
                                                  standby mode, or software reads ORER after it has been
                                                  set to 1, then writes 0 in ORER.
                                            1     A receive overrun error occurred
                                                  ORER is set to 1 if reception of the next serial data ends
                                                  when RDRF is set to 1.
4       Framing error (FER)            0     Receiving is in progress or has ended normally (Initial
                                                  value)
                                                  FER is cleared to 0 when the chip is reset or enters
                                                  standby mode, or software reads FER after it has been
                                                  set to 1, then writes 0 in FER.
                                            1     A receive framing error occurred
                                                  FER is set to 1 if the stop bit at the end of receive data is
                                                  checked and found to be 0.
3       Parity error (PER)               0     Receiving is in progress or has ended nomally (Initial
                                                  value)
                                                  PER is cleared to 0 when the chip is reset or enters
                                                  standby mode or software reads PER after it has been
                                                  set to 1, then writes 0 in PER.
                                            1     A receive parity error occurred
                                                  PER is set to 1 if the number of ls in receive data,
                                                  including the parity bit, does not match the even or odd
                                                  parity setting of the parity mode bit (O/E) in the serial
                                                  mode register (SMR).
2       Transmit end (TEND)          0     Transmission is in progress
                                                  TEND is cleared to 0 when software reads TDRD after it
                                                  has been set to 1, then writes 0 in TDRE, or the DMAC
                                                  writes data in TDR.
                                            1     End of transmission (Initial value)
                                                  TEND is set to 1 when the chip is reset or enters standby
                                                  mode, TE is cleared to 0 in the serial control register
                                                  (SCR), or TDRE is 1 when the last bit of a one-byte serial
                                                  character is transmitted.
1       Multiprocessor bit                0     Multiprocessor bit value in receive data is 0 (Initial value)
        (MPB)                                  1     Multiprocessor bit value in receive data is 1
0       Multiprocessor bit                  0     Multiprocessor bit value in transmit data is 0 (Initial value)
        transfer (MPBT)
                                            1     Multiprocessor bit value in transmit data is 1













                                                                                                                         577
```

## PDF page 594

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=594)

```text
              SCI



Receive data register (RDR)            H'FFFFFE05                            8

                                                                      Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name
 Initial Value          0               0               0               0               0               0               0               0
      R/WRRRRRRRR


     Bit                    Bit Name                                                    Description
   7 to 0     (Stores serial receive             Stores the received serial data
               data)












































578
```

## PDF page 595

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=595)

```text
                                                                                                                          FRT

Timer interrupt enable register
(TIER)                                              H'FFFFFE10                            8

                                                                                 Bit
      Item               7               6               5               4               3               2               1               0
     Name            ICIE            —              —             —          OCIAE      OCIBE       OVIE           —
 Initial Value          0               0               0               0               0               0               0               1
       R/W             R/W          R/W          R/W          R/W          R/W          R/W          R/W          R/W

     Bit                  Bit Name             Value                                     Description
      7       Input capture interrupt                  0     Disables interrupt requests (ICI) from ICF (Initial value)
              enable (ICIE)
                                                       1     Enables interrupt requests (ICI) from the ICF
      3       Output compare                           0     Disables interrupt requests (OCIA) from OCFA
              interrupt A enable                             (Initial value)
              (OCIAE)                              1     Enables interrupt requests (OCIA) from OCFA
      2       Output compare                           0     Disables interrupt requests (OCIB) from OCFB
              interrupt B enable                             (Initial value)
              (OCIBE)                              1     Enables interrupt requests (OCIB) from OCFB
      1       Timer overflow interrupt                 0     Disables interrupt requests (OVI) from OVF (Initial value)
              enable (OVIE)
                                                       1     Enables interrupt requests (OVI) from OVF


Free-running timer control/status
register (FTCSR)                             H'FFFFFE11                            8

                                                                                 Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name          ICF            —              —             —          OCFA       OCFB        OVF       CCLRA
 Initial Value          0               0               0               0               0               0               0               0
       R/W           R/(W)*          —              —             —          R/(W)*      R/(W)*      R/(W)*        R/W
Note:   For bits 7, and 3 to 1, the only value that can be written is 0 (to clear the flags)

     Bit                  Bit Name             Value                                     Description
      7       Input capture flag (ICF)       0     Clear conditions: When ICF = 1, ICF is read and then 0 is
                                                             written to it (Initial value)
                                                       1     Set conditions: When FRC value is sent to ICR by the
                                                             input capture signal
      3       Output compare flag A
              (OCFA)                                0     Clear conditions: When OCFA = 1, OCFA is read andthen 0 is written to it (Initial value)
                                                       1     Set conditions: When FRC value becomes equal to
                                                             OCRA










                                                                                                                                           579
```

## PDF page 596

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=596)

```text
             FRT

    Bit                  Bit Name             Value                                     Description
    2       Output compare flag B           0     Clear conditions: When OCFB = 1, OCFB is read and
           (OCFB)                                then 0 is written to it (Initial value)
                                            1     Set conditions: When FRC value becomes equal to
                                                 OCRB
    1       Timer overflow flag             0     Clear conditions: When OVF = 1, OVF is read and then 0
           (OVF)                                 is written to it (Initial value)
                                            1     Set conditions: When FRC value changes from H'FFFF to
                                                 H'0000
    0       Counter clear A                   0     Disables FRC clear (Initial value)
           (CCLRA)                             1     Clears FRC on compare match A



                                         H'FFFFFE12(FRCH)
Free-running counter (FRC)               H'FFFFFE13(FRCL)              16*
Note:   Access FRCH first and then FRCL, two 8-bit units.


                                                                Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  15 to 0    (Count value)                    Counts input clock pulses



Output compare register A/B*1            H'FFFFFE14(OCRA/BH
(OCRA/B)                                 H'FFFFFE15(OCRA/BL)             16*2
Notes:  1.   Switch registers with OCRS in TOCR.
         2.   Access OCRA/BH first and then OCRA/BL, in two 8-bit units.


                                                                Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  15 to 0    (FRC value comparison)   Sets OCFA when OCFA = FRC
                                            Sets OCFB when OCFB = FFC





580
```

## PDF page 597

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=597)

```text
                                                                                                                               FRT



Timer control register (TCR)            H'FFFFFE16                            8

                                                                                    Bit
       Item               7               6               5               4               3               2               1               0
   Bit Name       IEDGA          —              —             —              —             —          CKS1        CKS0
 Initial Value          0               0               0               0               0               0               0               0
       R/W             R/W          R/W          R/W          R/W          R/W          R/W          R/W          R/W


     Bit                  Bit Name             Value                                     Description
      7       Input edge select                 0     Captures input on falling edge (Initial value)
               (IEDG)                                 1     Captures input on rising edge
    1, 0     Clock selects                    0    0  Internal clock: count on f/8 (Initial value)
               (CKS1 and CKS0)            0    1  Internal clock: count on f/32
                                                      1    0  Internal clock: count on f/128
                                                      1    1  External clock: count on rising edge



Timer output compare control
register (TOCR)                               H'FFFFFE17                            8


                                                                                    Bit
       Item               7               6               5               4               3               2               1               0
   Bit Name           —             —              —          OCRS          —             —          OLVLA      OLVLB
 Initial Value          1               1               1               0               0               0               0               0
       R/W               —             —              —            R/W          R/W          R/W          R/W          R/W


     Bit                  Bit Name             Value                                     Description
      4       Output compare register     0     Selects OCRA register                      (Initial value)
               select (OCRS)                     1     Selects OCRB register
      1       Output level A (OLVLA)      0     Outputs 0 on compare match A        (Initial value)
                                                         1     Outputs 1 on compare match A
      0       Output level B (OLVLB)      0     Outputs 0 on compare match B        (Initial value)
                                                         1     Outputs 1 on compare match B












                                                                                                                                                 581
```

## PDF page 598

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=598)

```text
          FRT


                                 H'FFFFFE18 (ICRH)
Input capture register (ICR)     H'FFFFFE19 (ICRL)              16*
Note:   Access ICRH first and then ICRL, in two 8-bit units.


                                                    Bit
    Item            15     14     13     12      11     10      9       8 76543210
  Bit Name
 Initial Value 0000000000000000
    R/W RRRRRRRRRRRRRRRR


    Bit                    Bit Name                                                    Description
  15 to 0    (Stores FRC value)           Stores FRC value when an input capture signal occurs











































582
```

## PDF page 599

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=599)

```text
                                                                                             INTC



Interrupt priority level setting
register A (IPRA)                              H'FFFFFEE2                         8/16


                                                              Bit
    Item          15      14      13      12      11       10         9         8 76543210
 Bit Name    DIVU DIVU DIVU  DIVU DMACDMAC DMACDMAC  WDT  WDT  WDT  WDT
              IP3    IP2    IP1     IP0     IP3     IP2      IP1     IP0      P3     IP2    IP1    IP0     —     —     —     —
Initial Value      0        0        0        0        0         0         0         0 00000000
    R/W         R/W   R/W   R/W   R/W   R/W    R/W    R/W    R/W    R/W   R/W   R/W   R/W     R      R      R      R


     Bit                            Bit Name                                                      Description
 15 to 12   Division unit (DIVU) interrupt           These bits set the division unit (DIVU) interrupt
             priority level (DIVUIP3–DIVUIP0)        priority level
  11 to 8    DMA controller interrupt priority       These bits set the DMA controller (DMAC) interrupt
             level (DMACIP3–DMACIP0)                 priority level
   7 to 4     Watchdog timer (WDT) interrupt         These bits set the watchdog timer (WDT) interrupt
             priority level (WDTIP3–WDTIP0)          priority level and bus state controller (BSC) interrupt
                                                     priority level



Interrupt priority level setting
register B (IPRB)                              H'FFFFFE60                         8/16


                                                              Bit
    Item          15      14      13      12      11      10       9        8 76543210
 Bit Name     SCI    SCI    SCI    SCI    FRT   FRT   FRT   FRT
              IP3IP2IP1IP0IP3IP2IP1IP0————————
Initial Value 0000000000000000
    R/WR/WR/WR/WR/WR/WR/WR/WR/WRRRRRRRR


     Bit                            Bit Name                                                      Description
 15 to 12   Serial communication interface           These bits set the serial communication interface
             (SCI) interrupt priority level          (SCI) interrupt priority level
             (SCIIP3–SCIIP0)
  11 to 8    Free-running timer (FRT) interrupt      These bits set the free-running timer (FRT) interrupt
             priority level (FRTIP3–FRTIP0)          priority level










                                                                                                           583
```

## PDF page 600

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=600)

```text
             INTC


Vector number setting register A
(VCRA)                                            H'FFFFFE62                         8/16



                                                                     Bit
     Item          15      14      13      12      11      10       9        8 76543210
  Bit Name               SER  SER  SER   SER  SER  SER  SER             SRX  SRX  SRX   SRX  SRX  SRX  SRX
                 —      V6     V5     V4      V3     V2     V1     V0      —      V6     V5     V4      V3     V2     V1     V0
Initial Value 0000000000000000
     R/W           R     R/W   R/W   R/W   R/W   R/W   R/W   R/W      R     R/W   R/W   R/W   R/W   R/W   R/W   R/W



     Bit                            Bit Name                                                      Description
   14 to 8    Serial communication interface               These bits set the vector number for the serial
               (SCI) receive-error interrupt vector        communication interface (SCI) receive-error interrupt
               number (SERV6–SERV0)                        (ERI)
   6 to 0     Serial communication interface               These bits set the vector number for the serial
               (SCI) receive-data-full interrupt           communication interface (SCI) receive-data-full
               vector number (SRXV6–SRXV0)                 interrupt (RXI)



Vector number setting register B
(VCRB)                                            H'FFFFFE64                         8/16


                                                                     Bit
     Item          15      14      13      12      11      10       9        8 76543210
  Bit Name               STX   STX   STX   STX   STX   STX   STX             STE   STE   STE   STE   STE   STE   STE
                 —      V6     V5     V4      V3     V2     V1     V0      —      V6     V5     V4      V3     V2     V1     V0
Initial Value 0000000000000000
     R/W           R     R/W   R/W   R/W   R/W   R/W   R/W   R/W      R     R/W   R/W   R/W   R/W   R/W   R/W   R/W


     Bit                            Bit Name                                                      Description
   14 to 8    Serial communication interface               These bits set the vector number for the serial
               (SCI) transmit-data-empty                   communication interface (SCI) transmit-data-empty
               interrupt vector number (STXV6–             interrupt (TXI)
               STXV0)
   6 to 0     Serial communication interface               These bits set the vector number for the serial
               (SCI) transmit-end interrupt vector         communication interface (SCI) transmit-end interrupt
               number (STEV6–STEV0)                        (TEI)









584
```

## PDF page 601

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=601)

```text
                                                                                          INTC



Vector number setting register C
(VCRC)                                            H'FFFFFE66                         8/16


                                                           Bit
    Item          15      14      13      12      11      10       9        8 76543210
 Bit Name               FIC    FIC    FIC     FIC    FIC    FIC    FIC              FOC  FOC  FOC   FOC  FOC  FOC  FOC
              —      V6     V5     V4      V3     V2     V1     V0      —      V6     V5     V4      V3     V2     V1     V0
Initial Value 0000000000000000
    R/W           R     R/W   R/W   R/W   R/W   R/W   R/W   R/W      R     R/W   R/W   R/W   R/W   R/W   R/W   R/W


    Bit                            Bit Name                                                      Description
  14 to 8    Free-running timer (FRT) input-       These bits set the vector number for the free-running
             capture interrupt vector number       timer (FRT) input-capture interrupt (ICI)
             (FICV6–FICV0)
   6 to 0     Free-running timer (FRT) output-     These bits set the vector number for the free-running
             compare interrupt vector number       timer (FRT) output-compare interrupt (OCI)
             (FOCV6–FOCV0)



Vector number setting register D
(VCRD)                                            H'FFFFFE68                         8/16


                                                           Bit
    Item          15      14      13      12      11      10       9        8 76543210
 Bit Name               FOV  FOV  FOV   FOV  FOV  FOV  FOV
              —V6V5V4V3V2V1V0————————
Initial Value 0000000000000000
    R/WRR/WR/WR/WR/WR/WR/WR/WRRRRRRRR


    Bit                            Bit Name                                                      Description
  14 to 8    Free-running timer (FRT) overflow     These bit set the vector number for the free-running
             interrupt vector number (FOVV6–       timer(FRT) overflow interrupt (OVI)
             FOVV0)














                                                                                                       585
```

## PDF page 602

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=602)

```text
           INTC


Vector number setting register
WDT (VCRWDT)                             H'FFFFFEE4                         8/16


                                                             Bit
    Item          15      14      13      12      11      10       9        8 76543210
 Bit Name               WIT   WIT   WIT    WIT   WIT   WIT   WIT             BCM  BCM  BCM  BCM  BCM  BCM  BCM
               —      V6     V5     V4      V3     V2     V1     V0      —      V6     V5     V4      V3     V2     V1     V0
Initial Value 0000000000000000
    R/W           R     R/W   R/W   R/W   R/W   R/W   R/W   R/W      R     R/W   R/W   R/W   R/W   R/W   R/W   R/W


    Bit                            Bit Name                                                      Description
  14 to 8    Watchdog timer (WDT) interval          These bits set the vector number for the interval
             interupt vector number (WITV6–         interrupt (ITI) of the watchdog timer (WDT)
             WITV0)
   6 to 0     Bus state controller (BSC) compareThese bits set the vector number for the compare
             match interrupt vector number          match interrupt (CMI) of the bus state controller
             (BCMV6–BCMV0)                          (BSC)



Vector number setting register
DIV (VCRDIV)                                  H'FFFFFF0C                          32


                                                             Bit
    Item          31      30      29      28      27      26      25      24      23      22      21      20      19      18      17      16
 Bit Name ————————————————
Initial Value 0000000000000000
    R/WRRRRRRRRRRRRRRRR
    Item          15      14      13      12      11      10       9        8 76543210
 Bit Name
Initial Value ————————————————
    R/W         R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W


    Bit                            Bit Name                                                      Description
  15 to 0    (Vector number setting)                   These bits set the vector number for the interrupt
                                                    when caused by overflow or underflow of the division
                                                    unit












586
```

## PDF page 603

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=603)

```text
                                                                               INTC



Vector number setting registers      H'FFFFFFA0 (channel 0)
DMA0 and DMA1 (VCRDMA0,              H'FFFFFFA8 (channel 1)           32
VCRDMA1)


                                                    Bit
   Item          31      30      29      28      27      26      25      24      23      22      21      20      19      18      17      16
 Bit Name ————————————————
Initial Value 0000000000000000
   R/WRRRRRRRRRRRRRRRR
   Item          15      14      13      12      11      10       9        8 76543210
 Bit Name ————————VC7VC6VC5VC4VC3VC2VC1VC0
Initial Value 00000000————————
   R/WRRRRRRRRR/WR/WR/WR/WR/WR/WR/WR/W


    Bit                            Bit Name                                                      Description
  7 to 0     Vector number bits (VC7–VC0)       These bits set the vector number at the end of DMA
                                             transfer




Interrupt control register (ICR)         H'FFFFFEE0                         8/16


                                                    Bit
   Item          15      14     13     12      11     10      9        8 76543210
 Bit Name                                                                                                                                                         VEC
           NMIL    —      —      —      —      —      —    NMIE ———————MD
Initial Value   0/1*     0       0       0        0       0       0        0 ————————
   R/W RRRRRRRR/WRRRRRRRR/W
Note:   When NMI input is high: 1; when NMI input is low: 0


   Bit                  Bit Name             Value                                     Description
   15      NMI input level (NMIL)        0     NMI input level is low
                                   1     NMI input level is high
    8       NMI edge select (NMIE)      0     Interrupt request is detected on falling edge of NMI input
                                        (Initial value)
                                   1     Interrupt request is detected on rising edge of NMI input
    1       RL interrupt vector              0     Auto-vector mode, automatically set internall
                                        (Initial value)
         mode select (VECMD)        1     External vector mode, external input






                                                                                           587
```

## PDF page 604

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=604)

```text
             WDT


Watchdog timer control/status                                                  8 (read)
register (WTCSR)                            H'FFFFFE80                        16 (write)

                                                                      Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name         OVF        WT/IT         TME           —              —          CKS2        CKS1        CKS0
 Initial Value          0               0               0               1               1               0               0               0
      R/W           R/(W)*        R/W          R/W           —              —            R/W          R/W          R/W
Note:   WTCSR differs from other registers in being more difficult to write. See section 12.2.4,
         Register Access, for details.


    Bit                  Bit Name             Value                                     Description
     7       Overflow flag (OVF)            0     No overflow of WTCNT in interval timer mode (Initial
                                                     value)
                                                     Cleared by reading OVF, then writing 0 in OVF
                                               1     WTCNT overflow in interval timer mode
     6       Timer mode select                 0     Interval timer mode: Interval timer interrupt (ITI) request
            (WT/IT)                                  to the CPU when WTCNT overflows (Initial value)
                                               1     Watchdog timer mode: WDTOVF signal is output
                                                     externally when WTCNT overflows
     5       Timer enable (TME)            0     Timer disabled: WTCNT is initialized to H'00 and count-
                                                     up stops (Initial value)
                                               1     Timer enabled: WTCNT starts counting
                                                     A WDTOVF signal or interrupt is generated when
                                                     WTCNT overflows
  2 to 0    Clock select 2 to 0             CKS2  CKS1  CKS0 Clock Source          Overflow Interval
            (CKS2 to CKS0)                                                                      (f = 28.7 MHz)
                                               000f/2 (Initial value)     17.8ms
                                               001f/64                         570.8ms
                                               010f/128                       1.1ms
                                               011f/256                       2.2ms
                                               100f/512                       4.5ms
                                               101f/1024                     9.1ms
                                               110f/4096                     35.5ms
                                               111f/8192                     73.0ms
















588
```

## PDF page 605

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=605)

```text
                                                                                                                          WDT



Watchdog timer counter                              H'FFFFFE80 (write)                     16 (write)
(WTCNT)                                             H'FFFFFE81 (read)                       8 (read)

                                                                                 Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name
 Initial Value          0               0               0               0               0               0               0               0
       R/W             R/W          R/W          R/W          R/W          R/W          R/W          R/W          R/W



      Bit                    Bit Name                                                    Description
    7 to 0     (Count value)                    Input clock count value



Reset control/status register                       H'FFFFFE82 (write)                     16 (write)
(RSTCSR)                                            H'FFFFFE83 (read)                       8 (read)

                                                                                 Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name       WOVF      RSTE        RSTS          —              —             —              —              —
 Initial Value          0               0               0               1               1               1               1               1
       R/W           R/(W)*        R/W          R/W           —              —             —              —              —
Note:   Only 0 can be written in bit 7 to clear the flag.


     Bit                  Bit Name             Value                                     Description
      7       Watchdog timer                           0     No WTCNT overflow in watchdog timer mode
              overflow flag (WOVF)                           (Initial value)
                                                             Cleared when software reads WOVF, then writes 0 in
                                                             WOVF
                                                       1     Set by WTCNT overflow in watchdog timer mode
      6       Reset enable (RSTE)          0     No internal reset when WTCNT overflows
                                                             (Initial value)
                                                       1     Internal reset when WTCNT overflows
      5       Reset select (RSTS)           0     Power-on reset    (Initial value)
                                                       1     Manual reset











                                                                                                                                            589
```

## PDF page 606

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=606)

```text
            DIVU



Divisor register (DVSR)                   H'FFFFFE00                           32


                                                               Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  31 to 0    (Written with divisor)         Used to write the divisor for the operation



Dividend register L for 32-bit
division (DVDNT)                             H'FFFFFE04                           32


                                                               Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  31 to 0    (Dividend setting)              Set with the 32-bit dividend used for 32-bit/32-bit division
                                           operations















590
```

## PDF page 607

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=607)

```text
                                                                                   DIVU



Division control register (DVCR)      H'FFFFFF08                         16/32


                                                       Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name ————————————————
  Initial Value 0000000000000000
     R/W RRRRRRRRRRRRRRRR
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name                                                                                                                                            OVF
               ——————————————IEOVF
  Initial Value 0000000000000000
     R/W RRRRRRRRRRRRRRR/WR/W


   Bit                  Bit Name             Value                                     Description
    1       OVF interrupt enable     0     Disables interrupt request (OVFI) caused by OVF
          (OVFIE)                        (Initial value)
                                     1     Enables interrupt request (OVFI) caused by OVF
    0       Overflow flag (OVF)            0     No overflow has occurred  (Initial value)
                                     1     Overflow has occurred




Dividend register H (DVDNTH)        H'FFFFFF10                           32


                                                       Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


    Bit                    Bit Name                                                    Description
  31 to 1    (Dividend setting)              Set with the upper 32 bits of the dividend used for 64-bit/32-
                                      bit division operations









                                                                                               591
```

## PDF page 608

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=608)

```text
            DIVU



Dividend regiater L (DVDNTL)         H'FFFFFF14                           32


                                                              Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  31 to 1    (Dividend setting)              Set with the lower 32 bits of the dividend used for 64-bit/32-
                                           bit division operations






































592
```

## PDF page 609

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=609)

```text
                                                                                              UBC


Break address register AH
(BARAH)                                          H'FFFFFF40                         16/32


                                                               Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name      BAA  BAA   BAA   BAA    BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA
                 31    30    29    28     27    26    25    24    23    22    21    20    19    18    17    16
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  15 to 0    Break address BAA31–          These bits specify the upper bits (bit 31 to bit 16) of the
             BAA16                         channel A break condition address




Break address register AL
(BARAL)                                           H'FFFFFF42                           16


                                                               Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name      BAA  BAA   BAA   BAA    BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA   BAA
                 15    14    13    12     11    10    9     8      7    6     5     4      3     2     1     0
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  15 to 0    Break address BAA15–          These bits specify the lower bits (bit 15 to bit 0) of the
             BAA0                          channel A break condition address





















                                                                                                            593
```

## PDF page 610

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=610)

```text
            UBC


Break address mask register AH
(BAMRAH)                                       H'FFFFFF44                         16/32





                                                              Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name     BAM  BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM
                A31   A30   A29   A28   A27   A26   A25   A24   A23   A22   A21   A20   A19   A18   A17   A16
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W




   Bit                  Bit Name             Value                                     Description
 15 to 0   Break address                  0     Channel A break address BAAn is included in the break
           BAMA31–BAMA16                       conditions (Initial value)
                                          1     Channel A break address BAAn is not included in the
                                               break conditions
                                                                                                 n = 31 to 16



Break address mask register AL
(BAMRAL)                                        H'FFFFFF46                           16


                                                              Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name     BAM  BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM
                A15   A14   A13   A12   A11   A10    A9    A8    A7    A6    A5    A4    A3    A2    A1   A0
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


   Bit                  Bit Name             Value                                     Description
 15 to 0   Break address                  0     Channel A break address BAAn is included in the break
           BAMA15–BAMA0                        conditions (Initial value)
                                          1     Channel A break address BAAn is not included in the
                                               break conditions
                                                                                                   n = 15 to 0









594
```

## PDF page 611

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=611)

```text
                                                                                            UBC


Break bus cycle register A
(BBRA)                                             H'FFFFFF48                         16/32


                                                             Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name ————————CPACPA                                                 IDA1 IDA0  RWA     RW   SZA   SZA
                                                                 1     0                 1    A0     1     0
  Initial Value 0000000000000000
     R/W RRRRRRRRR/WR/WR/WR/WR/WR/WR/WR/W


   Bit                  Bit Name             Value                                     Description
   7, 6     CPU cycle/peripheral       0    0  No channel A user break interrupt generated
           cycle select A                     (Initial value)
           (CPA1, CPA0)                  0    1  Break only on CPU cycles
                                       1    0  Break only on peripheral cycles
                                       1    1  Break on both CPU and peripheral cycles
   5, 4     Instruction fetch/data     0    0  No channel A user break interrupt generated
           access select A                    (Initial value)
           (IDA1, IDA0)                     0    1  Break only on instruction fetch cycles
                                       1    0  Break only on data access cycles
                                       1    1  Break on both instruction fetch and data access cycles
   3, 2     Read/write select A        0    0  No channel A user break interrupt generated
           (RWA1, RWA0)                       (Initial value)
                                       0    1  Break only on read cycles
                                       1    0  Break only on write cycles
                                       1    1  Break on both read and write cycles
   1, 0     Operand size select A      0    0  Operand size is not a break condition (Initial value)
           (SZA1, SZA0)                   0    1  Break on byte access
                                       1    0  Break on word access
                                       1    1  Break on longword access



Break address register BH
(BARBH)                                          H'FFFFFF60                         16/32


                                                             Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name      BAB  BAB   BAB  BAB    BAB  BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB  BAB
                 31    30    29    28    27    26   25    24    23    22    21    20    19    18    17    16
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  15 to 0    Break address BAB31–         These bits specify the upper bits (bit 31 to bit 16) of the
             BAB16                        channel B break condition address


                                                                                                          595
```

## PDF page 612

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=612)

```text
            UBC


Break address register BL
(BARBL)                                           H'FFFFFF62                           16


                                                              Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name      BAB  BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB   BAB
                 15    14    13    12    11    10     9     8     7     6     5     4     3     2     1     0
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                    Bit Name                                                    Description
  15 to 0    Break address BAB15–          These bits specify the lower bits (bit 15 to bit 0) of the
             BAB0                          channel B break condition address



Break address mask register BH
(BAMRBH)                                       H'FFFFFF64                         16/32


                                                              Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name     BAM   BAM   BAM  BAM    BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM
                B31   B30   B29   B28    B27   B26  B25   B24    B23   B22   B21  B20    B19   B18   B17   B16
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


   Bit                  Bit Name             Value                                     Description
 15 to 0   Break address mask             0     Channel B break address BABn is included in the break
           BAMB31–BAMB16                       conditions (Initial value)
                                          1     Channel B break address BABn is not included in the
                                               break conditions
                                                                                                  n = 31 to 16



















596
```

## PDF page 613

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=613)

```text
                                                                                             UBC


Break address mask register BL
(BAMRBL)                                        H'FFFFFF66                           16


                                                             Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name     BAM  BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM   BAM  BAM   BAM
                B15   B14   B13   B12   B11   B10   B9    B8    B7    B6    B5    B4    B3    B2    B1    B0
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


   Bit                  Bit Name             Value                                     Description
 15 to 0   Break address mask            0     Channel B break address BABn is included in the break
           BAMB15–BAMB0                       conditions (Initial value)
                                         1     Channel B break address BABn is not included in the
                                              break conditions
                                                                                                  n = 15 to 0





Break data register BH (BDRBH)    H'FFFFFF70                         16/32


                                                             Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name      BDB  BDB   BDB   BDB   BDB   BDB  BDB   BDB   BDB   BDB   BDB   BDB   BDB   BDB   BDB   BDB
                 31    30    29    28    27    26    25   24     23   22    21    20    19    18    17    16
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W




     Bit                    Bit Name                                                    Description
  15 to 0    Break data BDB31–            These bits specify the upper bits (bit 31 to bit 16) of the
             BDB16                        channel B break condition data
















                                                                                                          597
```

## PDF page 614

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=614)

```text
            UBC



Break data register BL (BDRBL)      H'FFFFFF72                         16/32



                                                             Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name      BDB  BDB   BDB   BDB   BDB   BDB   BDB   BDB   BDB   BDB  BDB   BDB    BDB  BDB   BDB   BDB
                 15    14    13    12    11    10    9     8     7     6     5     4     3     2     1     0
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


     Bit                       Bit Name                                                    Description
  15 to 0    Break data BDB15–BDB0      These bits specify the lower bits (bit 15 to bit 0) of the
                                              channel B break condition data



Break data mask register BH
(BDMRBH)                                       H'FFFFFF74                         16/32


                                                             Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name     BDM  BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM
                B31   B30   B29   B28   B27   B26   B25   B24   B23   B22   B21   B20   B19   B18   B17  B16
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


   Bit                  Bit Name             Value                                     Description
 15 to 0   Break data mask                0     Channel B break address BDBn is included in the break
           BDMB31–BDMB16                      conditions (Initial value)
                                          1     Channel B break address BDBn is masked and therefore
                                              not included in the break conditions
                                                                                                 n = 31 to 16


















598
```

## PDF page 615

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=615)

```text
                                                                                             UBC


Break data mask register BL
(BDMRBL)                                       H'FFFFFF76                           16


                                                              Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name     BDM  BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM   BDM
                B15   B14   B13   B12   B11   B10    B9   B8     B7    B6   B5    B4     B3   B2    B1    B0
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


   Bit                  Bit Name             Value                                     Description
 15 to 0   Break data mask                0     Channel B break address BDBn is included in the break
           BDMB15–BDMB0                       conditions (Initial value)
                                          1     Channel B break address BDBn is masked and therefore
                                              not included in the break conditions
                                                                                                   n = 15 to 0







































                                                                                                          599
```

## PDF page 616

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=616)

```text
            UBC


Break bus cycle register B
(BBRB)                                             H'FFFFFF68                         16/32


                                                            Bit
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name ————————CPBCPB                                                 IDB  IDB  RWB    RWB   SZB   SZB
                                                                1     0     1    0      1     0     1    0
  Initial Value 0000000000000000
     R/W RRRRRRRRR/WR/WR/WR/WR/WR/WR/WR/W


   Bit                  Bit Name             Value                                     Description
   7, 6     CPU cycle/peripheral       0    0  No channel B user break interrupt generated
           cycle select B                    (Initial value)
           (CPB1, CPB0)                  0    1  Break only on CPU cycles
                                       1    0  Break only on peripheral cycles
                                       1    1  Break on both CPU and peripheral cycles
   5, 4     Instruction fetch/data     0    0  No channel B user break interrupt generated
           access select B                   (Initial value)
           (IDB1, IDB0)                     0    1  Break only on instruction fetch cyclcs
                                       1    0  Break only on data access cycles
                                       1    1  Break on both instruction fetch and data access cycles
   3, 2     Read/write select B        0    0  No channel B user break interrupt generated
           (RWB1, RWB0)                      (Initial value)
                                       0    1  Break only on read cycles
                                       1    0  Break only on write cycles
                                       1    1  Break on both read and write cycles
   1, 0     Operand size select B      0    0  Operand size is not a break condition (Initial value)
           (SZB1, SZB0)                   0    1  Break on byte access
                                       1    0  Break on word access
                                       1    1  Break on longword access






















600
```

## PDF page 617

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=617)

```text
                                                                                                          UBC


Break control register (BRCR)         H'FFFFFF78                         16/32

                                                                      Bit
      Item            15     14     13     12      11     10      9       8 76543210
   Bit Name        CMF   CMF                                                                     CMF   CMF
                   CA      PA   EBBE  UMD      —    PCBA    —       —       CB      PB       —     SEQ   DBEB  PCBB     —       —
  Initial Value 0000000000000000
      R/W           R/W  R/W  R/W  R/W     R     R/W     R       R     R/W  R/W     R     R/W   R/W  R/W     R       R

    Bit                  Bit Name             Value                                     Description
    15      CPU condition match                0     Channel A CPU cycle conditions do not match, no user
            flag A (CMFCA)                           break interrupt generated (Initial value)
                                               1     Channel A CPU cycle conditions have matched, user
                                                     break interrupt generated
    14      Peripheral condition               0     Channel A peripheral cycle conditions do not match, no
            match flag A (CMFPA)                     user break interrupt generated (Initial value)
                                               1     Channel A peripheral cycle conditions have matched,
                                                     user break interrupt generated
    13      External bus break                 0     Chip-external bus cycle not included in break conditions
            enable (EBBE)                            (Initial value)
                                               1     Chip-external bus cycle included in break conditions
    12      UBC mode (UMD)               0     Compatible mode for SH7000-series UBCs (Initial value)
                                               1     SH7604 mode
    10      PC break select A                  0     Places the channel A instruction fetch cycle break before
            (PCBA)                                   instruction execution (Initial value)
                                               1     Places the channel A instruction fetch cycle break after
                                                     instruction execution
     7       CPU condition match               1     Channel B CPU cycle conditions do not match, no user
            flag B (CMFCB)                           break interrupt generated (Initial value)
                                               0     Channel B CPU cycle conditions have matched, user
                                                     break interrupt generated
     6       Peripheral condition              0     Channel B peripheral cycle conditions do not match, no
            match flag B (CMFPB)                     user break interrupt generated (Initial value)
                                               1     Channel B peripheral cycle conditions have matched,
                                                     user break interrupt generated
     4       Sequence condition                0     Compare channel A and B conditions independently
            select (SEQ)                             (Initial value)
                                               1     Compare channel A and B conditions sequentially
                                                     (channel A, then channel B)
     3       Data break enable B               0     Do not include data bus conditions in the channel B
            (DBEB)                                   conditions (Initial value)
                                               1     Include data bus conditions in the channel B conditions
     2       Instruction break select          0     Places the channel B instruction fetch cycle break before
            B (PCBB)                                 instruction execution (Initial value)
                                               1     Places the channel B instruction fetch cycle break after
                                                     instruction execution




                                                                                                                         601
```

## PDF page 618

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=618)

```text
           DMAC


DMA source address registers 0         H'FFFFFF80 (channel 0)
and 1 (SAR0 and SAR1)                  H'FFFFFF90 (channel 1)            32


                                                             Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


    Bit                    Bit Name                                                    Description
  31 to 0    (Transfer source address     These bits specify the DMA transfer source address
             specification)



DMA destination address registers        H'FFFFFF84 (channel 0)
0 and 1 (DAR0 and DAR1)                  H'FFFFFF94 (channel 1)            32


                                                             Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


    Bit                    Bit Name                                                    Description
  31 to 0    (Transfer destination        These bits specify the DMA transfer destination address
             address specification)














602
```

## PDF page 619

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=619)

```text
                                                                                   DMAC


DMA transfer count registers 0 and   H'FFFFFF88 (channel 0)
1 (TCR0 and TCR1)                    H'FFFFFF98 (channel 1)            32


                                                       Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name ————————
  Initial Value 00000000————————
     R/W RRRRRRRRR/WR/WR/WR/WR/WR/WR/WR/W
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name
  Initial Value ————————————————
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W


    Bit                    Bit Name                                                    Description
  23 to 0    (Transfer count          Specifies the DMA transfer count (during a DMA transfer,
            specification)            these bits indicate the remaining transfer count)



DMA channel control registers 0, 1   H'FFFFFF8C (channel 0)
(CHCR0, CHCR1)                       H'FFFFFF9C (channel 1)           32


                                                       Bit
     Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
   Bit Name ————————————————
  Initial Value 0000000000000000
     R/W RRRRRRRRRRRRRRRR
     Item            15     14     13     12      11     10      9       8 76543210
   Bit Name      DM1  DM0  SM1  SM0   TS1   TS0    AR    AM     AL     DS    DL     TB     TA     IE      TE     DE
  Initial Value 0000000000000000
     R/W           R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W  R/W  R/W   R/W  R/W    R/R/W
                                                                                         (W)*
Note:   Only 0 can be written, after reading 1, to clear the flag.


   Bit                  Bit Name             Value                                     Description
 14, 15   Destination address         0    0  Fixed destination address (Initial value)
          mode bits 1, 0            0    1  Destination address is incremented (+1 for byte transfer
          (DM1, DM0)                      size, +2 for word transfer size, +4 for longword transfer
                                          size, and +16 for 16-byte transfer size)
                                    1    0  Destination address is decremented (–1 for byte transfer
                                          size, –2 for word transfer size, –4 for longword transfer
                                          size, and –16 for 16-byte transfer size)
                                    1    1  Reserved (setting prohibited)




                                                                                                603
```

## PDF page 620

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=620)

```text
             DMAC

    Bit                  Bit Name             Value                                     Description
  13, 12   Source address mode                 0    0  Fixed source address (+16 for 16-byte transfer size)
             bits 1, 0 (SM1, SM0)                      (Initial value)
                                               0    1  Source address is incremented (+1 for byte transfer size,
                                                       +2 for word transfer size, +4 for longword transfer size,
                                                       and +16 for 16-byte transfer size)
                                               1    0  Source address is decremented (–1 for byte transfer size,
                                                       –2 for word transfer size, –4 for longword transfer size,
                                                       and +16 for 16-byte transfer size)
                                               1    1  Reserved (setting prohibited)
  11, 10   Transfer size bits 1, 0          0     Byte unit (Initial value)
             (TS1, TS0)                          0     Word (2-byte) unit
                                                  1     Longword (4-byte) unit
                                                  1     16-byte unit (4 longword transfers)
     9       Auto-request mode bit         0     Module request mode (Initial value)
             (AR)                                     1     Auto-request mode
     8       Acknowledge/transfer                 0     DACK output in read cycle/transfer from memory to
             mode bit (AM)                             device (Initial value)
                                                  1     DACK output in write cycle/transfer from device to
                                                       memory
     7       Acknowledge level bit         0     DACK is an active-low signal (Initial value)
             (AL)                                     1     DACK is an active-high signal
     6       DREQ select bit (DS)          0     Detected by level (Initial value)
                                                  1     Detected by edge
     5       DREQ level bit (DL)            0     When DS is 0, DREQ is detected by low level; when DS
                                                       is 1, DREQ is detected by fall (Initial value)
                                                  1     When DS is 0, DREQ is detected by high level; when DS
                                                       is 1, DREQ is detected by rise
     4       Transfer bus mode bit                0     Cycle-steal mode
             (TB)
                                                  1     Burst mode
     3       Transfer address mode                0     Dnal address mode
             bit (TA)
                                                  1     Single address mode
     2       Interrupt enable bit (IE)       0     Interrupt disabled (Initial value)
                                                  1     Interrupt enabled
     1       Transfer-end flag bit                0     DMA has not ended or was aborted (Initial value)
             (TE)                                      Cleared by reading 1 from the TE bit and then writing 0
                                                  1     DMA has ended nomally (by TCR = 0)
     0       DMA enable bit (DE)           0     DMA transfer disabled (Initial value)
                                                  1     DMA transfer enabled











604
```

## PDF page 621

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=621)

```text
                                                                                                                            DMAC


DMA request/response selection control                        H'FFFFFE71 (channel 0)
registers 0 and 1 (DRCR0,                                     H'FFFFFE72 (channel 1)             8
DRCR1)

                                                                                   Bit
       Item               7               6               5               4               3               2               1               0
   Bit Name           —             —              —             —              —             —            RS1          RS0
 Initial Value          0               0               0               0               0               0               0               0
       R/W               R              R               R              R               R              R             R/W          R/W


     Bit                  Bit Name             Value                                     Description
    1, 0     Resource select bits         0    0  DREQ (external request) (Initial value)
               1, 0 (RS1, RS0)                0    1  RXI (receive-data-full interrupt transfer request of the on-
                                                              chip serial communication interface (SCI))
                                                     1    0  TXI (transmit-data-full interrupt transfer request of the on-
                                                              chip SCI)
                                                     1    1  Reserved (setting prohibited)






































                                                                                                                                               605
```

## PDF page 622

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=622)

```text
          DMAC



DMA operation register (DMAOR)      H'FFFFFFB0                              32


                                                       Bit
    Item            31     30     29     28      27     26     25     24      23     22     21     20      19     18     17     16
  Bit Name ————————————————
 Initial Value 0000000000000000
    R/W RRRRRRRRRRRRRRR
    Item            15     14     13     12      11     10      9       8 76543210
  Bit Name ————————————PRAENMIF DME
 Initial Value 0000000000000000
    R/W RRRRRRRRRRRRR/WR/R/                                                                   R/W
                                                                                   (W)* (W)*
Note:   Only 0 can be written, to clear the flag.


   Bit                  Bit Name             Value                                     Description
    3       Priority mode bit (PR)          0     Fixed priority (Ch 0 > Ch 1) (Initial value)
                                     1     Round-robin mode (High priority switches to low after
                                         each transfer)
                                         (The priority for the first DMA transfer after a reset is
                                         Ch 1 > Ch 0)
    2       Address error flag bit           0     No DMAC address error (Initial value)
         (AE)                                     1     Address error by DMAC
    1       NMI flag bit (NMIF)              0     No NMIF interrupt (Initial value)
                                         To clear the NMIF bit, read 1 from it and then write 0
                                     1     NMIF has occurred
    0       DMA master enable bit       0     DMA transfers disabled on all channels (Initial value)
         (DME)                                  1     DMA transfers enabled on all channels
























606
```

## PDF page 623

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=623)

```text
                                                                                                                          BSC



Bus control register 1 (BCR1)          H'FFFFFFE0                        16/32


                                                                                Bit
     Item          15     14    13     12      11      10        9         8        7        6        5        4       3       2         1         0
  Bit Name      MAS                       END    BST   PSHR  AHLW  AHLW A1LW  A1LW  A0LW  A0LW          DRAM  DRAM  DRAM
                   TER     —      —      IAN    ROM                  1          0         1         0          1         0       —        2           1          0
 Initial Value     —      0      0       0        0        0         1         1        1        1        1        1       0       0         0         0
     R/W           R      R     R     R/W   R/W   R/W    R/W    R/W   R/W   R/W   R/W   R/W    R     R/W    R/W    R/W


     Bit                 Bit Name                Value                                      Description
     15      Bus arbitration                        0        Master mode
              (MASTER)                             1        Slave mode
     12      Endian specification for          0        Big-endian, as in other areas (Initial value)
              area 2 (ENDIAN)                    1        Little-endian
     11      Area 0 burst ROM                  0        Area 0 is accessed normally (Initial value)
              enable (BSTROM)                 1        Area 0 is accessed as burst ROM
     10      Partial space share                0        Total master mode when MD5 = 0 (Initial mode)
              specification (PSHR)              1        Partial-share master mode when MD5 = 0
    9, 8     Long wait specification      0      0    3 waits (Initial value)
              for areas 2 and 3               0      1    4 waits
              (AHLW1, AHLW0)             1      0    5 waits
                                                     1      1    6 waits
    7, 6     Long wait specification      0      0    3 waits (Initial value)
              for area 1 (A1LW1,            0      1    4 waits
              A1LW0)                             1      0    5 waits
                                                     1      1    6 waits
    5, 4     Long wait specification      0      0    3 waits (Initial value)
              for area 0 (A0LW1,            0      1    4 waits
              A0LW0)                             1      0    5 waits
                                                     1      1    6 waits
  2 to 0    Enable for DRAM and      0    0    0  Areas 2 and 3 are ordinary spaces (Initial value)
              other memory (DRAM2–                  0    0    1  Area 2 is ordinary space; area 3 is synchronous
              DRAM0)                                              DRAM space
                                                    0    1    0  Area 2 is ordinary space; area 3 is DRAM space
                                                    0    1    1  Area 2 is ordinary space; area 3 is pseudo-SRAM
                                                                  space
                                                    1    0    0  Area 2 is synchronous DRAM space; area 3 is
                                                                  ordinary space
                                                    1    0    1  Areas 2 and 3 are synchronous DRAM spaces
                                                    1    1    0  Reserved (setting prohibited)
                                                    1    1    1  Reserved (setting prohibited)







                                                                                                                                            607
```

## PDF page 624

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=624)

```text
            BSC


Bus control register 2 (BCR2)          H'FFFFFFE4                        16/32


                                                               Bit
    Item          15      14      13      12      11      10       9        8 76543210
  Bit Name                                                                                      A3     A3     A2     A2      A1     A1
               ————————SZ1SZ0SZ1SZ0SZ1SZ0——
Initial Value 0000000011111100
    R/WRRRRRRRRR/WR/WR/WR/WR/WR/WRR

    Bit                 Bit Name                Value                                       Description
   7, 6     Bus size specification        0      0    Reserved (setting prohibited)
           for area 3 (A3SZ1–
           A3SZ0)(Valid only             0      1    Byte (8-bit) size
           when setting ordinary        1      0    Word (16-bit) size
           space)                                1      1    Longword (32-bit) size (Initial value)
   5, 4     Bus size specification        0      0    Reserved (setting prohibited)
           for area 2 (A2SZ1–
           A2SZ0) (Valid only            0      1    Byte (8-bit) size
           when setting ordinary        1      0    Word (16-bit) size
           space)                                1      1    Longword (32-bit) size (Initial value)
   3, 2     Bus size specification        0      0    Reserved (setting prohibited)
           for area 1 (A1SZ1–            0      1    Byte (8-bit) size
           A1SZ0)                              1      0    Word (16-bit) size
                                          1      1    Longword (32-bit) size (Initial value)


Wait control register (WCR)             H'FFFFFFE8                        16/32


                                                               Bit
    Item          15      14      13      12      11      10       9        8 76543210
  Bit Name    IW31 IW30 IW21 IW20  IW11 IW10 IW01 IW00   W31  W30  W21  W20   W11  W10  W01  W00
Initial Value 1010101011111111
    R/W         R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W   R/W

    Bit                 Bit Name                 Value                                      Description
 15 to 8   Idles between cycles         IW31  IW30
           for areas 3 to 0             IW21  IW20
           (IW31–IW00)                  IW11  IW10
                                        IW01  IW00
                                          0        0    No idle cycle
                                          0        1    One idle cycle inserted
                                          1        0    Two idle cycles inserted (Initial value)
                                          1        1    Reserved (setting prohibited)




608
```

## PDF page 625

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=625)

```text
                                                                                                          BSC

    Bit                 Bit Name                Value                                       Description
  7 to 0    Wait control of areas 3   During basic cycle
            to 0 (W31–W00)             W31         W30
                                            W21    W20
                                            W11    W10
                                            W01    W00
                                              0      0    External wait input disabled without waits
                                              0      1    External wait input enabled with one wait
                                              1      0    External wait input enabled with two waits
                                              1      1    Complies with the long wait specification of bus control
                                                         register 1 (BCR1)
                                                         External wait input is enabled (Initial value)
                                            When area 3 is DRAM
                                            W31 W30
                                              0      0    1 CAS assert cycle
                                              0      1    2 CAS assert cycles
                                              1      0    3 CAS assert cycles
                                              1      1    Reserved (setting prohibited)
                                            When area 2 or 3 is synchronous DRAM
                                            W31    W30
                                            W21    W20
                                              0      0    1 CAS latency cycle
                                              0      1    2 CAS latency cycles
                                              1      0    3 CAS latency cycles
                                              1      1    4 CAS latency cycles (Initial value)
                                            When area 3 is pseudo-SRAM
                                            W31 W30
                                              0      0    2 cycles from BS signal assertion to end of cycle
                                              0      1    3 cycles from BS signal assertion to end of cycle
                                              1      0    4 cycles from BS signal assertion to end of cycle
                                              1      1    Reserved (setting prohibited)



Individual memory control register
(MCR)                                              H'FFFFFFEC                        16/32


                                                                      Bit
     Item          15      14      13      12      11      10       9        8 76543210
  Bit Name                                       TRAS  TRAS
                TRP    RCD  TRWL     1          0        BE    RASD    —     AMX2    SZ    AMX1  AMX0  RFSH   RMD     —        —
Initial Value 0000000000000000
     R/W         R/W   R/W   R/W   R/W   R/W   R/W   R/W     R     R/W   R/W   R/W   R/W   R/W   R/W     R       R







                                                                                                                         609
```

## PDF page 626

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=626)

```text
               BSC

    Bit                    Bit Name                  Value                                    Description
     15      RAS precharge time                   0        1 cycle (Initial value)
             (TRP)                                          1        2 cycles
     14      RAS-CAS delay (RCD)               0        1 cycle (Initial value)
                                                         1        2 cycles
     13      Write-precharge delay                0        1 cycle (Initial value)
             (TRWL)                                       1        2 cycles
  12, 11   CAS-before-RAS                    0       0   2 cycles (Initial value)
             refresh RAS assert time         0       1   3 cycles
             (TRAS1, TRAS0)                    1       0   4 cycles
                                                      1       1   Reserved (setting prohibited)
     10      Burst enable (BE)                       0        Burst disabled (Initial value)
                                                         1        High-speed page mode during DRAM interface is
                                                                  enabled. Data is continuously transferred in static
                                                                  column mode during pseudo-SRAM interfacing.
                                                                  During synchronous DRAM access, burst is always
                                                                  enabled regardless of this bit.
     9       Bank active mode (RASD)          0        For synchronous DRAM, read or write is performed
                                                                  using auto-precharge mode.
                                                                  The next access always starts with a bank active
                                                                  command.
                                                         1        For synchronous DRAM, access ends with bank
                                                                  active status. This is only valid for area 3. When
                                                                  area 2 is synchronous DRAM, the mode is always
                                                                  auto-precharge.






























610
```

## PDF page 627

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=627)

```text
                                                                                                              BSC

  Bit                  Bit Name                Value                                     Description
7, 5, 4   Address multiplex            For DRAM interface
           (AMX2–AMX0)                  0    0    0  8-bit column address DRAM (Initial value)
                                               0    0    1  9-bit column address DRAM
                                               0    1    0  10-bit column address DRAM
                                               0    1    1  11-bit column address DRAM
                                               1    0    0  Reserved (setting prohibited)
                                               1    0    1  Reserved (setting prohibited)
                                               1    1    0  Reserved (setting prohibited)
                                               1    1    1  Reserved (setting prohibited)
                                             For synchronous DRAM interface
                                               0    0    0  16-Mbit DRAM (1M · 16 bits) (Initial value)
                                               0    0    1  16-Mbit DRAM (2M · 8 bits)
                                               0    1    0  16-Mbit DRAM (4M · 4 bits)
                                               0    1    1  4-Mbit DRAM (256k · 16 bits)
                                               1    0    0  Reserved (setting prohibited)
                                               1    0    1  Reserved (setting prohibited)
                                               1    1    0  Reserved (setting prohibited)
                                               1    1    1  2-Mbit DRAM (128k · 16 bits)
   6       Memory data size (SZ)            0       Word (Initial value)
                                                    1       Longword
   3       Refresh control (RFSH)           0       No refresh (Initial value)
                                                    1       Refresh
   2       Refresh mode (RMODE)         0       Normal refresh (Initial value)
                                                    1       Self-refresh





























                                                                                                                              611
```

## PDF page 628

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=628)

```text
          BSC

Refresh timer control/status
register (RTCSR)                             H'FFFFFFF0                        16/32


                                                  Bit
   Item          15      14      13      12      11      10       9        8 76543210
 Bit Name ————————CMFCMIECKS2CKS1 CKS0    —      —      —
Initial Value 0000000000000000
   R/WRRRRRRRRR/WR/WR/WR/WR/WRRR

   Bit                 Bit Name                Value                                       Description
   7       Compare match flag      —       RTCNT and RTCOR match
         (CMF)                           Clear condition: After RTCSR is read when CMF is 1,
                                         0 is written in CMF
   6       Compare match            0        Disables interrupt request caused by CMF (Initial value)
         interrupt enable (CMIE)
                                    1        Enables interrupt request caused by CMF
 5 to 3    Clock select bits               0    0    0  Disables count up (Initial value)
         (CKS2–CKS0)                  0    0    1  CLK/4
                                0    1    0  CLK/16
                                0    1    1  CLK/64
                                1    0    0  CLK/256
                                1    0    1  CLK/1024
                                1    1    0  CLK/2048
                                1    1    1  CLK/4096


Refresh timer counter (RTCNT)       H'FFFFFFF4                        16/32

                                                  Bit
   Item          15      14      13      12      11      10       9        8 76543210
 Bit Name ————————
Initial Value 0000000000000000
   R/WRRRRRRRRR/WR/WR/WR/WR/WR/WR/WR/W

    Bit                    Bit Name                                                      Description
  7 to 0     (Count value)                    Input clock count value

Refresh time constant register
(RTCOR)                                          H'FFFFFFF8                        16/32

                                                  Bit
   Item          15      14      13      12      11      10       9        8 76543210
 Bit Name ————————
Initial Value 0000000000000000
   R/WRRRRRRRRR/WR/WR/WR/WR/WR/WR/WR/W

    Bit                    Bit Name                                                      Description
  7 to 0      (Timer constant)               Sets the refresh cycle

612
```

## PDF page 629

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=629)

```text
                                                                                                                          Cache



Cache control register (CCR)          H'FFFFFE92                            8

                                                                                  Bit
       Item               7               6               5               4               3               2               1               0
   Bit Name          W1           W0             —            CP            TW            OD             ID             CE
 Initial Value          0               0               0               0               0               0               0               0
       R/W             R/W          R/W          R/W          R/W          R/W          R/W          R/W          R/W


     Bit                  Bit Name             Value                                     Description
    7, 6     Way specification             0    0  Way 0 (Initial value)
              (W1, W0)                          0    1  Way 1
                                                     1    0  Way 2
                                                     1    1  Way 3
      4       Cache purge (CP)               0     Normal operation (Initial value)
                                                       1     Cache purge
      3       Two-way mode (TW)           0     Four-way mode (Initial value)
                                                       1     Two-way mode
      2       Data replacement                0     Normal operation (Initial value)
              disable (OD)                        1     Data not replaced even when cache miss occurs in data
                                                              access
      1       Instruction replacement       0     Normal operation (Initial value)
              disable (ID)                          1     Data not replaced even when cache miss occurs in
                                                              instruction fetch
      0       Cache enable (CE)             0     Cache disabled (Initial value)
                                                       1     Cache enabled



























                                                                                                                                             613
```

## PDF page 630

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=630)

```text
          Power-down



Standby control register                          H'FFFFFE91                            8
(SBYCR)

                                                                               Bit
      Item               7               6               5               4               3               2               1               0
   Bit Name         SBY          HIZ             —         MSTP4     MSTP3     MSTP2     MSTP1     MSTP0
 Initial Value          0               0               0               0               0               0               0               0
       R/W             R/W          R/W            —            R/W          R/W          R/W          R/W          R/W


     Bit                   Bit Name              Value                                    Description
      7       Standby (SBY)                       0     Executing SLEEP instruction puts the chip into sleep
                                                             mode (Initial value)
                                                       1     Executing SLEEP instruction puts the chip into standby
                                                             mode
      6       Port high impedance             0     Pin states held in standby mode (Initial value)
              (HIZ)                                      1     Pins at high impedance in standby mode
      4       Module stop 4 (MSTP4)        0     DMAC running (Initial value)
                                                       1     Clock supply to DMAC halted
      3       Module stop 3 (MSTP3)        0     MULT running (Initial value)
                                                       1     Clock supply to MULT halted
      2       Module stop 2 (MSTP2)        0     DIVU running (Initial value)
                                                       1     Clock supply to DIVU halted
      1       Module stop 1 (MSTP1)        0     FRT running (Initial value)
                                                       1     Clock supply to FRT halted
      0       Module stop 0 (MSTP0)        0     SCI running (Initial value)
                                                       1     Clock supply to SCI halted

























614
```
