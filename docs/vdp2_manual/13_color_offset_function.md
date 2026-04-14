# Chapter 13 Color Offset Function

Introduction ....................................................................250 13.1 Color Offset Selection ..........................................250 Color Offset Enable Register ............................251 Color Offset Select Register .............................252 Color Offset Register ........................................253 ST-58-R2



<!-- Page 268 -->

## Introduction

The color offset function causes a change in the screen color without changing color RAM data by adding the offset value when sprite and data of each screen are output. Can also be used for fade-in and fade-out.

## 13.1 Color Offset Selection

The color offset value can set two values, color offset A and color offset B in each RGB, and can designate which of the two values to use for each screen. The color offset value is 9-bit data corresponding to individual RGB. When resulting color data added to individual RGB is smaller than 00H, the data is treated as 00H; when larger than FFH, the data is treated as FFH. Because the color offset function process follows the color calculation function process, the color offset value is added to the color data resulting from color calculations. In addition, because the result screen of color calculation is treated as the top image screen, designation of the color offset enable register is done with the screen bit of that top image. Figure 13.1 shows the color offset data. Bit 7 6 5 4 3 2 1 0 8 Bit Color Data + Bit 8 7 6 5 4 3 2 1 0 Color Offset A Sign 8 Bit Color Offset Data Color Offset B Select per screen Bit 7 6 5 4 3 2 1 0 8 Bit Output Color Data

**Figure 13.1 Color Offset Data**



<!-- Page 269 -->

### Color Offset Enable Register

The color offset enable register is a write-only 16-bit register that designates whether to use the color offset function for each screen, and is located at address 180110H. Because the value is cleared to 0 after power on or reset, it must be set. 15 14 13 12 11 10 9 8 CLOFEN ~ ~ ~ ~ ~ ~ ~ ~ 180110H 7 6 5 4 3 2 1 0 ~ SPCOEN BKCOEN R0COEN N3COEN N2COEN N1COEN N0COEN Color offset enable bit (N0COEN, N1COEN, N2COEN, N3COEN, R0COEN, BKCOEN, SPCOEN) Designates whether to use the color offset function. N0COEN 180110H Bit 0 For NBG0 (or RBG1) N1COEN 180110H Bit 1 For NBG1 (or EXBG) N2COEN 180110H Bit 2 For NBG2 N3COEN 180110H Bit 3 For NBG3 R0COEN 180110H Bit 4 For RBG0 BKCOEN 180110H Bit 5 For Back SPCOEN 180110H Bit 6 For Sprite xxCOEN Process 0 Do not use color offset function 1 Use color offset function

> Note: N0, N1, N2, N3, R0, BK, or SP is entered in bit name for xx.

Using the color calculation function designates the color offset enable bit of the top image screen. ST-58-R2



<!-- Page 270 -->

### Color Offset Select Register

The color offset select register designates the color offset register used for each screen. This is a write-only 16-bit register located at address 180112H. Because the value is cleared to 0 after power on or reset, it must be set. 15 14 13 12 11 10 9 8 CLOFSL ~ ~ ~ ~ ~ ~ ~ ~ 180112H 7 6 5 4 3 2 1 0 ~ SPCOSL BKCOSL R0COSL N3COSL N2COSL N1COSL N0COSL Color offset select bit (N0COSL, N1COSL, N2COSL, N3COSL, R0COSL, BKCOSL, SPCOSL) Designates the color offset register to use when using the color offset function. N0COSL 180112H Bit 0 For NBG0 (or RBG1) N1COSL 180112H Bit 1 For NBG1 (or EXBG) N2COSL 180112H Bit 2 For NBG2 N3COSL 180112H Bit 3 For NBG3 R0COSL 180112H Bit 4 For RBG0 BKCOSL 180112H Bit 5 For Back SPCOSL 180112H Bit 6 For Sprite xxCOSL Process 0 Use color offset A value 1 Use color offset B value

> Note: N0, N1, N2, N3, R0, BK, or SP is entered in bit name for xx.

When using the color calculation function, designates with the color offset select bit of the top image screen.



<!-- Page 271 -->

### Color Offset Register

The color offset register is a write-only 16-bit register that designates RGB individual values of the color offset value, and is located at addresses 180114H to 18011EH. Because the value is cleared to 0 after power on or reset, it must be set. 15 14 13 12 11 10 9 8 COAR ~ ~ ~ ~ ~ ~ ~ COARD8 180114H 7 6 5 4 3 2 1 0 COARD7 COARD6 COARD5 COARD4 COARD3 COARD2 COARD1 COARD0 15 14 13 12 11 10 9 8 COAG ~ ~ ~ ~ ~ ~ ~ COAGR8 180116H 7 6 5 4 3 2 1 0 COAGR7 COAGR6 COAGR5 COAGR4 COAGR3 COAGR2 COAGR1 COAGR0 15 14 13 12 11 10 9 8 COAB ~ ~ ~ ~ ~ ~ ~ COABL8 180118H 7 6 5 4 3 2 1 0 COABL7 COABL6 COABL5 COABL4 COABL3 COABL2 COABL1 COABL0 15 14 13 12 11 10 9 8 COBR ~ ~ ~ ~ ~ ~ ~ COBRD8 18011AH 7 6 5 4 3 2 1 0 COBRD7 COBRD6 COBRD5 COBRD4 COBRD3 COBRD2 COBRD1 COBRD0 15 14 13 12 11 10 9 8 COBG ~ ~ ~ ~ ~ ~ ~ COBGR8 18011CH 7 6 5 4 3 2 1 0 COBGR7 COBGR6 COBGR5 COBGR4 COBGR3 COBGR2 COBGR1 COBGR0 15 14 13 12 11 10 9 8 COBB ~ ~ ~ ~ ~ ~ ~ COBBL8 18011EH 7 6 5 4 3 2 1 0 COBBL7 COBBL6 COBBL5 COBBL4 COBBL3 COBBL2 COBBL1 COBBL0 Color offset value bit: Color offset data bit (COARD8 to COARD0, COAGR8 to COAGR0, COABL8 to COABL0, COBRD8 to COBRD0, COBGR8 to COBGR0, COBBL8 to COBBL0) Sets the RGB individual value of color offset A and B. Negative numbers should be set by two’s- complement values. ST-58-R2



<!-- Page 272 -->

COARD8~COARD0 180114H Bit 8~0 For color offset A RED data COAGR8~COAGR0 180116H Bit 8~0 For color offset A GREEN data COABL8~COABL0 180118H Bit 8~0 For color offset A BLUE data COBRD8~COBRD0 18011AH Bit 8~0 For color offset B RED data COBGR8~COBGR0 18011CH Bit 8~0 For color offset B GREEN data COBBL8~COBBL0 18011EH Bit 8~0 For color offset B BLUE data



<!-- Page 273 -->

