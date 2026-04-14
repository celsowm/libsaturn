# Chapter 12 Color Calculations

Introduction ....................................................................234 12.1 Color Calculation Function...................................234 Normal Color Calculation ..................................234 Extended Color Calculation Function ...............236 12.2 Gradation Calculation Function ...........................238 Color Calculation Control Register ...................240 Color Calculation Ratio Register....................... 243 12.3 Special Color Calculation Function...................... 245 Special Color Calculation Mode Register .........247 ST-58-R2



<!-- Page 252 -->

## Introduction

VDP2 calculates, by a designated ratio, color data of second and top images obtained comparing priorities of sprite and each scroll screen. The sprite and color calculation ratio can select each character from one of eight values. The color calculation enable of every scroll screen can designate each character and dot by using the special color calculation function. With the second and third images at a fixed ratio, the colors of up to four screens can be calculated by doing color calculations in the second image. In addition, distant backgrounds can be shaded causing the gradation calculation function to be enabled.

## 12.1 Color Calculation Function

The color calculation function calculates with a ratio designating the top and second images (or three images, the second image, third image, forth image, images calculated at a set ratio) by individual RGB color, and is able to produce an effect of overlapping a group of semi-transparent screens. The color calculation function is shown in Figure 12.1. Priority Number = 4 Priority Number = 2 Transparent Transparent Back Screen F igure 12.1 Color calculation function

### Normal Color Calculation

There are two types of modes when calculating color by the top and second images: 1. Top and second images add according to the value of the color calculation ratio. 2. Top and second images add by the value unchanged.



<!-- Page 253 -->

The value of the color calculation ratio of each screen is designated in the register when using the color calculation mode to add in proportion to the value of color calculation ratio. The color calculation ratio of the sprite can designate one register from a maximum of eight for each character; the scroll screen color calculation ratio designates to the register of each screen. For more about sprite color calculation ratio see “Color Calculation Ratio Register” in “9.2 Priority and Color Calculation.” The color calculation ratio of each screen is designated by 5 bits, and a total of 32 steps: top image : second image = 31 : 1 to 0 : 32 can be selected. There are two kinds of modes for designating the value of that ratio. 1. Designates by the top image 2. Designates by the second image When designating with the top image, the sprite that becomes the top image or the color calculation ratio value of the scroll screen is used without any relation to the second image screen. When designating with the second image, the color calculation ratio can be changed by the screen of the second image. The sprite, scroll screen values, line color screen, and back screen color calculation ratio values can be used. When using the color calculation mode to add by the value, gradually rewrite from the top image color data from 00H to the original color to create a fade-in effect of the top image over the second image. Be aware that doing so may result in color that is concurrent with fade-in becoming brighter than the original. Addition results that exceed FFH are treated as FFH. Figure 12.2 shows the color calculation ratio mode. ST-58-R2



<!-- Page 254 -->

Priority Number = 6 Priority Number = 4 Priority Number = 2 Screen B Transparent Back Screen Screen C Transparent Screen A Color Calculation Ratio Color Calculation Ratio Color Calculation Ratio Color Calculation Ratio 16:16 24:8 28:4 20:12 When selecting the top screen image When selecting the second screen image Screen B Screen B Back Back Screen Screen Screen C Screen C Screen A : Screen B =16 : 16 Screen A : Screen B = 20 : 12 Screen A : Screen C =16 : 16 Screen A : Screen C = 24 : 8 Screen A : Back =16 : 16 Screen A : Back = 28 : 4

**Figure 12.2**

Color calculation ratio mode There is no limitation in calculating color when the TV screen mode is the Normal mode. But when the TV screen mode is the high resolution mode or Exclusive monitor mode, the color RAM mode and second image color format hav limitations.

Table 12.1 shows limitations of the color calculation function.

**Table 12.1 Color calculation function when in the high resolution mode or special monitor mode**

Color RAM Mode Second Image Color Format Color Calculation Function Mode 0 Palette format or RGB format Can be used Mode 1 Palette format Cannot be used RGB format Can be used Mode 2 Palette format Cannot be used RGB format Can be used

### Extended Color Calculation Function

The extended color calculation function makes second image data of the result of the second and third images calculated by a fixed ratio. Calculating the color of the second and top images makes possible three screen color calculation. Inserting a line color screen creates third image data resulting from color calculation of the third and fourth images calculated by a fixed ratio, and makes second image data by adding the third image and line color data calculated by a fixed ratio. Therefore, four screen color calculation is possible. The extended color calculation function can be used only when in the TV screen mode is the Normal mode.



<!-- Page 255 -->

When performing extended color calculation, whether to add the third and fourth images complies with the screen color calculation enable bit of the third image screen, and whether to add the second and third images complies with the color calculation enable bit of the second image screen. When the extended color calculation function is used, the extended color calculation ratio changes according to the color RAM mode, the line color screen insertion, the color calculation enable bit value of second and third images and color format, and the color format of the fourth image.

**Figure 12.3 shows the extended color calculation function;**

Table 12.2 shows the

extended color calculation ratios. Normal Case When inserting line color screen When second When second When line color When line color image screen image screen calculation calculation color calculation color calculation enable bit =1, enable bit =1, enable bit =0 enable bit =1 third image third image screen color screen color calculation calculation enable bit =0 enable bit =1 Second image Second image Second image Second image after expanded after expanded after expanded after expanded color color color color calculation calculation calculation calculation Line Color Line Color Second image Second Image Screen Screen + + + Third image Third image Third image + Fourth Image

**Figure 12.3 Extended Color Calculation Function**

ST-58-R2



<!-- Page 256 -->

**Table 12.2 Extended Color Calculation Ratio**

Color Line Color Color Format Color Calculation Extended Color RAM Screen Enable Bit Value Calculation Ratio Mode 2nd Image 3rd Image 4th Image 2nd Image 3rd Image 2nd:3rd:4th (Image) Does not Palette or Palette or - 0 - 4:0:0 Mode Insert RGB format RGB format 1 - 2:2:0 0 Palette Palette 0 0 or 1 4:0:0 Inserts or RGB or RGB 1 0 2:2:0 format format 1 1 2:1:0 Does not Palette Palette - 0 or 1 - 4:0:0 Insert or RGB format format RGB - 0 - 4:0:0 Mode format 1 - 2:2:0 1 Palette Palette or 0 or 1 0 or 1 4:0:0 or format RGB format Mode Palette 0 0 or 1 4:0:0 2 Inserts - RGB format 1 0 or 1 2:2:0 format RGB 0 0 or 1 4:0:0 format 1 0 2:2:0 1 1 2:1:1

> Note:

The extended color calculation function cannot be used when the TV screen mode in is the high-resolution or Exclusive monitor mode. When inserting the line color screen, the color format of the second image becomes the palette format. The extended color calculation ratio is for a value that is 1/4 times the individual RGB data.

## 12.2 Gradation Calculation Function

The gradation calculation function calculates the horizontal color data of the designated one screen by a fixed ratio, and can create a gradation effect of the distant background. This function can only be used when the TV screen mode is the Normal mode, and the color RAM mode is mode 0. Gradation Calculation adds color data of a designated screen for values 1/4 times that of individual RGB by the ratio below. (2 dots left display coordinates : (1 dot left display coordinates : (display coordinates = 1:1:2 color data) color data) color data) Here, the added results of the area in which the designated screen becomes the top or second images is forced to be the second image. By calculating color of the second and top images, it is possible to display pictures in which gradation calculations have been done. The second image of the area, in which the gradation calculation designated screen is not a top or second image, becomes the image determined by normal priority.



<!-- Page 257 -->

Screens using gradation calculation have no transparent dots. If gradation calculation is performed for screens having transparent dots, correct calculation cannot be done by that boundary. If the gradation calculation function is used, yit is not possible to insert the line color screen. In addition, the extended color calculation function can no longer be used. Figure 12.4 shows the gradation calculation function. Priority number = 6 Priority number = 4 Priority number = 2 transparent transparent Screen C Screen A Screen B Screen designated to be shaded Top Image Second Image Back Screen Screen C Screen C Screen B Screen A Screen B The area in screen C that is the top image or second image is forced to switch to shaded screen C Second Image Shaded Screen C Screen B Color Operation Color Function used in Screen C Display Image Shaded Screen C Screen B Screen A

**Figure 12.4 Gradation Calculation Function**

ST-58-R2



<!-- Page 258 -->

### Color Calculation Control Register

The color calculation control register is a write-only 16-bit register that controls color calculation, and is located at address 1800ECH. Because the value is cleared to 0 after power on or reset, the value must be set. 15 14 13 12 11 10 9 8 1800ECH BOKEN BOKN2 BOKN1 BOKN0 ~ EXCCEN CCRTMD CCMD 7 6 5 4 3 2 1 0 ~ SPCCEN LCCCEN R0CCEN N3CCEN N2CCEN N1CCEN N0CCEN Gradation enable bit (BOKEN), bit 15 Determines whether to use the gradation function. BOKEN Process 0 Do not use gradation calculation function 1 Use gradation calculation function If this bit is 1, the extended color calculation function can no longer be used. The gradation calculation function can only be used when the TV screen mode is the Normal mode, and the color RAM mode is mode 0. Gradation screen number bit: Gradation number bit (BOKN2 to BOKN0), bits 14 to 12 Designates the screen using the gradation (shading) calculation function. BOKN2 BOKN1 BOKN0 Screen Using Gradation Calculation Function 0 0 0 Sprite 0 0 1 RBG0 0 1 0 NBG0 or RBG1 0 1 1 Invalid 1 0 0 NBG1 or EXBG 1 0 1 NBG2 1 1 0 NBG3 1 1 1 Invalid Extended color calculation enable bit (EXCCEN), bit 10 Determines whether to use the extended color calculation function.



<!-- Page 259 -->

EXCCEN Process 0 Do not use extended color calculation 1 Use extended color calculation The above calculation function cannot be used at the same time as the gradation calculation function. When the BOKEN bit is 1, this bit is ignored. The extended color calculation function can only be used when in the TV screen and Normal modes, and cannot be used when in the high-resolution mode or Exclusive monitor mode. Color calculation ratio mode bit (CCRTMD), bit 9 Designates the color calculation ratio mode. CCRTMD Mode Process 0 0 For color calculation ratio, select per top screen side 1 1 For color calculation ratio, select per second screen side The top image always designates whether to perform normal color calculation. Color calculation mode bit (CCMD), bit 8 Designates the color calculation mode. CCMD Mode Process 0 0 Add according to the color calculation register value 1 1 Add as is When in mode 1, the values of the color calculation ratio registers of each screen are ignored. ST-58-R2



<!-- Page 260 -->

Color calculation enable bit (N0CCEN, N1CCEN, N2CCEN, N3CCEN, R0CCEN, LCCCEN, SPCCEN) Designates whether to perform color calculation (color calculation enable) N0CCEN 1800ECH Bit 0 For NBG0 (or RBG1) N1CCEN 1800ECH Bit 1 For NBG1 (or EXBG) N2CCEN 1800ECH Bit 2 For NBG2 N3CCEN 1800ECH Bit 3 For NBG3 R0CCEN 1800ECH Bit 4 For RBG0 LCCCEN 1800ECH Bit 5 For LNCL SPCCEN 1800ECH Bit 6 For Sprite xxCCEN Process 0 Does not color-calculate 1 Color-calculates

> Note: N0, N1, N2, N3, R0, LC, or SP is entered in bit name for xx.

When calculating color between the top and second images, calculation is controlled by the color calculation enable bit of the top image. When using the extended color calculation function, control between the second and third images is done by the color calculation enable bit of the second image, and control between the third and fourth images is done by the color calculation enable bit of the third image.



<!-- Page 261 -->

### Color Calculation Ratio Register

The color calculation ratio register is a write-only 16-bit register that designates the color calculation ratio, and is located at addresses 180108H to 18010EH. Because the value is cleared to 0 after power on or reset, the value must be set. 15 14 13 12 11 10 9 8 CCRNA ~ ~ ~ N1CCRT4 N1CCRT3 N1CCRT2 N1CCRT1 N1CCRT0 180108H 7 6 5 4 3 2 1 0 ~ ~ ~ N0CCRT4 N0CCRT3 N0CCRT2 N0CCRT1 N0CCRT0 15 14 13 12 11 10 9 8 CCRNB ~ ~ ~ N3CCRT4 N3CCRT3 N3CCRT2 N3CCRT1 N3CCRT0 18010AH 7 6 5 4 3 2 1 0 ~ ~ ~ N2CCRT4 N2CCRT3 N2CCRT2 N2CCRT1 N2CCRT0 15 14 13 12 11 10 9 8 CCRR ~ ~ ~ ~ ~ ~ ~ ~ 18010CH 7 6 5 4 3 2 1 0 ~ ~ ~ R0CCRT4 R0CCRT3 R0CCRT2 R0CCRT1 R0CCRT0 15 14 13 12 11 10 9 8 CCRLB ~ ~ ~ BKCCRT4 BKCCRT3 BKCCRT2 BKCCRT1 BKCCRT0 18010EH 7 6 5 4 3 2 1 0 ~ ~ ~ LCCCRT4 LCCCRT3 LCCCRT2 LCCCRT1 LCCCRT0 Color calculation ratio bit (for scroll screens): (N0CCRT4 to N0CCRT0, N1CCRT4 to N1CCRT0, N2CCRT4 to N2CCRT0, N3CCRT4 to N3CCRT0, R0CCRT4 to R0CCRT0, LCCCRT4 to LCCCRT0, BKCCRT4 to BKCCRT0) Designates the color calculation ratio of each scroll screen. The color calculation ratio corresponds to a value 1/32 times R,G,B color data. N0CCRT4~NOCCRT0 180108H Bit 4~0 For NBG0 (or RBG1) N1CCRT4~N1CCRT0 180108H Bit 12~8 For NBG1 (or EXBG) N2CCRT4~N2CCRT0 18010AH Bit 4~0 For NBG2 N3CCRT4~N3CCRT0 18010AH Bit 12~8 For NBG3 R0CCRT4~R0CCRT0 18010CH Bit 4~0 For RBG0 LCCCRT4~LCCCRT0 18010EH Bit 4~0 For LNCL BKCCRT4~BKCCRT0 18010EH Bit 12~8 For Back ST-58-R2



<!-- Page 262 -->

xxCCRT4 xxCCRT3 xxCCRT2 xxCCRT1 xxCCRT0 Color Calculation Ratio Top Image : Second Image 0 0 0 0 0 31:1 0 0 0 0 1 30:2 0 0 0 1 0 29:3 0 0 0 1 1 28:4 0 0 1 0 0 27:5 0 0 1 0 1 26:6 0 0 1 1 0 25:7 0 0 1 1 1 24:8 0 1 0 0 0 23:9 0 1 0 0 1 22:10 0 1 0 1 0 21:11 0 1 0 1 1 20:12 0 1 1 0 0 19:13 0 1 1 0 1 18:14 0 1 1 1 0 17:15 0 1 1 1 1 16:16 1 0 0 0 0 15:17 1 0 0 0 1 14:18 1 0 0 1 0 13:19 1 0 0 1 1 12:20 1 0 1 0 0 11:21 1 0 1 0 1 10:22 1 0 1 1 0 9:23 1 0 1 1 1 8:24 1 1 0 0 0 7:25 1 1 0 0 1 6:26 1 1 0 1 0 5:27 1 1 0 1 1 4:28 1 1 1 0 0 3:29 1 1 1 0 1 2:30 1 1 1 1 0 1:31 1 1 1 1 1 0:32

> Note: N0, N1, N2, N3, R0, LC, or BK is entered in bit name for xx.

For more about the color calculation ratio register of sprites see “Color Calculation Ratio Register” in “9.2 Priority and Color Calculation.”



<!-- Page 263 -->

## 12.3 Special Color Calculation Function

The special color calculation function designates the color calculation enable not only by the entire screen but by character units and dot units. See the four modes below. 1. Color calculation enable designates each screen. 2. Color calculation enable designates each character. 3. Color calculation enable designates each dot. 4. Color calculation enable designates by the most significant bit of color data. When designating color calculation enable for each screen, color calculation is performed when the color calculation enable bit value in the color calculation control register that corresponds to each scroll screen is 1. When designating each character in a scroll screen that the color calculation enable bit has designated 1, color calculation is performed only in character patterns of a special color calculation bit value of 1 in pattern name data. For more about the special color calculation bit in pattern name data see “4.6 Pattern Name Table.” When designating each dot, color calculation is performed only in dots that agree with the dot color code designated in the special function code, and in the character pattern designated 1 in which the value of the special color calculation bit within the pattern name data of the scroll screen has a color calculation enable bit of 1. Do not set this mode when the color format of the scroll screen is the RGB format. For more about the special function code see “10.3 Special Function Code.” When designating with the most significant bit of color data, color calculation is performed only in dots that used color data when the most significant bit is set at 1, and when the scroll screen where the color calculation enable bit is designated 1 is in a palette format. Color calculation will always be performed if this mode is designated when the scroll screen, where the color calculation enable bit is designated 1, is in the RGB format. When mode 1 or 2 is designated in a bit map format scroll screen, the special color calculation bit of the bit map number register is used, not the special color calculation bit within pattern name data.

Table 12.3 shows the special color calculation

mode. ST-58-R2



<!-- Page 264 -->

**Table 12.3 Special Color Calculation Mode**

Special Color Special Color Color Format Color Calculation Enable Calculation Mode Calculation Selection Condition Mode 0 Select per screen Palette format or RGB Color calculation enable bit =1 format Mode 1 Select per character Palette format or RGB Color calculation bit = 1 and format pattern name data special color calculation bit = 1 Mode 2 Select per dot Palette format Color calculation bit = 1 and pattern name data special color calculation bit = 1 and The dot that matches dot color code selected per special function code RGB format Invalid Mode 3 Select with color data Palette format Color calculation bit = 1 and MSB The dot using color data where MSB = 1 RGB format Color calculation enable bit =1 The special color calculation mode can designate only for top images. Otherwise, it is fixed at 0.



<!-- Page 265 -->

### Special Color Calculation Mode Register

The special color calculation mode register is a write-only 16-bit register that designates the special color calculation function mode for each scroll screen, and is located at address 1800EEH. Because the value is cleared to 0 after power on or reset, you must set the value. 15 14 13 12 11 10 9 8 SFCCMD ~ ~ ~ ~ ~ ~ R0SCCM1 R0SCCM0 1800EEH 7 6 5 4 3 2 1 0 N3SCCM1 N3SCCM0 N2SCCM1 N2SCCM0 N1SCCM1 N1SCCM0 N0SCCM1 N0SCCM0 Special color calculation mode bit (N0SCCM1, N0SCCM0, N1SCCM1, N1SCCM0, N2SCCM1, N2SCCM0, N3SCCM1, N3SCCM0, R0SCCM1, R0SCCM0) Designates the special color calculation function mode of each scroll screen. N0SCCM1, N0SCCM0 1800EEH Bit 1,0 For NBG0 (or RBG1) N1SCCM1, N1SCCM0 1800EEH Bit 3,2 For NBG1 (or EXBG) N2SCCM1, N2SCCM0 1800EEH Bit 5,4 For NBG2 N3SCCM1, N3SCCM0 1800EEH Bit 7,6 For NBG3 R0SCCM1, R0SCCM0 1800EEH Bit 9,8 For RBG0 xxSCCM1 xxSCCM0 Mode Process 0 0 0 Select color calculation enable per screen 0 1 1 Select color calculation enable per character 1 0 2 Select color calculation enable per dot 1 1 3 Select color calculation enable with color data MSB

> Note: N0, N1, N2, N3, or R0 is entered in bit name for xx.

Special color calculation mode designation is effective only when each screen is a top image. Otherwise, the mode must be set at 0. When the color format of scroll screen is the RGB format, do not fixed at mode 2. Color is calculated by all dots when mode 3 has been designated. Finally, do not designate modes 1 and 2 in EXBG. ST-58-R2



<!-- Page 266 -->

(This page was blank in the original Japanese document)



<!-- Page 267 -->

