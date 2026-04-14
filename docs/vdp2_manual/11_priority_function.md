# Chapter 11 Priority Function

Introduction ....................................................................224 11.1 Priority Function...................................................224 Priority Number .................................................224 Priority Number Register ..................................225 11.2 Special Priority Function ......................................227 Special Priority Mode Register .........................229 11.3 Insertion of Line Color Screen .............................230 Line Color Screen Enable Register ..................231 ST-58-R2



<!-- Page 242 -->

## Introduction

VDP2 compares the priority number values of sprites and scroll screens and decides the display priority order from the top three. The priority number of a sprite then selects each character from a maximum of eight values. The priority number of all scroll screens can also change the value of each dot and character by using the special priority function. The line color screen can be inserted into the number two position, one below the screen, when the designated screen is at the highest priority.

## 11.1 Priority Function

The priority (display priority order) of sprites and scroll screens compares sizes of the values of the screen priority number without transparent dots positioned in the same TV screen coordinates for each dot. The screen priority increases as the value of the priority number increases. The top image is made of the highest priority dots; the second image is made of the second highest dots; and the third image is made of the third highest dots. All sprite and scroll screen dots display the back screen in the transparent position. Figure 11.1 shows the priority function. Priority Priority Priority Priority Number= 6 Number= 4 Number= 2 Number= 1 Transparent Transparent Transparent Back Screen Back Screen Top Image Second Image Third Image

**Figure 11.1**

Priority Function

### Priority Number

The scroll screen has one 3-bit priority number register in each screen. This priority number normally is used in the entire surface, but can change the value of the least significant bit in each dot and character according to the special priority mode. The sprite priority number can select one of eight 3-bit priority number registers for each character. For information about selecting a sprite priority number register see “Priority Number Selection” in section “9.2 Priority and Color Calculation.”



<!-- Page 243 -->

Screen priority increases when the value of the priority number increases. When priority numbers are equal, they follow the order shown in

**Table 11.1. When the**

value of a priority number is OH, it is read as transparent.

**Table 11.1 Priority when the priority numbers are equal**

Priority Normal When inputting When displaying 2 When inputting external external image data screens of the rotation images per 2 screens of scroll screen the rotation scroll screen Highest Sprite Sprite Sprite Sprite : RBG0 RBG0 RBG0 RBG0 : NBG0 NBG0 RBG1 RBG1 : NBG1 EXBG - EXBG : NBG2 NBG2 - - Lowest NBG3 NBG3 - -

### Priority Number Register

The priority number register designates the priority number. This is a write-only 16bit register located at addresses 1800F8H to 1800FCH. Because the value is cleared to 0 after power on or reset, it must be set. 15 14 13 12 11 10 9 8 PRINA ~ ~ ~ ~ ~ N1PRIN2 N1PRIN1 N1PRIN0 1800F8H 7 6 5 4 3 2 1 0 ~ ~ ~ ~ ~ N0PRIN2 N0PRIN1 N0PRIN0 15 14 13 12 11 10 9 8 PRINB ~ ~ ~ ~ ~ N3PRIN2 N3PRIN1 N3PRIN0 1800FAH 7 6 5 4 3 2 1 0 ~ ~ ~ ~ ~ N2PRIN2 N2PRIN1 N2PRIN0 15 14 13 12 11 10 9 8 PRIR ~ ~ ~ ~ ~ ~ ~ ~ 1800FCH 7 6 5 4 3 2 1 0 ~ ~ ~ ~ ~ R0PRIN2 R0PRIN1 R0PRIN0 ST-58-R2



<!-- Page 244 -->

Priority number bit (for scroll screen) (N0PRIN2 to N0PRIN0, N1PRIN2 to N1PRIN0, N2PRIN2 to N2PRIN0, N3PRIN2 to N3PRIN0, R0PRIN2 to R0PRIN0) Designates the priority number of each screen scroll. N0PRIN2~N0PRIN0 1800F8H Bit 2~0 For NBG0 (or RBG1) N1PRIN2~N1PRIN0 1800F8H Bit 10~8 For NBG1 (or EXBG) N2PRIN2~N2PRIN0 1800FAH Bit 2~0 For NBG2 N3PRIN2~N3PRIN0 1800FAH Bit 10~8 For NBG3 R0PRIN2~R0PRIN0 1800FCH Bit 2~0 For RBG0 Larger priority numbers are given a higher display priority order. When the value of the priority number is 0, it is treated as transparent and not displayed. For more about priority number register of sprites, see “Priority Number Register” of “9.2 Priority and Color Calculation.”



<!-- Page 245 -->

## 11.2 Special Priority Function

The special priority function changes the least significant bit of the 3-bit priority number corresponding to every scroll screen in each character and dot. Using this function to change the priority of a portion of the scroll screen displays one surface as multiple screens. Furthermore, the least significant bit of the priority number changes only by the special priority function; the highest 2 bits are used with the register values. The special priority function has the following three modes. 1. Designates the least significant bit of the priority number in each screen 2. Designates the least significant bit of the priority number in each character 3. Designates the least significant bit of the priority number in each dot When designating the least significant bit of the priority number in each screen, the value of the priority number register in each scroll screen is used unchanged. When designating each character, the value of a special priority bit within pattern name data is used as the least significant bit of the priority number. For more information about special priority bits, see “4.6 Pattern Name Table (Page).” When designating each dot in character patterns designating 1 (the special priority bit within pattern name data), only dots that coincide with the dot color code are designated in the special function code. The least significant bit of the priority number is set to 1, the rest are fixed at 0. Do not set this mode when the color format of scroll screen is RGB. For more about the dot color code see “10.3 Special Function Code.” When the 3-bit priority number value obtained by the special priority function is OH, that screen, character, or dots are treated as transparent.

Table 11.2 shows the

special priority function by mode. ST-58-R2



<!-- Page 246 -->

**Table 11.2**

Special priority function by mode Special Priority Special Priority Color Format Priority Number LSB Value Mode Selection Mode 0 Selected per each Palette Format or RGB Priority number register LSB screen Format value Mode 1 Select per each Palette Format or RGB Value of the special priority bit character Format in the pattern name data Mode 2 Setting not allowed Palette Format When the special priority bit in the pattern name data is equal to one, only the dot coinciding with the dot color code selected for special function code becomes 1, while the rest become 0. RGB Format Setting Invalid When the display format designates mode 1 or 2 in the scroll screen of the bit map format, it is not the special priority bit within the pattern name data, but the special priority bit of the bit map palette number register that is used.



<!-- Page 247 -->

### Special Priority Mode Register

The special priority mode register is a write-only 16-bit register that designates the mode of the special priority function corresponding to each scroll screen, and is located at address 1800EA. Because the value clears to 0 after power on or reset, it must be set. 15 14 13 12 11 10 9 8 SFPRMD ~ ~ ~ ~ ~ ~ R0SPRM1 R0SPRM0 1800EAH 7 6 5 4 3 2 1 0 N3SPRM1 N3SPRM0 N2SPRM1 N2SPRM0 N1SPRM1 N1SPRM0 N0SPRM1 N0SPRM0 Special priority mode bit (N0SPRM1, N0SPRM0, N1SPRM1, N1SPRM0, N2SPRM1, N2SPRM0, N3SPRM1, N3SPRM0, R0SPRM1, R0SPRM0) Designates the special priority function mode of each screen scroll. N0SPRM1, N0SPRM0 1800EAH Bit 1,0 For NBG0 (or RBG1) N1SPRM1, N1SPRM0 1800EAH Bit 3,2 For NBG1 (or EXBG) N2SPRM1, N2SPRM0 1800EAH Bit 5,4 For NBG2 N3SPRM1, N3SPRM0 1800EAH Bit 7,6 For NBG3 R0SPRM1, R0SPRM0 1800EAH Bit 9,8 For RBG0 xxSPRM1 xxSPRM0 Mode Process 0 0 Mode 0 Select the priority number LSB per each screen 0 1 Mode 1 Select the priority number LSB per each character 1 0 Mode 2 Select the priority number LSB per each dot 1 1 - Selection not allowed

> Note: N0, N1, N2, N3, or R0 is entered in bit name for xx.

Do not set mode 2 when the scroll screen color format is the RGB mode. Do not set in EXBG any other mode but 0. Character or dot in which priority number is 0 is considered transparent. ST-58-R2



<!-- Page 248 -->

## 11.3 Insertion of Line Color Screen

The line color screen does not have a priority number register; it forcefully inserts the top image as the second image, and calculates color. Meanwhile, the original second image becomes the new third image, and the old third image becomes the fourth image. Figure 11.2 shows insertion of the line color screen. Priority Priority Priority Priority Number= 6 Number= 4 Number= 2 Number= 1 Transparent Transparent Transparent Screen that forces Screen that forces the insertion of the insertion of line line color screen color screen Line Color Screen Back Screen Back Screen Back Screen Top Image Second Image Third Image Fourth Image

**Figure 11.2 Line Color Screen Insertion**



<!-- Page 249 -->

### Line Color Screen Enable Register

The line color screen enable register designates whether to insert the line color screen when each screen is a top image. This is a write-only 16-bit register located at address 1800E8H. Because the value is cleared to 0 after power on or reset, you must set it. 15 14 13 12 11 10 9 8 LNCLEN ~ ~ ~ ~ ~ ~ ~ ~ 1800E8H 7 6 5 4 3 2 1 0 ~ ~ SPLCEN R0LCEN N3LCEN N2LCEN N1LCEN N0LCEN Line color enable bit (N0LCEN, N1LCEN, N2LCEN, N3LCEN, R0LCEN, SPLCEN) Designates whether to insert the line color screen when each screen is a top image. N0LCEN 1800E8H Bit 0 For NBG0 (or RBG1) N1LCEN 1800E8H Bit 1 For NBG1 (or EXBG) N2LCEN 1800E8H Bit 2 For NBG2 N3LCEN 1800E8H Bit 3 For NBG3 R0LCEN 1800E8H Bit 4 For RBG0 SPLCEN 1800E8H Bit 5 For Sprite xxLCEN Process 0 Does not insert the line color screen when corresponding screen is top image 1 Inserts the line color screen when corresponding screen is top image

> Note: N0, N1, N2, N3, R0, or SP is entered in the bit name for xx.

A line color screen is inserted only in the second image section of the screen designated for insertion, thus becoming the top image. Sprites can only be designated in their entirety. To designate each character, they must be controlled by their color calculation ratio value. This register cannot be used at the same time as the gradation calculation function. ST-58-R2



<!-- Page 250 -->

(This is page was blank in the original Japanese document)



<!-- Page 251 -->

