# Chapter 10 Pixels

Introduction ....................................................................214 10.1 Palette Format Pixels...........................................214 Sprite Dot Pixels ............................................... 214 Scroll Dot Pixels................................................ 216 Color RAM Address Offset Register ................. 217 10.2 RGB Format Pixels .............................................. 218 Sprite Pixels ...................................................... 218 Scroll Pixels ...................................................... 218 10.3 Special Function Code.........................................220 Special Function Code Select Register ............ 221 Special Function Code Register ....................... 222 ST-58-R2



<!-- Page 232 -->

## Introduction

When sprites and dot color data of each scroll screen are in a palette format, the color RAM address offset register value added to the dot color data (configured from the palette number and dot color code) becomes the color RAM address. Color data of that address is output as color data. In the RGB format, dot color data composed of individual red, green and blue values are pixels. Scroll screen dot color data in a palette format designates whether to use special priority and special color calculation functions according to the lowest 4-bit color data.

## 10.1 Palette Format Pixels

Palette format pixels are 11 bits wide, and is the color RAM addresses that store pixel data-the value of the color RAM address offset register of the corresponding screen added to the highest 3-bits.

### Sprite Dot Pixels

Palette format sprite pixels change according to the sprite type that has been designated. When pixels designate sprite types that are 10-bit or lower, missing highorder bits are taken as 0, and the sprite color RAM address offset value is added to the highest 3 bits and is treated as the color RAM address of that dot. When the color RAM mode is set to mode 0 or mode 2, the highest bit of the color RAM address will be ignored.



<!-- Page 233 -->

Palette format sprite pixels are shown in Figure 10.1. The sprite color RAM address is shown in Figure 10.2.

- When Sprite Type 0~3, 5
Bit 10 9 8 7 6 5 4 3 2 1 0 11 Bit pixel

- When Sprite Type 4, 6
Bit 10 9 8 7 6 5 4 3 2 1 0 0 10 Bit pixel

- When Sprite Type 7
Bit 10 9 8 7 6 5 4 3 2 1 0 0 0 9 Bit pixel

- When Sprite Type C~F
Bit 10 9 8 7 6 5 4 3 2 1 0 0 0 0 8 Bit pixel

- When Sprite Type 8
Bit 10 9 8 7 6 5 4 3 2 1 0 0 0 0 0 7 Bit pixel

- When Sprite Type 9~B
Bit 10 9 8 7 6 5 4 3 2 1 0 0 0 0 0 0 6 Bit pixel

**Figure 10.1 Palette format sprite dot color data**

Sprite 11 Bit pixel Dot Color Data + Offset Value Color RAM Address 3 Bit 0 0 0 0 0 0 0 0 Offset Value for Sprite Dot Color RAM 11 Bit Color RAM Address Address

> Note: When Color RAM is in mode 0 or mode 2, the MSB of the color

RAM address is ignored.

**Figure 10.2**

Sprite Color RAM Address ST-58-R2



<!-- Page 234 -->

### Scroll Dot Pixels

Scroll pixels in a palette format changes according to the designated character color count. The color RAM address offset value corresponding to each surface is added to the highest 3 bits of 11-bit dot color data, and is treated as the color RAM address of that dot. When color RAM mode is set to mode 0 or mode 2, the highest bit of color RAM address will be ignored. Because the line color screen doesn’t have a corresponding color RAM address offset value, the 11-bit value read from line color screen table becomes the color RAM address. Palette format scroll dot color data is shown in Figure 10.3. The scroll screen color RAM address is shown in Figure 10.4.

- When Character Color count is 16 Colors
9 8 7 6 5 4 3 2 1 0 Bit 10 7 Bit Palette Number 4 Bit Dot Color Code

- When Character Color count is 256 Colors
9 8 7 6 5 4 3 2 1 0 Bit 10 8 Bit Dot Color Code 3 Bit Palette No.

- When Character Color count is 2048 Colors
9 Bit 10 8 7 6 5 4 3 2 1 0 11 Bit Dot Color Code

**Figure 10.3**

Palette format scroll dot color data Scroll 11 Bit Dot Color Data Dot Color Data + Offset Value Color RAM Address 3 Bit 0 0 0 0 0 0 0 0 Offset Value For Each Scroll Dot Color RAM 11 Bit Color Ram Address Address

> Note: When the color RAM mode is 0 or 2, the color RAM address MSB is

ignored.

**Figure 10.4 Scroll Color RAM Address**



<!-- Page 235 -->

### Color RAM Address Offset Register

The color RAM address offset register is a write only 16-bit register that designates the sprite and color RAM address offset values corresponding to each scroll screen. It is located at addresses 1800E4H through 1800E7. Because the value is cleared to 0 after power on or reset, you must set it. 15 14 13 12 11 10 9 8 CRAOFA ~ N3CAOS2 N3CAOS1 N3CAOS0 ~ N2CAOS2 N2CAOS1 N2CAOS0 1800E4H 7 6 5 4 3 2 1 0 ~ N1CAOS2 N1CAOS1 N1CAOS0 ~ N0CAOS2 N0CAOS1 N0CAOS0 15 14 13 12 11 10 9 8 CRAOFB ~ ~ ~ ~ ~ ~ ~ ~ 1800E6H 7 6 5 4 3 2 1 0 ~ SPCAOS2 SPCAOS1 SPCAOS0 ~ R0CAOS2 R0CAOS1 R0CAOS0 Color RAM address offset bit (N0CAOS2 to N0CAOS0, N1CAOS2 to N1CAOS0, N2CAOS2 to N2CAOS0, N3CAOS2 to N3CAOS0, R0CAOS2 to R0CAOS0, SPCAOS2 to SPCAOS0) Designates color RAM address offset values with respect to the sprite and each scroll screen. N0CAOS0~N0CAOS2 1800E4H Bit 2~0 For NBG0 (or RBG1) N1CAOS0~N1CAOS2 1800E4H Bit 6~4 For NBG1 (or EXBG) N2CAOS0~N2CAOS2 1800E4H Bit 10~8 For NBG2 N3CAOS0~N3CAOS3 1800E4H Bit 14~12 For NBG3 R0CAOS0~R0CAOS2 1800E6H Bit 2~0 For RBG0 SPCAOS0~SPCAOS2 1800E6H Bit 6~4 For Sprite The actual color RAM address offset value is calculated by the expression below. When the color RAM mode is set to mode 0 or mode 2, the highest bit of color RAM address that calculated the color RAM address offset value will be ignored. When color RAM mode is mode 0 or mode 2: (color RAM address offset value [= offsetReg << 9] ) = (color RAM address offset register 3-bit value) X 200H When color RAM mode is mode L: (color RAM address offset value [= offsetReg << 10] ) = (color RAM address offset register 3-bit value) X 400H ST-58-R2



<!-- Page 236 -->

## 10.2 RGB Format Pixels

RGB format dot color data is 15-bit in sprite and 15-bit or 24-bit data, depending on character color count in scroll, and becomes dot color data without going through color RAM. When dot color data is 15-bit, the lowest three bits of each individual is fixed at 0 and used.

### Sprite Pixels

RGB format sprite dot color data is RGB 5-bit data that outputs each of the lowest 3 bits fixed at 0. RGB format sprite dot color data is shown in Figure 10.5. Bit 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 Sprite 5 Bit Red Data 5 Bit Blue Data 5 Bit Green Data Dot Color Data 5 Bit Blue Data 0 0 0 Output Blue Data 5 Bit Green Data 0 0 0 Output Green Data 5 Bit Red Data 0 0 0 Output Red Data

**Figure 10.5 RGB format sprite dot color data**

### Scroll Pixels

RGB format scroll dot color data changes according to the character color count. When 15-bit, the lowest three bits of each individual RGB is fixed at 0 and output. The back screen is output by fixing the lower 3-bit of each RGB at 0, the same as when the character color count of the scroll screen is 32,768 colors.



<!-- Page 237 -->

Scroll dot color data of the RGB format is shown in Figure 10.6.

- When Character Color Count is 32768 Colors
Bit 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 Scroll Sprite 5 Bit Blue Data 5 Bit Green Data 5 Bit Red Data Dot Color Data 5 Bit Blue Data 0 0 0 Output Blue Data 5 Bit Green Data 0 0 0 Output Green Data 5 Bit Red Data 0 0 0 Output Red Data

- When Character Color Count is 16,770,000 Colors
Bit 23 22 21 20 19 18 17 16 Scroll 8 Bit Blue Data Dot Color Data Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 8 Bit Green Data 8 Bit Red Data 8 Bit Blue Data Output Blue Data 8 Bit Green Data Output Green Data 8 Bit Red Data Output Red Data

**Figure 10.6 RGB Format Scroll Dot Color Data**

ST-58-R2



<!-- Page 238 -->

## 10.3 Special Function Code

The special function, which performs in all scroll screens, has two functions: the special priority function, and the special color calculation function. When used in every dot, the dot color code that activates the special function can designate two special function code registers. Also, each scroll screen designates which of the two special function code registers will be used. The special function code register has two 8-bit registers: special function code A, and special function code B. Each bit corresponds to two dot color code lower 4-bit values using the special function. The dot color code changes the bit number according to the character color count of each scroll screen. However, each bit of the special function code register will always correspond to the value of the lowest four bits of the dot color code. Moreover, the special function code is used only when the color format of scroll screen is the palette format. See “11.2 Special Priority Function” and “12.2 Special Color Calculation Function” for using the special function. Figure 10.7 shows the dot color code that corresponds to special function code.

- When Character Color Count is 16 Colors
Bit 3 2 1 0 Corresponding 4 bits

- When Character Color Count is 256 Colors
Bit 7 6 5 4 3 2 1 0 Corresponding 4 bits

- When Character Color Count is 2048 Colors
9 8 7 6 5 4 3 2 1 Bit 10 0 Corresponding 4 bits

**Figure 10.7**

Dot Color Data Corresponding to Special Function Code



<!-- Page 239 -->

### Special Function Code Select Register

The special function code select register is a write-only 16-bit register that designates the special function code that activates all scroll screens. It is located at address 180024H. Because the value is cleared to 0 after power on or reset, it must be set. 15 14 13 12 11 10 9 8 SFSEL ~ ~ ~ ~ ~ ~ ~ ~ 180024H 7 6 5 4 3 2 1 0 ~ ~ ~ R0SFCS N3SFCS N2SFCS N1SFCS N0SFCS Special function code select bit (N0SFCS, N1SFCS, N2SFCS, N3SFCS, R0SFCS) Designates the special function code effecting every scroll screen. N0SFCS 180024H Bit 0 For NBG0 (or RBG1) N1SFCS 180024H Bit 1 For NBG1 N2SFCS 180024H Bit 2 For NBG2 N3SFCS 180024H Bit 3 For NBG3 R0SFCS 180024H Bit 4 For RBG0 xxSFCS Process 0 Enables special function code A 1 Enables special function code B

> Note: N0, N1, N2, N3, or R0 is entered in bit name for xx.

ST-58-R2



<!-- Page 240 -->

### Special Function Code Register

The special function code register is a write-only 16-bit register that designates special function code A and special function code B. It is located at address 180026H. Because the value is cleared to 0 after power on or reset, it must be set. 15 14 13 12 11 10 9 8 SFCODE SFCDB7 SFCDB6 SFCDB5 SFCDB4 SFCDB3 SFCDB2 SFCDB1 SFCDB0 180026H 7 6 5 4 3 2 1 0 SFCDA7 SFCDA6 SFCDA5 SFCDA4 SFCDA3 SFCDA2 SFCDA1 SFCDA0 Special function code bit (SFCDA7 to SFCDA0, SFCDB7 to SFCDB0) Designates special function codes A and B. SFCDA7~SFCDA0 180026H Bit 7~0 For Special Function Code A SFCDB7~SFCDB0 180026H Bit 15~8 For Special Function Code B Bit Name Dot Color Code SFCDx0 When lower 4 bits of dot color code are, 0H or 1H SFCDx1 When lower 4 bits of dot color code are, 2H or 3H SFCDx2 When lower 4 bits of dot color code are, 4H or 5H SFCDx3 When lower 4 bits of dot color code are, 6H or 7H SFCDx4 When lower 4 bits of dot color code are, 8H or 9H SFCDx5 When lower 4 bits of dot color code are, AH or BH SFCDx6 When lower 4 bits of dot color code are, CH or DH SFCDx7 When lower 4 bits of dot color code are, EH or FH

> Note: A or B is entered in bit name x.

Settings Process 0 Does not use special functions 1 Uses special functions The special function code is used when mode 2 is designated in the special priority mode registers or when designating mode 2 in the special color calculation mode register. For more information see “11.2 Special Priority Function” or “12.2 Special Color Calculation Function.”



<!-- Page 241 -->

