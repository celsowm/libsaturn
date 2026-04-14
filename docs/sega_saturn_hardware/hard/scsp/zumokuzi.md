# SCSP User's Manual/Table of Figures

[Japanese](javascript:window.location.href = window.location.pathname.replace('.htm', '_j.htm');)

---

[![]()](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.htm) ★ [HARDWARE Manual](../index.md) ★ [SCSP User's Manual](index.md)

---

**SCSP User's Manual**

## Table of contents

---

## table

---

### Chapter 1 Sound system configuration : [Table 1.1 Sound memory mapping (overview)](hon/p01_10.md): [Table 1.2 Initialization setting data after reset](hon/p01_20.md): [Table 1.3 Reset vector](hon/p01_20.md): [Table 1.4 Interrupt vector table for sound CPU](hon/p01_20.md): [Table 1.5 Register setting table](hon/p01_30.md) ### Chapter 4 SCSP register : [Table 4.1 Address map by slot](hon/p04_11.md): [Table 4.2 Control registers by slot](hon/p04_11.md): [Table 4.3 SCSP common control register](hon/p04_12.md): [Table 4.4 Sound data stack](hon/p04_13.md): [Table 4.5 DSP Control Register](hon/p04_14.md): [Table 4.6 DSP microprogram](hon/p04_14.md): [Table 4.7 DSP buffer map](hon/p04_14.md): [Table 4.16 Relationship between MDXSL/MDYSL and slots](hon/p04_fm3.md): [Table 4.10 Maximum address displacement by register setting value](hon/p04_fm3.md): [Table 4.18 TL, attenuation and waveform amplitude](hon/p04_24.md): [Table 4.12 Actual frequency versus number of cents](hon/p04_25.md): [Table 4.13 FNS.OCT parameter table](hon/p04_25.md): [Table 4.14 Oscillator oscillation frequency](hon/p04_26.md): [Table 4.15 AM modulation waveform by LFO](hon/p04_26.md): [Table 4.16 PM modulation waveform by LFO](hon/p04_26.md): [Table 4.17 Degree of amplitude modulation and frequency modulation](hon/p04_26.md): [Table 4.18 Relationship between the number of sources that can be input to IMXL and MIXS](hon/p04_27.md): [Table 4.19 Mix stack register input level](hon/p04_27.md): [Table 4.20 D/A converter output level](hon/p04_27.md): [Table 4.21 Localization data by DIPAN](hon/p04_27.md): [Table 4.22 Send level to D/A converter](hon/p04_27.md): [Table 4.23 Localization data by EFPAN](hon/p04_27.md): [Table 4.24 Register address of EFSDL and EFPAN corresponding to each EFREG and EXTS](hon/p04_27.md): [Table 4.25 Memory capacity](hon/p04_29.md): [Table 4.26 Increment period of timer A](hon/p04_2b.md): [Table 4.27 Increment period of timer B](hon/p04_2b.md): [Table 4.28 Increment period of timer C](hon/p04_2b.md): [Table 4.29 Count period for TACTL, TBCTL, TCCTL setting values](hon/p04_2b.md): [Table 4.30 Shortest interrupt time and longest interrupt time](hon/p04_2b.md): [Table 4.31 Interrupt register bit factors](hon/p04_2c.md): [Table 4.32 DMA transfer direction](hon/p04_2d.md): [Table 4.33 DMA transfer](hon/p04_2d.md): [Table 4.34 RBL and ring buffer length](hon/p04_30.md)

---

## figure

---

### Chapter 1 Sound system configuration : [Figure 1.1 Sound block](hon/p01_10.md) ### Chapter 2 SCSP Overview : [Figure 2.3 Reset sequence (operation order diagram)](hon/p01_20.md): [Figure 2.4 Interrupt relationship](hon/p01_20.md): [Figure 3.1 CD-DA pathway](hon/p01_30.md): [Figure 2.1 SCSP chip block diagram](hon/p02_10.md) ### Chapter 3 SCSP function : [Figure 3.1 Access overview](hon/p03_10.md): [Figure 3.2 Memory access priority](hon/p03_10.md) ### Chapter 4 SCSP register : [Figure 4.1 SCSP memory map (1906Word)](hon/p04_10.md): [Figure 4.2 KEY\_ON and KEY\_OFF functions](hon/p04_21.md): [Figure 4.3 Block diagram related to noise generation and relationship between LFO](hon/p04_21.md): [Figure 4.4 Types of loops](hon/p04_21.md): [Figure 4.5 Loop waveform](hon/p04_21.md): [Figure 4.6 KEY\_OFF during attack state transition](hon/p04_22.md): [Figure 4.7 KEY-OFF during decay state transition](hon/p04_22.md): [Figure 4.8 Change in attenuation](hon/p04_22.md): [Figure 4.9 Transition from attack state to decay 1 (1)](hon/p04_22.md): [Figure 4.10 Transition from attack state to decay 1 (2)](hon/p04_22.md): [Figure 4.11 Transition from attack state to decay 1 (3)](hon/p04_22.md): [Figure 4.12 Slot block diagram](hon/p04_fm1.md): [Figure 4.13 Waveform address generation calculation section](hon/p04_fm1.md): [Figure 4.14 Waveform address generation/waveform data reading](hon/p04_fm1.md): [Figure 4.15 Enlarged view of address pointer output](hon/p04_fm1.md): [Figure 4.16 Frequency address pointer output value](hon/p04_fm1.md): [Figure 4.17 Address pointer output value when executing FM voice synthesis (1)](hon/p04_fm1.md): [Figure 4.18 Address pointer output value when executing FM voice synthesis (2)](hon/p04_fm1.md): [Figure 4.19 Normal loop](hon/p04_fm1.md): [Figure 4.20 Reverse loop](hon/p04_fm1.md): [Figure 4.21 Alternative loop](hon/p04_fm1.md): [Figure 4.22 FM sound source configuration diagram](hon/p04_fm1.md): [Figure 4.23 Averaging operation formula](hon/p04_fm1.md): [Figure 4.24 Slot calculation and sound stack status](hon/p04_fm2.md): [Figure 4.25 Time difference until slots are written to the sound stack](hon/p04_fm2.md): [Figure 4.26 Slot averaging operation](hon/p04_fm2.md): [Figure 4.27 Algorithm for 4-slot configuration](hon/p04_fm2.md): [Figure 4.28 Slot 0 algorithm](hon/p04_fm2.md): [Figure 4.29 Slot 2 algorithm](hon/p04_fm2.md): [Figure 4.30 Slot 2 algorithm (by input slot)](hon/p04_fm2.md): [Figure 4.31 Slot 3 algorithm](hon/p04_fm2.md): [Figure 4.32 MDL modulation depth](hon/p04_fm3.md): [Figure 4.33 Maximum displacement by waveform read address](hon/p04_fm3.md): [Figure 4.34 Address displacement during FM synthesis](hon/p04_fm3.md): [Figure 4.35 Wave data during clipping processing](hon/p04_fm3.md): [Figure 4.36 Number of slot connections](hon/p04_fm3.md): [Figure 4.37 Self-feedback modulation](hon/p04_fm3.md): [Figure 4.38 Multi-stage feedback](hon/p04_fm3.md): [Figure 4.39 Composite feedback](hon/p04_fm3.md): [Figure 4.40 Complex modulation](hon/p04_fm3.md): [Figure 4.41 FM configuration algorithm pattern 1](hon/p04_fm3.md): [Figure 4.42 FM configuration algorithm pattern 2](hon/p04_fm3.md): [Figure 4.43 7-slot FM configuration](hon/p04_fm3.md): [Figure 4.44 Wave data when TL bit4=1](hon/p04_24.md): [Figure 4.45 Relationship between OCT and FNS](hon/p04_25.md): [Figure 4.46 LFO block diagram](hon/p04_26.md): [Figure 4.47 Digital mixer block diagram](hon/p04_27.md): [Figure 4.48 Path of direct component and effect component](hon/p04_27.md): [Figure 4.49 Localization calculation using DSP](hon/p04_27.md): [Figure 4.50 Digital mixer block diagram](hon/p04_27.md): [Figure 4.51 SCSP and DAC connection](hon/p04_27.md): [Figure 4.52 Memory address mapping diagram](hon/p04_29.md): [Figure 4.53 MIDI-I/F block diagram](hon/p04_2a.md): [Figure 4.54 MIDI OUT section and interrupt generation section](hon/p04_2a.md): [Figure 4.55 Sound interrupt signal connection diagram](hon/p04_2c.md): [Figure 4.56 Interrupt register bit correspondence](hon/p04_2c.md): [Figure 4.57 Correspondence between 3-bit code and register](hon/p04_2c.md): [Figure 4.58 Interrupt level setting register format](hon/p04_2c.md): [Figure 4.59 DMA controller block diagram](hon/p04_2d.md) ### Chapter 5 Operation of DSP in SCSP : [Figure 5.1 DSP configuration diagram](hon/p05_10.md)

---

[![]()](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/index.htm) ★ [HARDWARE Manual](../index.md) ★ [SCSP User's Manual](index.md)

---

 Copyright SEGA ENTERPRISES, LTD., 1997
