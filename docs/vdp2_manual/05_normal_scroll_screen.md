
| +7FBFC | +7FBFD | +7FBFE | +7FBFF |
| --- | --- | --- | --- |
| +7FFFC | +7FFFD | +7FFFE | +7FFFF |



<!-- Page 128 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 |
| --- |
| Dot 0-1 |

|   | +00002 |   | +00006 |
| --- | --- | --- | --- |
| +00000 | +00002 | +00004 | +00006 |
|   | +00802 |   | +00806 |
| +00800 | +00802 | +00804 | +00806 |

| +007F8 | +007FA | +007FC | +007FE |
| --- | --- | --- | --- |
| +00FF8 | +00FFA | +00FFC | +00FFE |

| +FF000 | +FF002 | +FF004 | +FF006 |
| --- | --- | --- | --- |
| +FF800 | +FF802 | +FF804 | +FF806 |

| +FF7F8 | +FF7FA | +FF7FC | +FF7FE |
| --- | --- | --- | --- |
| +FFFF8 | +FFFFA | +FFFFC | +FFFFE |



<!-- Page 129 -->

### Bit Map Palette Number

The bit map palette number designates the lead address of the palette used in the bit map pattern. With the 3-bit data designated by the bit map palette number register, the bit map palette number can only be used when the color format is in the palette format. It cannot be used when in the RGB format. Because the palette number is added to the dot color code of the bit map pattern to make an 11 bit dot color code, the bit count that is used by the color count on each surface changes. Figure 4.18 shows dot color data by bit map number colors.

**Figure 4.18 Dot color data by bit map color numbers**

### Special Function Bit

The special function bit designates whether to use the special function for bit map patterns. The special function bit has a special priority bit that controls the priority number and the special color calculation bit that controls color calculation. For more information on the special priority bit see “11.2 Special Priority Function.” For more information on the color calculation bit see “12.2 Special Color Calculation Function.”

| Palette Number |   |   |   |   |   |   |   | Dot Color Code |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 6 | 5 | 4 | 3 | 2 | 1 | 0 | 3 |   | 2 | 1 | 0 |

|   | Palette No. |   |   |   | Dot Color Code |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 6 |   | 5 | 4 |   | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |

| Dot Color Code |   |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |



<!-- Page 130 -->

### Bit Map Palette Number Register

Bit map palette number register selects the palette number when the scroll screen is displayed by the bit map format and special function bit. This register is a writeonly 16-bit register located in addresses 18002CH to 18002EH. Because the value is cleared to 0 after the power is turned on or reset, make sure the value is set. Designates the special priority bit when the scroll screen is displayed by the bit map format. See section “11.2 Special Priority Function” on how to use this bit. Designates the special color calculation bit when the scroll screen is displayed by the bit map format. See section “12.2 Special Color Calculation Function” on how to use this bit.

| BMPNA | ~ | ~ | N1BMPR | N1BMCC | ~ | N1BMP6 | N1BMP5 | N1BMP4 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18002CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N0BMPR | N0BMCC | ~ | N0BMP6 | N0BMP5 | N0BMP4 |

| BMPNB | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18002EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | R0BMPR | R0BMCC | ~ | R0BMP6 | R0BMP5 | R0BMP4 |

| N0BMPR | 18002CH | Bit 5 | For NBG0 |
| --- | --- | --- | --- |
| N1BMPR | 18002CH | Bit 13 | For NBG1 |
| R0BMPR | 18002EH | Bit 5 | For RBG0 |

| N0BMCC | 18002CH | Bit 4 | For NBG0 |
| --- | --- | --- | --- |
| N1BMCC | 18002CH | Bit 12 | For NBG1 |
| R0BMCC | 18002EH | Bit 4 | For RBG0 |



<!-- Page 131 -->

Designates the highest three bits of the palette number when the scroll screen is displayed in the bit map format. When the bit map color count is 16 colors, a “0” is attached to the lowest four bits and used as the 7-bit palette number.

| N0BMP6~N0BMP4 | 18002CH | Bit 2~0 | For NBG0 |
| --- | --- | --- | --- |
| N1BMP6~N1BMP4 | 18002CH | Bit 10~8 | For NBG1 |
| R0BMP6~R0BMP4 | 18002EH | Bit 2~0 | For RBG0 |



<!-- Page 132 -->

## 4.10 Display Area

The display area of the scroll screen changes depending on the register setting. The display area image is repeated and displayed when display coordinate values exceed the display area in the Normal scroll screen. Control is executed by the register setting when display coordinate values exceed the display area in the rotation scroll surface.

### Display Area

The display area changes according to the plane size when scroll screen display format is the cell format, and according to the bit map size when in the bit map format. NBG0 and NBG1 also change by setting the reduction display up to 1/4. Tables 4.12 and 4.13 show the display areas.

**Table 4.12 Normal scroll screen display area**

**Table 4.13 Rotation Scroll Surface display area**

| Display<br>Format | Reduction<br>Setting | Plane Size | Bitmap Size | Display Area |
| --- | --- | --- | --- | --- |
|   |   | 1 H page X<br>1 V page | - | 0£ X<1024, 0£ Y<1024 |
|   | No reduction~ 1/2<br>reduction | 2 H pages X<br>1 V page | - | 0£ X<2048, 0£ Y<1024 |
| Cell format |   | 2 H pages X<br>2 V pages | - | 0£ X<2048, 0£ Y<2048 |
|   | Up to 1/4<br>reduction | 1 H page X<br>1 V page | - | 0£ X<1024, 0£ Y<2048 |
|   |   | 2 H pages X<br>1 V page | - | 0£ X<2048, 0£ Y<2048 |
|   |   | - | 512 H dots X<br>256 V dots | 0£ X<512, 0£ Y<256 |
| Bitmap Format | - | - | 512 H dots X<br>512 V dots | 0£ X<512, 0£ Y<512 |
|   |   | - | 1024 H dots X<br>256 V dots | 0£ X<1024, 0£ Y<256 |
|   |   | - | 1024 H dots X<br>512 V dots | 0£ X<1024, 0£ Y<512 |

| Display Format | Plane Size | Bitmap Size | Display Area |
| --- | --- | --- | --- |
|   | 1 H page X<br>1 V page | - | 0£ X<2048, 0£ Y<2048 |
| Cell format | 2 H pages X<br>1 V page | - | 0£ X<4096, 0£ Y<2048 |
|   | 2 H pages X<br>2 V pages | - | 0£ X<4096, 0£ Y<4096 |
| Bitmap Format | - | 512 H dots X<br>256 V dots | 0£ X<512, 0£ Y<256 |
| E | - | 512 H dots X<br>512 V dots | 0£ X<512, 0£ Y<512 |



<!-- Page 133 -->

### Screen-Over Process

While the rotation scroll surface is displayed, and if the calculated results of display coordinate values of an display area have been exceeded, select one of the four below and set it to the register. The setting of the screen-over process is not performed for RBG0 and RBG1, but is performed for the scroll screen by rotation parameter A and the scroll screen by rotation parameter B. 1. The outside of the display area repeats the image set in the display area. 2. The outside of the display area repeats the character pattern designated by the screen-over pattern name register (only when the rotation scroll surface is in the cell format). 3. The outside of the display area is transparent. 4. With no relationship to the plane size and bit map size, the display area is at 0 ≤ X < 512 and 0 ≤ Y < 512. The outside of the display area is all made to be transparent.

### Display-Over Pattern Name

When the designated character pattern is made to be repeated, pattern name data selects the screen-over control setting in a16-bit screen-over pattern name register. The screen-over pattern name data selected in the register is handled the same as when the data size of the scroll surface pattern name table is 1-word; it uses supplemental data in the lowest 10 bits of the pattern name control register, supplements insufficient bits, and does screen-over pattern name data of a total 26 bits. The bit configuration of the screen-over pattern name register is the same as when the pattern name data size is 1-word, as in “Pattern Name Table” of section 4.6; and changes depending on character size, character color number, and the character number supplement mode. The size of the repeated character pattern follows the setting of the character size. 16-bit screen over pattern name data designates for the scroll screen by rotation parameter A and scroll screen by rotation parameter B, but you should be careful when designating for RBG0 and RBG1 by 10-bit supplement data designated by the pattern name control register. Besides, screen-over pattern name data cannot be used when the rotation scroll screen is displayed in the bit map format. ST-58-R2



<!-- Page 134 -->

### Screen-Over Pattern Name Register

In the screen-over process of the rotation scroll surface, the screen-over pattern name register selects pattern name data when the repetition of the character pattern is set. This register is a write-only 16-bit register and is in addresses 1800B8H to 1800BAH. Because the value is cleared to 0 after the power is turned on or reset, be sure to set the value. Designates pattern name data when the screen-over process repeating the character pattern is set. The bit configuration is the same as when the data size of the pattern name table is one-word; and changes depending on the settings of the character size, character color number, and character number supplement mode. This register action is executed for the scroll screen by rotation parameter A and B, but the character size that decides the bit configuration as well as the character number supplement mode performs in RBG0 and RBG1. Therefore, be careful when simultaneously displaying screens by rotation parameter A and B in RBG0.

| OVPNRA | RAOPN15 | RAOPN14 | RAOPN13 | RAOPN12 | RAOPN11 | RAOPN10 | RAOPN9 | RAOPN8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800B8H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | RAOPN7 | RAOPN6 | RAOPN5 | RAOPN4 | RAOPN3 | RAOPN2 | RAOPN1 | RAOPN0 |

| OVPNRB | RBOPN15 | RBOPN14 | RBOPN13 | RBOPN12 | RBOPN11 | RBOPN10 | RBOPN9 | RBOPN8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800BAH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | RBOPN7 | RBOPN6 | RBOPN5 | RBOPN4 | RBOPN3 | RBOPN2 | RBOPN1 | RBOPN0 |

| RAOPN15~RAOPN0 | 1800B8H | Bit 15~0 | For Rotation Parameter A |
| --- | --- | --- | --- |
| RBOPN15~RBOPN0 | 1800BAH | Bit 15~0 | For Rotation Parameter B |



<!-- Page 135 -->

## 4.11 Mosaic Process

The mosaic process can be done for each screen using the scroll surface. The mosaic size can be set for the respective horizontal and vertical directions. The mosaic process divides each scroll screen into several areas of pre-determined size. This function displays all dots within various areas of colored dots in the upper left. The mosaic pattern can be achieved by aligning different color areas. The size of the mosaic area can be individually selected. Size in the horizontal direction can select from 1 to 16 dots in single dot units. Size in the vertical direction can select from 1 to 16 dots in the non-interlace mode in single dot units, and 2 to 32 dots in the interlace mode in two-dot units. If the register is set to do mosaic processing when in the double-density interlace mode, the screen is made to display in the single-density interlace mode. When using the mosaic process in NBG0 or NBG1, the vertical cell scroll function can no longer be used. Also, mosaic processing of RBG0 and RBG1 can only be done in the horizontal direction. Figure 4.19 shows the mosaic pattern.

**Figure 4.19 Mosaic Pattern**



<!-- Page 136 -->

### Mosaic Control Register

The mosaic control register selects whether to perform the mosaic process. It is a write-only 16-bit register and is in address 180022H. Because the value is cleared to 0 after the power is turned on or reset, be sure to set the value. Designates the horizontal and vertical mosaic size.

| MZCTL | MZSZV3 | MZSZV2 | MZSZV1 | MZSZV0 | MZSZH3 | MZSZH2 | MZSZH1 | MZSZH0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180022H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | R0MZE | N3MZE | N2MZE | N1MZE | N0MZE |

| MZSZV3~MZSZV0 | 180022H | Bit 15~12 | For vertical mosaic size |
| --- | --- | --- | --- |
| MZSZH3~MZSZH0 | 180022H | Bit 11~8 | For horizontal mosaic size |

| MZSZH3 | MZSZH2 | MZSZH1 | MZSZH0 | Horizontal Mosaic Size |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 1 dot |
| 0 | 0 | 0 | 1 | 2 dots |
| 0 | 0 | 1 | 0 | 3 dots |
| 0 | 0 | 1 | 1 | 4 dots |
| 0 | 1 | 0 | 0 | 5 dots |
| 0 | 1 | 0 | 1 | 6 dots |
| 0 | 1 | 1 | 0 | 7 dots |
| 0 | 1 | 1 | 1 | 8 dots |
| 1 | 0 | 0 | 0 | 9 dots |
| 1 | 0 | 0 | 1 | 10 dots |
| 1 | 0 | 1 | 0 | 11 dots |
| 1 | 0 | 1 | 1 | 12 dots |
| 1 | 1 | 0 | 0 | 13 dots |
| 1 | 1 | 0 | 1 | 14 dots |
| 1 | 1 | 1 | 0 | 15 dots |
| 1 | 1 | 1 | 1 | 16 dots |



<!-- Page 137 -->

> Note: There is no relationship with the interlace setting.

Designates the screen performing mosaic process.

> Note: N0, N1, N2, N3, or R0 is entered in bit name for xx.

Only horizontal mosaic processing is performed when the mosaic process is in the rotation scroll surface. If performing mosaic processing in the double-density interlace mode, the screen is made to be displayed by the single-density interlace mode. If performing mosaic processing in NBG0 or NBG1, the mosaic screen will not be able to use the vertical cell-scroll function. As a result, the mosaic process is done on the display screen for screens that don’t cell-scroll vertically.

| MZSZV3 | MZSZV2 | MZSZV1 | MZSZV0 | Vertical Mosaic Size |   |
| --- | --- | --- | --- | --- | --- |
|   |   |   |   | Non-Interlace | Interlace |
| 0 | 0 | 0 | 0 | 1 dot | 2 dots |
| 0 | 0 | 0 | 1 | 2 dots | 4 dots |
| 0 | 0 | 1 | 0 | 3 dots | 6 dots |
| 0 | 0 | 1 | 1 | 4 dots | 8 dots |
| 0 | 1 | 0 | 0 | 5 dots | 10 dots |
| 0 | 1 | 0 | 1 | 6 dots | 12 dots |
| 0 | 1 | 1 | 0 | 7 dots | 14 dots |
| 0 | 1 | 1 | 1 | 8 dots | 16 dots |
| 1 | 0 | 0 | 0 | 9 dots | 18 dots |
| 1 | 0 | 0 | 1 | 10 dots | 20 dots |
| 1 | 0 | 1 | 0 | 11 dots | 22 dots |
| 1 | 0 | 1 | 1 | 12 dots | 24 dots |
| 1 | 1 | 0 | 0 | 13 dots | 26 dots |
| 1 | 1 | 0 | 1 | 14 dots | 28 dots |
| 1 | 1 | 1 | 0 | 15 dots | 30 dots |
| 1 | 1 | 1 | 1 | 16 dots | 32 dots |

| N0MZE | 180022H | Bit 0 | For NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1MZE | 180022H | Bit 1 | For NBG1 |
| N2MZE | 180022H | Bit 2 | For NBG2 |
| N3MZE | 180022H | Bit 3 | For NBG3 |
| R0MZE | 180022H | Bit 4 | For RBG0 |

| xxMZE | Process |
| --- | --- |
| 0 | Does not execute mosaic process |
| 1 | Processes mosaic process |



<!-- Page 138 -->

(This page was blank in the original Japanese document)



<!-- Page 139 -->

# Chapter 5 Normal Scroll Screen

Introduction.......................................................................... 122

## 5.1 Screen Scroll Function .................................................. 122

Screen Scroll Value Register...................................... 123

## 5.2 Expansion/Contraction Function ................................... 126

Coordinate Increment Register .................................. 127 Reduction Enable Register......................................... 129

## 5.3 Line and Vertical Cell Scroll Function............................ 131

Line Scroll Function .................................................... 131 Vertical Cell Scroll Function........................................ 134 Line and Vertical Cell Scroll Control Register............. 137 Line Scroll Table Address Register............................. 140 Vertical Cell Scroll Table Address Register ................ 141 ST-58-R2



<!-- Page 140 -->

## Introduction

The normal scroll screen has four surfaces, NBG0 to NBG3; each surface can be scrolled up and down, left and right. NBG0 and NBG1 can be expanded and reduced, line scrolled, and cell scrolled vertically.

## 5.1 Screen Scroll Function

All four surfaces of the normal scroll screen can dot scroll up, down, left, or right in surface units. The screen scroll value selects, in the screen scroll value register, the dot coordinates displayed in the upper left of the TV screen. The screen scroll value is in effect up to and including values that don’t exceed the display area set for each screen. The display area of the screen is repeated when a value that exceeds the display area is selected. The fractional part of the screen scroll value for NBG0 and NBG1 is used only in calculating coordinates; the final display coordinate values are discarded. The horizontal (X) coordinate is selected by the horizontal screen scroll value integer part bit and horizontal screen scroll value fractional part bit. The vertical (Y) coordinate is selected by the vertical screen scroll value integer part bit and vertical screen scroll value fractional part bit. The fractional part bit is added immediately below the integer bit. Figure 5.1 shows the bit configuration.

**Figure 5.1 Screen scroll value bit configuration**



<!-- Page 141 -->

### Screen Scroll Value Register

The screen scroll value register designates the screen scroll value. It is a write-only 16- or 32-bit register located at addresses 180070H to 180076H, 180080H to 180086H, and 180090H to 180096H. Because the value is cleared to 0, it must be set after power on or reset.

| SCXIN0 | ~ | ~ | ~ | ~ | ~ | N0SCXI10 | N0SCXI9 | N0SCXI8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180070H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N0SCXI7 | N0SCXI6 | N0SCXI5 | N0SCXI4 | N0SCXI3 | N0SCXI2 | N0SCXI1 | N0SCXI0 |

| SCXDN0 | N0SCXD1 | N0SCXD2 | N0SCXD3 | N0SCXD4 | N0SCXD5 | N0SCXD6 | N0SCXD7 | N0SCXD8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180072H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |

| SCYIN0 | ~ | ~ | ~ | ~ | ~ | N0SCYI10 | N0SCYI9 | N0SCYI8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180074H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N0SCYI7 | N0SCYI6 | N0SCYI5 | N0SCYI4 | N0SCYI3 | N0SCYI2 | N0SCYI1 | N0SCYI0 |

| SCYDN0 | N0SCYD1 | N0SCYD2 | N0SCYD3 | N0SCYD4 | N0SCYD5 | N0SCYD6 | N0SCYD7 | N0SCYD8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180076H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |

| SCXIN1 | ~ | ~ | ~ | ~ | ~ | N1SCXI10 | N1SCXI9 | N1SCXI8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180080H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N1SCXI7 | N1SCXI6 | N1SCXI5 | N1SCXI4 | N1SCXI3 | N1SCXI2 | N1SCXI1 | N1SCXI0 |

| SCXDN1 | N1SCXD1 | N1SCXD2 | N1SCXD3 | N1SCXD4 | N1SCXD5 | N1SCXD6 | N1SCXD7 | N1SCXD8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180082H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |

| SCYIN1 | ~ | ~ | ~ | ~ | ~ | N1SCYI10 | N1SCYI9 | N1SCYI8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180084H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N1SCYI7 | N1SCYI6 | N1SCYI5 | N1SCYI4 | N1SCYI3 | N1SCYI2 | N1SCYI1 | N1SCYI0 |

| SCYDN1 | N1SCYD1 | N1SCYD2 | N1SCYD3 | N1SCYD4 | N1SCYD5 | N1SCYD6 | N1SCYD7 | N1SCYD8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180086H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |



<!-- Page 142 -->

Designates the horizontal and vertical screen scroll values (coordinate values) of the Normal scroll screen.

| SCXN2 | ~ | ~ | ~ | ~ | ~ | N2SCX10 | N2SCX9 | N2SCX8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180090H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N2SCX7 | N2SCX6 | N2SCX5 | N2SCX4 | N2SCX3 | N2SCX2 | N2SCX1 | N2SCX0 |

| SCYN2 | ~ | ~ | ~ | ~ | ~ | N2SCY10 | N2SCY9 | N2SCY8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180092H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N2SCY7 | N2SCY6 | N2SCY5 | N2SCY4 | N2SCY3 | N2SCY2 | N2SCY1 | N2SCY0 |

| SCXN3 | ~ | ~ | ~ | ~ | ~ | N3SCX10 | N3SCX9 | N3SCX8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180094H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N3SCX7 | N3SCX6 | N3SCX5 | N3SCX4 | N3SCX3 | N3SCX2 | N3SCX1 | N3SCX0 |

| SCYN3 | ~ | ~ | ~ | ~ | ~ | N3SCY10 | N3SCY9 | N3SCY8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180096H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N3SCY7 | N3SCY6 | N3SCY5 | N3SCY4 | N3SCY3 | N3SCY2 | N3SCY1 | N3SCY0 |

| N0SCXI10~N0SCXI0 | 180070H | Bit 10~0 | For NBG0 horizontal direction (integer part) |
| --- | --- | --- | --- |
| N0SCXD1~N0SCXD8 | 180072H | Bit 15~8 | For NBG0 horizontal direction (fractional part) |
| N0SCYI10~N0SCYI0 | 180074H | Bit 10~0 | For NBG0 vertical direction (integer part) |
| N0SCYD1~N0SCYD8 | 180076H | Bit 15~8 | For NBG0 vertical direction (fractional part) |
| N1SCXI10~N1SCXI0 | 180080H | Bit 10~0 | For NBG1 horizontal direction (integer part) |
| N1SCXD1~N1SCXD8 | 180082H | Bit 15~8 | For NBG1 horizontal direction (fractional part) |
| N1SCYI10~N1SCYI0 | 180084H | Bit 10~0 | For NBG1 vertical direction (integer part) |
| N1SCYD1~N1SCYD8 | 180086H | Bit 15~8 | For NBG1 vertical direction (fractional part) |
| N2SCX10~N2SCX0 | 180090H | Bit 10~0 | For NBG2 horizontal direction |
| N2SCY10~N2SCY0 | 180092H | Bit 10~0 | For NBG2 vertical direction |
| N3SCX10~N3SCX0 | 180094H | Bit 10~0 | For NBG3 horizontal direction |
| N3SCY10~N3SCY0 | 180096H | Bit 10~0 | For NBG3 vertical direction |



<!-- Page 143 -->

The value of the screen scroll value register is effective up to a range not exceeding the display area of each screen. When the display area is exceeded, the screen of the display area is repeatedly displayed. All screen scroll values must be identified as positive values. By changing the value during the horizontal retrace, the scroll value can also be changed in the middle of the image screen. ST-58-R2



<!-- Page 144 -->

## 5.2 Expansion/Contraction Function

NBG0 and NBG1 can expand and reduce the entire screen both horizontally and vertically. Controlling expansion and reduction is done by selecting horizontal and vertical coordinate increments required in display coordinate calculations. When reducing in horizontally, the reduction enable register must be set as certain screens cannot be displayed, depending on this setting. Display coordinates are calculated by the expressions below.

> Note: the fractional part of the calculated results are discarded.

Screen expansion and reduction are controlled by setting the horizontal and vertical coordinate increments in the coordinate increment register. The horizontal coordinate increment is selected by the horizontal coordinate increment integer part bit and horizontal coordinate increment fractional part bit. The vertical coordinate increment is selected by the vertical coordinate increment integer part bit and horizontal coordinate increment fractional part bit. The fractional part bit is added immediately below the integer bit part. Figure 5.2 shows the bit configuration

**Figure 5.2 Configuration of coordinate increment register.**



<!-- Page 145 -->

### Coordinate Increment Register

The coordinate increment register designates the coordinate increment when calculating the coordinates of the scroll screen. This is a write-only 32-bit register located at addresses 180078H to 18007EH, and 180088H to 18008EH. Because the value of the register is cleared to 0 after power on or reset, the value must be set.

| ZMXIN0 | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180078H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | N0ZMXI2 | N0ZMXI1 | N0ZMXI0 |

| ZMXDN0 | N0ZMXD1 | N0ZMXD2 | N0ZMXD3 | N0ZMXD4 | N0ZMXD5 | N0ZMXD6 | N0ZMXD7 | N0ZMXD8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18007AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |

| ZMYIN0 | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18007CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | N0ZMYI2 | N0ZMYI1 | N0ZMYI0 |

| ZMYDN0 | N0ZMYD1 | N0ZMYD2 | N0ZMYD3 | N0ZMYD4 | N0ZMYD5 | N0ZMYD6 | N0ZMYD7 | N0ZMYD8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18007EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |

| ZMXIN1 | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180088H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | N1ZMXI2 | N1ZMXI1 | N1ZMXI0 |

| ZMXDN1 | N1ZMXD1 | N1ZMXD2 | N1ZMXD3 | N1ZMXD4 | N1ZMXD5 | N1ZMXD6 | N1ZMXD7 | N1ZMXD8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18008AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |

| ZMYIN1 | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18008CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | N1ZMYI2 | N1ZMYI1 | N1ZMYI0 |

| ZMYDN1 | N1ZMYD1 | N1ZMYD2 | N1ZMYD3 | N1ZMYD4 | N1ZMYD5 | N1ZMYD6 | N1ZMYD7 | N1ZMYD8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18008EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| E | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |



<!-- Page 146 -->

Designates horizontal and vertical coordinate increments for calculating display coordinates when expanding and reducing all Normal scroll screens. The coordinate increment should be a value smaller than “1” in the expansion display, and larger than “1” in the reduction display. The normal display is when the coordinate increment is 1. Selections are all by positive values. The coordinate parts of NBG2 and NBG3 are fixed at “1”. By changing the value during the horizontal retrace, the coordinate increment value can also be changed. The reduction enable register must be set when reduction display is horizontal. Depending on the setting of the reduction enable bit, do not set horizontal coordinate increment to a value other than the set range decided upon.

**Table 5.1 shows**

coordinate increments and reduction settings in the horizontal direction.

**Table 5.1 Horizontal coordinate increments and reduction settings**

| N0ZMXI2~N0ZMXI0 | 180078H | Bit 2~0 | For NBG0 horizontal direction (integer part) |
| --- | --- | --- | --- |
| N0ZMXD1~N0ZMXD8 | 18007AH | Bit 15~8 | For NBG0 horizontal direction (fractional part) |
| N0ZMYI2~N0ZMYI0 | 18007CH | Bit 2~0 | For NBG0 vertical direction (integer part) |
| N0ZMYD1~N0ZMYD8 | 18007EH | Bit 15~8 | For NBG0 vertical direction (fractional part) |
| N1ZMXI2~N1ZMXI0 | 180088H | Bit 2~0 | For NBG1 horizontal direction (integer part) |
| N1ZMXD1~N1ZMXD8 | 18008AH | Bit 15~8 | For NBG1 horizontal direction (fractional part) |
| N1ZMYI2~N1ZMYI0 | 18008CH | Bit 2~0 | For NBG1 vertical direction (integer part) |
| N1ZMYD1~N1ZMYD8 | 18008EH | Bit 15~8 | For NBG1 vertical direction (fractional part) |

| Horizontal Reduction Display Setting | Horizontal Coordinate Increment Setting Range |
| --- | --- |
| Not allowed | 0 £ (Horizontal Coordinate Increment) £ 1 |
| Up to 1/2 | 0 £ (Horizontal Coordinate Increment) £ 2 |
| Up to 1/4 | 0 £ (Horizontal Coordinate Increment) £ 4 |



<!-- Page 147 -->

### Reduction Enable Register

The reduction enable register is a write-only 16 bit register that controls the horizontal reduction display, and is located at address 180098H. Because the value of the register is cleared to 0 after power on or reset, the value must be set. Designates the maximum reducible range of each Normal scroll screen in the horizontal direction.

> Note: 0 or 1 is entered in bit name for x.

For reduction of up to 1/2, set the corresponding screen character color count (bit map pattern color count) to 16 or 256 colors. For reduction of up to 1/4, set to 16 colors. The horizontal coordinate increment should not exceed the set range of these bits.

| ZMCTL | ~ | ~ | ~ | ~ | ~ | ~ | N1ZMQT | N1ZMHF |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180098H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | N0ZMQT | N0ZMHF |

| N0ZMHF | 180098H | Bit 0 | For NBG0 |
| --- | --- | --- | --- |
| N0ZMQT | 180098H | Bit 1 | For NBG0 |
| N1ZMHF | 180098H | Bit 8 | For NBG1 |
| N1ZMQT | 180098H | Bit 9 | For NBG1 |

| NxZMQT | NxZMHF | Horizontal Reduction Display | Restriction Items |
| --- | --- | --- | --- |
| 0 | 0 | Not allowed | None |
| 0 | 1 | Up to 1/2 | The number of character colors can b<br>set for 16 or 256 colors only. |
| 1 | 0 | Up to 1/4 | The number of character colors can b<br>set for 16 colors only. |
| 1 | 1 | Up to 1/4 | The number of character colors can b<br>set for 16 colors only. |



<!-- Page 148 -->

Certain screens cannot display depending on the reduction setting. Limits are shown in

**Table 5.2.**

**Table 5.2**

| Screen | Character Color Count<br>(Bitmap Color Count) | Reduction Enable Setting | Screens That Cannot<br>Display |
| --- | --- | --- | --- |
|   | 16 Colors | Up to 1/2 | None |
| NBG0 |   | Up to 1/4 | NBG2 |
|   | 256 Colors | Up to 1/2 | NBG2 |
|   | 16 Colors | Up to 1/2 | None |
| NBG1 |   | Up to 1/4 | NBG3 |
|   | 256 Colors | Up to 1/2 | NBG3 |



<!-- Page 149 -->

## 5.3 Line and Vertical Cell Scroll Function

Within the Normal scroll screen, there is a line scroll function and vertical cell scroll function in NBG0 and NBG1. The line scroll function selects the horizontal and vertical screen scroll value and horizontal coordinate increment in line units. The vertical cell scroll function selects the vertical screen scroll value in horizontal cell units. Both functions can be used without relationship to the cell format and bit map format.

### Line Scroll Function

The line scroll function selects the horizontal and vertical screen scroll value and horizontal coordinate increment in line units, and specifies by line scroll tables stored in VRAM. Data values of line scroll tables are designated by relative values. To values stored in line scroll tables, values selected in the screen scroll value register are added, becoming display coordinates. The table data read interval can be selected from four types, one line, two line, four line, and eight line. Values of the vertical coordinate increment register are used for vertical coordinate calculations when two-line intervals or greater are selected. The horizontal coordinate increment should not set a value that the exceeds the setting of the reduction enable register. The line scroll function is shown in Figure 5.3.

**Figure 5.3 Line scroll function**

Line scroll tables store from small addresses in order of the horizontal screen scroll value, vertical screen scroll value, and horizontal coordinate increments. Stored line scroll data is only composed of data required by the line scroll register setting.

| Horizontal Screen Scroll value<br>for 1st line |
| --- |
| Vertical Screen Scroll value for<br>1st line |
| Horiz. Coordinate<br>increment for 1st line |
| Horizontal Screen Scroll value<br>for 2nd line |
| Vertical Screen Scroll value<br>for 2nd line |
| Horiz. Coordinate<br>increment for 2nd line |



<!-- Page 150 -->

Each horizontal screen scroll value, vertical screen scroll value, and horizontal coordinate increment configuration is identical to the data configuration set in each register. Figure 5.4 shows the bit configuration of line scroll table data.

**Table 5.5**

shows the configuration of line scroll tables.

> Note: Shaded areas are ignored

**Figure 5.4 Bit configuration of line scroll table data**

|   | Integer Part : 11 bits |
| --- | --- |

| Fractional Part : 8 btis |
| --- |

| Fractional Part : 8 bits | f |
| --- | --- |



<!-- Page 151 -->

> Note: Display coordinates in the vertical direction for lines not

**Figure 5.5 Line scroll table**

| Line 1 Horiz. Screen Scroll Value (Integer Part) |
| --- |
| Line 1 Horiz. Screen Scroll Value (Fractional Part) |
| Line 1 Vertical Screen Scroll Value (Integer Part) |
| Line 1 Vertical Screen Scroll Value (Fractional Part) |
| Line 1 Horiz. Coordinate Increment (Integer Part) |
| Line 1 Horiz. Coordinate Increment (Fractional Part) |
| Line 2 Horiz. Screen Scroll Value (Integer Part) |
| Line 2 Horiz. Screen Scroll Value (Fractional Part) |
| Line 2 Vertical Screen Scroll Value (Integer Part) |
| Line 2 Vertical Screen Scroll Value (Fractional Part) |
| Line 2 Horiz. Coordinate Increment (Integer Part) |
| Line 2 Horiz. Coordinate Increment (Fractional Part) |

| Line 1 Vertical Screen Scroll Value (Integer Part) |
| --- |
| Line 1 Vertical Screen Scroll Value (Fractional Part) |
| Line 1, 2 Horiz. Coordinate Increment (Integer Part) |
| Line 1, 2 Horiz. Coordinate Increment (Fractional Part) |
| Line 3 Vertical Screen Scroll Value (Integer Part) |
| Line 3 Vertical Screen Scroll Value (Fractional Part) |
| Line 3, 4 Horiz. Coordinate Increment (Integer Part) |
| Line 3, 4 Horiz. Coordinate Increment (Fractional Part) |

| Line 1~4 Horiz. Screen Scroll Value (Integer Part) |
| --- |
| Line 1~4 Horiz. Screen Scroll Value (Fractional Part) |
| Line 1~4 Horiz. Coordinate Increment (Integer Part) |
| Line 1~4 Horiz. Coordinate Increment (Fractional Part) |
| Line 5~8 Horiz. Screen Scroll Value (Integer Part) |
| Line 5~8 Horiz. Screen Scroll Value (Fractional Part) |
| Line 5~8 Lines Horiz. Coordinate Increment (Integer Part) |
| Line 5~8 Lines Horiz. Coordinate Increment (Fractional Part) |



<!-- Page 152 -->

### Vertical Cell Scroll Function

The vertical cell scroll function selects the vertical screen scroll value in horizontal cell units in each vertically separated area, and is selected by the vertical cell scroll table stored in VRAM. The data value of the vertical cell scroll table is designated by relative values. The value selected by the screen scroll value register is added to the screen scroll value stored in the vertical cell scroll table, becoming the display coordinate. Selection can be done in horizontal 8 dot units when displaying in bit map format. NBG0 and NBG1 have the only vertical cell scroll functions inside the Normal scroll screen. This vertical cell scroll function and mosaic function can not be used simultaneously; the mosaic function has priority. Figure 5.6 shows the vertical cell scroll function.

**Figure 5.6 Vertical cell scroll function**

The bit configuration of the vertical screen scroll value is the same when set in all registers. Data of the vertical cell scroll table is treated as a table in the order from data in the left side cell of the TV screen. When both NBG0 and NBG1 use the vertical cell scroll function, the various vertical cell scroll table data should be alternately stored in NBG0 and NBG1, one cell at a time.

| 1st Cell Vertica l<br>Screen Scroll Value |
| --- |
| 2nd Cell Vertical<br>Screen Scroll Value |
| 3rd Cell Vertical<br>Screen Scroll Value |

|   |   |   |   |   | o |
| --- | --- | --- | --- | --- | --- |
|   |   |   |   |   | C |



<!-- Page 153 -->

**Figure 5.7 shows the bit configuration of the vertical cell scroll table data. Figure 5.8**

shows the vertical cell scroll table configuration.

> Note: Shaded area is ignored

**Figure 5.7 Data configuration on the vertical cell scroll table**

|   | 11 bit integer part |
| --- | --- |

| 8 bit fractional part | n |
| --- | --- |



<!-- Page 154 -->

**Figure 5.8 Vertical cell scroll table**

| NBG0 1st cell vertical screen scroll value (integer part) |
| --- |
| NBG0 1st cell vertical screen scroll value (fractional part) |
| NBG0 2nd cell vertical screen scroll value (integer part) |
| NBG0 2nd cell vertical screen scroll value (fractional part) |
| NBG0 3rd cell vertical screen scroll value (integer part) |
| NBG0 3rd cell vertical screen scroll value (fractional part) |
| NBG0 4th cell vertical screen scroll value (integer part) |
| NBG0 4th cell vertical screen scroll value (fractional part) |
| NBG0 5th cell vertical screen scroll value (integer part) |
| NBG0 5th cell vertical screen scroll value (fractional part) |

| NBG1 1st cell vertical screen scroll value (integer part) |
| --- |
| NBG1 1st cell vertical screen scroll value (fractional part) |
| NBG1 2nd cell vertical screen scroll value (integer part) |
| NBG1 2nd cell vertical screen scroll value (fractional part) |
| NBG1 3rd cell vertical screen scroll value (integer part) |
| NBG1 3rd cell vertical screen scroll value (fractional part) |
| NBG1 4th cell vertical screen scroll value (integer part) |
| NBG1 4th cell vertical screen scroll value (fractional part) |
| NBG1 5th cell vertical screen scroll value (integer part) |
| NBG1 5th cell vertical screen scroll value (fractional part) |

| NBG0 1st cell vertical screen scroll value (integer part) |
| --- |
| NBG0 1st cell vertical screen scroll value (fractional part) |
| NBG1 1st cell vertical screen scroll value (integer part) |
| NBG1 1st cell vertical screen scroll value (fractional part) |
| NBG0 2nd cell vertical screen scroll value (integer part) |
| NBG0 2nd cell vertical screen scroll value (fractional part) |
| NBG1 2nd cell vertical screen scroll value (integer part) |
| NBG1 2nd cell vertical screen scroll value (fractional part) |
| NBG0 3rd cell vertical screen scroll value (integer part) |
| NBG0 3rd cell vertical screen scroll value (fractional part) |



<!-- Page 155 -->

### Line and Vertical Cell Scroll Control Register

The line and vertical cell scroll control register is a write-only 16-bit register that controls the line scroll function and vertical cell scroll function, and is at address 18009AH. Because the value of the register is cleared to 0 after the power is turned on or reset, the value must be set. Designates the interval that reads line scroll table data from the table. The interval changes depending on the interlace of the TV screen.

> Note: 0 or 1 is entered in bit name for x.

When reading line scroll table data at intervals of two lines or greater, line horizontal scroll screen value not read and horizontal coordinate increments use line scroll data that has been previously read. The vertical scroll screen value is calculated from vertical coordinate increment register value and line scroll data that was previously read.

| SCRCTL | ~ | ~ | N1LSS1 | N1LSS0 | N1LZMX | N1LSCY | N1LSCX | N1VCSC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18009AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N0LSS1 | N0LSS0 | N0LZMX | N0LSCY | N0LSCX | N0VCSC |

| N0LSS1, N0LSS0 | 18009AH | Bit 5, 4 | For NBG0 |
| --- | --- | --- | --- |
| N1LSS1, N1LSS0 | 18009AH | Bit 13,12 | For NBG1 |

| NxLSS1 | NxLSS0 | Interlace Setting |   |   |
| --- | --- | --- | --- | --- |
|   |   | Non-Interlace | Single-Density Interlace | Double-Density Interlace |
| 0 | 0 | Each line | Every 2 lines | Each line |
| 0 | 1 | Every 2 lines | Every 4 lines | Every 2 lines |
| 1 | 0 | Every 4 lines | Every 8 lines | Every 4 lines |
| 1 | 1 | Every 8 lines | Every 16 lines | Every 8 lines |



<!-- Page 156 -->

Designates whether expansion-reduction is done horizontally in line units.

> Note: 0 or 1 is entered in bit name for x.

When using this function, the horizontal coordinate increment must be stored in the line scroll table of VRAM. Make sure that the horizontal coordinate increment does not exceed the reduction setting. Designates whether scroll is performed by vertical line units.

> Note: 0 or 1 is entered in bit name for x.

When using this function, the vertical screen scroll value must be stored in the line scroll table of VRAM.

| N0LZMX | 18009AH | Bit 3 | For NBG0 |
| --- | --- | --- | --- |
| N1LSCX | 18009AH | Bit 11 | For NBG1 |

| NxLZMX | Process |
| --- | --- |
| 0 | Does not scale horizontally per line units |
| 1 | Scales horizontally per line units |

| N0LSCY | 18009AH | Bit 2 | For NBG0 |
| --- | --- | --- | --- |
| N1LSCY | 18009AH | Bit 10 | For NBG1 |

| NxLSCY | Process |
| --- | --- |
| 0 | Does not scroll vertically per line units |
| 1 | Scrolls vertically per line units |



<!-- Page 157 -->

Designates whether scroll is performed by horizontal line units.

> Note: 0 or 1 is entered in bit name for x.

When using this function, be sure to store the horizontal scroll screen value in the VRAM line scroll table. Designates whether to perform vertical cell scroll.

> Note: 0 or 1 is entered in bit name for x.

When using the vertical cell scroll function, make sure the access command of the vertical cell scroll table data read is designated in the VRAM cycle pattern register. In addition, vertical cell scroll data must be stored in VRAM. The vertical cell scroll function cannot be used simultaneously with the mosaic function; the mosaic function has priority.

| N0LSCX | 18009AH | Bit 1 | For NBG0 |
| --- | --- | --- | --- |
| N1LSCX | 18009AH | Bit 9 | For NBG1 |

| NxLSCX | Process |
| --- | --- |
| 0 | Does not scroll horizontally per line units |
| 1 | Scrolls horizontally per line units |

| N0VCSC | 18009AH | Bit 0 | For NBG0 |
| --- | --- | --- | --- |
| N1VCSC | 18009AH | Bit 8 | For NBG1 |

| NxVCSC | Process |
| --- | --- |
| 0 | Does not cell-scroll vertically |
| 1 | Cell-scrolls vertically |



<!-- Page 158 -->

### Line Scroll Table Address Register

The line scroll table address register is a write-only 32-bit register that selects the lead address of the line scroll table, and is at addresses 1800A0H to 1800A6H. Because the value of the register is cleared to 0 after power on or reset, the value must be set. Designates the lead address of the line scroll table on the VRAM. The actual lead VRAM address is calculated by the expression below. When the VRAM has a 4 Mbit capacity, the address of the most significant bit is ignored.

| LSTA0U | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800A0H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | N0LSTA18 | N0LSTA17 | N0LSTA16 |

| LSTA0L | N0LSTA15 | N0LSTA14 | N0LSTA13 | N0LSTA12 | N0LSTA11 | N0LSTA10 | N0LSTA9 | N0LSTA8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800A2H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
