# Chapter 9 Sprite Data

9.1 Sprite Data ............................................................200 Sprite Types .......................................................200 Sprite Color Mode ..............................................203 9.2 Priority and Color Calculation ............................... 204 Priority Number Selection ..................................204 Color Calculation Enable Conditions .................205 Color Calculation Ratio Selection ...................... 206 Sprite Control Register .......................................207 Priority Number Register....................................209 Color Calculation Ratio Registers ...................... 210 ST-58-R2



<!-- Page 218 -->

## 9.1 Sprite Data

Sprite frame buffer data received from VDP1 may be either 8-bit pixel or 16-bit pixels. When the 16-bit pixel format is read, the data may be either RGB or palette format, but the frame buffer must be either all 8-bit pixel or all 16-bit pixels.

### Sprite Types

When VDP2 receives palette format sprite data written by VDP1 in the frame buffer, there are eight types of bit configurations for 16 bits per pixel and eight types of bit configurations for 8 bits per pixel, for a total of 16 types. These are called sprite types. Data per one dot consists of dot color data, priority bit, color calculation ratio bit, and shadow bit composed from dot color code and palette number. Each bit number changes depending on the sprite type. The value of a bit not having a high enough order in the various bits is regarded as 0. Sprite data of RGB format is composed of data of RGB for each 5-bit and color format discriminator bit. Priority bits, color calculation ratio bits, and shadow bits are considered to be 0. Sprite data, when 16-bit per pixel, designates types 0 through 7; when 8 bit per pixel, designates types 8 through F. When types C through F are designated, priority bit, color calculation ratio bit, and dot color data bit have a shared bit. The shared bits are shown in

**Table 9.1.**

**Table 9.1 Shared Bits**

SP: Priority bit, color RAM address shared bit SC: Color calculation ratio bit, color RAM address shared bit PR: Priority bit DC:Dot color data CC: Color calculation ratio bit

| Sprite Type | Shared Bits |   |   |   |
| --- | --- | --- | --- | --- |
|   | SP1 | SP0 | SC1 | SC0 |
| Tpye C | - | PR0 and DC7 | - | - |
| Type D | - | PR0 and DC7 | - | CC0 and DC6 |
| Type E | PR1 and DC7 | PR0 and DC6 | - | - |
| Type F | - | - | CC1 and DC7 | CC0 and DC6 |



<!-- Page 219 -->

Sprite types are shown in Figure 9.1.

- Type 0
- Type 1
- Type 2
- Type 3
- Type 4
- Type 5
- Type 6
- Type 7
**Figure 9.1**

| PR1 PR0 | CC2 CC1 CC0 | DC10DC9 DC8 DC7 DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- |

| PR2 PR1 PR0 | CC1 CC0 | DC10DC9 DC8 DC7 DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- |

| SD | PR0 | CC2 CC1 CC0 | DC10DC9 DC8 DC7 DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- | --- |

| SD | PR1 PR0 | CC1 CC0 | DC10DC9 DC8 DC7 DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- | --- |

| SD | PR1 PR0 | CC2 CC1 CC0 | DC9 DC8 DC7 DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- | --- |

| SD | PR2 PR1 PR0 | CC0 | DC10DC9 DC8 DC7 DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- | --- |

| SD | PR2 PR1 PR0 | CC1 CC0 | DC9 DC8 DC7 DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- | --- |

| SD | PR2 PR1 PR0 | CC2 CC1 CC0 | DC8 DC7 DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- | --- |



<!-- Page 220 -->

- Type 8
- Type 9
- Type A
- Type B
- Type C
- Type D
- Type E
- Type F
> Note: Shaded areas are ignored.

**Figure 9.1**

|   | PR0 | DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- |

|   | PR0 | CC0 | DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- | --- |

|   | PR1 PR0 | DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- |

|   | CC1 CC0 | DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- |

|   | SP0 | DC6 DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- |

|   | SP0 | SC0 | DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- | --- |

|   | SP1 SP0 | DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- |

|   | SC1 SC0 | DC5 DC4 DC3 DC2 DC1 DC0 |
| --- | --- | --- |



<!-- Page 221 -->

### Sprite Color Mode

Sprite character data has a palette and RGB format, the same as the scroll screen. When the bit count per one dot is 16 (bits), all 16-bits composed of bits selected by the sprite type can be used when data inside the frame buffer is only the palette format. However, when data of the palette and RGB formats are mixed (because the most significant bit is used to discriminate the color format,) palette format data is be set to 0 and RGB format set to “1”. Palette format data is then processed with the value of the selected sprite type MSB (priority bit or shadow bit) as 0. Sprite data when data of palette and RGB formats are mixed is shown in Figure 9.2.

**Figure 9.2 Sprite data when palette format and RGB format data are mixed**

| 0 | 15 bits other than dot color data |
| --- | --- |

| 1 | 5 Bit Blue Data | 5 Bit Green Data | 5 Bit Red Data |
| --- | --- | --- | --- |



<!-- Page 222 -->

## 9.2 Priority and Color Calculation

The priority of sprite and scroll screen is selected according to the size of 3-bit value called the priority number. Sprites can designate a maximum of eight priority numbers and can select one for each character according to the priority bit within sprite data. When using the color calculation function between the sprite and scroll screen, it can be determined whether to perform color calculation by the value of priority number selected by sprite character. Up to eight color calculation ratios can be selected; with one of each character being selected by color calculation ratio in sprite data.

### Priority Number Selection

Sprite priority number selects one from among eight priority numbers in each sprite character by the priority bit of the selected sprite type. When the priority bit of the selected sprite type is 2 bits or less, bits lower than 2 are read as 0, and a 3-bit without a priority number bit are read as 0. When sprite data is in an RGB format, sprite register 0 is selected. The priority number register selected through the value of the priority bit is shown in

**Table 9.2.**

**Table 9.2 Selection of sprite priority number register**

| For Priority Bits |   |   | Priority Number Register Selection |
| --- | --- | --- | --- |
| PR2 | PR1 | PR0 |   |
| 0 | 0 | 0 | For Sprite Register 0 (1800F0H bit 2~0) |
| 0 | 0 | 1 | For Sprite Register 1 (1800F0H bit 10~8) |
| 0 | 1 | 0 | For Sprite Register 2 (1800F2H bit 2~0) |
| 0 | 1 | 1 | For Sprite Register 3 (1800F2H bit 10~8) |
| 1 | 0 | 0 | For Sprite Register 4 (1800F4H bit 2~0) |
| 1 | 0 | 1 | For Sprite Register 5 (1800F4H bit 10~8) |
| 1 | 1 | 0 | For Sprite Register 6 (1800F6H bit 2~0) |
| 1 | 1 | 1 | For Sprite Register 7 (1800F6H bit 10~8) |



<!-- Page 223 -->

### Color Calculation Enable Conditions

A sprite not only designates whether to do color calculation by the entire sprite, but can also designate by the value of the priority number selected in each character and the value of the most significant bit of color data selected in each dot. There are four conditions that can be selected. 1. When (priority number) ≤ (color calculation condition number) 2. When (priority number) = (color calculation condition number) 3. When (priority number) ≥ (color calculation condition number) 4. When color data most significant bit is 1 The color calculation condition number is designated in the sprite control register by the value of the priority number selected in each sprite character, and the 3-bit value for comparing size. These conditions are in effect only when the SPCCEN bit of the color calculation control register is 1; color calculation will not be done when the register is 0. ST-58-R2



<!-- Page 224 -->

### Color Calculation Ratio Selection

The color calculation ratio of sprites select one of eight color calculation ratios in each sprite character by the color calculation ratio bit of the designated sprite type data. When two or less bits are used for color calculation ratio of the designated sprite type, the low bit is read as 0. When there is no color calculation ratio bit, 8-bit is also read as 0. When sprite data is in an RGB format, the sprite register 0 is selected. Selection of the color calculation ratio register through the value of the color calculation ratio bit is shown in

**Table 9.3.**

**Table 9.3 Selection of sprite color calculation ratio register**

| For Color Calculation Ratio Bits |   |   | Color Calculation Ratio Register Selection |
| --- | --- | --- | --- |
| CC2 | CC1 | CC0 |   |
| 0 | 0 | 0 | For Sprite Register 0 (180100H bit 4~0) |
| 0 | 0 | 1 | For Sprite Register 1 (180100H bit 12~8) |
| 0 | 1 | 0 | For Sprite Register 2 (180102H bit 4~0) |
| 0 | 1 | 1 | For Sprite Register 3 (180102H bit 12~8) |
| 1 | 0 | 0 | For Sprite Register 4 (180104H bit 4~0) |
| 1 | 0 | 1 | For Sprite Register 5 (180104H bit 12~8) |
| 1 | 1 | 0 | For Sprite Register 6 (180106H bit 4~0) |
| 1 | 1 | 1 | For Sprite Register 7 (180106H bit 12~8) |



<!-- Page 225 -->

### Sprite Control Register

The sprite control register controls sprite data, and is a write-only 16-bit register located at address 1800E0H. Because the value is cleared to 0 after power on or reset, it must be set. Designates the color calculation condition of sprites. When the sprite color format is RGB, color calculation is always performed if SPCCCS is set to “3”. Designates the color calculation condition number of sprites. This value is ignored when SPCCCS is set to “3”. Designates the sprite color mode. Do not designate a “1” when sprite data are 8-bit pixels.

| SPCTL | ~ | ~ | SPCCCS1 | SPCCCS0 | ~ | SPCCN2 | SPCCN1 | SPCCN0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800E0H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | SPCLMD | SPWINEN | SPTYPE3 | SPTYPE2 | SPTYPE1 | SPTYPE0 |

| SPCCCS | SPCCCS1 | SPCCCS0 | Condition |
| --- | --- | --- | --- |
| 0 | 0 | 0 | (Priority number) £ (Color calculation condition number) only |
| 1 | 0 | 1 | (Priority number) = (Color calculation condition number) only |
| 2 | 1 | 0 | (Priority number) ‡ (Color calculation condition number) only |
| 3 | 1 | 1 | Only when Color Data MSB is 1. |

| SPCLMD | Sprite Color Format Data |
| --- | --- |
| 0 | Sprite data is all in palette format |
| 1 | Sprite data is in palette format and RGB format |



<!-- Page 226 -->

See “8.1 Window Area” Designates the sprite type. When sprite data are 16-bit pixels, designate type 0 to 7; and when 8-bit pixels, designate type 8 to F.

| STYPE3 | STYPE2 | STYPE1 | STYPE0 | Sprite Data Type |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | Type 0 |
| 0 | 0 | 0 | 1 | Type 1 |
| 0 | 0 | 1 | 0 | Type 2 |
| 0 | 0 | 1 | 1 | Type 3 |
| 0 | 1 | 0 | 0 | Type 4 |
| 0 | 1 | 0 | 1 | Type 5 |
| 0 | 1 | 1 | 0 | Type 6 |
| 0 | 1 | 1 | 1 | Type 7 |
| 1 | 0 | 0 | 0 | Type 8 |
| 1 | 0 | 0 | 1 | Type 9 |
| 1 | 0 | 1 | 0 | Type A |
| 1 | 0 | 1 | 1 | Type B |
| 1 | 1 | 0 | 0 | Type C |
| 1 | 1 | 0 | 1 | Type D |
| 1 | 1 | 1 | 0 | Type E |
| 1 | 1 | 1 | 1 | Type F |



<!-- Page 227 -->

### Priority Number Register

The priority number register designates the priority number, and is a write-only 16bit register located at addresses 1800F0H through 1800F6H. Because the value is cleared to 0 after power on or reset, it must be set. Designates the sprite priority number.

| PRISA | ~ | ~ | ~ | ~ | ~ | S1PRIN2 | S1PRIN1 | S1PRIN0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800F0H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | S0PRIN2 | S0PRIN1 | S0PRIN0 |

| PRISB | ~ | ~ | ~ | ~ | ~ | S3PRIN2 | S3PRIN1 | S3PRIN0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800F2H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | S2PRIN2 | S2PRIN1 | S2PRIN0 |

| PRISC | ~ | ~ | ~ | ~ | ~ | S5PRIN2 | S5PRIN1 | S5PRIN0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800F4H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | S4PRIN2 | S4PRIN1 | S4PRIN0 |

| PRISD | ~ | ~ | ~ | ~ | ~ | S7PRIN2 | S7PRIN1 | S7PRIN0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800F6H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | S6PRIN2 | S6PRIN1 | S6PRIN0 |

| S0PRIN2~S0PRIN0 | 1800F0H | Bit 2~0 | For Sprite Register 0 |
| --- | --- | --- | --- |
| S1PRIN2~S1PRIN0 | 1800F0H | Bit 10~8 | For Sprite Register 1 |
| S2PRIN2~S2PRIN0 | 1800F2H | Bit 2~0 | For Sprite Register 2 |
| S3PRIN2~S3PRIN0 | 1800F2H | Bit 10~8 | For Sprite Register 3 |
| S4PRIN2~S4PRIN0 | 1800F4H | Bit 2~0 | For Sprite Register 4 |
| S5PRIN2~S5PRIN0 | 1800F4H | Bit 10~8 | For Sprite Register 5 |
| S6PRIN2~S6PRIN0 | 1800F6H | Bit 2~0 | For Sprite Register 6 |
| S7PRIN2~S7PRIN0 | 1800F6H | Bit 10~8 | For Sprite Register 7 |



<!-- Page 228 -->

Display priority order increases with the size of the priority number. Sprite characters that use the register set to a priority number value of 0 are treated as transparent and are not displayed.

### Color Calculation Ratio Registers

The color calculation ratio registers selects the color calculation ratio, and are writeonly 16-bit registers located at addresses 180100H through 180106H. Because they are cleared to 0 after power on or reset, they must be set. Designates the sprite color calculation ratio. The color calculation ratio is for a value 1/32 of RGB various color data.

| CCRSA | ~ | ~ | ~ | S1CCRT4 | S1CCRT3 | S1CCRT2 | S1CCRT1 | S1CCRT0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180100H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | S0CCRT4 | S0CCRT3 | S0CCRT2 | S0CCRT1 | S0CCRT0 |

| CCRSB | ~ | ~ | ~ | S3CCRT4 | S3CCRT3 | S3CCRT2 | S3CCRT1 | S3CCRT0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180102H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | S2CCRT4 | S2CCRT3 | S2CCRT2 | S2CCRT1 | S2CCRT0 |

| CCRSC | ~ | ~ | ~ | S5CCRT4 | S5CCRT3 | S5CCRT2 | S5CCRT1 | S5CCRT0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180104H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | S4CCRT4 | S4CCRT3 | S4CCRT2 | S4CCRT1 | S4CCRT0 |

| CCRSD | ~ | ~ | ~ | S7CCRT4 | S7CCRT3 | S7CCRT2 | S7CCRT1 | S7CCRT0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180106H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | S6CCRT4 | S6CCRT3 | S6CCRT2 | S6CCRT1 | S6CCRT0 |



<!-- Page 229 -->

| S0CCRT4~S0CCRT0 | 180100H | Bit 4~0 | For Sprite Register 0 |
| --- | --- | --- | --- |
| S1CCRT4~S1CCRT0 | 180100H | Bit 12~8 | For Sprite Register 1 |
| S2CCRT4~S2CCRT0 | 180102H | Bit 4~0 | For Sprite Register 2 |
| S3CCRT4~S3CCRT0 | 180102H | Bit 12~8 | For Sprite Register 3 |
| S4CCRT4~S4CCRT0 | 180104H | Bit 4~0 | For Sprite Register 4 |
| S5CCRT4~S5CCRT0 | 180104H | Bit 12~8 | For Sprite Register 5 |
| S6CCRT4~S6CCRT0 | 180106H | Bit 4~0 | For Sprite Register 6 |
| S7CCRT4~S7CCRT0 | 180106H | Bit 12~8 | For Sprite Register 7 |



<!-- Page 230 -->

xxCCRT4 xxCCRT3 xxCCRT2 xxCCRT1 xxCCRT0 Color Calculation Ratio Top Image : Second Image 0 0 0 0 0 31:1 0 0 0 0 1 30:2 0 0 0 1 0 29:3 0 0 0 1 1 28:4 0 0 1 0 0 27:5 0 0 1 0 1 26:6 0 0 1 1 0 25:7 0 0 1 1 1 24:8 0 1 0 0 0 23:9 0 1 0 0 1 22:10 0 1 0 1 0 21:11 0 1 0 1 1 20:12 0 1 1 0 0 19:13 0 1 1 0 1 18:14 0 1 1 1 0 17:15 0 1 1 1 1 16:16 1 0 0 0 0 15:17 1 0 0 0 1 14:18 1 0 0 1 0 13:19 1 0 0 1 1 12:20 1 0 1 0 0 11:21 1 0 1 0 1 10:22 1 0 1 1 0 9:23 1 0 1 1 1 8:24 1 1 0 0 0 7:25 1 1 0 0 1 6:26 1 1 0 1 0 5:27 1 1 0 1 1 4:28 1 1 1 0 0 3:29 1 1 1 0 1 2:30 1 1 1 1 0 1:31 1 1 1 1 1 0:32

> Note: S0 to S7 are entered in bit name for xx.

This register is in effect only when the CCMD bit of the color calculation control register is 0, and is ignored when “1”.



<!-- Page 231 -->

