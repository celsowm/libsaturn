# Appendix A Pin States

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=579)

Physical PDF pages 579–580. Text extraction; consult the PDF for figures.

## PDF page 579

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=579)

```text
                                Appendix A   Pin States


Table A.1     Pin States During Resets, Power-Down State, and Bus-Released State

                                                                Pin States

                                       Reset                Reset             Power-Down
                                     Power-On              Manual                Modes           Bus-

                                                     Bus        Bus                            Released
Category             Pin                         Master    SlaveAcquiredReleased     Standby   SleepMode

Clock                   CKIO                      IO*1         IO*1         IO*1             IO*1              IO*1           IO*1       IO*1

                EXTAL                   I*1            I*1            I*1                I*1                I*1              I*1          I*1

                XTAL                      O*1          O*1          O*1              O*1               O*1            O*1        O*1

                CKPREQ ZZIIIII
                CKPACK HHHHH*2 HH

System control     RESET IIIIIII

                WDTOVF HHHHOOO

                BACK, BRLS ZZIIZII

                BREQ, BGR HHOOHOO

                MD5–MD0             I              I               I                  I                    I                 I            I

Interrupt               NMI                        I              I               I                  I                    I                 I            I

                IRL3–IRL0 ZZZZIII
                IVECF HHHHH*3 HH

Address bus        A26–A0                  O            Z              O                Z                  Z               O           Z*4

Data bus              D31–D0                 Z             Z              IO               Z                  Z               Z           Z
Bus control          CS3–CS0 HZOZHHZ*4

                BS  HZOZHHZ
                RD/WR HZOZHHZ*4

                RAS, CE HZOZHHZ

                CAS, OE HZOZHHZ

                CASHH, DQMUU  H             Z              O                Z                  H               H           Z

                CASHL, DQMUL   H             Z              O                Z                  H               H           Z

                CASLH, DQMLU   H             Z              O                Z                  H               H           Z

                CASLL, DQMLL     H             Z              O                Z                  H               H           Z

                RD  HZOZHHZ

                CKE                       H             H             O                H                  O               O           H

                WAIT                      Z             Z              I                  Z                  Z               I            Ignored

                                                                                                     563
```

## PDF page 580

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=580)

```text
Table A.1     Pin States During Resets, Power-Down State, and Bus-Released State (cont)

                                                                                     Pin States

                                                    Reset                       Reset                   Power-Down
                                                 Power-On                      Manual                        Modes           Bus-

                                                                      Bus            Bus                                      Released
Category             Pin                         Master    Slave      Acquired       Released     Standby   Sleep             Mode

Direct memory     DACK0, DACK1     H             H             H                H                  K*3 OO
access                DREQ0, DREQ1    Z             Z              Z                 Z                  Z               I            I
controller
(DMAC)

16-bit free-           FTOA                     L             L              L                 L                   K*3 OO
running timer       FTOB                     L             L              L                 L                   K*3 OO
(FRT)                   FTI                         Z             Z              Z                 Z                  K*3 II


                      FTCI                       Z             Z              Z                 Z                  K*3 II

Serial                   RXD                       Z             Z              Z                 Z                  K*3 II
communication    TXD                       H             H             H                H                  K*3 OO
interface (SCI)     SCK                       Z             Z              Z                 Z                  K*3             IO          I

I: Input
O: Output
H: High-level output
L: Low-level output
Z: High impedance
K: Input pins are high impedance, output pins retain their state
Notes:  1.   Depends on the clock mode (MD2–MD0 setting).
           2.   Low-level output in standby mode when the clock is paused.
           3.   When the high impedance bit (HIZ) in the standby control register (SBYCR) is set to 1,
                output pins become high impedance.
           4.   Input when the external bus cycle address monitor function is used.
Other:  In sleep mode, if the DMAC is running, the address/data bus and bus control signals
          change according to the DMAC operation (the same applies during refreshing).
















564
```
