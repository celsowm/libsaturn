# Section 15 Electrical Characteristics (5V Version)

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=411)

Physical PDF pages 411–494. Text extraction; consult the PDF for figures.

## Bookmarks

- [15.1 Absolute Maximum Ratings](#pdf-page-411)
- [15.2 DC Characteristics](#pdf-page-412)
- [15.3 AC Characteristics](#pdf-page-414)
- [15.3.1 Clock Timing](#pdf-page-414)
- [15.3.2 Control Signal Timing](#pdf-page-418)
- [15.3.3 Bus Timing](#pdf-page-424)
- [15.3.4 DMAC Timing](#pdf-page-490)
- [15.3.5 Free-Running Timer Timing](#pdf-page-491)
- [15.3.6 Watchdog Timer Timing](#pdf-page-492)
- [15.3.7 Serial Communication Interface Timing](#pdf-page-493)
- [15.3.8 AC Characteristics Measurement Conditions](#pdf-page-494)

## PDF page 411

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=411)

```text
           Section 15   Electrical Characteristics (5V Version)


15.1      Absolute Maximum Ratings

Table 15.1 shows the absolute maximum ratings.

Table 15.1    Absolute Maximum Ratings

Item                                                                         Symbol        Rating                             Unit
Power supply voltage                                              VCC              –0.3 to +7.0                     V
Input voltage                                                           Vin                –0.3 to VCC + 0.3            V
Operating temperature                                            Topr             –20 to +75                         °C
Storage temperature                                               Tstg              –55 to +125                       °C
Caution:   Operating the chip in excess of the absolute maximum rating may result in permanent
              damage.







































                                                                                                                                      395
```

## PDF page 412

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=412)

```text
15.2      DC Characteristics

Tables 15.2 and 15.3 list DC characteristics.

Table 15.2    DC Characteristics (Conditions: VCC = 5.0 V – 10%, Ta = –20 to +75° C)

Item                                         Symbol   Min            Typ    Max           Unit   Test Conditions
Input high-   RES, NMI,            VIH           VCC – 0.5   —      VCC + 0.3   V        During standby
level             MD5–MD0                            VCC – 0.7   —      VCC + 0.3   V        Normal operation
voltage        EXTAL, CKIO                       VCC – 0.7   —      VCC + 0.3   V

                  Other input pins                   2.2             —      VCC + 0.3   V
Input low-    RES, NMI,            VIL           –0.3           —      0.5             V        During standby
level             MD5–MD0                            –0.3           —      0.8             V        Normal operation
voltage        Other input pins                   –0.3           —      0.8             V

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
voltage                                                     3.5             —      —               V        IOH = –1 mA
Output            All output pins      VOL          —              —      0.4             V        IOL = 1.6 mA
low-level
voltage
Input            RES                     Cin           —              —      15              pF      Vin = 0 V
capaci-        NMI                                      —              —      15              pF      f = 1 MHz
tance           All other input
                                                           —              —      15              pF      Ta = 25°C
                  pins (including
                  D31–D0)











396
```

## PDF page 413

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=413)

```text
Table 15.2    DC Characteristics (Conditions: VCC = 5.0 V – 10%, Ta = –20 to +75° C) (cont)

Item                                         Symbol   Min            Typ    Max           Unit   Test Conditions
Current        Normal                 ICC           —              60      80              mA     f = 8 MHz
consump-    operation                              —              80      100            mA     f = 16 MHz
tion                                                           —              110    160            mA     f = 28.7 MHz

                Sleep                                    —              30      55              mA     f = 8 MHz
                                                    —              50      70              mA     f = 16 MHz
                                                    —              80      100            mA     f = 28.7 MHz
                Standby                                —              1        15                mA      Ta £ 50°C
                                                    ——60mA50°C < Ta
Notes:  1.   When no PLL is used, do not leave the PLLVCC and PLLVSS pins open. Connect
              PLLVCC to VCC and PLLVSS to VSS.
          2.   Current consumption values shown are the values at which all output pins are without
              load under conditions of VIH min = VCC – 0.5 V, VIL max = 0.5 V.


Table 15.3    Permitted Output Current Values (Conditions: VCC = 5.0 V – 10%, Ta = –20 to
                 +75°C)

Item                                                                         Symbol     Min         Typ        Max        Unit
Output low-level permissible current (per pin)        IOL             —           —           2.0          mA
Output low-level permissible current (total)              å  IOL ——80mA
Output high-level permissible current (per pin)       –IOH           —           —           2.0          mA
Output high-level permissible current (total)            å (–IOH)——25mA
Caution:   To ensure chip reliability, do not exceed the output current values given in table 15.3.























                                                                                                                          397
```

## PDF page 414

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=414)

```text
15.3      AC Characteristics


15.3.1      Clock Timing

Table 15.4    Clock Timing (Conditions: VCC = 5.0 V – 10%, Ta = –20 to +75°C)

Item                                                                  Symbol    Min            Max          Unit     Figures
Operating frequency                                         fOP            4                28.7          MHz     15.1
Clock cycle time                                                tcyc            35              143*1 or
                                                                                       250*2         ns
Clock high pulse width                                      tCH            8*1 or 15*2—ns
Clock low pulse width                                       tCL            8*1 or 15*2—ns
Clock rise time                                                  tCR —5ns
Clock fall time                                                   tCF —5ns
EXTAL clock input frequency                           fEX            4                8               MHz     15.2
EXTAL clock input cycle time                           tEXcyc        125            250           ns
EXTAL clock input low-level pulse width          tEXL           50              —              ns
EXTAL clock input high-level pulse width         tEXH          50              —              ns
EXTAL clock input rise time                              tEXR —5ns
EXTAL clock input clock fall time                      tEXF —5ns
Power-on oscillation settling time                     tOSC1        10              —              ms        15.3
Software standby oscillation settling time 1      tOSC2        10              —              ms        15.4
Software standby oscillation settling time 2      tOSC3        10              —              ms        15.5
PLL synchronization settling time                     tPLL 1—ms         15.6
Notes:  1.   With PLL circuit 1 operating.
          2.   With PLL circuit 1 not used.




                                                                 tcyc

                                                      tCH                   tCL


                                                 VIH                 VIH             VIH
                      CKIO                      1/2 VCC                VIL          VIL     1/2 VCC
                      (input)
                                                                       tCF                       tCR


                                        Figure 15.1   CKIO Input Timing

398
```

## PDF page 415

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=415)

```text
                                                               tEXcyc

                                                       tEXH                 tEXL


                      EXTAL       1/2 VCC         VIH       VIH                            VIH
                       (input)                                         VIL        VIL      1/2 VCC

                                                                      tEXF                    tEXR




Note:   External clock input from EXTAL pin.

                                  Figure 15.2   EXTAL Clock Input Timing


                                                                                         Stable oscillation



            CKIO,
   internal clock



              VCC              VCC min
                                                                                        tRESW
                                                            tOSC1



              RES




Note:   Oscillation settling time when on-chip crystal oscillator is used.

                           Figure 15.3   Oscillation Settling Time at Power-On
















                                                                                                                        399
```

## PDF page 416

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=416)

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

                Figure 15.4   Oscillation Settling Time at Standby Return (via RES)


                    Standby
                     period                                                              Stable oscillation

        CKIO,
      internal
         clock


                                                        tOSC3



          NMI



Note:   Oscillation settling time when on-chip crystal oscillator is used.

                Figure 15.5   Oscillation Settling Time at Standby Return (via NMI)


















400
```

## PDF page 417

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=417)

```text
                   Stable              Oscillation frequency                          Stable
                 oscillation                modification                            oscillation

 EXTAL
or CKIO
                     PLL                                                    tPLL                 PLL
              synchronization                                                             synchronization

Internal
   clock


                        Figure 15.6   PLL Synchronization Settling Time













































                                                                                                             401
```

## PDF page 418

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=418)

```text
15.3.2      Control Signal Timing

Table 15.5    Control Signal Timing (Conditions: VCC = 5.0 V – 10%, Ta = –20 to +75°C)

Item                                                                     Symbol   Min               Max                Unit    Figure
RES rise, fall                                                       tRESr,           —                 200                 ns       15.7
                                                                       tRESf
RES pulse width                                                 tRESW      20                 —                   tcyc
NMI reset setup time                                           tNMIRS      tcyc + 10      —                   ns
NMI reset hold time                                             tNMIRH      tcyc + 10      —                   ns
NMI rise, fall                                                        tNMIr,          —                 200                 ns
                                                                       tNMIf
NMI minimum pulse width                                   tIRQES      3                   —                   tcyc
RES setup time*                                                  tRESS       30                 —                   ns       15.8,
NMI setup time*                                                   tNMIS        30                 —                   ns       15.9
IRL3–IRL0 setup time*                                        tIRLS         30                 —                   ns
RES hold time                                                     tRESH       10                 —                   ns       15.8,
NMI hold time                                                      tNMIH        10                 —                   ns       15.9
IRL3–IRL0 hold time                                           tIRLH        10                 —                   ns
BRLS setup time 1 (PLL on)                               tBLSS1      1/2 tcyc + 9  —                   ns       15.10
BRLS hold time 1 (PLL on)                                 tBLSH1      9 – 1/2 tcyc  —                   ns
BGR delay time 1 (PLL on)                                 tBGRD1     —                 1/2 tcyc + 18  ns
BRLS setup time 1 (PLL on, 1/4 cycle delay)     tBLSS1      1/4 tcyc + 9  —                   ns       15.10
BRLS hold time 1 (PLL on, 1/4 cycle delay)       tBLSH1      9 – 1/4 tcyc  —                   ns
BGR delay time 1 (PLL on, 1/4 cycle delay)       tBGRD1     —                 3/4 tcyc + 18  ns
BRLS setup time 2 (PLL off)                               tBLSS2      9                   —                   ns       15.11
BRLS hold time 2 (PLL off)                                 tBLSH2      19                 —                   ns
BGR delay time 2 (PLL off)                                 tBGRD2—28ns
Note:   The RES, NMI and IRL3-IRL0 signals are asynchronous inputs, but when the setup times
           shown here are observed, the signals are considered to have changed at clock fall. If the
           setup times are not observed, recognition may be delayed until the next clock fall.












402
```

## PDF page 419

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=419)

```text
Table 15.5    Control Signal Timing (Conditions: VCC = 5.0 V – 10%, Ta = –20 to +75°C)
                 (cont)

Item                                                                     Symbol   Min               Max                Unit    Figure
BREQ delay time 1 (PLL on)                              tBRQD1     —                 1/2 tcyc + 18  ns       15.12
BACK setup time 1 (PLL on)                               tBAKS1      1/2 tcyc + 9  —                   ns
BACK hold time 1 (PLL on)                                 tBAKH1      9 – 1/2 tcyc  —                   ns
BREQ delay time 1 (PLL on, 1/4 cycle delay)     tBRQD1     —                 3/4 tcyc + 18  ns       15.12
BACK setup time 1 (PLL on, 1/4 cycle delay)     tBAKS1      1/4 tcyc + 9  —                   ns
BACK hold time 1 (PLL on, 1/4 cycle delay)       tBAKH1      9 – 1/4 tcyc  —                   ns
BREQ delay time 2 (PLL off)                              tBRQD2     —                 28                   ns       15.13
BACK setup time 2 (PLL off)                               tBAKS29—ns
BACK hold time 2 (PLL off)                                 tBAKH2      19                 —                   ns
Bus tri-state delay time 1 (PLL on)                      tBOFF1      0                   25                   ns       15.10,
Bus buffer on time 1 (PLL on)                             tBON1018ns15.12

Bus tri-state delay time 1 (PLL on, 1/4 cycle                 tBOFF1      1/4 tcyc        1/4 tcyc + 25  ns       15.10,
delay)                                                                                                                 15.12
Bus buffer on time 1 (PLL on, 1/4 cycle delay)   tBON1       1/4 tcyc        1/4 tcyc + 18  ns
Bus tri-state delay time 1 (PLL off)                      tBOFF1      0                   30                   ns       15.11,
Bus buffer on time 1 (PLL off)                             tBON1025ns15.13

Bus tri-state delay time 2 (PLL on)                      tBOFF2      1/2 tcyc        1/2 tcyc + 25  ns       15.10,
Bus buffer on time 2 (PLL on)                             tBON2       1/2 tcyc        1/2 tcyc + 18  ns       15.12
Bus tri-state delay time 2 (PLL on, 1/4 cycle                 tBOFF2      3/4 tcyc        3/4 tcyc + 25  ns       15.10,
delay)
Bus buffer on time 2 (PLL on, 1/4 cycle delay)   tBON2       3/4 tcyc        3/4 tcyc + 18  ns       15.12
Bus tri-state delay time 3 (PLL off)                      tBOFF3      0                   30                   ns       15.11,
Bus buffer on time 3 (PLL off)                             tBON3025ns15.13


















                                                                                                                            403
```

## PDF page 420

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=420)

```text
                                       tRESf                                    tRESr

                            VIH                                          VIH
   RES                       VIL                               VIL

                    tNMIRS                        tRESW                                 tNMIRH

                   VIH                                                                                    VIH
   NMI             VIL                                                                                    VIL


                                       Figure 15.7   Reset Input Timing



         CKIO

                                         tRESH           tRESS
                                                     VIH
          RES                                        VIL

                                         tNMIH           tINMIS
                                                     VIH
           NMI                                       VIL

                                          tIRLH          tIRLS
                                                     VIH
   IRL3–IRL0                                         VIL


                    Figure 15.8   Interrupt Signal Input Timing (With PLL1 Off)






















404
```

## PDF page 421

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=421)

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


                   Figure 15.9   Interrupt Signal Input Timing (PLL1 On)


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

                Figure 15.10   Bus Release Timing (Master Mode, PLL1 On)














                                                                                                          405
```

## PDF page 422

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=422)

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

                     Figure 15.11   Bus Release Timing (Master Mode, PLL1 Off)



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

                       Figure 15.12   Bus Release Timing (Slave Mode, PLL1 On)





















406
```

## PDF page 423

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=423)

```text
       CKIO
       BREQ              tBRQD2                                              tBRQD1
      (output)                                                                                            tBAKS2
        BACK/CR           tBAKH2       tBAKS2                                             tBAKH2
       (input)

RD, RD/WR,                           tBON3                                                            tBOFF3
 RAS, CAS,
 CSn, WEn,
  BS, IVECF                                 tBON1                                                           tBOFF1

     A26–A0
     D31–D0

                  Figure 15.13   Bus Release Timing (Slave Mode, PLL1 Off)










































                                                                                                                   407
```

## PDF page 424

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=424)

```text
15.3.3      Bus Timing

Table 15.6    Bus Timing With PLL On [Mode 0, 4] (Conditions: VCC = 5.0 V –10%,
                   Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
Address delay time                 tAD             3                     18                   ns       15.14, 15.20, 15.40,
                                                                                                                 15.52, 15.66, 15.68
BS delay time                         tBSD           —                    21                   ns       15.14, 15.20, 15.40,
                                                                                                                 15.52, 15.66
CS delay time 1                      tCSD1         —                    21                   ns       15.14, 15.20, 15.40,
                                                                                                                 15.52, 15.66
CS delay time 2                         tCSD2          —                    1/2 tcyc + 21   ns       15.14, 15.66
Read/write delay time             tRWD          3                     18                   ns       15.14, 15.20, 15.40,
                                                                                                                 15.52, 15.66
Read strobe delay time 1        tRSD1         —                    1/2 tcyc + 16   ns       15.14, 15.40, 15.52,
                                                                                                                 15.66, 15.68
Read data setup time 1          tRDS1         1/2 tcyc + 10   —                    ns       15.14, 15.40, 15.52,
                                                                                                                 15.66, 15.68
Read data setup time 3                     tRDS3         1/2 tcyc + 8     —                    ns       15.20
(SDRAM)
Read data hold time 2            tRDH2         0                     —                    ns       15.14, 15.66
Read data hold time 4                      tRDH4         0                     —                    ns       15.20
(SDRAM)
Read data hold time 5                      tRDH5         0                     —                    ns       15.40
(DRAM)
Read data hold time 6                      tRDH6         0                     —                    ns       15.52
(PSRAM)
Read data hold time 7                      tRDH7         0                     —                    ns       15.68
(interrupt vector)
Write enable delay time          tWED1         1/2 tcyc + 3     1/2 tcyc + 18   ns       15.14, 15.15, 15.52,
                                                                                                                 15.53
Write data delay time 1           tWDD          3                     18                   ns       15.15, 15.27, 15.41,
                                                                                                                 15.53
Write data hold time 1             tWDH1         3                     —                    ns       15.15, 15.27, 15.41,
                                                                                                                 15.53
Data buffer on time                 tDON           —                    18                   ns       15.15, 15.27, 15.41,
                                                                                                                 15.53
Data buffer off time                 tDOF           —                    18                   ns       15.15, 15.27, 15.41,
                                                                                                                 15.53



408
```

## PDF page 425

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=425)

```text
Table 15.6    Bus Timing With PLL On [Mode 0, 4] (cont)
                    (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                        Symbol     Min                 Max                Unit    Figures
DACK delay time 1                 tDACD1       —                    18                   ns       15.14, 15.20, 15.40,
                                                                                                                   15.52, 15.66
DACK delay time 2                 tDACD2       —                    1/2 tcyc + 18   ns       15.14, 15.20, 15.40,
                                                                                                                   15.52, 15.66
WAIT setup time                     tWTS           20                   —                    ns       15.19, 15.43, 15.55,
                                                                                                                   15.66, 15.70
WAIT hold time                       tWTH          5                     —                    ns       15.19, 15.43, 15.55,
                                                                                                                   15.66, 15.70
RAS delay time 1 (SDRAM)   tRASD1       —                    18                   ns       15.20
RAS delay time 2 (DRAM)      tRASD2       1/2 tcyc + 3     1/2 tcyc + 18   ns       15.40
CAS delay time 1 (SDRAM)   tCASD1       —                    18                   ns       15.20
CAS delay time 2 (DRAM)      tCASD2       1/2 tcyc + 3     1/2 tcyc + 18   ns       15.40
DQM delay time                      tDQMD        —                    18                   ns       15.20
CKE delay time                       tCKED         —                    21                   ns       15.37
CE delay time 1                      tCED1         1/2 tcyc + 3     1/2 tcyc + 18   ns       15.52
OE delay time 1                      tOED1         —                    1/2 tcyc + 18   ns       15.52
IVECF delay time                   tIVD            —                    18                   ns       15.68
Address input setup time        tASIN          14                   —                    ns       15.71
Address input hold time          tAHIN          3                     —                    ns       15.71
BS input setup time                tBSS           15                   —                    ns       15.71
BS input hold time                  tBSH           3                     —                    ns       15.71
Read/write input setup time    tRWS          15                   —                    ns       15.71
Read/write input hold time      tRWH          3                     —                    ns       15.71
Address hold time 1                tAH1           5                     —                    ns       15.15

















                                                                                                                                               409
```

## PDF page 426

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=426)

```text
Table 15.7    Bus Timing With PLL On and 1/4 Cycle Delay [Mode 1, 5]
                   (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                       Symbol      Min                 Max                Unit   Figures
Address delay time                tAD              1/4 tcyc + 3     1/4 tcyc + 18   ns       15.14, 15.20, 15.40,
                                                                                                              15.52, 15.66, 15.68
BS delay time                        tBSD            —                    1/4 tcyc + 21   ns       15.14, 15.20, 15.40,
                                                                                                              15.52, 15.66
CS delay time 1                     tCSD1           —                    1/4 tcyc + 21   ns       15.14, 15.20, 15.40,
                                                                                                              15.52, 15.66
CS delay time 2                     tCSD2           —                    3/4 tcyc + 21   ns       15.14, 15.66
Read/write delay time            tRWD           1/4 tcyc + 3     1/4 tcyc + 18   ns       15.14, 15.20, 15.40,
                                                                                                              15.52, 15.66
Read strobe delay time 1      tRSD1           —                    3/4 tcyc + 16   ns       15.14, 15.40, 15.52,
                                                                                                              15.66, 15.68
Read data setup time 1         tRDS1           1/4 tcyc + 10   —                    ns       15.14, 15.40, 15.52,
                                                                                                              15.66, 15.68
Read data setup time 3                   tRDS3           1/4 tcyc + 8     —                    ns       15.20
(SDRAM)
Read data hold time 2           tRDH2          0                     —                    ns       15.14, 15.66
Read data hold time 4                    tRDH4          0                     —                    ns       15.20
(SDRAM)
Read data hold time 5                    tRDH5          0                     —                    ns       15.40
(DRAM)
Read data hold time 6                    tRDH6          0                     —                    ns       15.52
(PSRAM)
Read data hold time 7                    tRDH7          0                     —                    ns       15.68
(interrupt vector)
Write enable delay time         tWED1          3/4 tcyc + 3     3/4 tcyc + 18   ns       15.14, 15.15, 15.52,
                                                                                                              15.53
Write data delay time 1          tWDD           1/4 tcyc + 3     1/4 tcyc + 18   ns       15.15, 15.27, 15.41,
                                                                                                              15.53
Write data hold time 1           tWDH1          1/4 tcyc + 3     —                    ns       15.15, 15.27, 15.41,
                                                                                                              15.53
Data buffer on time                tDON            —                    1/4 tcyc + 18   ns       15.15, 15.27, 15.41,
                                                                                                              15.53
Data buffer off time                tDOF            —                    1/4 tcyc + 18   ns       15.15, 15.27, 15.41,
                                                                                                              15.53






410
```

## PDF page 427

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=427)

```text
Table 15.7    Bus Timing With PLL On and 1/4 Cycle Delay [Mode 1, 5] (cont)
                   (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                       Symbol      Min                 Max                Unit    Figures
DACK delay time 1                tDACD1         —                    1/4 tcyc + 18   ns       15.14, 15.20, 15.40,
                                                                                                                15.52, 15.66
DACK delay time 2                tDACD2         —                    3/4 tcyc + 18   ns       15.14, 15.20, 15.40,
                                                                                                                15.52, 15.66
WAIT setup time                    tWTS            20 – 1/4 tcyc   —                    ns       15.19, 15.43, 15.55,
                                                                                                                15.66, 15.70
WAIT hold time                      tWTH            1/4 tcyc + 5     —                    ns       15.19, 15.43, 15.55,
                                                                                                                15.66, 15.70
RAS delay time 1 (SDRAM)  tRASD1         —                    1/4 tcyc + 18   ns       15.20
RAS delay time 2 (DRAM)    tRASD2         3/4 tcyc + 3     3/4 tcyc + 18   ns       15.40
CAS delay time 1 (SDRAM)  tCASD1         —                    1/4 tcyc + 18   ns       15.20
CAS delay time 2 (DRAM)    tCASD2         3/4 tcyc + 3     3/4 tcyc + 18   ns       15.40
DQM delay time                     tDQMD         —                    1/4 tcyc + 18   ns       15.20
CKE delay time                      tCKED          —                    1/4 tcyc + 21   ns       15.37
CE delay time 1                     tCED1           3/4 tcyc + 3     3/4 tcyc + 18   ns       15.52
OE delay time 1                     tOED1          —                    3/4 tcyc + 18   ns       15.52
IVECF delay time                  tIVD             —                    1/4 tcyc + 18   ns       15.68
Address input setup time       tASIN           14 – 1/4 tcyc   —                    ns       15.71
Address input hold time         tAHIN           1/4 tcyc + 3     —                    ns       15.71
BS input setup time               tBSS            15 – 1/4 tcyc   —                    ns       15.71
BS input hold time                 tBSH            1/4 tcyc + 3     —                    ns       15.71
Read/write input setup time   tRWS           15 – 1/4 tcyc   —                    ns       15.71
Read/write input hold time     tRWH           1/4 tcyc + 3     —                    ns       15.71
Address hold time 1               tAH1             5                     —                    ns       15.15

















                                                                                                                                           411
```

## PDF page 428

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=428)

```text
Table 15.8    Bus Timing With PLL Off (CKIO Input) [Mode 6]
                    (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                       Symbol      Min                 Max                Unit    Figures
Address delay time                tAD              13                   28                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67, 15.69
BS delay time                        tBSD            —                    30                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67
CS delay time 1                     tCSD1           —                    30                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67
CS delay time 3                       tCSD3           —                    28                   ns       15.16, 15.67
Read write delay time            tRWD           13                   28                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67
Read strobe delay time 2      tRSD2           —                    26                   ns       15.16, 15.47, 15.60,
                                                                                                                   15.67, 15.69
Read data setup time 2         tRDS2           10                   —                    ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67, 15.69
Read data hold time 2           tRDH2          0                     —                    ns       15.16, 15.67
Read data hold time 3           tRDH3          15                   —                    ns       15.38
Read data hold time 5                      tRDH5          0                     —                    ns       15.47
(DRAM)
Read data hold time 6                      tRDH6          0                     —                    ns       15.60
(PSRAM)
Read data hold time 7                      tRDH7          0                     —                    ns       15.69
(interrupt vector)
Write enable delay time 2      tWED2          10                   25                   ns       15.17, 15.61
Write data delay time             tWDD           10                   25                   ns       15.17, 15.39, 15.48,
                                                                                                                   15.61
Write data hold time 1           tWDH1          3                                            ns       15.17, 15.39, 15.48,
                                                                                                                   15.61
Write data hold time 2           tWDH2          5                                            ns       15.17
Write data hold time 3           tWDH3          3                                            ns       15.61
DACK delay time 1                tDACD1         —                    25                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67
DACK delay time 3                tDACD3         —                    25                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67







412
```

## PDF page 429

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=429)

```text
Table 15.8     Bus Timing With PLL Off (CKIO Input) [Mode 6] (cont)
                      (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                       Symbol      Min                 Max                Unit    Figures
WAIT setup time                    tWTS            20                   —                    ns       15.19, 15.43, 15.55,
                                                                                                                     15.67, 15.70
WAIT hold time                      tWTH            15                   —                    ns       15.19, 15.43, 15.55,
                                                                                                                     15.67, 15.70
RAS delay time 1 (SDRAM)  tRASD1         —                    25                   ns       15.38
RAS delay time 3 (DRAM)    tRASD3         10                   25                   ns       15.47
CAS delay time 1 (SDRAM)  tCASD1         —                    25                   ns       15.38
CAS delay time 3 (DRAM)    tCASD3         10                   25                   ns       15.47
DQM delay time                     tDQMD         —                    25                   ns       15.38
CKE delay time                      tCKED          —                    25                   ns       15.37
CE delay time 2                     tCED2           10                   25                   ns       15.60
OE delay time 2                     tOED2          —                    25                   ns       15.60
IVECF delay time                  tIVD             —                    25                   ns       15.69
WE setup time                       tWES1          0                     —                    ns       15.16
Address setup time 1             tAS1             0                     —                    ns       15.17
Address setup time 2             tAS2             3                     —                    ns       15.60
Address hold time 2               tAH2             0                     —                    ns       15.17
Row address setup time        tASR            3                     —                    ns       15.47
Column address setup time   tASC            3                     —                    ns       15.47
Write command setup time    tWCS           3                     —                    ns       15.48
Write data setup time            tWDS           3                     —                    ns       15.48
Address input setup time*      tASIN           15                   —                    ns       15.71
Address input hold time*        tAHIN           10                   —                    ns       15.71
BS input setup time*              tBSS            15                   —                    ns       15.71
BS input hold time*                tBSH            10                   —                    ns       15.71
Read/write input setup time*  tRWS           15                   —                    ns       15.71
Read/write input hold time*    tRWH           10                   —                    ns       15.71
Data buffer on time                tDON            —                    25                   ns       15.17, 15.39, 15.48,
                                                                                                                     15.61
Data buffer off time                tDOF            —                    25                   ns       15.17, 15.39, 15.48,
                                                                                                                     15.61
Note:   When the external addresses monitor function is used, the PLL must be on.



                                                                                                                                                  413
```

## PDF page 430

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=430)

```text
Table 15.9    Bus Timing With PLL Off (CKIO Output) [Mode 2]
                    (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                       Symbol      Min                 Max                Unit    Figures
Address delay time                tAD              3                     18                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67, 15.69
BS delay time                        tBSD            —                    21                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67
CS delay time 1                     tCSD1           —                    21                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67
CS delay time 3                       tCSD3           —                    21                   ns       15.16, 15.67
Read write delay time            tRWD           3                     18                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67
Read strobe delay time 2      tRSD2           —                    16                   ns       15.16, 15.47, 15.60,
                                                                                                                   15.67, 15.69
Read data setup time 2         tRDS2           12                   —                    ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67, 15.69
Read data hold time 2           tRDH2          0                     —                    ns       15.16, 15.67
Read data hold time 3                      tRDH3          1/2 tcyc           —                    ns       15.38
(SDRAM)
Read data hold time 5                      tRDH5          0                     —                    ns       15.47
(DRAM)
Read data hold time 6                      tRDH6          0                     —                    ns       15.60
(PSRAM)
Read data hold time 7                      tRDH7          0                     —                    ns       15.69
(interrupt vector)
Write enable delay time 2      tWED2          3                     18                   ns       15.17, 15.61
Write data delay time             tWDD           3                     18                   ns       15.17, 15.39, 15.48,
                                                                                                                   15.61
Write data hold time 1           tWDH1          3                     —                    ns       15.17, 15.39, 15.48,
                                                                                                                   15.61
Write data hold time 2           tWDH2          5                     —                    ns       15.17
Write data hold time 3           tWDH3          3                     —                    ns       15.61
DACK delay time 1                tDACD1         —                    18                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67
DACK delay time 3                tDACD3         —                    18                   ns       15.16, 15.38, 15.47,
                                                                                                                   15.60, 15.67






414
```

## PDF page 431

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=431)

```text
Table 15.9     Bus Timing With PLL Off (CKIO Output) [Mode 2] (cont)
                      (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                       Symbol      Min                 Max                Unit    Figures
WAIT setup time                    tWTS            22                   —                    ns       15.19, 15.43, 15.55,
                                                                                                                    15.67, 15.70
WAIT hold time                      tWTH            5                     —                    ns       15.19, 15.43, 15.55,
                                                                                                                    15.67, 15.70
RAS delay time 1 (SDRAM)  tRASD1         —                    18                   ns       15.38
RAS delay time 3 (DRAM)    tRASD3         3                     18                   ns       15.47
CAS delay time 1 (SDRAM)  tCASD1         —                    18                   ns       15.38
CAS delay time 3 (DRAM)    tCASD3         3                     18                   ns       15.47
DQM delay time                     tDQMD         —                    18                   ns       15.38
CKE delay time                      tCKED          —                    21                   ns       15.37
CE delay time 2                     tCED2           3                     18                   ns       15.60
OE delay time 2                     tOED2          —                    18                   ns       15.60
IVECF delay time                  tIVD             —                    18                   ns       15.69
Address input setup time*     tASIN           14                   —                    ns       15.71
Address input hold time*       tAHIN           3                     —                    ns       15.71
BS input setup time*              tBSS            15                   —                    ns       15.71
BS input hold time*                tBSH            3                     —                    ns       15.71
Read/write input setup time* tRWS           15                   —                    ns       15.71
Read/write input hold time*   tRWH           3                     —                    ns       15.71
Data buffer on time                tDON            —                    18                   ns       15.17, 15.39, 15.48,
                                                                                                                    15.61
Data buffer off time                tDOF            —                    18                   ns       15.17, 15.39, 15.48,
                                                                                                                    15.61
Address hold time 2               tAH2             5                     —                    ns       15.17
Note:   When the external addresses monitor function is used, the PLL must be on.















                                                                                                                                                 415
```

## PDF page 432

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=432)

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

                           Figure 15.14   Basic Read Cycle (No Waits, PLL On)







416
```

## PDF page 433

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=433)

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
                          Figure 15.15   Basic Write Cycle (No Waits, PLL On)





                                                                                                                         417
```

## PDF page 434

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=434)

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

                          Figure 15.16   Basic Read Cycle (No Waits, PLL Off)






418
```

## PDF page 435

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=435)

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

                             Figure 15.17   Basic Write Cycle (No Waits, PLL Off)







                                                                                                                                      419
```

## PDF page 436

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=436)

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
                                                                tWTS   tWTH

            WAIT


             RAS,
                CE


             CAS,
               OE


              CKE
   Notes:   1.    The dotted line shows the waveform when synchronous DRAM is connected.
            2.    The DACKn waveform shown is for the case where active-high has been specified.

                            Figure 15.18   Basic Bus Cycle (1 Wait Cycle)





420
```

## PDF page 437

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=437)

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

                         Figure 15.19   Basic Bus Cycle (External Wait Input)






                                                                                                                    421
```

## PDF page 438

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=438)

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

                                 Figure 15.20   Synchronous DRAM Read Bus Cycle
                        (RCD = 1 Cycle, CAS Latency = 1 Cycle, Bursts = 4, PLL On)



422
```

## PDF page 439

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=439)

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
                              tRASD1       tRASD1                                                           tRASD1

               RAS,
                  CE                       tCASD1       tCASD1                                              tCASD1       tCASD1

               CAS,
                  OE


                 CKE
Note:   The dotted line shows the waveform when synchronous DRAM in another CS space is
          accessed.

                          Figure 15.21   Synchronous DRAM Single Read Bus Cycle
                       (RCD = 1 Cycle, CAS Latency = 1 Cycle, Bursts = 4, PLL On)





                                                                                                                                         423
```

## PDF page 440

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=440)

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

                         Figure 15.22   Synchronous DRAM Read Bus Cycle
                       (RCD = 2 Cycles, CAS Latency = 2 Cycles, Bursts = 4)




424
```

## PDF page 441

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=441)

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

                      Figure 15.23   Synchronous DRAM Read Bus Cycle
                  (Bank Active, Same Row Access, CAS Latency = 1 Cycle)










                                                                                                                   425
```

## PDF page 442

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=442)

```text
                       TC             TW            Td1          Td2           Td3          Td4

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
                    tDQMD
      WEn/CR
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

                         Figure 15.24   Synchronous DRAM Read Bus Cycle
                     (Bank Active, Same Row Access, CAS Latency = 2 Cycles)






426
```

## PDF page 443

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=443)

```text
                     Tp             Tr             Tc           Td1          Td2         Td3           Td4

    CKIO
                   tAD
    Upper
 address                                  tAD

    Lower
 address
                      tBSD
       BS

                      tCSD1
      CSn
                  tRWD         tRWD

  RD/WR/CR
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
     RAS
       CE

     CAS
       OE

     CKE
 Note:    The DACKn waveform shown is for the case where active-high has been specified.

Figure 15.25   Synchronous DRAM Read Bus Cycle (Bank Active, Different Row Access,
                    TRP = 1 Cycle, RCD = 1 Cycle, CAS Latency = 1 Cycle)






                                                                                                            427
```

## PDF page 444

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=444)

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

        WEn
     CASxx
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

  Figure 15.26   Synchronous DRAM Read Bus Cycle (Bank Active, Different Row Access,
                      TRP = 2 Cycles, RCD = 1 Cycle, CAS Latency = 1 Cycle)






428
```

## PDF page 445

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=445)

```text
                                         Tr           Tc          Tap
                          CKIO
                                        tAD                  tAD
                          Upper
                        address                    tAD

                          Lower
                        address        tBSD       tBSD       tBSD

                             BS
                                       tCSD1                tCSD1      tCSD1

                            CSn
                                       tRWD       tRWD      tRWD
                        RD/WR/CR
                            WE                               tRSD1

                             RD
                                      tDQMD                 tDQMD
                           WEn
                         CASxx
                        DQMxx              tWDD  tDON             tDOF
                                                               tWDH1
                       D31–D0
                                      tDACD1     tDACD2               tDACD1

                        DACKn


                          WAIT
                                      tRASD1      tRASD1    tRASD1    tRASD1
                           RAS
                             CE                   tCASD1    tCASD1    tCASD1

                           CAS
                             OE

                           CKE
Notes:   1.   Dotted lines show the waveforms when synchronous DRAM in another CS space
              is accessed.
         2.   The DACKn waveform shown is for the case where active-high has been specified.

                       Figure 15.27   Synchronous DRAM Write Bus Cycle
                            (RCD = 1 Cycle, TRWL = 1 Cycle, PLL On)






                                                                                                         429
```

## PDF page 446

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=446)

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


            WEn
         CASxx
         DQMxx

        D31–D0                                                                               tDACD1


         DACKn


           WAIT

                               tRASD1                                               tRASD1
            RAS/CR                                   tCASD1
              CE

            CAS/CR
             OE


            CKE
Notes:   1.    Dotted lines show the waveforms when synchronous DRAM in another CS space
               is accessed.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

                          Figure 15.28   Synchronous DRAM Write Bus Cycle
                                   (RCD = 2 Cycles, TRWL = 2 Cycles)


430
```

## PDF page 447

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=447)

```text
                                                    Tc
                                 CKIO
                                                tAD                   tAD

                                Upper
                              address

                                Lower
                              address          tBSD           tBSD


                                    BS
                                                tCSD1            tCSD1

                                  CSn
                                               tRWD              tRWD

                               RD/WR
                                   WE

                                   RD
                                               tDQMD           tDQMD
                                  WEn/CR
                               CASxx/CR
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

Figure 15.29   Synchronous DRAM Write Bus Cycle (Bank Active, Same Row Access)






                                                                                                                  431
```

## PDF page 448

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=448)

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


                             RD/WR
                                 WE


                                 RD
                                                          tDQMD
                                WEn
                              CASxx
                             DQMxx                 tWDD
                                              tWDH1
                            D31–D0

                                              tDACD2              tDACD1


                             DACKn



                               WAIT


                                RAS
                                 CE
                                                    tCASD1                              tCASD1
                                CAS
                                 OE


                                CKE
    Note:    The DACKn waveform shown is for the case where active-high has been specified.

                   Figure 15.30   Synchronous DRAM Consecutive Write Cycles
                                     (Bank Active, Same Row Access)





432
```

## PDF page 449

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=449)

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


                     WEn
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

                      Figure 15.31   Synchronous DRAM Write Bus Cycle
           (Bank Active, Different Row Access, TRP = 1 Cycle, RCD = 1 Cycle)





                                                                                                                  433
```

## PDF page 450

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=450)

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

                                       tRASD1      tRASD1

              RAS/CR
               CE

              CAS/CR
               OE


              CKE
     Note:    The DACKn waveform shown is for the case where active-high has been specified.

                         Figure 15.32   Synchronous DRAM Write Bus Cycle
              (Bank Active, Different Row Access, TRP = 2 Cycles, RCD = 2 Cycles)





434
```

## PDF page 451

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=451)

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

Figure 15.33   Synchronous DRAM Mode Register Write Cycle (TRP = 1 Cycle)








                                                                                                                 435
```

## PDF page 452

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=452)

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

       Figure 15.34   Synchronous DRAM Mode Register Write Cycle (TRP = 2 Cycles)








436
```

## PDF page 453

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=453)

```text
                                 Trr                 Trc1                                    Tre                TnopTrc2

             CKIO


            Upper
          address            tAD           tAD


            Lower
          address
                                                                                       tBSD

                BS
                                            tCSD1                                                      tCSD1

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

            Figure 15.35   Synchronous DRAM Auto-Refresh Cycle (TRAS = 2 Cycles)





                                                                                                                       437
```

## PDF page 454

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=454)

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

                        Figure 15.36   Synchronous DRAM Auto-Refresh Cycle
                 (Shown From Precharge Cycle, TRP = 1 Cycle, TRAS = 2 Cycles)







438
```

## PDF page 455

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=455)

```text
                    Trr    Trc1        Trc2     Tre                                        Trc1Tre     Tnap

       CKIO


      Upper
    address         tAD      tAD
      Lower
    address

          BS
                            tCSD1                                                                      tCSD1
        CSn
                   tRWD
     RD/WR
         WE

          RD

        WEn
     CASxx
     DQMxx

    D31–D0

     DACKn


       WAIT
              tRASD1          tRASD1                                                         tRASD1
        RAS
          CE  tCASD1
                              tCASD1
        CAS
          OE                                                         tCKED
                     tCKED
        CKE

Note:   A precharge cycle always precedes the self-refresh cycle by the number of cycles specified
        by TRP.

                 Figure 15.37   Synchronous DRAM Self-Refresh Cycle (TRAS = 2)












                                                                                                                     439
```

## PDF page 456

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=456)

```text
                     Tr            Tc          Td1         Td2          Td3         Td4

   CKIO                                                                                tAD
                   tAD
   Upper
address                                             tAD        tAD         tAD
                                tAD
   Lower
address
                     tBSD                   tBSD                                tBSD
      BS

                     tCSD1                                                     tCSD1
     CSn
                     tRWD                                                                           tRWD
 RD/WR/CR
      WE                                                                                    tRSD2

      RD
                 tDQMD                                                                 tDQMD
    WEn                                    tRDH3       tRDH3       tRDH3       tRDH3
  CASxx                                            tRDS2       tRDS2       tRDS2
 DQMxx                           tRDS2

D31–D0
                                           tDACD3     tDACD3      tDACD3      tDACD3
                 tDACD1                          tDACD1     tDACD1      tDACD1

 DACKn


   WAIT
                 tRASD1      tRASD1                                                    tRASD1

    RAS/CR
      CE                     tCASD1      tCASD1                                        tCASD1      tCASD1

    CAS/CR
      OE

    CKE

Notes:   1.    The dotted line shows the waveform when synchronous DRAM in another CS space
               is accessed.
         2.    The DACKn waveform shown is for the case where active-high has been specified.

                         Figure 15.38   Synchronous DRAM Read Bus Cycle
         (RCD = 1 Cycle, CAS Latency = 1 Cycle, TRP = 1 Cycle, Bursts = 4, PLL Off)



440
```

## PDF page 457

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=457)

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

                           WEn
                         CASxx
                        DQMxx                      tWDD         tDOF
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
Notes:   1.   Dotted lines show the waveforms when synchronous DRAM in another CS space
              is accessed.
         2.   The DACKn waveform shown is for the case where active-high has been specified.

                        Figure 15.39   Synchronous DRAM Write Bus Cycle
                             (RCD = 1 Cycle, TRWL = 1 Cycle, PLL Off)




                                                                                                             441
```

## PDF page 458

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=458)

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

    Figure 15.40   DRAM Read Cycle (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL On)







442
```

## PDF page 459

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=459)

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

                                 Figure 15.41   DRAM Write Cycle
                     (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL On)




                                                                                                                 443
```

## PDF page 460

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=460)

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

           Figure 15.42   DRAM Bus Cycle (TRP = 2 Cycles, RCD = 2 Cycles, 1 Wait)







444
```

## PDF page 461

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=461)

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

Figure 15.43   DRAM Bus Cycle (TRP = 1 Cycle, RCD = 1 Cycle, External Wait Input)





                                                                                                           445
```

## PDF page 462

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=462)

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

                                    Figure 15.44   DRAM Burst Read Cycle
                           (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL On)






446
```

## PDF page 463

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=463)

```text
                          Tp               Tr              Tc1            Tc2            Tc1           Tc2

          CKIO


          Upper
        address                                                        tAD

          Lower
        address
                                                           tBSD        tBSD

             BS


            CSn


       RD/WR,
             WE


             RD                                            tCASD2      tCASD2

          WEn,
        CASxx,                                                         tWDD
        DQMxx
                                                                       tWDH1

       D31–D0
                                                           tDACD2      tDACD1

        DACKn


          WAIT


           RAS,/CR
             CE

           CAS/CR
             OE


           CKE
Note:    The DACKn waveform shown is for the case where active-high has been specified.

                             Figure 15.45   DRAM Burst Write Cycle
                     (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL On)





                                                                                                                447
```

## PDF page 464

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=464)

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

                          Figure 15.46   DRAM CAS-Before-RAS Refresh Cycle
                                 (TRP = 1 Cycle, TRAS = 2 Cycles, PLL On)











448
```

## PDF page 465

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=465)

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

   Figure 15.47   DRAM Read Cycle (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL Off)






                                                                                                                    449
```

## PDF page 466

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=466)

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

   Figure 15.48   DRAM Write Cycle (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL Off)







450
```

## PDF page 467

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=467)

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

                                   Figure 15.49   DRAM Burst Read Cycle
                           (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL Off)







                                                                                                                        451
```

## PDF page 468

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=468)

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

                                   Figure 15.50   DRAM Burst Write Cycle
                           (TRP = 1 Cycle, RCD = 1 Cycle, No Waits, PLL Off)








452
```

## PDF page 469

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=469)

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

              Figure 15.51   DRAM CAS-Before-RAS Refresh Cycle
                    (TRP = 1 Cycle, TRAS = 2 Cycles, PLL Off)






                                                                                                       453
```

## PDF page 470

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=470)

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

                                    Figure 15.52   Pseudo-SRAM Read Cycle
                            (PLL On, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








454
```

## PDF page 471

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=471)

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

                             Figure 15.53   Pseudo-SRAM Write Cycle
                      (PLL On, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)




                                                                                                                   455
```

## PDF page 472

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=472)

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

      Figure 15.54   Pseudo-SRAM Bus Cycle (TRP = 2 Cycles, RCD = 2 Cycles, 1 Wait)










456
```

## PDF page 473

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=473)

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

                           Figure 15.55   Pseudo-SRAM Bus Cycle
                   (TRP = 1 Cycle, RCD = 1 Cycle, External Wait Input)







                                                                                                          457
```

## PDF page 474

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=474)

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

                                 Figure 15.56   Pseudo-SRAM Read Cycle
            (Static Column Mode, PLL On, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)




458
```

## PDF page 475

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=475)

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

                            Figure 15.57   Pseudo-SRAM Write Cycle
       (Static Column Mode, PLL On, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








                                                                                                                 459
```

## PDF page 476

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=476)

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

                              Figure 15.58   Pseudo-SRAM Auto-Refresh Cycle
                                  (PLL On, TRP = 1 Cycle, TRAS = 2 Cycles)











460
```

## PDF page 477

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=477)

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

                   Figure 15.59   Pseudo-SRAM Self-Refresh Cycle
                      (PLL On, TRP = 1 Cycle, TRAS = 2 Cycles)





                                                                                                           461
```

## PDF page 478

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=478)

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

                                   Figure 15.60   Pseudo-SRAM Read Cycle
                           (PLL Off, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








462
```

## PDF page 479

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=479)

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

                               Figure 15.61   Pseudo-SRAM Write Cycle
                       (PLL Off, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








                                                                                                                             463
```

## PDF page 480

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=480)

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

                                  Figure 15.62   Pseudo-SRAM Read Cycle
            (Static Column Mode, PLL Off, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)








464
```

## PDF page 481

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=481)

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

                            Figure 15.63   Pseudo-SRAM Write Cycle
       (Static Column Mode, PLL Off, TRP = 1 Cycle, RCD = 1 Cycle, No Waits)





                                                                                                              465
```

## PDF page 482

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=482)

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

                            Figure 15.64   Pseudo-SRAM Auto-Refresh Cycle
                                (PLL Off, TRP = 1 Cycle, TRAS = 2 Cycles)






466
```

## PDF page 483

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=483)

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

                  Figure 15.65   Pseudo-SRAM Self-Refresh Cycle
                     (PLL Off, TRP = 1 Cycle, TRAS = 2 Cycles)





                                                                                                          467
```

## PDF page 484

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=484)

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

                            Figure 15.66   Burst ROM Read Cycle (PLL On, 1 Wait)









468
```

## PDF page 485

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=485)

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

                    Figure 15.67   Burst ROM Read Cycle (PLL Off, 1 Wait)








                                                                                                                             469
```

## PDF page 486

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=486)

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

                     Figure 15.68   Interrupt Vector Fetch Cycle (PLL On, No Waits)



















470
```

## PDF page 487

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=487)

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

          Figure 15.69   Interrupt Vector Fetch Cycle (PLL Off, No Waits)



















                                                                                                                           471
```

## PDF page 488

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=488)

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


                 Figure 15.70   Interrupt Vector Fetch Cycle (1 External Wait Cycle)
























472
```

## PDF page 489

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=489)

```text
  CKIO


                                        tASIN       tAHIN

A26–A2


                                         tBSS        tBSH

     BS

                                        tRWS       tRWH


RD/WR

              Figure 15.71   Address Monitor Cycle






































                                                                                             473
```

## PDF page 490

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=490)

```text
15.3.4      DMAC Timing

Table 15.10    DMAC Timing (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                                                                 Symbol   Min                 Max   Unit   Figure
DREQ0, DREQ1 setup time (PLL Off, On)                      tDRQS       30                   —      ns      15.72
DREQ0, DREQ1 setup time (PLL On, 1/4 cycle delay)   tDRQS       30 – 1/4 tcyc   —      ns
DREQ0, DREQ1 hold time (PLL Off, On)                        tDRQH       15                   —      ns
DREQ0, DREQ1 hold time (PLL On, 1/4 cycle delay)     tDRQH       1/4 tcyc + 15   —      ns
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

                                 Figure 15.72   DREQ0, DREQ1 Input Timing


















474
```

## PDF page 491

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=491)

```text
15.3.5      Free-Running Timer Timing

Table 15.11    Free-Running Timer Timing (Conditions: VCC = 5.0 V –10%, Ta = –20 to
                    +75°C)

Item                                                             Symbol     Min                  Max                  Unit     Figure
Output compare output delay time                          tTOCD         —                    160                   ns        15.73
(PLL Off, On)
Output compare output delay time                          tTOCD         —                    1/4 tcyc + 160  ns
(PLL On, 1/4 cycle delay)
Input capture input setup time                            tTICS          80                    —                     ns
(PLL Off, On)
Input capture input setup time                            tTICS          80 – 1/4 tcyc    —                     ns
(PLL On, 1/4 cycle delay)
Timer clock input setup time                              tTCKS         80                    —                     ns        15.74
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

                                     Figure 15.73   FRT Input/Output Timing



   CKIO
                                                                     tTCKS

    FTCI

                                     tTCKWL                                                tTCKWH

                                       Figure 15.74   FRT Clock Input Timing

                                                                                                                                  475
```

## PDF page 492

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=492)

```text
15.3.6      Watchdog Timer Timing

Table 15.12    Watchdog Timer Timing (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                                               Symbol       Min          Max                  Unit       Figure
WDTOVF delay time (PLL Off, On)               tWOVD          —            70                     ns          15.75
WDTOVF delay time (PLL On, 1/4 cycle                     tWOVD          —            1/4 tcyc + 70    ns
delay)




         CKIO                                       tWOVD                       tWOVD



   WDTOVF


                               Figure 15.75   Watchdog Timer Output Timing






































476
```

## PDF page 493

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=493)

```text
15.3.7      Serial Communication Interface Timing

Table 15.13    Serial Communication Interface Timing
                    (Conditions: VCC = 5.0 V –10%, Ta = –20 to +75°C)

Item                                                                         Symbol     Min           Max          Unit     Figure
Input clock cycle                                                      tscyc           16             —             tcyc       15.76
Input clock cycle (clocked synchronous mode)       tscyc           24             —             tcyc
Input clock pulse width                                             tsckw           0.4            0.6            tscyc
Transmit data delay time (clocked synchronous                        tTXD           —             70             ns         15.77
mode)
Receive data setup time (clocked synchronous                         tRXS           70             —             ns
mode)
Receive data hold time (clocked synchronous                          tRXH           70             —             ns
mode)



                                                      tSCKW


                                SCK0

                                                                              tscyc

                                Figure 15.76   Input Clock Input/Output Timing


                                                     tscyc

                SCK0

                                            tTXD

                TxD0
    (transmit data)

                                                                          tRXS     tRXH

                RxD0
     (receive data)


                Figure 15.77   SCI Input/Output Timing (Clocked Synchronous Mode)





                                                                                                                                   477
```

## PDF page 494

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=494)

```text
15.3.8      AC Characteristics Measurement Conditions

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
         2.    IOL and  IOH values are as shown in section 15.2, DC Characteristics, and table
               15.3, Permitted Output Current Values.

                                    Figure 15.78   Output Load Circuit


















478
```
