# Chapter 8 Windows

8.1 Window Area .........................................................180 Normal Rectangular Window .............................180 Window Position Register ..................................181 Normal Line Window ..........................................184 Line Window Table Address Register ................186 Sprite Window ....................................................187 Sprite Control Register .......................................188 Window Active Area in the Screen ..................... 189 8.2 Window Process ...................................................190 Window Process Register ..................................193 ST-58-R2



<!-- Page 198 -->

## 8.1 Window Area

The scroll IC window has two Normal windows, W0 and W1, and one sprite window, SW. The Normal window selects start and end coordinates on the TV screen; the sprite window designates the most significant bit value of sprite data written to the frame buffer. Various windows can designate which scroll screen is to be put into effect, and whether the inside or outside of the area will go into effect. Moreover, when more than one window is used, they can be overlapped other by AND or OR logic. The Normal window selects the Normal rectangular window designated through the horizontal and vertical start and end coordinates, and selects the Normal line window designated through horizontal start and end coordinates in each line. The start and end coordinates set the coordinate values on the TV screen in each register, and not on the scroll screen.

### Normal Rectangular Window

The normal rectangular window is obtained by selecting the start coordinates in the upper left corner in the window position register, and the end coordinates in the lower right corner of the window. The area surrounded by selected coordinates is inside, the rest of area is outside. The border line of the window is considered part of the inside. If the start coordinate of either the horizontal or vertical direction is larger than the end coordinate, then the whole screen is considered an area outside the window.

**Figure 8.1 shows the Normal rectangular window.**

**Figure 8.1**

|   |   |   |   |   |   |   |   | A |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |



<!-- Page 199 -->

### Window Position Register

The window position register is a write-only 16-bit register that selects the horizontal and vertical start and end coordinates of the Normal window, and is located from addresses 1800C0H through 1800CEH. Because the value is cleared to 0, it must be set after power on or reset.

| WPSX0 | ~ | ~ | ~ | ~ | ~ | ~ | W0SX9 | W0SX8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800C0H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W0SX7 | W0SX6 | W0SX5 | W0SX4 | W0SX3 | W0SX2 | W0SX1 | W0SX0 |

| WPSY0 | ~ | ~ | ~ | ~ | ~ | ~ | ~ | W0SY8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800C2H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W0SY7 | W0SY6 | W0SY5 | W0SY4 | W0SY3 | W0SY2 | W0SY1 | W0SY0 |

| WPEX0 | ~ | ~ | ~ | ~ | ~ | ~ | W0EX9 | W0EX8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800C4H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W0EX7 | W0EX6 | W0EX5 | W0EX4 | W0EX3 | W0EX2 | W0EX1 | W0EX0 |

| WPEY0 | ~ | ~ | ~ | ~ | ~ | ~ | ~ | W0EY8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800C6H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W0EY7 | W0EY6 | W0EY5 | W0EY4 | W0EY3 | W0EY2 | W0EY1 | W0EY0 |

| WPSX1 | ~ | ~ | ~ | ~ | ~ | ~ | W1SX9 | W1SX8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800C8H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W1SX7 | W1SX6 | W1SX5 | W1SX4 | W1SX3 | W1SX2 | W1SX1 | W1SX0 |

| WPSY1 | ~ | ~ | ~ | ~ | ~ | ~ | ~ | W1SY8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800CAH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W1SY7 | W1SY6 | W1SY5 | W1SY4 | W1SY3 | W1SY2 | W1SY1 | W1SY0 |

| WPEX1 | ~ | ~ | ~ | ~ | ~ | ~ | W1EX9 | W1EX8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800CCH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W1EX7 | W1EX6 | W1EX5 | W1EX4 | W1EX3 | W1EX2 | W1EX1 | W1EX0 |

| WPEY1 | ~ | ~ | ~ | ~ | ~ | ~ | ~ | W1EY8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800CEH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W1EY7 | W1EY6 | W1EY5 | W1EY4 | W1EY3 | W1EY2 | W1EY1 | W1EY0 |



<!-- Page 200 -->

Designates the horizontal start and end coordinates. Designated coordinate value is the coordinate value (H counter value) on the TV screen. The bit configuration of the register changes according to the setting of the graphics mode. For normal graphics, the least significant bit becomes invalid data. For exclusive normal graphics, the most significant bit becomes invalid data; moreover, for special high-resolution graphics, the most significant bit becomes invalid data. Since it doesn’t have an HO bit, values are in 2 pixel units.

Table 8.1 shows the bit

content of the window position register by graphic mode setting.

**Table 8.1 Bit content of window position register for horizontal coordinates**

> Note: 0S, 0E, 1S, or 1E is entered in bit name for xx.

Designates the vertical start and end coordinates. The designated coordinate value is the coordinate value (V counter value) on the TV screen. The bit configuration of the register changes according to the screen mode setting. Single-density interlace of Normal and high-resolution modes designate the V counter value in the respective even-numbered and odd-numbered fields.

| W0SX9~W0SX0 | 1800C0H | Bit 9~0 | For W0 start point coordinates |
| --- | --- | --- | --- |
| W0EX9~W0EX0 | 1800C4H | Bit 9~0 | For W0 end point coordinates |
| W1SX9~W1SX0 | 1800C8H | Bit 9~0 | For W1 start point coordinates |
| W1EX9~W1EX0 | 1800CCH | Bit 9~0 | For W1 end point coordinates |

| Graphics<br>Mode | WxxX9 | WxxX8 | WxxX7 | WxxX6 | WxxX5 | WxxX4 | WxxX3 | WxxX2 | WxxX1 | WxxX0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Normal | H8 | H7 | H6 | H5 | H4 | H3 | H2 | H1 | H0 | Invalid |
| Hi-Res | H9 | H8 | H7 | H6 | H5 | H4 | H3 | H2 | H1 | H0 |
| Exclusive<br>Normal | Invalid | H8 | H7 | H6 | H5 | H4 | H3 | H2 | H1 | H0 |
| Exclusive<br>Hi-Res | Invalid | H9 | H8 | H7 | H6 | H5 | H4 | H3 | H2 | H1 |

| W0SY8~W0SY0 | 1800C2H | Bit 8~0 | For W0 start point coordinates |
| --- | --- | --- | --- |
| W0EY8~W0EY0 | 1800C6H | Bit 8~0 | For W0 end point coordinates |
| W1SY8~W1SY0 | 1800CAH | Bit 8~0 | For W1 start point coordinates |
| W1EY8~W1EY0 | 1800CEH | Bit 8~0 | For W1 end point coordinates |



<!-- Page 201 -->

The lowest significant bit is invalid for the double-density interlace of Normal and high-resolution modes. Remaining bits designate the V counter value in various fields. Bit content of the window position register by setting of the screen mode is shown in

**Table 8.2.**

**Table 8.2**

> Note: 0S, 0E, 1S or 1E is entered in bit name for xx.

| TV Screen<br>(Interlace) Mode | WxxY8 | WxxY7 | WxxY6 | WxxY5 | WxxY4 | WxxY3 | WxxY2 | WxxY1 | WxxY0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Normal, Hi-Res<br>(Non-interlace,<br>Single-Density<br>Interlace) | V8 | V7 | V6 | V5 | V4 | V3 | V2 | V1 | V0 |
| Normal, Hi-Res<br>(Double-Density<br>Interlace) | V7 | V6 | V5 | V4 | V3 | V2 | V1 | V0 | Invalid |
| Exclusive Monitor | V8 | V7 | V6 | V5 | V4 | V3 | V2 | V1 | V0 |



<!-- Page 202 -->

### Normal Line Window

The Normal line window stores the horizontal start and end coordinates of each window line as a table in VRAM, and is obtained by designating the vertical start and end coordinates in the window position register. The area surrounded by selected coordinates is inside, the rest of the area is outside. The border line of the window is considered part of the inside. The Normal line window is illustrated in

**Figure 8.2.**

The bit configuration of data stored in the line window table of horizontal start and end coordinates is shown in Figure 8.3. Coordinates in each line can be selected in the non-interlace and double-density interlace modes, and in the single-density interlace mode for each two lines. Configuration of the Normal line window table is shown in Figure 8.4. If the start coordinate of either the horizontal or vertical direction is larger than the end coordinate, then the whole screen is considered an area outside the window.

**Figure 8.2 Normal line window**

> Note: Shaded areas are ignored

**Figure 8.3 Bit configuration of Normal line window table data**

| Outside<br>Window<br>Inside |
| --- |

| G | Horizontal Start Point Coordinates (10 bits) |
| --- | --- |

|   | Horizontal End Point Coordinates (10 bits) |
| --- | --- |



<!-- Page 203 -->

> Note: In the case of double-density interlace, store line data of

**Figure 8.4 Configuration of Normal line window table**

| 1st line horizontal start point coordinates |
| --- |
| 1st line horizontal end point coordinates |
| 2nd line horizontal start point coordinates |
| 2nd line horizontal end point coordinates |
| 3rd line horizontal start point coordinates |
| 3rd line horizontal end point coordinates |

| 1st & 2nd line horizontal start point coordinates |
| --- |
| 1st & 2nd line horizontal end point coordinates |
| 3rd & 4th line horizontal start point coordinates |
| 3rd & 4th line horizontal end point coordinates |
| 5th & 6th line horizontal start point coordinates |
| 5th & 6th line horizontal end point coordinates |



<!-- Page 204 -->

### Line Window Table Address Register

The line window table address register is a write-only 16-bit register that designates whether to make the Normal window the line window, as well as the lead address of that table. It is located from addresses 1800D8H through 1800DEH. Because the value is cleared to 0, it must be set after power on or reset. Designates whether to make the Normal window a line window.

> Note: 0 or 1 is entered in bit name for x.

When this bit is “1”, the line window table must be stored in VRAM.

| LWTA0U | W0LWE | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800D8H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | W0LWTA18 | W0LWTA17 | W0LWTA16 |

| LWTA0L | W0LWTA15 | W0LWTA14 | W0LWTA13 | W0LWTA12 | W0LWTA11 | W0LWTA10 | W0LWTA9 | W0LWTA8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800DAH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W0LWTA7 | W0LWTA6 | W0LWTA5 | W0LWTA4 | W0LWTA3 | W0LWTA2 | W0LWTA1 | ~ |

| LWTA1U | W1LWE | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800DCH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | W1LWTA18 | W1LWTA17 | W1LWTA16 |

| LWTA1L | W1LWTA15 | W1LWTA14 | W1LWTA13 | W1LWTA12 | W1LWTA11 | W1LWTA10 | W1LWTA9 | W1LWTA8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800DEH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | W1LWTA7 | W1LWTA6 | W1LWTA5 | W1LWTA4 | W1LWTA3 | W1LWTA2 | W1LWTA1 | ~ |

| W0LWE | 1800D8H | Bit 15 | For W0 |
| --- | --- | --- | --- |
| W1LWE | 1800DCH | Bit 15 | For W1 |

| WxLWE | Process |
| --- | --- |
| 0 | Does not process Normal Window to Line Window |
| 1 | Processes Normal Window to Line Window |



<!-- Page 205 -->

Designates the lead address of the line window table in VRAM. The actual lead address is calculated by the expression below. The most significant bit of the address is ignored when VRAM is 4 Mbits.

### Sprite Window

The sprite window is obtained by selecting the most significant bit of data when all frame buffer data of the sprite is palette format data and sprite types are 2 through 7. The most significant single bit is inside, and the rest of the area is outside. For more about sprite types see “Sprite types” in section “9.1 Sprite Data.” Figure 8.5 shows a sprite window.

**Figure 8.5 Sprite Window**

| W0LWTA18~W0LWTA16 | 1800D8H | Bit 2~0 | For W0 |
| --- | --- | --- | --- |
| W0LWTA15~W0LWTA1 | 1800DAH | Bit 15~1 | For W0 |
| W1LWTA18~W1LWTA16 | 1800DCH | Bit 2~0 | For W1 |
| W1LWTA15~W1LWTA1 | 1800DEH | Bit 15~1 | For W1 |

| 1 | 1 | 0 | 0 | 0 | 0 | 1 | 1 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | 1 | 0 | 0 | 1 | 1 | 1 |
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| 1 | 1 | 1 | 0 | 0 | 1 | 1 | 1 |
| 1 | 1 | 0 | 0 | 0 | 0 | 1 | 1 |



<!-- Page 206 -->

### Sprite Control Register

The sprite control register controls sprites. This is a write-only 16-bit register that is at address 1800E0H. Because the value is cleared to 0, it must be set after power on or reset. See “9.2 Priority and Color Calculation” See “9.2 Priority and Color Calculation” See “9.1 Sprite Data” Designates whether to use the sprite window SW. This bit is only effective when the sprite color mode is mode 0, and for only sprites 2 to 7. WHen this bit is “1”, the most significant bit of the sprite frame buffer is used as the bit for the sprite window. As a result, MSB shadow can no longer be used. For more about shadows see “14.1 Shadow Process.” Do not set this bit to 1, when setting SPCLMD bit to 1. See “9.1 Sprite Data”

| SPCTL | ~ | ~ | SPCCCS1 | SPCCCS0 | ~ | SPCCN2 | SPCCN1 | SPCCN0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800E0H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | SPCLMD | SPWINEN | SPTYPE3 | SPTYPE2 | SPTYPE1 | SPTYPE0 |

| SPWINEN | Process |
| --- | --- |
| 0 | Does not use sprite window |
| 1 | Uses sprite window |



<!-- Page 207 -->

### Window’s Active Area for the Screen

Normal and sprite windows can designate whether to use a window in each scroll screen. The window being used can select inside or outside, in each window, as well as perform color calculation and transparent processes in active areas. When using multiple windows, the method of overlap can be selected from AND or OR logic.

**Figure 8.6 shows the active area when normal and sprite windows are overlaid by**

AND or OR logic.

- OR
- AND
**Figure 8.6 Active area of window**



<!-- Page 208 -->

## 8.2 Window Process

The three window processes are: 1. Transparency process window 2. Rotation parameter window 3. Color calculation window The transparency process window forces the selected window effective area to be transparent, and can be used in each screen. When displaying the RBG0 screen, the rotation parameter window designates the area displaying the image obtained by rotation parameter A, and designates which image obtained by rotation parameter B is displayed. Images obtained by rotation parameter B are displayed in the active area of the designated window; images obtained by rotation parameter A are displayed outside the window’s active area. The color calculation window is a window in which color calculation in the active area of the designated window is not performed, and is effective for screens using the color calculation function.



<!-- Page 209 -->

Window process is shown in Figure 8.7.

**Figure 8.7 Window Process**

| Transparent |
| --- |

| ABCDEFGHIJKLMNOPQ<br>RSTUVWXYZABCDEFGH |
| --- |
| IJKLMNOPQRSTUVWXY<br>ZABCDEFGHIJKLMNOP<br>QRSTUVWXYZABCDEFG<br>HIJKLMNOPQRSTUVWX |



<!-- Page 210 -->

**Figure 8.7 Window Process (continued)**

| Transparent |
| --- |
| Screen that uses<br>Color Calculation<br>Function |



<!-- Page 211 -->

### Window Control Register

Window control register designates the method for using windows in each screen, and is a write-only 16-bit register that is located from addresses 1800D0H through 1800D6H. Because the value is cleared to 0, it must be set after power on or reset. Designates the method of overlapping windows used in each screen.

| WCTLA | N1LOG | ~ | N1SWE | N1SWA | N1W1E | N1W1A | N1W0E | N1W0A |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800D0H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N0LOG | ~ | N0SWE | N0SWA | N0W1E | N0W1A | N0W0E | N0W0A |

| WCTLB | N3LOG | ~ | N3SWE | N3SWA | N3W1E | N3W1A | N3W0E | N3W0A |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800D2H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N2LOG | ~ | N2SWE | N2SWA | N2W1E | N2W1A | N2W0E | N2W0A |

| WCTLC | SPLOG | ~ | SPSWE | SPSWA | SPW1E | SPW1A | SPW0E | SPW0A |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800D4H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | R0LOG | ~ | R0SWE | R0SWA | R0W1E | R0W1A | R0W0E | R0W0A |

| WCTLD | CCLOG | ~ | CCSWE | CCSWA | CCW1E | CCW1A | CCW0E | CCW0A |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800D6H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | RPLOG | ~ | ~ | ~ | RPW1E | RPW1A | RPW0E | RPW0A |

| N0LOG | 1800D0H | Bit 7 | Transparent Process Window for NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1LOG | 1800D0H | Bit 15 | Transparent Process Window for NBG1 (or EXBG) |
| N2LOG | 1800D2H | Bit 7 | Transparent Process Window for NBG2 |
| N3LOG | 1800D2H | Bit 15 | Transparent Process Window for NBG3 |
| R0LOG | 1800D4H | Bit 7 | Transparent Process Window for RBG0 |
| SPLOG | 1800D4H | Bit 15 | Transparent Process Window for Sprite |
| RPLOG | 1800D6H | Bit 7 | For Rotation Parameter Window |
| CCLOG | 1800D6H | Bit 15 | For Color Calculation Window |



<!-- Page 212 -->

> Note: N0, N1, N2, N3, R0, SP, RP or CC is entered in bit name for xx.

When W0, W1, and SW window enable bits are all 0, with this bit set to 0, the whole screen will be window disabled area, and with this bit set to 1, the whole screen will become window enabled area. Designates whether to use the Normal window W0 in each screen.

> Note: N0, N1, N2, N3, R0, SP, RP, or CC is entered in bit name for xx.

Designates whether to use the Normal window W1 in each screen.

| xxLOG | Overlaid Logic |
| --- | --- |
| 0 | OR |
| 1 | AND |

| N0W0E | 1800D0H | Bit 1 | Transparent Process Window for NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1W0E | 1800D0H | Bit 9 | Transparent Process Window for NBG1 (or EXBG) |
| N2W0E | 1800D2H | Bit 1 | Transparent Process Window for NBG2 |
| N3W0E | 1800D2H | Bit 9 | Transparent Process Window for NBG3 |
| R0W0E | 1800D4H | Bit 1 | Transparent Process Window for RBG0 |
| SPW0E | 1800D4H | Bit 9 | Transparent Process Window for Sprite |
| RPW0E | 1800D6H | Bit 1 | For Rotation Parameter Window |
| CCW0E | 1800D6H | Bit 9 | For Color Calculation Window |

| xxW0E | Process |
| --- | --- |
| 0 | Does not use W0 window |
| 1 | Uses W0 window |



<!-- Page 213 -->

> Note: N0, N1, N2, N3, R0, SP, RP, or CC is entered in bit name for xx.

Designates whether to use the sprite window SW in each screen.

> Note: N0, N1, N2, N3, R0, SP, or CC is entered in bit name for xx.

When using the sprite window, set the sprite window enable bit (SPWINEN) of the sprite control register to 1. Designates the valid area of the Normal window W0 used in each screen.

| N0W1E | 1800D0H | Bit 3 | Transparent Process Window for NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1W1E | 1800D0H | Bit 11 | Transparent Process Window for NBG1 (or EXBG) |
| N2W1E | 1800D2H | Bit 3 | Transparent Process Window for NBG2 |
| N3W1E | 1800D2H | Bit 11 | Transparent Process Window for NBG3 |
| R0W1E | 1800D4H | Bit 3 | Transparent Process Window for RBG0 |
| SPW1E | 1800D4H | Bit 11 | Transparent Process Window for Sprite |
| RPW1E | 1800D6H | Bit 3 | For Rotation Parameter Window |
| CCW1E | 1800D6H | Bit 11 | For Color Calculation Window |

| xxW1E | Process |
| --- | --- |
| 0 | Does not use W1 window |
| 1 | Uses W1 window |

| N0SWE | 1800D0H | Bit 5 | Transparent Process Window for NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1SWE | 1800D0H | Bit 13 | Transparent Process Window for NBG1 (or EXBG) |
| N2SWE | 1800D2H | Bit 5 | Transparent Process Window for NBG2 |
| N3SWE | 1800D2H | Bit 13 | Transparent Process Window for NBG3 |
| R0SWE | 1800D4H | Bit 5 | Transparent Process Window for RBG0 |
| SPSWE | 1800D4H | Bit 13 | Transparent Process Window for Sprite |
| CCSWE | 1800D6H | Bit 13 | For Color Calculation Window |

| xxSWE | Process |
| --- | --- |
| 0 | Does not use SW window |
| 1 | Uses SW window |



<!-- Page 214 -->

> Note: N0, N1, N2, N3, R0, SP, RP, or CC is entered in bit name for xx.

Designates the valid area of the Normal window W1 used in each screen.

> Note: N0, N1, N2, N3, R0, SP, RP, or CC is entered in bit name for xx.

Designates the valid area of the sprite window SW used in each screen.

| N0W0A | 1800D0H | Bit 0 | Transparent Process Window for NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1W0A | 1800D0H | Bit 8 | Transparent Process Window for NBG1 (or EXBG) |
| N2W0A | 1800D2H | Bit 0 | Transparent Process Window for NBG2 |
| N3W0A | 1800D2H | Bit 8 | Transparent Process Window for NBG3 |
| R0W0A | 1800D4H | Bit 0 | Transparent Process Window for RBG0 |
| SPW0A | 1800D4H | Bit 8 | Transparent Process Window for Sprite |
| RPW0A | 1800D6H | Bit 0 | For Rotation Parameter Window |
| CCW0A | 1800D6H | Bit 8 | For Color Calculation Window |

| xxW0A | Process |
| --- | --- |
| 0 | Enables the inside of W0 window |
| 1 | Enables the outside of W0 window |

| N0W1A | 1800D0H | Bit 2 | Transparent Process Window for NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1W1A | 1800D0H | Bit 10 | Transparent Process Window for NBG1 (or EXBG) |
| N2W1A | 1800D2H | Bit 2 | Transparent Process Window for NBG2 |
| N3W1A | 1800D2H | Bit 10 | Transparent Process Window for NBG3 |
| R0W1A | 1800D4H | Bit 2 | Transparent Process Window for RBG0 |
| SPW1A | 1800D4H | Bit 10 | Transparent Process Window for Sprite |
| RPW1A | 1800D6H | Bit 2 | For Rotation Parameter Window |
| CCW1A | 1800D6H | Bit 10 | For Color Calculation Window |

| xxW1A | Process |
| --- | --- |
| 0 | Enables the inside of W1 window |
| 1 | Enables the outside of W1 window |



<!-- Page 215 -->

> Note: N0, N1, N2, N3, R0, SP or CC is entered in bit name for xx.

| N0SWA | 1800D0H | Bit 4 | Transparent Process Window for NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1SWA | 1800D0H | Bit 12 | Transparent Process Window for NBG1 (or EXBG) |
| N2SWA | 1800D2H | Bit 4 | Transparent Process Window for NBG2 |
| N3SWA | 1800D2H | Bit 12 | Transparent Process Window for NBG3 |
| R0SWA | 1800D4H | Bit 4 | Transparent Process Window for RBG0 |
| SPSWA | 1800D4H | Bit 12 | Transparent Process Window for Sprite |
| CCSWA | 1800D6H | Bit 12 | For Color Calculation Window |

| xxSWA | Process |
| --- | --- |
| 0 | Enables the inside of SW window |
| 1 | Enables the outside of SW window |



<!-- Page 216 -->

(This page was blank in the original Japanese document)



<!-- Page 217 -->

