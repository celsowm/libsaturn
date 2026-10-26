# Section 16 Electrical Characteristics (3V Version)

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=495)

Physical PDF pages 495–578. Text extraction; consult the PDF for figures.

## Bookmarks

- [16.1 Absolute Maximum Ratings](#pdf-page-495)
- [16.2 DC Characteristics](#pdf-page-496)
- [16.3 AC Characteristics](#pdf-page-498)
- [16.3.1 Clock Timing](#pdf-page-498)
- [16.3.2 Control Signal Timing](#pdf-page-502)
- [16.3.3 Bus Timing](#pdf-page-508)
- [16.3.4 DMAC Timing](#pdf-page-574)
- [16.3.5 Free-Running Timer Timing](#pdf-page-575)
- [16.3.6 Watchdog Timer Timing](#pdf-page-576)
- [16.3.7 Serial Communication Interface Timing](#pdf-page-577)
- [16.3.8 AC Characteristics Measurement Conditions](#pdf-page-578)

## PDF page 495

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=495)

```text
           Section 16   Electrical Characteristics (3V Version)


16.1      Absolute Maximum Ratings

Table 16.1 shows the absolute maximum ratings.

Table 16.1    Absolute Maximum Ratings

Item                                                                         Symbol        Rating                             Unit
Power supply voltage                                              VCC              –0.3 to +7.0                     V
Input voltage                                                           Vin                –0.3 to VCC + 0.3            V
Operating temperature                                            Topr             –20 to +75                         °C
Storage temperature                                               Tstg              –55 to +125                       °C
Caution:   Operating the chip in excess of the absolute maximum rating may result in permanent
              damage.







































                                                                                                                                      479
```

## PDF page 496

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=496)

```text
16.2      DC Characteristics

Tables 16.2 and 16.3 list DC characteristics.

Table 16.2        DC Characteristics (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75° C)

Item                                         Symbol   Min            Typ    Max           Unit   Test Conditions
Input high-   RES, NMI,            VIH           VCC · 0.9   —      VCC + 0.3   V        During standby
level             MD5–MD0                            VCC · 0.9   —      VCC + 0.3   V        Normal operation
voltage        EXTAL, CKIO                       VCC · 0.9   —      VCC + 0.3   V

                  Other input pins                   VCC · 0.7   —      VCC + 0.3   V
Input low-    RES, NMI,            VIL           –0.3           —      VCC·0.1     V        During standby
level             MD5–MD0                            –0.3           —      VCC·0.1     V        Normal operation
voltage        Other input pins                   –0.3           —      VCC·0.1     V

Input leak    RES                     |Iin|          —              —      1.0               mA      Vin = 0.5 to VCC – 0.5 V
current         NMI, MD5–MD0                   —              —      1.0               mA      Vin = 0.5 to VCC – 0.5 V

                  Other input pins                   —              —      1.0               mA      Vin = 0.5 to VCC – 0.5 V
3-state           A26–A0, D31–              |ISTI|         —              —      1.0               mA      Vin = 0.5 to VCC – 0.5 V
leak              D0, BS, CS3–
current           CS0, RD/WR,
(while off)       RAS, CAS,
                  WE3–WE0, RD,
                  IVECF
Output         All output pins      VOH         VCC – 0.5   —      —               V        IOH = –200 mA
high-level
voltage                                                     VCC – 1.0   —      —               V        IOH = –1 mA
Output low        All output pins      VOL          —              —      0.4             V        IOL = 1.6 mA
level
voltage
Input            RES                     Cin           —              —      15              pF      Vin = 0 V
capaci-        NMI                                      —              —      15              pF      f = 1 MHz
tance           All other input
                                                          —              —      15              pF      Ta = 25°C
                  pins (including
                  D31–D0)











480
```

## PDF page 497

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=497)

```text
Table 16.2    DC Characteristics (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75° C) (cont)

Item                                         Symbol   Min            Typ    Max           Unit   Test Conditions
Current        Normal                 ICC           —              25      30              mA     f = 8 MHz
consump-    operation                              —              45      55              mA     f = 16 MHz
tion                                                           —              60      70              mA     f = 28.7 MHz

                Sleep                                    —              15      20              mA     f = 8 MHz
                                                    —              30      40              mA     f = 16 MHz
                                                    —              40      50              mA     f = 28.7 MHz
                Standby                                —              1        5                  mA      Ta £ 50°C
                                                    ——20mA50°C < Ta
Notes:  1.   When no PLL is used, do not leave the PLLVCC and PLLVSS pins open. Connect
              PLLVCC to VCC and PLLVSS to VSS.
          2.   Current consumption values shown are the values at which all output pins are without
              load under conditions of VIH min = VCC – 0.5 V, VIL max = 0.5 V.


Table 16.3    Permitted Output Current Values (Conditions: VCC = 5.0 V – 10%, Ta = –20 to
                 +75°C)

Item                                                                         Symbol     Min         Typ        Max        Unit
Output low-level permissible current (per pin)        IOL             —           —           2.0          mA
Output low-level permissible current (total)              å  IOL ——80mA
Output high-level permissible current (per pin)       –IOH           —           —           2.0          mA
Output high-level permissible current (total)            å (–IOH)——25mA
Caution:   To ensure chip reliability, do not exceed the output current values given in table 16.3.























                                                                                                                           481
```

## PDF page 498

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=498)

```text
16.3      AC Characteristics


16.3.1      Clock Timing

Table 16.4    Clock Timing (Conditions: VCC = 3.0 to 0.5 V, Ta = –20 to +75°C)

Item                                                                  Symbol    Min            Max                 Unit   Figures
Operating frequency                                         fOP            4                20                    MHz   16.1
Clock cycle time                                                tcyc            50              143*1 or 250*2  ns
Clock high pulse width                                      tCH            8*1 or 15*2—ns
Clock low pulse width                                       tCL            8*1 or 15*2—ns
Clock rise time                                                  tCR —5 ns
Clock fall time                                                   tCF —5 ns
EXTAL clock input frequency                           fEX            4                8                      MHz   16.2
EXTAL clock input cycle time                           tEXcyc        125            250                  ns
EXTAL clock input low level pulse width          tEXL           50              —                     ns
EXTAL clock input high level pulse width         tEXH          50              —                     ns
EXTAL clock input rise time                              tEXR —5 ns
EXTAL clock input clock fall time                      tEXF —5 ns
Power-on oscillation settling time                     tOSC1        10              —                     ms     16.3
Software standby oscillation settling time 1      tOSC2        10              —                     ms     16.4
Software standby oscillation settling time 2      tOSC3        10              —                     ms     16.5
PLL synchronization settling time                     tPLL 1—ms      16.6
Notes:  1.   With PLL circuit 1 operating.
          2.   With PLL circuit 1 not used.




                                                                tcyc

                                                      tCH                  tCL


                     CKIO                       VIH                 VIH             VIH
                     (input)                   1/2 VCC               VIL          VIL      1/2 VCC


                                                                      tCF                       tCR


                                        Figure 16.1   CKIO Input Timing


482
```

## PDF page 499

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=499)

```text
                                                               tEXcyc

                                                       tEXH                 tEXL


                      EXTAL       1/2 VCC         VIH       VIH                            VIH
                       (input)                                         VIL        VIL      1/2 VCC

                                                                      tEXF                    tEXR



Note:   External clock input from EXTAL pin.

                                  Figure 16.2   EXTAL Clock Input Timing


                                                                                         Stable oscillation



            CKIO,
   internal clock



              VCC              VCC min
                                                                                        tRESW
                                                            tOSC1



              RES



Note:   Oscillation settling time when on-chip crystal oscillator is used.

                           Figure 16.3   Oscillation Settling Time at Power-On


















                                                                                                                        483
```

## PDF page 500

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=500)

```text
                  Standby
                   period                                                              Stable oscillation
     CKIO,
   internal
      clock

                                                                                      tRESW
                                                             tOSC2


       RES



Note:   Oscillation settling time when on-chip crystal oscillator is used.

                Figure 16.4   Oscillation Settling Time at Standby Return (via RES)


                    Standby
                     period                                                              Stable oscillation

        CKIO,
      internal
         clock


                                                        tOSC3



          NMI



Note:   Oscillation settling time when on-chip crystal oscillator is used.

                Figure 16.5   Oscillation Settling Time at Standby Return (via NMI)

















484
```

## PDF page 501

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=501)

```text
                   Stable              Oscillation frequency                          Stable
                 oscillation                modification                            oscillation

 EXTAL
or CKIO
                     PLL                                                    tPLL                 PLL
              synchronization                                                             synchronization

Internal
   clock


                        Figure 16.6   PLL Synchronization Settling Time













































                                                                                                             485
```

## PDF page 502

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=502)

```text
16.3.2      Control Signal Timing

Table 16.5    Control Signal Timing (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                                                   Symbol   Min                 Max                Unit    Figure
RES rise, fall                                                     tRESr,          —                   200                 ns       16.7
                                                                     tRESf
RES pulse width                                               tRESW      20                   —                   tcyc
NMI reset setup time                                         tNMIRS      tcyc + 10        —                   ns
NMI reset hold time                                           tNMIRH      tcyc + 10        —                   ns
NMI rise, fall                                                      tNMIr,         —                   200                 ns
                                                                     tNMIf
NMI minimum pulse width                                 tIRQES      3                     —                   tcyc
RES setup time*                                                tRESS       40                   —                   ns       16.8,
NMI setup time*                                                 tNMIS        40                   —                   ns       16.9
IRL3–IRL0 setup time*                                      tIRLS         40                   —                   ns
RES hold time                                                   tRESH       20                   —                   ns       16.8,
NMI hold time                                                    tNMIH        20                   —                   ns       16.9
IRL3–IRL0 hold time                                         tIRLH        20                   —                   ns
BRLS setup time 1 (PLL on)                             tBLSS1      1/2 tcyc + 20  —                   ns       16.10
BRLS hold time 1 (PLL on)                               tBLSH1      15 – 1/2 tcyc  —                   ns
BGR delay time 1 (PLL on)                               tBGRD1     —                   1/2 tcyc + 25  ns
BRLS setup time 1 (PLL on, 1/4 cycle delay)   tBLSS1      1/4 tcyc + 20  —                   ns       16.10
BRLS hold time 1 (PLL on, 1/4 cycle delay)     tBLSH1      15 – 1/4 tcyc  —                   ns
BGR delay time 1 (PLL on, 1/4 cycle delay)     tBGRD1     —                   3/4 tcyc + 25  ns
BRLS setup time 2 (PLL off)                             tBLSS2      20                   —                   ns       16.11
BRLS hold time 2 (PLL off)                               tBLSH2      30                   —                   ns
BGR delay time 2 (PLL off)                               tBGRD2—40ns
Note:   The RES, NMI and IRL3-IRL0 signals are asynchronous inputs, but when the setup times
           shown here are observed, the signals are considered to have changed at clock fall. If the
           setup times are not observed, recognition may be delayed until the next clock fall.












486
```

## PDF page 503

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=503)

```text
Table 16.5    Control Signal Timing (cont)
                 (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                                                   Symbol   Min                 Max                Unit    Figure
BREQ delay time 1 (PLL on)                            tBRQD1     —                   1/2 tcyc + 25  ns       16.12
BACK setup time 1 (PLL on)                            tBAKS1      1/2 tcyc + 20  —                   ns
BACK hold time 1 (PLL on)                              tBAKH1      15 – 1/2 tcyc  —                   ns
BREQ delay time 1 (PLL on, 1/4 cycle delay)   tBRQD1     —                   3/4 tcyc + 25  ns       16.12
BACK setup time 1 (PLL on, 1/4 cycle delay)   tBAKS1      1/4 tcyc + 20  —                   ns
BACK hold time 1 (PLL on, 1/4 cycle delay)     tBAKH1      15 – 1/4 tcyc  —                   ns
BREQ delay time 2 (PLL off)                            tBRQD2     —                   40                   ns       16.13
BACK setup time 2 (PLL off)                             tBAKS2      20                   —                   ns
BACK hold time 2 (PLL off)                               tBAKH2      30                   —                   ns
Bus tri-state delay time 1 (PLL on)                    tBOFF1      0                     35                   ns       16.10,
Bus buffer on time 1 (PLL on)                           tBON1033ns16.12

Bus tri-state delay time 1 (PLL on, 1/4 cycle                tBOFF1      1/4 tcyc          1/4 tcyc + 35  ns       16.10,
delay)
Bus buffer on time 1 (PLL on, 1/4 cycle delay) tBON1       1/4 tcyc          1/4 tcyc + 33  ns       16.12
Bus tri-state delay time 1 (PLL off)                    tBOFF1      0                     45                   ns       16.11,
Bus buffer on time 1 (PLL off)                           tBON1040ns16.13

Bus tri-state delay time 2 (PLL on)                    tBOFF2      1/2 tcyc          1/2 tcyc + 35  ns       16.10,
Bus buffer on time 2 (PLL on)                           tBON2       1/2 tcyc          1/2 tcyc + 33  ns       16.12
Bus tri-state delay time 2 (PLL on, 1/4 cycle                tBOFF2      3/4 tcyc          3/4 tcyc + 35  ns       16.10,
delay)
Bus buffer on time 2 (PLL on, 1/4 cycle delay) tBON2       3/4 tcyc          3/4 tcyc + 33  ns       16.12
Bus tri-state delay time 3 (PLL off)                    tBOFF3      0                     45                   ns       16.11,
Bus buffer on time 3 (PLL off)                           tBON3040ns16.13


















                                                                                                                              487
```

## PDF page 504

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=504)

```text
                                       tRESf                                    tRESr

                            VIH                                          VIH
   RES                       VIL                               VIL

                    tNMIRS                        tRESW                                  tNMIRH

                   VIH                                                                                    VIH
   NMI             VIL                                                                                    VIL


                                       Figure 16.7   Reset Input Timing



         CKIO

                                         tRESH           tRESS
                                                     VIH
          RES                                        VIL

                                          tNMIH          tINMIS
                                                     VIH
           NMI                                       VIL

                                          tIRLH          tIRLS
                                                     VIH
   IRL3–IRL0                                         VIL


                        Figure 16.8   Interrupt Signal Input Timing (PLL1 Off)






















488
```

## PDF page 505

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=505)

```text
                     1/2 tcyc or 3/4 tcyc              1/2 tcyc or 3/4 tcyc


         CKIO

                                          tRESH            tRESS

                                                         VIH
          RES                                            VIL

                                          tNMIH            tNMIS

                                                         VIH
           NMI                                           VIL

                                           tIRLH           tIRLS

                                                         VIH
   IRL3–IRL0                                             VIL


                   Figure 16.9   Interrupt Signal Input Timing (PLL1 On)


        CKIO

                    tBLSH1             tBLSS1                                  tBLSH1             tBLSS1
       BRLS
       (input)                                      tBGRD1                             tBGRD1

         BGR
     (output)
RD, RD/WR,                                                      tBOFF2                 tBON2
 RAS, CAS,
 CSn, WEn,                                                      tBOFF1                             tBON1
  BS, IVECF
     A26–A0
     D31–D0

                Figure 16.10   Bus Release Timing (Master Mode, PLL1 On)














                                                                                                          489
```

## PDF page 506

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=506)

```text
           CKIO
                 tBLSH2                 tBLSS2                                   tBLSH2        tBLSS2
           BRLS
           (input)
                                          tBGRD2                                                       tBGRD2
             BGR/CR
         (output)
   RD, RD/WR,                                                        tBOFF3                  tBON3
     RAS, CAS,
     CSn, WEn,                                             tBOFF1                                         tBON1
     BS, IVECF
         A26–A0
        D31–D0

                     Figure 16.11   Bus Release Timing (Master Mode, PLL1 Off)



            CKIO

            BREQ                tBRQD1                                  tBRQD1
          (output)                                                                                           tBAKS1
            BACK                     tBAKH1                 tBAKS1                               tBAKH1
            (input)
    RD, RD/WR,                                 tBON2                                                      tBOFF2
      RAS, CAS,
      CSn, WEn,
      BS, IVECF                                   tBON1                                                    tBOFF1
          A26–A0
         D31–D0

                       Figure 16.12   Bus Release Timing (Slave Mode, PLL1 On)





















490
```

## PDF page 507

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=507)

```text
       CKIO
       BREQ              tBRQD2                                              tBRQD1
      (output)                                                                                             tBAKS2
        BACK/CR           tBAKH2       tBAKS2                                             tBAKH2
       (input)

RD, RD/WR,                           tBON3                                                             tBOFF3
 RAS, CAS,
 CSn, WEn,
  BS, IVECF                                  tBON1                                                           tBOFF1

     A26–A0
     D31–D0

                  Figure 16.13   Bus Release Timing (Slave Mode, PLL1 Off)










































                                                                                                                    491
```

## PDF page 508

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=508)

```text
16.3.3      Bus Timing

Table 16.6    Bus Timing With PLL On [Mode 0, 4]
                   (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
Address delay time                 tAD             —                    28                   ns       16.14, 16.20, 16.40,
                                                                                                                 16.52, 16.66, 16.68
BS delay time                         tBSD           —                    25                   ns       16.14, 16.20, 16.40,
                                                                                                                 16.52, 16.66
CS delay time 1                      tCSD1         —                    25                   ns       16.14, 16.20, 16.40,
                                                                                                                 16.52, 16.66
CS delay time 2                         tCSD2          —                    1/2 tcyc + 25   ns       16.14, 16.66
Read/write delay time             tRWD          —                    25                   ns       16.14, 16.20, 16.40,
                                                                                                                 16.52, 16.66
Read strobe delay time 1        tRSD1         —                    1/2 tcyc + 25   ns       16.14, 16.40, 16.52,
                                                                                                                 16.66, 16.68
Read data setup time 1          tRDS1         1/2 tcyc + 10   —                    ns       16.14, 16.40, 16.52,
                                                                                                                 16.66, 16.68
Read data setup time 3                     tRDS3         1/2 tcyc + 10   —                    ns       16.20
(SDRAM)
Read data hold time 2            tRDH2         0                     —                    ns       16.14, 16.66
Read data hold time 4                      tRDH4         0                     —                    ns       16.20
(SDRAM)
Read data hold time 5                      tRDH5         0                     —                    ns       16.40
(DRAM)
Read data hold time 6                      tRDH6         0                     —                    ns       16.52
(PSRAM)
Read data hold time 7                      tRDH7         0                     —                    ns       16.68
(interrupt vector)
Write enable delay time          tWED1         1/2 tcyc + 3     1/2 tcyc + 25   ns       16.14, 16.15, 16.52,
                                                                                                                 16.53
Write data delay time 1           tWDD          —                    25                   ns       16.15, 16.27, 16.41,
                                                                                                                 16.53
Write data hold time 1             tWDH1         3                     —                    ns       16.15, 16.27, 16.41,
                                                                                                                 16.53
Data buffer on time                 tDON           —                    25                   ns       16.15, 16.27, 16.41,
                                                                                                                 16.53
Data buffer off time                 tDOF           —                    25                   ns       16.15, 16.27, 16.41,
                                                                                                                 16.53



492
```

## PDF page 509

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=509)

```text
Table 16.6    Bus Timing With PLL On [Mode 0, 4] (cont)
                    (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
DACK delay time 1                 tDACD1       —                    25                   ns       16.14, 16.20, 16.40,
                                                                                                                   16.52, 16.66
DACK delay time 2                 tDACD2       —                    1/2 tcyc + 25   ns       16.14, 16.20, 16.40,
                                                                                                                   16.52, 16.66
WAIT setup time                     tWTS           20                   —                    ns       16.19, 16.43, 16.55,
                                                                                                                   16.66, 16.70
WAIT hold time                       tWTH          10                   —                    ns       16.19, 16.43, 16.55,
                                                                                                                   16.66, 16.70
RAS delay time 1 (SDRAM)   tRASD1       —                    25                   ns       16.20
RAS delay time 2 (DRAM)      tRASD2       1/2 tcyc + 3     1/2 tcyc + 25   ns       16.40
CAS delay time 1 (SDRAM)   tCASD1       —                    25                   ns       16.20
CAS delay time 2 (DRAM)      tCASD2       1/2 tcyc + 3     1/2 tcyc + 25   ns       16.40
DQM delay time                      tDQMD        —                    25                   ns       16.20
CKE delay time                       tCKED         —                    33                   ns       16.37
CE delay time 1                      tCED1         1/2 tcyc + 3     1/2 tcyc + 25   ns       16.52
OE delay time 1                      tOED1         —                    1/2 tcyc + 25   ns       16.52
IVECF delay time                   tIVD            —                    25                   ns       16.68
Address input setup time        tASIN          25                   —                    ns       16.71
Address input hold time          tAHIN          10                   —                    ns       16.71
BS input setup time                tBSS           25                   —                    ns       16.71
BS input hold time                  tBSH           10                   —                    ns       16.71
Read/write input setup time    tRWS          25                   —                    ns       16.71
Read/write input hold time      tRWH          10                   —                    ns       16.71


















                                                                                                                                              493
```

## PDF page 510

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=510)

```text
Table 16.7        Bus Timing With PLL On and 1/4 Cycle Delay [Mode 1, 5]
                       (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
Address delay time                 tAD             —                    1/4 tcyc + 28   ns       16.14, 16.20, 16.40,
                                                                                                               16.52, 16.66, 16.68
BS delay time                         tBSD           —                    1/4 tcyc + 25   ns       16.14, 16.20, 16.40,
                                                                                                               16.52, 16.66
CS delay time 1                      tCSD1         —                    1/4 tcyc + 25   ns       16.14, 16.20, 16.40,
                                                                                                               16.52, 16.66
CS delay time 2                      tCSD2         —                    3/4 tcyc + 25   ns       16.14, 16.66
Read/write delay time             tRWD          —                    1/4 tcyc + 25   ns       16.14, 16.20, 16.40,
                                                                                                               16.52, 16.66
Read strobe delay time 1        tRSD1         —                    3/4 tcyc + 25   ns       16.14, 16.40, 16.52,
                                                                                                               16.66, 16.68
Read data setup time 1          tRDS1         1/4 tcyc + 10   —                    ns       16.14, 16.40, 16.52,
                                                                                                               16.66, 16.68
Read data setup time 3                     tRDS3         1/4 tcyc + 10   —                    ns       16.20
(SDRAM)
Read data hold time 2            tRDH2         0                     —                    ns       16.14, 16.66
Read data hold time 4                      tRDH4         0                     —                    ns       16.20
(SDRAM)
Read data hold time 5                      tRDH5         0                     —                    ns       16.40
(DRAM)
Read data hold time 6                      tRDH6         0                     —                    ns       16.52
(PSRAM)
Read data hold time 7                      tRDH7         0                     —                    ns       16.68
(interrupt vector)
Write enable delay time          tWED1         3/4 tcyc + 3     3/4 tcyc + 25   ns       16.14, 16.15, 16.52,
                                                                                                               16.53
Write data delay time 1           tWDD          —                    1/4 tcyc + 25   ns       16.15, 16.27, 16.41,
                                                                                                               16.53
Write data hold time 1             tWDH1         1/4 tcyc + 3     —                    ns       16.15, 16.27, 16.41,
                                                                                                               16.53
Data buffer on time                 tDON           —                    1/4 tcyc + 25   ns       16.15, 16.27, 16.41,
                                                                                                               16.53
Data buffer off time                 tDOF           —                    1/4 tcyc + 25   ns       16.15, 16.27, 16.41,
                                                                                                               16.53






494
```

## PDF page 511

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=511)

```text
Table 16.7    Bus Timing With PLL On and 1/4 Cycle Delay [Mode 1, 5] (cont)
                   (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
DACK delay time 1                 tDACD1       —                    1/4 tcyc + 25   ns       16.14, 16.20, 16.40,
                                                                                                               16.52, 16.66
DACK delay time 2                 tDACD2       —                    3/4 tcyc + 25   ns       16.14, 16.20, 16.40,
                                                                                                               16.52, 16.66
WAIT setup time                     tWTS           20 –1/4 tcyc    —                    ns       16.19, 16.43, 16.55,
                                                                                                               16.66, 16.70
WAIT hold time                       tWTH          1/4 tcyc+10     —                    ns       16.19, 16.43, 16.55,
                                                                                                               16.66, 16.70
RAS delay time 1 (SDRAM)   tRASD1       —                    1/4 tcyc + 25   ns       16.20
RAS delay time 2 (DRAM)      tRASD2       3/4 tcyc + 3     3/4 tcyc + 25   ns       16.40
CAS delay time 1 (SDRAM)   tCASD1       —                    1/4 tcyc + 25   ns       16.20
CAS delay time 2 (DRAM)      tCASD2       3/4 tcyc + 3     3/4 tcyc + 25   ns       16.40
DQM delay time                      tDQMD        —                    1/4 tcyc + 25   ns       16.20
CKE delay time                       tCKED         —                    1/4 tcyc + 33   ns       16.37
CE delay time 1                      tCED1         3/4 tcyc + 3     3/4 tcyc + 25   ns       16.52
OE delay time 1                      tOED1         —                    3/4 tcyc + 25   ns       16.52
IVECF delay time                   tIVD            —                    1/4 tcyc + 25   ns       16.68
Address input setup time        tASIN          25 – 1/4 tcyc   —                    ns       16.71
Address input hold time          tAHIN          1/4 tcyc+10     —                    ns       16.71
BS input setup time                tBSS           25 – 1/4 tcyc   —                    ns       16.71
BS input hold time                  tBSH           1/4 tcyc +10    —                    ns       16.71
Read/write input setup time    tRWS          25 – 1/4 tcyc   —                    ns       16.71
Read/write input hold time      tRWH          1/4 tcyc +10    —                    ns       16.71


















                                                                                                                                          495
```

## PDF page 512

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=512)

```text
Table 16.8    Bus Timing With PLL Off (CKIO Input) [Mode 6]
                    (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
Address delay time                 tAD             —                    43                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67, 16.69
BS delay time                         tBSD           —                    40                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67
CS delay time 1                      tCSD1         —                    40                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67
CS delay time 3                         tCSD3          —                    40                   ns       16.16, 16.67
Read write delay time             tRWD          —                    40                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67
Read strobe delay time 2        tRSD2         —                    40                   ns       16.16, 16.47, 16.60,
                                                                                                                   16.67, 16.69
Read data setup time 2          tRDS2         10                   —                    ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67, 16.69
Read data hold time 2            tRDH2         0                     —                    ns       16.16, 16.67
Read data hold time 3            tRDH3         30                   —                    ns       16.38
Read data hold time 5                       tRDH5         0                     —                    ns       16.47
(DRAM)
Read data hold time 6                       tRDH6         0                     —                    ns       16.60
(PSRAM)
Read data hold time 7                       tRDH7         0                     —                    ns       16.69
(interrupt vector)
Write enable delay time 2       tWED2         —                    40                   ns       16.17, 16.61
Write data delay time              tWDD          —                    40                   ns       16.17, 16.39, 16.48,
                                                                                                                   16.61
Write data hold time 1             tWDH1         3                                            ns       16.17, 16.39, 16.48,
                                                                                                                   16.61
Write data hold time 2             tWDH2         5                                            ns       16.17
Write data hold time 3             tWDH3         3                                            ns       16.61
DACK delay time 1                 tDACD1       —                    40                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67
DACK delay time 3                 tDACD3       —                    40                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67







496
```

## PDF page 513

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=513)

```text
Table 16.8      Bus Timing With PLL Off (CKIO Input) [Mode 6] (cont)
                       (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
WAIT setup time                     tWTS           20                   —                    ns       16.19, 16.43, 16.55,
                                                                                                                     16.67, 16.70
WAIT hold time                       tWTH          25                   —                    ns       16.19, 16.43, 16.55,
                                                                                                                     16.67, 16.70
RAS delay time 1 (SDRAM)   tRASD1       —                    40                   ns       16.38
RAS delay time 3 (DRAM)      tRASD3       —                    40                   ns       16.47
CAS delay time 1 (SDRAM)   tCASD1       —                    40                   ns       16.38
CAS delay time 3 (DRAM)      tCASD3       —                    40                   ns       16.47
DQM delay time                      tDQMD        —                    40                   ns       16.38
CKE delay time                       tCKED         —                    48                   ns       16.37
CE delay time 2                      tCED2         —                    40                   ns       16.60
OE delay time 2                      tOED2         —                    40                   ns       16.60
IVECF delay time                   tIVD            —                    40                   ns       16.69
WE setup time                        tWES1         0                     —                    ns       16.16
Address setup time 1              tAS1           0                     —                    ns       16.17
Address setup time 2              tAS2           3                     —                    ns       16.60
Address hold time 2                tAH2           0                     —                    ns       16.17
Row address setup time         tASR           3                     —                    ns       16.47
Column address setup time    tASC           3                     —                    ns       16.47
Write command setup time     tWCS          3                     —                    ns       16.48
Write data setup time              tWDS          3                     —                    ns       16.48
Address input setup time*       tASIN          20                   —                    ns       16.71
Address input hold time*         tAHIN          25                   —                    ns       16.71
BS input setup time*               tBSS           20                   —                    ns       16.71
BS input hold time*                 tBSH           25                   —                    ns       16.71
Read/write input setup time*   tRWS          20                   —                    ns       16.71
Read/write input hold time*     tRWH          25                   —                    ns       16.71
Data buffer on time                 tDON           —                    40                   ns       16.17, 16.39, 16.48,
                                                                                                                     16.61
Data buffer off time                 tDOF           —                    40                   ns       16.17, 16.39, 16.48,
                                                                                                                     16.61
Note:   When the external addresses monitor function is used, the PLL must be on.



                                                                                                                                                  497
```

## PDF page 514

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=514)

```text
Table 16.9    Bus Timing With PLL Off (CKIO Output) [Mode 2]
                    (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
Address delay time                 tAD             —                    28                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67, 16.69
BS delay time                         tBSD           —                    25                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67
CS delay time 1                      tCSD1         —                    25                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67
CS delay time 3                         tCSD3          —                    25                   ns       16.16, 16.67
Read write delay time             tRWD          —                    25                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67
Read strobe delay time 2        tRSD2         —                    25                   ns       16.16, 16.47, 16.60,
                                                                                                                   16.67, 16.69
Read data setup time 2          tRDS2         10                   —                    ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67, 16.69
Read data hold time 2            tRDH2         0                     —                    ns       16.16, 16.67
Read data hold time 3                       tRDH3         1/2 tcyc           —                    ns       16.38
(SDRAM)
Read data hold time 5                       tRDH5         0                     —                    ns       16.47
(DRAM)
Read data hold time 6                       tRDH6         0                     —                    ns       16.60
(PSRAM)
Read data hold time 7                       tRDH7         0                     —                    ns       16.69
(interrupt vector)
Write enable delay time 2       tWED2         3                     25                   ns       16.17, 16.61
Write data delay time              tWDD          —                    25                   ns       16.17, 16.39, 16.48,
                                                                                                                   16.61
Write data hold time 1             tWDH1         3                     —                    ns       16.17, 16.39, 16.48,
                                                                                                                   16.61
Write data hold time 2             tWDH2         5                     —                    ns       16.17
Write data hold time 3             tWDH3         3                     —                    ns       16.61
DACK delay time 1                 tDACD1       —                    25                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67
DACK delay time 3                 tDACD3       —                    25                   ns       16.16, 16.38, 16.47,
                                                                                                                   16.60, 16.67






498
```

## PDF page 515

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=515)

```text
Table 16.9     Bus Timing With PLL Off (CKIO Output) [Mode 2] (cont)
                      (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
WAIT setup time                     tWTS           20                   —                    ns       16.19, 16.43, 16.55,
                                                                                                                    16.67, 16.70
WAIT hold time                       tWTH          10                   —                    ns       16.19, 16.43, 16.55,
                                                                                                                    16.67, 16.70
RAS delay time 1 (SDRAM)   tRASD1       —                    25                   ns       16.38
RAS delay time 3 (DRAM)      tRASD3       3                     25                   ns       16.47
CAS delay time 1 (SDRAM)   tCASD1       —                    25                   ns       16.38
CAS delay time 3 (DRAM)      tCASD3       3                     25                   ns       16.47
DQM delay time                      tDQMD        —                    25                   ns       16.38
CKE delay time                       tCKED         —                    33                   ns       16.37
CE delay time 2                      tCED2         3                     25                   ns       16.60
OE delay time 2                      tOED2         —                    25                   ns       16.60
IVECF delay time                   tIVD            —                    25                   ns       16.69
Address input setup time*       tASIN          25                   —                    ns       16.71
Address input hold time*         tAHIN          10                   —                    ns       16.71
BS input setup time*               tBSS           25                   —                    ns       16.71
BS input hold time*                 tBSH           10                   —                    ns       16.71
Read/write input setup time*   tRWS          25                   —                    ns       16.71
Read/write input hold time*     tRWH          10                   —                    ns       16.71
Data buffer on time                 tDON           —                    25                   ns       16.17, 16.39, 16.48,
                                                                                                                    16.61
Data buffer off time                 tDOF           —                    25                   ns       16.17, 16.39, 16.48,
                                                                                                                    16.61
Note:   When the external addresses monitor function is used, the PLL must be on.

















                                                                                                                                                499
```

## PDF page 516

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=516)

```text
                                                     T1                  T2

                       CKIO
                                                 tAD                         tAD

                    A26–A0
                                                 tBSD              tBSD

                          BS
                                                 tCSD1           tCSD2

                       CSn
                                                 tRWD                                 tRWD
                    RD/WR,
                         WE
                                                    tRSD1           tRSD1

                          RD
                                     tWED1                                      tWED1
                       WEn,
                     CASxx,                                                   tRDH2
                     DQMxx
                                                                      tRDS1

                    D31–D0
                                                 tDACD1            tDACD2

                     DACKn



                      WAIT


                       RAS,
                          CE


                       CAS,
                          OE


                        CKE
Notes:   1.     The dotted line shows the waveform when synchronous DRAM is connected.
          2.    tRDH2 is specified from the rise of CSn or RD, whichever is first.
          3.    The DACKn waveform shown is for the case where active-high has been specified.

                           Figure 16.14   Basic Read Cycle (No Waits, PLL On)







500
```

## PDF page 517

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=517)

```text
                                                       T1                  T2

                     CKIO
                                                  tAD                              tAD


                 A26–A0
                                                  tBSD              tBSD

                        BS
                                                  tCSD1              tCSD2

                     CSn
                                                   tRWD                                 tRWD
                 RD/WR,
                       WE
                                     tRSD1                                                     tRSD1

                        RD
                                                     tWED1          tWED1      tAH1
                     WEn,
                  CASxx,
                  DQMxx
                                                    tWDD                             tDOF
                                           tDON                                tWDH1

                 D31–D0
                                                  tDACD1            tDACD2


                  DACKn


                    WAIT


                     RAS,
                        CE


                     CAS,
                        OE


                      CKE
Notes:   1.     The dotted line shows the waveform when synchronous DRAM is connected.
          2.    The DACKn waveform shown is for the case where active-high has been
                specified.
                          Figure 16.15   Basic Write Cycle (No Waits, PLL On)





                                                                                                                         501
```

## PDF page 518

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=518)

```text
                                                    T1                     T2

                     CKIO
                                                 tAD                               tAD

                  A26–A0
                                                 tBSD                  tBSD

                        BS
                                                  tCSD1                    tCSD3

                     CSn
                                                tRWD                               tRWD
                  RD/WR,
                       WE
                                                          tRSD2           tRSD2

                        RD
                                               tWES1
                     WEn,
                   CASxx,                                                      tRDH2
                   DQMxx
                                                                    tRDS2

                  D31–D0
                                                  tDACD1                          tDACD3

                   DACKn


                    WAIT


                     RAS,
                        CE


                     CAS,
                        OE


                      CKE
Notes:   1.     The dotted line shows the waveform when synchronous DRAM is connected.
          2.    tRDH2 is specified from the rise of CSn or RD, whichever is first.
          3.    The DACKn waveform shown is for the case where active-high has been specified.

                          Figure 16.16   Basic Read Cycle (No Waits, PLL Off)






502
```

## PDF page 519

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=519)

```text
                                                                  T1                     T2

                  CKIO
                                                    tAD     tAS1                                tAD


               A26–A0
                                                     tBSD                     tBSD           tAH2

                     BS
                                                    tCSD1                            tCSD3

                  CSn
                                                    tRWD                                                  tRWD
               RD/WR,
                    WE
                                         tRSD2                                                                              tRSD2

                     RD
                                                                tWED2                tWED2
                  WEn,
                CASxx,
                DQMxx                                                                      tWDH2
                                                     tWDD                                            tDOF
                                              tDON                                      tWDH1
               D31–D0
                                                    tDACD1                                 tDACD3

                DACKn


                 WAIT


                  RAS,
                     CE


                  CAS,
                     OE


                   CKE
Notes:   1.       The dotted line shows the waveform when synchronous DRAM is connected.
           2.     The DACKn waveform shown is for the case where active-high has been specified.

                             Figure 16.17   Basic Write Cycle (No Waits, PLL Off)







                                                                                                                                      503
```

## PDF page 520

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=520)

```text
                                           T1                  Tw                T2

         CKIO


       A26–A0



            BS



          CSn


      RD/WR,
            WE



            RD

          WEn,
        CASxx,
        DQMxx


      D31–D0



       DACKn
                                                             tWTS  tWTH

         WAIT


          RAS,
            CE


          CAS,
            OE


          CKE
Notes:   1.    The dotted line shows the waveform when synchronous DRAM is connected.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

                            Figure 16.18   Basic Bus Cycle (1 Wait Cycle)







504
```

## PDF page 521

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=521)

```text
                                          T1                  Tw               Twx                T2

          CKIO



       A26–A0



             BS



          CSn


       RD/WR,
            WE



            RD

          WEn,
        CASxx,
        DQMxx


       D31–D0



        DACKn
                                                          tWTS  tWTH    tWTS  tWTH


         WAIT


          RAS,
            CE


          CAS,
            OE


           CKE
Notes:   1.    The dotted line shows the waveform when synchronous DRAM is connected.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

                         Figure 16.19   Basic Bus Cycle (External Wait Input)






                                                                                                                    505
```

## PDF page 522

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=522)

```text
                                Tr            Tc           Td1          Td2          Td3          Td4
             CKIO
                            tAD                                                                                        tAD

            Upper
         address
                                        tAD                        tAD         tAD          tAD
            Lower
         address
                           tBSD                                                                                      tBSDtBSD

                BS
                            tCSD1                                                                                    tCSD1

             CSn
                            tRWD                                                                                      tRWD
         RD/WR,
               WE
                                                                                                          tRSD1

                RD
                            tDQMD                                                                        tDQMD
             WEn,
          CASxx,
          DQMxx                                                       tRDS3    tRDH4             tRDS3 tRDH4
                                                          tRDS3 tRDH4              tRDS3 tRDH4
         D31–D0

                                                     tDACD2     tDACD2     tDACD2     tDACD2

                           tDACD1                                 tDACD1        tDACD1    tDACD1

          DACKn

            WAIT
                           tRASD1     tRASD1                                                                   tRASD1

             RAS,
                CE                      tCASD1     tCASD1                                                                   tCASD1tCASD1

             CAS,
                OE

              CKE
Notes:   1.        The dotted line shows the waveform when synchronous DRAM in another CS space
                   is accessed.
            2.     The DACKn waveform shown is for the case where active-high has been specified.

                                 Figure 16.20   Synchronous DRAM Read Bus Cycle
                        (RCD = 1 Cycle, CAS Latency = 1 Cycle, Bursts = 4, PLL On)



506
```

## PDF page 523

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=523)

```text
                                  Tr            Tc            Td1          Td2           Td3           Td4

               CKIO
                              tAD                                                                                           tAD
              Upper
            address
                                           tAD
              Lower
            address
                              tBSD                                                                                         tBSDtBSDtBSD

                  BS
                              tCSD1                                                                                       tCSD1

               CSn
                               tRWD                                                                                      tRWD

           RD/WR,
                 WE
                                                                                                              tRSD1

                  RD

                              tDQMD                                                                         tDQMD
               WEn,
            CASxx,
            DQMxx                                           tRDS3  tRDH4


           D31–D0



              WAIT
                              tRASD1       tRASD1                                                            tRASD1

               RAS,
                  CE                       tCASD1       tCASD1                                              tCASD1       tCASD1

               CAS,
                  OE


                 CKE
Note:      The dotted line shows the waveform when synchronous DRAM in another CS space is
             accessed.

                          Figure 16.21   Synchronous DRAM Single Read Bus Cycle
                       (RCD = 1 Cycle, CAS Latency = 1 Cycle, Bursts = 4, PLL On)





                                                                                                                                          507
```

## PDF page 524

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=524)

```text
                   Tr       Trw       Tc        Tw       Td1       Td2      Td3      Td4
   CKIO


   Upper
 address

   Lower
 address                                          tBSD


      BS


    CSn

RD/WR,
      WE


      RD

   WEn,
 CASxx,
 DQMxx

D31–D0


 DACKn


   WAIT
                tRASD1  tRASD1

    RAS,
      CE                         tCASD1 tCASD1

    CAS,
      OE

    CKE
Notes:   1.   The dotted line shows the waveform when synchronous DRAM in another CS space
              is accessed.
         2.   The DACKn waveform shown is for the case where active-high has been specified.

                         Figure 16.22   Synchronous DRAM Read Bus Cycle
                       (RCD = 2 Cycles, CAS Latency = 2 Cycles, Bursts = 4)




508
```

## PDF page 525

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=525)

```text
                             Tnop           Tc            Td1          Td2           Td3           Td4
              CKIO
                          tAD

              Upper
            address


              Lower
            address
                          tBSD

                 BS
                          tCSD1

               CSn
                          tRWD
            RD/WR,
                WE


                 RD
                          tDQMD
              WEn,
            CASxx,
            DQMxx

            D31–D0

                                    tDACD1
            DACKn


              WAIT

                                                                                        tRASD1


              RAS,
                CE        tCASD1     tCASD1    tCASD1                                   tCASD1

               CAS,
                 OE


               CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                      Figure 16.23   Synchronous DRAM Read Bus Cycle
                  (Bank Active, Same Row Access, CAS Latency = 1 Cycle)










                                                                                                                   509
```

## PDF page 526

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=526)

```text
                      TC             TW            Td1          Td2           Td3          Td4

     CKIO

    Upper
  address

    Lower
  address


        BS


      CSn


   RD/WR
       WE

        RD
                    tDQMD
      WEn
    CASxx
   DQMxx
  D31–D0


   DACKn


     WAIT


      RAS
        CE

      CAS
        OE

      CKE
  Note:    The DACKn waveform shown is for the case where active-high has been specified.

                        Figure 16.24   Synchronous DRAM Read Bus Cycle
                    (Bank Active, Same Row Access, CAS Latency = 2 Cycles)






510
```

## PDF page 527

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=527)

```text
                     Tp             Tr             Tc           Td1          Td2         Td3           Td4

    CKIO
                   tAD
    Upper
 address                                   tAD

    Lower
 address
                      tBSD
       BS

                      tCSD1
      CSn
                  tRWD         tRWD

  RD/WR
       WE

       RD
     WEn             tDQMD

   CASxx
  DQMxx

 D31–D0
                       tDACD1
  DACKn


    WAIT

                       tRASD1                 tRASD1
     RAS/CR
       CE

     CAS/CR
       OE

     CKE
 Note:    The DACKn waveform shown is for the case where active-high has been specified.

Figure 16.25   Synchronous DRAM Read Bus Cycle (Bank Active, Different Row Access,
                    TRP = 1 Cycle, RCD = 1 Cycle, CAS Latency = 1 Cycle)







                                                                                                            511
```

## PDF page 528

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=528)

```text
                        Tp            Tpw            Tr            Tc           Td1                        Td4

       CKIO

      Upper
    address

      Lower
    address

          BS


        CSn
                                  tRWD

     RD/WR/CR
         WE

          RD

        WEn/CR
     CASxx/CR
     DQMxx

    D31–D0


     DACKn


       WAIT

                       tRASD1                  tRASD1
        RAS/CR
          CE

        CAS/CR
          OE

        CKE

   Note:    The DACKn waveform shown is for the case where active-high has been specified.

  Figure 16.26   Synchronous DRAM Read Bus Cycle (Bank Active, Different Row Access,
                      TRP = 2 Cycles, RCD = 1 Cycle, CAS Latency = 1 Cycle)






512
```

## PDF page 529

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=529)

```text
                                          Tr           Tc          Tap
                          CKIO
                                         tAD                  tAD
                          Upper
                        address                    tAD

                          Lower
                        address         tBSD       tBSD       tBSD

                              BS
                                       tCSD1                 tCSD1      tCSD1

                            CSn
                                        tRWD       tRWD      tRWD
                        RD/WR/CR
                             WE                               tRSD1

                             RD
                                       tDQMD                 tDQMD
                           WEn/CR
                         CASxx
                         DQMxx              tWDD  tDON             tDOF
                                                                tWDH1
                        D31–D0
                                       tDACD1     tDACD2               tDACD1

                         DACKn


                           WAIT
                                       tRASD1      tRASD1    tRASD1    tRASD1
                            RAS/CR
                             CE                    tCASD1    tCASD1     tCASD1

                            CAS/CR
                             OE

                            CKE
Notes:   1.   Dotted lines show the waveforms when synchronous DRAM in another CS space
              is accessed.
         2.   The DACKn waveform shown is for the case where active-high has been specified.

                        Figure 16.27   Synchronous DRAM Write Bus Cycle
                             (RCD = 1 Cycle, TRWL = 1 Cycle, PLL On)






                                                                                                           513
```

## PDF page 530

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=530)

```text
                              Tr              Trw              Tc              Trwl             Tap
           CKIO


          Upper
        address

          Lower
        address


              BS
                                                                                  tCSD1

            CSn


         RD/WR/CR
             WE


              RD


            WEn/CR
          CASxx
         DQMxx

        D31–D0                                                                               tDACD1


         DACKn


           WAIT

                               tRASD1                                                tRASD1
            RAS/CR                                    tCASD1
              CE

            CAS/CR
              OE


            CKE
Notes:   1.    Dotted lines show the waveforms when synchronous DRAM in another CS space
               is accessed.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

                          Figure 16.28   Synchronous DRAM Write Bus Cycle
                                    (RCD = 2 Cycles, TRWL = 2 Cycles)


514
```

## PDF page 531

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=531)

```text
                                                   Tc
                                CKIO
                                               tAD                   tAD

                                Upper
                              address

                                Lower
                              address          tBSD          tBSD


                                   BS
                                               tCSD1            tCSD1

                                  CSn
                                               tRWD              tRWD

                              RD/WR
                                  WE

                                   RD
                                               tDQMD           tDQMD
                                 WEn
                               CASxx
                              DQMxx             tWDD             tDOF

                                               tDON          tWDH1

                             D31–D0
                                                tDACD2
                                               tDACD1

                              DACKn


                                WAIT

                                                             tRASD1

                                 RAS/CR
                                   CE
                                               tCASD1        tCASD1

                                 CAS/CR
                                   OE


                                 CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

Figure 16.29   Synchronous DRAM Write Bus Cycle (Bank Active, Same Row Access)






                                                                                                                 515
```

## PDF page 532

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=532)

```text
                                                   Tc          Tc
                                CKIO
                                                            tAD

                                Upper
                              address


                                Lower
                              address


                                   BS



                                 CSn


                              RD/WR/CR
                                  WE


                                   RD
                                                            tDQMD
                                 WEn/CR
                               CASxx
                              DQMxx                  tWDD
                                                tWDH1
                             D31–D0

                                                tDACD2              tDACD1


                              DACKn



                                WAIT


                                 RAS/CR
                                   CE
                                                      tCASD1                              tCASD1
                                 CAS/CR
                                   OE


                                 CKE
    Note:    The DACKn waveform shown is for the case where active-high has been specified.

                    Figure 16.30   Synchronous DRAM Consecutive Write Cycles
                                      (Bank Active, Same Row Access)





516
```

## PDF page 533

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=533)

```text
                                      Tp               Tr               Tc
                    CKIO
                                   tAD

                    Upper
                 address


                    Lower
                 address
                                         tBSD

                       BS
                                        tCSD1

                     CSn
                                  tRWD                      tRWD

                  RD/WR/CR
                      WE


                       RD          tDQMD                    tDQMD


                     WEn/CR
                   CASxx
                  DQMxx

                 D31–D0

                                          tDACD1

                  DACKn



                    WAIT

                                          tRASD1
                     RAS/CR
                       CE
                                          tCASD1
                     CAS/CR
                       OE


                     CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                      Figure 16.31   Synchronous DRAM Write Bus Cycle
           (Bank Active, Different Row Access, TRP = 1 Cycle, RCD = 1 Cycle)





                                                                                                                  517
```

## PDF page 534

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=534)

```text
                               Tp            Tpw             Tr            Trw            Tc

             CKIO


            Upper
          address

            Lower
          address


                BS


              CSn
                                         tRWD

           RD/WR/CR
               WE

                RD
                                        tDQMD
              WEn/CR
            CASxx/CR
           DQMxx


          D31–D0



           DACKn


             WAIT

                                        tRASD1       tRASD1

              RAS/CR
                CE

              CAS/CR
                OE


              CKE
     Note:    The DACKn waveform shown is for the case where active-high has been specified.

                          Figure 16.32   Synchronous DRAM Write Bus Cycle
              (Bank Active, Different Row Access, TRP = 2 Cycles, RCD = 2 Cycles)





518
```

## PDF page 535

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=535)

```text
                                         Tp                 Tmw

                    CKIO
                                    tAD                           tAD

                   Upper
                 address
                                                   tAD

                   Lower
                 address            tBSD                         tBSD


                       BS
                                    tCSD1                         tCSD1         tCSD1

                    CSn
                                    tRWD                                 tRWD
                 RD/WR,
                      WE

                       RD           tDQMD

                    WEn,
                  CASxx,
                  DQMxx


                 D31–D0


                  DACKn


                   WAIT
                                    tRASD1                              tRASD1  tRASD1

                    RAS,
                       CE
                                                   tCSD1            tCSD1

                    CAS,
                       OE

                     CKE

Figure 16.33   Synchronous DRAM Mode Register Write Cycle (TRP = 1 Cycle)








                                                                                                                 519
```

## PDF page 536

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=536)

```text
                                       Tp                 Tpw              Tmw

                    CKIO


                   Upper
                 address


                   Lower
                 address


                       BS


                    CSn
                                                 tRWD             tRWD
                 RD/WR,
                      WE


                       RD

                    WEn,
                  CASxx,
                  DQMxx


                 D31–D0


                  DACKn


                   WAIT
                                                 tRASD1           tRASD1

                    RAS,
                       CE


                    CAS,
                      OE


                     CKE

       Figure 16.34   Synchronous DRAM Mode Register Write Cycle (TRP = 2 Cycles)








520
```

## PDF page 537

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=537)

```text
                                 Trr                 Trc1                                    Tre                TnopTrc2

             CKIO


            Upper
          address            tAD           tAD


            Lower
          address
                                                                                       tBSD

                BS
                                            tCSD1                                                     tCSD1

              CSn
                             tRWD                                                      tRWD
          RD/WR,
               WE


               RD

             WEn,
           CASxx,
           DQMxx

          D31–D0


           DACKn


             WAIT

                             tRASD1        tRASD1                                                      tRASD1

             RAS,
                CE
                             tCASD1        tCASD1

             CAS,
               OE

              CKE
Note:      A precharge cycle always precedes the auto-refresh cycle by the number of cycles
           specified by TRP.

            Figure 16.35   Synchronous DRAM Auto-Refresh Cycle (TRAS = 2 Cycles)





                                                                                                                       521
```

## PDF page 538

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=538)

```text
                              Tp                Trr                             Trc2            Tre             TnopTrc1

             CKIO


             Upper
          address           tAD           tAD


             Lower
          address           tBSD


                BS
                            tCSD1

              CSn
                            tRWD        tRWD

          RD/WR,
               WE


                RD

             WEn,
           CASxx,
           DQMxx


          D31–D0


           DACKn


            WAIT
                            tRASD1

              RAS,
                CE                      tCASD1

              CAS,
                OE


              CKE

                        Figure 16.36   Synchronous DRAM Auto-Refresh Cycle
                 (Shown From Precharge Cycle, TRP = 1 Cycle, TRAS = 2 Cycles)







522
```

## PDF page 539

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=539)

```text
                    Trr    Trc1        Trc2     Tre                                        Trc1Tre     Tnap

       CKIO


       Upper
     address         tAD      tAD
       Lower
     address

          BS
                            tCSD1                                                                        tCSD1
         CSn
                    tRWD
     RD/WR/CR
          WE

          RD

        WEn/CR
      CASxx
     DQMxx

    D31–D0

     DACKn


       WAIT
              tRASD1           tRASD1                                                          tRASD1
        RAS
          CE  tCASD1
                              tCASD1
        CAS
          OE                                                           tCKED
                      tCKED
        CKE

Note:   A precharge cycle always preceds the self-refresh cycle by the number of cycles specified
         by TRP.

                 Figure 16.37   Synchronous DRAM Self-Refresh Cycle (TRAS = 2)












                                                                                                                       523
```

## PDF page 540

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=540)

```text
                     Tr            Tc          Td1         Td2          Td3         Td4

   CKIO                                                                                 tAD
                   tAD
   Upper
 address                                            tAD         tAD         tAD
                                tAD
   Lower
 address
                     tBSD                    tBSD                                tBSD
      BS

                     tCSD1                                                      tCSD1
     CSn
                     tRWD                                                                            tRWD
 RD/WR/CR
      WE                                                                                     tRSD2

      RD
                 tDQMD                                                                  tDQMD
    WEn/CR                                  tRDH3       tRDH3       tRDH3      tRDH3
  CASxx/CR                                          tRDS2       tRDS2       tRDS2
 DQMxx                           tRDS2

D31–D0
                                           tDACD3      tDACD3     tDACD3      tDACD3
                 tDACD1                          tDACD1      tDACD1      tDACD1

 DACKn


   WAIT
                 tRASD1      tRASD1                                                     tRASD1

    RAS/CR
      CE                     tCASD1      tCASD1                                         tCASD1      tCASD1

    CAS/CR
      OE

    CKE

Notes:   1.    The dotted line shows the waveform when synchronous DRAM in another CS space
               is accessed.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

                         Figure 16.38   Synchronous DRAM Read Bus Cycle
         (RCD = 1 Cycle, CAS Latency = 1 Cycle, TRP = 1 Cycle, Bursts = 4, PLL Off)



524
```

## PDF page 541

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=541)

```text
                                            Tr            Tc            Tap

                          CKIO


                          Upper
                        address

                          Lower
                        address


                             BS


                            CSn


                        RD/WR/CR
                            WE


                             RD

                           WEn/CR
                         CASxx/CR
                        DQMxx                       tWDD         tDOF
                                              tDON           tWDH1

                        D31–D0

                                                            tDACD3
                        DACKn


                           WAIT



                            RAS/CR
                             CE

                            CAS/CR
                             OE


                            CKE
Notes:   1.    Dotted lines show the waveforms when synchronous DRAM in another CS space
               is accessed.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

                        Figure 16.39   Synchronous DRAM Write Bus Cycle
                             (RCD = 1 Cycle, TRWL = 1 Cycle, PLL Off)




                                                                                                              525
```

## PDF page 542

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=542)

```text
                                Tp                Tr              Tc1              Tc2

                CKIO                     tAD                                      tAD


               Upper
             address
                                                       tAD
               Lower
             address
                           tBSD                       tBSD          tBSD

                  BS
                                         tCSD1                                    tCSD1

                 CSn
                                         tRWD                                     tRWD
            RD/WR,
                  WE
                             tRSD1                     tRSD1          tRSD1

                  RD
                             tCASD2                     tCASD2        tCASD2
                WEn,
             CASxx,                                                              tRDH5
             DQMxx
                                                                        tRDS1

            D31–D0
                                          tDACD1                     tDACD2

             DACKn



               WAIT
                             tRASD2        tRASD2                    tRASD2

                RAS,
                  CE

                CAS,
                  OE
                 CKE
Notes:   1.    tRDH5 is specified from the rise of RD or CASxx, whichever is first.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

    Figure 16.40   DRAM Read Cycle (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL On)







526
```

## PDF page 543

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=543)

```text
                                     Tp                Tr              Tc1              Tc2

                     CKIO                     tAD                                    tAD


                    Upper
                  address
                                                           tAD
                    Lower
                  address
                                tBSD                       tBSD         tBSD

                        BS
                                              tCSD1                                  tCSD1

                      CSn
                                              tRWD          tRWD                     tRWD
                  RD/WR,
                       WE
                                  tRSD1

                        RD
                                  tCASD2                    tCASD2       tCASD2
                     WEn,
                   CASxx,                                   tWDD                       tDOF
                   DQMxx                                   tDON                              tWDH1


                  D31–D0
                                               tDACD1                    tDACD2

                   DACKn


                     WAIT
                                  tRASD2       tRASD2                    tRASD2

                     RAS,
                        CE

                     CAS,
                        OE
                      CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                                 Figure 16.41   DRAM Write Cycle
                     (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL On)




                                                                                                                 527
```

## PDF page 544

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=544)

```text
                        Tp           Tpw           Tr           Trw           Tc1                          Tc2Tw

        CKIO

        Upper
     address

        Lower
     address


           BS


          CSn

     RD/WR,
          WE

           RD

        WEn,
      CASxx,
      DQMxx


     D31–D0

      DACKn
                                                                                 tWTS    tWTH

        WAIT


         RAS,
           CE


         CAS,
           OE

         CKE
    Note:    The DACKn waveform shown is for the case where active-high has been specified.

           Figure 16.42   DRAM Bus Cycle (TRP = 2 Cycles, RCD = 2 Cycles, 1 Wait)







528
```

## PDF page 545

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=545)

```text
                      Tp                Tr             Tc1            Tw             Twx            Tc2

       CKIO

      Upper
    address

      Lower
    address


          BS


        CSn


    RD/WR,
         WE


         RD


       WEn,
    CASxx,
    DQMxx


   D31–D0

    DACKn
                                                               tWTS  tWTH  tWTS tWTH

       WAIT


       RAS,
         CE

       CAS,
         OE


        CKE
 Note:    The DACKn waveform shown is for the case where active-high has been specified.

Figure 16.43   DRAM Bus Cycle (TRP = 1 Cycle, RCD = 1 Cycle, External Wait Input)





                                                                                                           529
```

## PDF page 546

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=546)

```text
                            Tp             Tr              Tc1            Tc2           Tc1             Tc2

            CKIO

            Upper
          address                                                      tAD

            Lower
          address
                                                           tBSD        tBSD

               BS



              CSn


         RD/WR,
              WE
                                                            tRSD1       tRSD1

               RD
                                                            tCASD2       tCASD2
            WEn,
          CASxx,
          DQMxx                                                     tRDH5                   tRDH5

                                                                tRDS1                  tRDS1

         D31–D0
                                                            tDACD2     tDACD1

          DACKn



            WAIT


             RAS,
               CE

             CAS,
               OE

             CKE
Notes:   1.     tRDH5 is specified from the rise of RD or CASxx, whichever is first.
          2.    The DACKn waveform shown is for the case where active-high has been specified.

                                    Figure 16.44   DRAM Burst Read Cycle
                           (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL On)






530
```

## PDF page 547

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=547)

```text
                          Tp               Tr              Tc1            Tc2            Tc1           Tc2

          CKIO


          Upper
        address                                                       tAD

          Lower
        address
                                                          tBSD        tBSD

             BS


            CSn


       RD/WR,
            WE


             RD                                           tCASD2       tCASD2

          WEn,
        CASxx,                                                        tWDD
        DQMxx
                                                                      tWDH1

       D31–D0
                                                           tDACD2     tDACD1

        DACKn


          WAIT


           RAS,
             CE

           CAS/CR
             OE


           CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                             Figure 16.45   DRAM Burst Write Cycle
                     (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL On)





                                                                                                               531
```

## PDF page 548

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=548)

```text
                                   Tp           Trr                Trc1               Trc2               Tre

                  CKIO


                 Upper
               address


                 Lower
               address


                    BS
                              tCSD1                                                              tCSD1

                   CSn


               RD/WR,
                    WE


                    RD
                                tCASD2       tCASD2                    tCASD2
                  WEn,
                CASxx,
                DQMxx


               D31–D0



                DACKn


                  WAIT

                                tRASD2                     tRASD2                    tRASD2

                  RAS,
                    CE

                  CAS,
                    OE

                   CKE

                          Figure 16.46   DRAM CAS-Before-RAS Refresh Cycle
                                 (TRP = 1 Cycle, TRAS = 2 Cycles, PLL On)











532
```

## PDF page 549

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=549)

```text
                                     Tp                Tr              Tc1              Tc2

                     CKIO                    tAD                                    tAD


                    Upper
                  address                                 tAD

                    Lower
                  address
                                tBSD                     tBSD          tBSD

                        BS                 tCSD1                                 tCSD1


                      CSn
                                           tRWD                                     tRWD
                  RD/WR,
                       WE                                       tRSD2        tRSD2
                                    tRSD2

                        RD
                                    tCASD3                      tCASD3       tCASD3
                     WEn,
                   CASxx,                                     tASC                 tRDH5
                   DQMxx                                               tRDS2


                  D31–D0
                                         tDACD1                               tDACD3

                   DACKn
                                                    tRASD3

                     WAIT
                                       tRASD3     tASR                       tRASD3

                     RAS,
                        CE

                     CAS,
                        OE
                      CKE
Notes:   1.    tRDH5 is specified from the rise of RD or CASxx, whichever is first.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

   Figure 16.47   DRAM Read Cycle (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL Off)






                                                                                                                    533
```

## PDF page 550

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=550)

```text
                                           Tp                Tr              Tc1             Tc2

                           CKIO                    tAD                                    tAD


                          Upper
                        address
                                                                tAD
                          Lower
                        address
                                      tBSD                    tBSD           tBSD

                              BS
                                                tCSD1                                     tCSD1

                            CSn
                                                  tRWD          tRWD                      tRWD

                        RD/WR,
                             WE
                                             tRSD2                     tCASD3

                              RD                                     tASC
                                          tCASD3                    tWCS            tCASD3
                           WEn,
                         CASxx,
                         DQMxx                                   tWDD   tWDS                tDOF

                                                                tDON                              tWDH1

                        D31–D0
                                                tDACD1                              tDACD3

                         DACKn
                                                          tRASD3

                           WAIT
                                                        tASR                        tRASD3

                           RAS,
                              CE

                           CAS,
                              OE

                            CKE
     Note:    The DACKn waveform shown is for the case where active-high has been specified.

   Figure 16.48   DRAM Write Cycle (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL Off)







534
```

## PDF page 551

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=551)

```text
                            Tp                Tr              Tc1           Tc2            Tc1             Tc2

             CKIO


            Upper
          address                                                       tAD

            Lower
          address
                                                            tBSD      tBSD

                BS


              CSn


          RD/WR,
               WE                                               tRSD2         tRSD2


                RD
                                                               tCASD3         tCASD3
             WEn,
           CASxx,
           DQMxx                                                     tRDH5  tASC             tRDH5
                                                           tRDS2                   tRDS2

          D31–D0
                                                                tDACD3  tDACD1

           DACKn


             WAIT


             RAS,
                CE

             CAS,
                OE


              CKE
Notes:   1.     tRDH5 is specified from the rise of RD or CASxx, whichever is first.
          2.    The DACKn waveform shown is for the case where active-high has been specified.

                                   Figure 16.49   DRAM Burst Read Cycle
                           (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL Off)







                                                                                                                        535
```

## PDF page 552

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=552)

```text
                                 Tp              Tr             Tc1             Tc2            Tc1             Tc2

                   CKIO


                  Upper
                address                                                      tAD

                  Lower
                address
                                                                 tBSD      tBSD

                     BS


                    CSn


                RD/WR,
                     WE


                     RD                                              tCASD3        tCASD3
                                                                                tASC
                   WEn,
                 CASxx,
                 DQMxx                                                       tWDD  tWDS
                                                                          tWDH1

                D31–D0
                                                                    tDACD3   tDACD1

                 DACKn


                   WAIT


                   RAS,
                     CE

                   CAS,
                     OE


                    CKE
     Note:    The DACKn waveform shown is for the case where active-high has been specified.

                                   Figure 16.50   DRAM Burst Write Cycle
                           (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL Off)








536
```

## PDF page 553

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=553)

```text
                    Tp           Trr                Trc1               Trc2               Tre

  CKIO


  Upper
address


  Lower
address



     BS
              tCSD1                                                                 tCSD1

    CSn
                             tRWD

RD/WR,
    WE
                     tRSD2

     RD
                      tCASD3       tCASD3                      tCASD3
  WEn,
CASxx,
DQMxx


D31–D0



DACKn


  WAIT

                      tRASD3                      tRASD3                     tRASD3

   RAS,
     CE

   CAS,
     OE

   CKE

              Figure 16.51   DRAM CAS-Before-RAS Refresh Cycle
                    (TRP = 1 Cycle, TRAS = 2 Cycles, PLL Off)






                                                                                                       537
```

## PDF page 554

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=554)

```text
                                           Tp                 Tr                 Tc1                Tc2

                          CKIO
                                                     tAD                                        tAD

                         Upper
                       address
                                                                   tBSD          tBSD

                             BS
                                                      tCSD1                                     tCSD1

                           CSn
                                                      tRWD                                      tRWD

                       RD/WR,
                            WE
                                        tRSD1                        tRSD1         tRSD1         tRSD1

                            RD
                                        tWED1                                                    tWED1
                          WEn,
                        CASxx,                                                               tRDH6
                        DQMxx                                                        tRDS1


                       D31–D0
                                                    tDACD1                         tDACD2

                        DACKn


                          WAIT

                                        tCED1            tCED1                     tCED1

                          RAS,
                            CE
                                        tOED1                                tOED1 tOED1

                          CAS,
                            OE


                           CKE
     Note:    The DACKn waveform shown is for the case where active-high has been specified.

                                    Figure 16.52   Pseudo-SRAM Read Cycle
                            (PLL On, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








538
```

## PDF page 555

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=555)

```text
                                    Tp                 Tr                 Tc1                Tc2

                  CKIO
                                              tAD                                         tAD

               Address
                                                            tBSD          tBSD

                     BS
                                             tCSD1                                        tCSD1

                   CSn
                                                            tRWD                          tRWD

               RD/WR,
                    WE
                                 tRSD1                                                     tRSD1

                     RD
                                 tWED1                        tWED1         tWED1          tWED1
                  WEn,
                CASxx,                                        tWDD                          tDOF
                DQMxx                                       tDON                                   tWDH1


               D31–D0
                                             tDACD1                         tDACD2

                DACKn


                  WAIT

                                 tCED1            tCED1                      tCED1

                  RAS,
                     CE
                                 tOED1

                  CAS,
                     OE

                   CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                             Figure 16.53   Pseudo-SRAM Write Cycle
                      (PLL On, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)




                                                                                                                   539
```

## PDF page 556

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=556)

```text
                        Tp           Tpw           Tr           Trw           Tc1                          Tc2Tw

        CKIO


     Address


           BS


          CSn

     RD/WR,
          WE

           RD

        WEn,
      CASxx,
      DQMxx


     D31–D0

      DACKn
                                                                                  tWTS   tWTH

        WAIT


         RAS,
           CE

         CAS,
           OE

         CKE
    Note:    The DACKn waveform shown is for the case where active-high has been specified.

      Figure 16.54   Pseudo-SRAM Bus Cycle (TRP = 2 Cycles, RCD = 2 Cycles, 1 Wait)










540
```

## PDF page 557

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=557)

```text
                     Tp                Tr             Tc1            Tw            Twx            Tc2

      CKIO


  Address


         BS


       CSn


  RD/WR,
        WE


        RD

      WEn,
   CASxx,
   DQMxx


  D31–D0


   DACKn
                                                             tWTS  tWTH  tWTS tWTH

     WAIT


      RAS,
        CE

      CAS,
        OE


       CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                           Figure 16.55   Pseudo-SRAM Bus Cycle
                   (TRP = 1 Cycle, RCD = 1 Cycle, External Wait Input)







                                                                                                          541
```

## PDF page 558

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=558)

```text
                                 Tp             Tr              Tc1            Tc2           Tc1             Tc2

                CKIO

               Upper
             address                                                       tAD

               Lower
             address
                                                               tBSD        tBSD

                   BS


                  CSn

             RD/WR,
                  WE
                                                                 tRSD1         tRSD1

                   RD

                WEn,
              CASxx,                                                    tRDH6                   tRDH6
              DQMxx
                                                                    tRDS1                tRDS1

             D31–D0
                                                                tDACD2     tDACD1

              DACKn


                WAIT


                RAS,
                   CE                                            tOED1         tOED1

                CAS,
                   OE


                 CKE
     Note:    The DACKn waveform shown is for the case where active-high has been specified.

                                 Figure 16.56   Pseudo-SRAM Read Cycle
            (Static Column Mode, PLL On, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)




542
```

## PDF page 559

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=559)

```text
                            Tp              Tr             Tc1               Tc2           Tc1             Tc2

             CKIO


             Upper
           address                                                    tAD

             Lower
           address
                                                           tBSD       tBSD

                BS


              CSn


          RD/WR,
               WE


                RD                                          tWED1      tWED1


             WEn,
           CASxx,                                                      tWDD
           DQMxx
                                                                      tWDH1

          D31–D0
                                                           tDACD2     tDACD1

           DACKn


             WAIT


             RAS,
                CE

             CAS,
                OE


              CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                            Figure 16.57   Pseudo-SRAM Write Cycle
       (Static Column Mode, PLL On, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








                                                                                                                 543
```

## PDF page 560

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=560)

```text
                                    Tp                 Trr                Trc1               Trc2                Tre

                   CKIO


                 Address



                      BS



                     CSn



                 RD/WR,
                     WE


                      RD
                                 tWED1                                                                tWED1
                   WEn,
                 CASxx,
                 DQMxx


                 D31–D0


                 DACKn



                   WAIT

                                 tCED1                                                                tCED1

                    RAS,
                      OE
                                 tOED1         tOED1                                     tOED1        tOED1

                    CAS,
                      OE


                    CKE

                              Figure 16.58   Pseudo-SRAM Auto-Refresh Cycle
                                  (PLL On, TRP = 1 Cycle, TRAS = 2 Cycles)











544
```

## PDF page 561

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=561)

```text
               Tp        Trc      Trc1      Trc2                                         Trc1       Tre

  CKIO



Address



     BS



    CSn



RD/WR,
     WE


     RD
             tWED1
   WEn,
CASxx,
DQMxx


D31–D0



DACKn



  WAIT
             tCED1                                                                       tCED1

   RAS,
     CE
             tOED1    tOED1                                                      tOED1   tOED1

   CAS,
     OE


    CKE

                   Figure 16.59   Pseudo-SRAM Self-Refresh Cycle
                      (PLL On, TRP = 1 Cycle, TRAS = 2 Cycles)





                                                                                                           545
```

## PDF page 562

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=562)

```text
                                             Tp                Tr              Tc1             Tc2

                             CKIO                     tAD                                    tAD


                          Address
                                                                  tBSD         tBSD

                                BS
                                                  tCSD1                                      tCSD1

                               CSn
                                                  tRWD                                        tRWD
                           RD/WR,
                                WE
                                               tRSD2                      tRSD2        tRSD2

                                RD
                                            tWED2
                             WEn,                                                        tRDH6
                           CASxx,
                           DQMxx                                              tRDS2


                          D31–D0
                                                tDACD1                                     tDACD3

                           DACKn



                             WAIT
                                                            tCED2
                                               tCED2      tAS2                    tCED2

                              RAS,
                                CE
                                               tOED2                     tOED2        tOED2

                              CAS,
                                OE


                              CKE
     Note:    The DACKn waveform shown is for the case where active-high has been specified.

                                   Figure 16.60   Pseudo-SRAM Read Cycle
                           (PLL Off, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








546
```

## PDF page 563

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=563)

```text
                                           Tp               Tr               Tc1             Tc2

                           CKIO                     tAD                                    tAD


                        Address
                                                                 tBSD         tBSD

                              BS
                                                 tCSD1                                     tCSD1

                            CSn

                                                 tRWD            tRWD                      tRWD

                        RD/WR,
                              WE
                                           tRSD2

                             RD
                                          tWED2                                                tWED2tWED2
                           WEn,
                        CASxx,
                        DQMxx                                    tWDD                       tDOF

                                                                 tDON                      tWDH1

                       D31–D0
                                                                                      tWDH3
                                              tDACD1                                        tDACD3

                        DACKn



                          WAIT
                                             tCED2        tCED2
                                                        tAS2                         tCED2

                           RAS,
                             CE              tOED2

                           CAS,
                             OE

                            CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                               Figure 16.61   Pseudo-SRAM Write Cycle
                       (PLL Off, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








                                                                                                                             547
```

## PDF page 564

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=564)

```text
                                   Tp                Tr            Tc1           Tc2             Tc1            Tc2

                   CKIO


                  Upper
                address
                                                                             tAD

                  Lower
                address                                          tBSD        tBSD


                     BS


                    CSn


                RD/WR,
                     WE                                               tRSD2       tRSD2

                     RD


                   WEn,
                 CASxx
                 DQMxx                                                                           tRDH6
                                                                tRDS2        tRDH6     tRDS2


                D31–D0
                                                                   tDACD3    tDACD1

                    CSn


                   WAIT


                   RAS,
                     CE                                             tOED2         tOED2


                   CAS,
                     OE


                    CKE
     Note:    The DACKn waveform shown is for the case where active-high has been specified.

                                  Figure 16.62   Pseudo-SRAM Read Cycle
            (Static Column Mode, PLL Off, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








548
```

## PDF page 565

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=565)

```text
                      Tp              Tr             Tc1             Tc2            Tc1            Tc2

      CKIO
                               tAD                                                               tAD

     Upper
   address
                                                                      tAD

     Lower
   address
                                            tBSD          tBSD        tBSD           tBSD

         BS
                               tCSD1                                                             tCSD1

       CSn
                                            tRWD                                                tRWD

  RD/WR,
        WE

        RD
                                                   tWED2     tWED2           tWED2        tWED2
      WEn,
   CASxx,                                                              tWDD                       tDOF
   DQMxx                                     tWDD                     tWDH1                     tWDH1

                                            tDON                   tWDH3                         tWDH3

  D31–D0
                               tDACD1                            tDACD3tDACD1             tDACD3

   DACKn


     WAIT
                                     tCED2                                                 tCED2
      RAS,
        CE

      CAS,
        OE


       CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                            Figure 16.63   Pseudo-SRAM Write Cycle
       (Static Column Mode, PLL Off, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)





                                                                                                              549
```

## PDF page 566

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=566)

```text
                               Tp                 Trr                Trc1               Trc2                Tre

              CKIO



           Address



                BS



               CSn



           RD/WR,
                WE


                RD

              WEn,
           CASxx,
           DQMxx


           D31–D0



           DACKn



             WAIT

                                     tCED2                                                       tCED2
              RAS,
                OE
                         tOED2                     tOED2                           tOED2         tOED2
              CAS,
                OE


               CKE

                            Figure 16.64   Pseudo-SRAM Auto-Refresh Cycle
                                (PLL Off, TRP = 1 Cycle, TRAS = 2 Cycles)






550
```

## PDF page 567

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=567)

```text
               Tp        Trc        Trc1    Trc2    Trc2                  Trc2     Trc1      Tre

  CKIO



Address



     BS



    CSn


RD/WR,
     WE



     RD


  WEn,
CASxx,
DQMxx


D31–D0



DACKn



  WAIT
                 tCED2                                                                       tCED2

   RAS,
     CE
                tOED2    tOED2                                              tOED2            tOED2

   CAS,
     OE


   CKE

                  Figure 16.65   Pseudo-SRAM Self-Refresh Cycle
                     (PLL Off, TRP = 1 Cycle, TRAS = 2 Cycles)





                                                                                                          551
```

## PDF page 568

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=568)

```text
                                                     T1             TW              T2             TW             T2

                   CKIO
                                               tAD                                         tAD                              tAD

                   CKIO
                                               tBSD           tBSD                            tBSD           tBSD

                      BS
                                              tCSD1                                                        tCSD2

                    CSn
                                               tRWD                                                                     tRWD

               RD/WR,
                     WE                          tRSD1                        tRSD1          tRSD1        tRSD1


                     RD
                                  tWED1                                                                                                tWED1
                  WEn,
                CASxx,                                                                     tRDH2                        tRDH2
                DQMxx
                                                                                  tRDS1                        tRDS1

               D31–D0
                                               tDACD1                                          tDACD1tDACD2                         tDACD2

                DACKn
                                                                     tWTS  tWTH                  tWTS  tWTH

                  WAIT


                   RAS,
                     CE

                   CAS,
                     OE


                   CKE

      Note:    The DACKn waveform shown is for the case where active-high has been specified.

                            Figure 16.66   Burst ROM Read Cycle (PLL On, 1 Wait)









552
```

## PDF page 569

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=569)

```text
                                          T1              TW             T2             TW             T2
           CKIO
                                     tAD                                       tAD                              tAD

       A26–A0
                                     tBSD          tBSD                            tBSD           tBSD

             BS
                                   tCSD1                                                          tCSD3

           CSn
                                     tRWD                                                                   tRWD

      RD/WR,
            WE                               tRSD2                     tRSD2        tRSD2            tRSD2


             RD
                            tWED2
          WEn,
       CASxx,                                                               tRDH2                          tRDH2
       DQMxx
                                                                 tRDS2                      tRDS2

      D31–D0
                                     tDACD1                               tDACD3tDACD1               tDACD3

       DACKn
                                                          tWTS  tWTH                  tWTS  tWTH

          WAIT


          RAS,
             CE

          CAS,
             OE


           CKE

Note:    The DACKn waveform shown is for the case where active-high has been specified.

                    Figure 16.67   Burst ROM Read Cycle (PLL Off, 1 Wait)








                                                                                                                             553
```

## PDF page 570

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=570)

```text
                                                 T1                                           T2

             CKIO

                                      tAD                                                                                 tAD


           A4–A0


                                      tIVD                                                                        tIVD


            IVECF

                                     tRWD


          RD/WR

                                                tRSD1                                  tRSD1


                RD
                                                                                  tRDS1

                                                                                                                       tRDH7

           D7–D0

                                                                  tWTS      tWTH


             WAIT

                     Figure 16.68   Interrupt Vector Fetch Cycle (PLL On, No Waits)



















554
```

## PDF page 571

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=571)

```text
                                      T1                                           T2

  CKIO

                           tAD                                                                                 tAD


 A4–A0


                           tIVD                                                                        tIVD


 IVECF

                          tRWD


RD/WR

                                              tRSD2                        tRSD2


      RD
                                                                       tRDS2

                                                                                                           tRDH7

D7–D0

                                                       tWTS     tWTH


  WAIT

          Figure 16.69   Interrupt Vector Fetch Cycle (PLL Off, No Waits)



















                                                                                                                           555
```

## PDF page 572

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=572)

```text
                                        T1                               TW                             T2

          CKI0




        A4–A0




        IVECF



      RD/WR




            RD



       D7–D0

                                                tWTS                                      tWTHtWTHtWTS

         WAIT


                 Figure 16.70   Interrupt Vector Fetch Cycle (1 External Wait Cycle)
























556
```

## PDF page 573

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=573)

```text
  CKIO


                                        tASIN       tAHIN

A26–A2


                                         tBSS        tBSH

     BS

                                        tRWS       tRWH


RD/WR

              Figure 16.71   Address Monitor Cycle






































                                                                                             557
```

## PDF page 574

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=574)

```text
16.3.4      DMAC Timing

Table 16.10      DMAC Timing (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                                                                 Symbol   Min                 Max   Unit   Figure
DREQ0, DREQ1 setup time (PLL Off, On)                      tDRQS       50                   —      ns      16.72
DREQ0, DREQ1 setup time (PLL On, 1/4 cycle delay)   tDRQS       50 – 1/4 tcyc   —      ns
DREQ0, DREQ1 hold time (PLL Off, On)                        tDRQH       50                   —      ns
DREQ0, DREQ1 hold time (PLL On, 1/4 cycle delay)     tDRQH       1/4 tcyc + 50   —      ns
DREQ0, DREQ1 low level width                                     tDRQW      1.5                  —      tcyc




                           CKIO

                                                                         tDRQS

            DREQ0, DREQ1
                            level

                                                                         tDRQS              tDRQH

            DREQ0, DREQ1
                            edge

                                                                         tDRQS

            DREQ0, DREQ1
            level cancellation

                                 Figure 16.72   DREQ0, DREQ1 Input Timing


















558
```

## PDF page 575

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=575)

```text
16.3.5      Free-Running Timer Timing

Table 16.11      Free-Running Timer Timing (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to
                      +75°C)

Item                                                             Symbol     Min                  Max                  Unit     Figure
Output compare output delay time                          tTOCD         —                    320                   ns        16.73
(PLL Off, On)
Output compare output delay time                          tTOCD         —                    1/4 tcyc + 320  ns
(PLL On, 1/4 cycle delay)
Input capture input setup time                            tTICS          80                    —                     ns
(PLL Off, On)
Input capture input setup time                            tTICS          80 – 1/4 tcyc    —                     ns
(PLL On, 1/4 cycle delay)
Timer clock input setup time                              tTCKS         80                    —                     ns        16.74
(PLL Off, On)
Timer clock input setup time                              tTCKS         80 –1/4 tcyc     —                     ns
(PLL On, 1/4 cycle delay)
Timer clock pulse width (single edge)         tTCKWH       4.5                   —                     tcyc
Timer clock pulse width (both edges)         tTCKWL       8.5                   —                     tcyc



      CKIO

                                                    tTOCD

     FTOA,
     FTOB
                                                                                                    tTICS


         FTI

                                     Figure 16.73   FRT Input/Output Timing



   CKIO
                                                                     tTCKS

    FTCI

                                     tTCKWL                                                 tTCKWH

                                       Figure 16.74   FRT Clock Input Timing

                                                                                                                                   559
```

## PDF page 576

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=576)

```text
16.3.6      Watchdog Timer Timing

Table 16.12      Watchdog Timer Timing (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                                               Symbol       Min          Max                  Unit       Figure
WDTOVF delay time (PLL Off, On)               tWOVD          —            70                     ns          16.75
WDTOVF delay time (PLL On, 1/4 cycle                     tWOVD          —            1/4 tcyc + 70    ns
delay)




         CKIO                                       tWOVD                       tWOVD



   WDTOVF


                               Figure 16.75   Watchdog Timer Output Timing






































560
```

## PDF page 577

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=577)

```text
16.3.7      Serial Communication Interface Timing

Table 16.13      Serial Communication Interface Timing
                      (Conditions: VCC = 3.0 to 5.5 V, Ta = –20 to +75°C)

Item                                                                         Symbol     Min           Max          Unit     Figure
Input clock cycle                                                      tscyc           16             —             tcyc       16.76
Input clock cycle (clocked synchronous mode)       tscyc           24             —             tcyc
Input clock pulse width                                             tsckw           0.4            0.6            tscyc
Transmission data delay time (clocked                                tTXD           —             70             ns         16.77
synchronous mode)
Receive data setup time (clocked synchronous                         tRXS           70             —             ns
mode)
Receive data hold time (clocked synchronous                          tRXH           70             —             ns
mode)



                                                      tSCKW


                                SCK0

                                                                              tscyc

                                Figure 16.76   Input Clock Input/Output Timing


                                                     tscyc

                SCK0

                                            tTXD

                TxD0
    (transmit data)
                                                                          tRXS     tRXH

                RxD0
     (receive data)


                Figure 16.77   SCI Input/Output Timing (Clocked Synchronous Mode)





                                                                                                                                   561
```

## PDF page 578

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=578)

```text
16.3.8      AC Characteristics Measurement Conditions

•    I/O signal reference level: 1.5 V
•    Input pulse level: VSS to 3.0 V (where RES, NMI, CKIO and MD5-MD0 are within the range
    VSS to VCC)
•    Input rise and fall times: 1 ns


                                                    IOL







               SH7604
                 output                                                         DUT output
                     pin           CL                                       V   VREF







                                                    IOH


Notes:   1.    CL is a total value that includes the capacitance of measurement instruments, etc.,
               and is set as follows for each pin.
               30 pF: CKIO, RAS, CAS, CKE, CS0–CS3, BREQ, BACK, DACK0, DACK1, IVECF,
               CKPACK.
               50 pF: All output pins other than the above.
         2.    IOL and  IOH values are as shown in section 16.2, DC Characteristics, and table
               16.3, Permitted Output Current Values.

                                    Figure 16.78   Output Load Circuit


















562
```
