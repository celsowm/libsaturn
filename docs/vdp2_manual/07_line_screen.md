# Chapter 7 Line Screen

Introduction ..........................................................................172 7.1 Line Color Screen .......................................................172 Line Color Screen Table Address Register ..............174 7.2 Back Screen................................................................175 Back Screen Table Address Register .......................176 ST-58-R2



<!-- Page 190 -->

## Introduction

There are two line screen surfaces: the line color screen (LNCL) and the back screen (BACK). The line screen designates the color in each line, or the entire screen in a single color. Unlike the scroll screen, the line screen cannot display characters. The line color screen stores the data of each line in VRAM as a line color screen table. If single colored, lead data of the table is used in the entire screen. The line screen is shown in Figure 7.1.

> Note: In the case of single color, the 1st line data is used in the entire screen.

**Figure 7.1 Line Screen**

## 7.1 Line Color Screen

The line color screen is used only for color calculations, and chooses whether to designate the entire screen in a single color, or designate the color for each line. The color RAM address of the color used is stored in VRAM as line color screen data. The line number designated by one line color screen data changes, depending on the interlace setting. The non-interlace and double-density interlace modes can designate the color for each line; the single-density interlace mode can designate for each two lines. The line color screen can also be made to rotate if line color screen data is used within coefficient data. For more about coefficient data see section “6.4 Coefficient Table Control.” Figure 7.2 shows the configuration of the line color screen table separate of the interlace mode. Figure 7.3 shows the configuration of data on the line color screen table.

| Line Screen Data for<br>1st Line |
| --- |
| Line Screen Data for<br>2nd Line |
| Line Screen Data for<br>3rd Line |



<!-- Page 191 -->

> Note: In the case of single color, the first line color RAM

> Note: In the case of single color, the first and second line

**Figure 7.2 Configuration of line color screen table**

> Note: Shaded areas are ignored. Also, when color RAM is in mode 0 or mode 2,

**Figure 7.3 Bit configuration of line color screen table data**

| 1st Line Color RAM Address |
| --- |
| 2nd Line Color RAM Address |
| 3rd Line Color RAM Address |
| 4th Line Color RAM Address |
| 5th Line Color RAM Address |
| 6th Line Color RAM Address |

| 1st and 2nd Line Color Ram Address |
| --- |
| 3rd and 4th Line Color Ram Address |
| 5th and 6th Line Color Ram Address |
| 7th and 8th Line Color Ram Address |
| 9th and 10th Line Color Ram Address |
| 11th and 12th Line Color Ram Address |

|   | 11 bit color Ram address |
| --- | --- |



<!-- Page 192 -->

### Line Color Screen Table Address Register

The line color screen table address register is a 32-bit register, and designates the lead address of the table and the color mode of the line color screen. Its addresses are 1800A8H through 1800AAH. Because the value is cleared to 0, it must be set after power on, or reset. Designates the color mode of the line color screen. Designates the lead address of the line color screen table on the VRAM. The actual lead VRAM address is calculated by the expression below. When the VRAM size is 4 Mbits, the most significant bit of the address is ignored.

| LCTAU | LCCLMD | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800A8H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | LCTA18 | LCTA17 | LCTA16 |

| LCTAL | LCTA15 | LCTA14 | LCTA13 | LCTA12 | LCTA11 | LCTA10 | LCTA9 | LCTA8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800AAH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | LCTA7 | LCTA6 | LCTA5 | LCTA4 | LCTA3 | LCTA2 | LCTA1 | LCTA0 |

| LCCLMD | Line Color Screen Color |
| --- | --- |
| 0 | Single color |
| 1 | Select per each line |

| LCTA18~LCTA16 | 1800A8H | Bit 2~0 |
| --- | --- | --- |
| LCTA15~LCTA0 | 1800AAH | Bit 15~0 |



<!-- Page 193 -->

## 7.2 Back Screen

The back screen (BACK) is displayed only when other screens aren’t, and chooses whether to designate a single color for the entire screen or for each line. Color data used by a line is designated by each 5-bit RGB. The non-interlace and double-density interlace mode designates the color in each line, but the single-density interlace mode can designate only in each two lines. Figure 7.4 shows the configuration of the back screen table by the interlace mode. Figure 7.5 shows the configuration of data on the back screen table.

> Note: In the case of single color, the first line RGB data is us

> Note: In the case of single color, the first and second line RGB

**Figure 7.4 Configuration of back screen table**

| 1st Line RGB Data |
| --- |
| 2nd Line RGB Data |
| 3rd Line RGB Data |
| 4th Line RGB Data |
| 5th Line RGB Data |
| 6th Line RGB Data |

| 1st and 2nd Line RGB Data |
| --- |
| 3rd and 4th Line RGB Data |
| 5th and 6th Line RGB Data |
| 7th and 8th Line RGB Data |
| 9th and 10th Line RGB Data |
| 11th and 12th Line RGB Data |



<!-- Page 194 -->

> Note: Shaded area is ignored. Add 0 bit 3 bits at a time to the lower bits of

**Figure 7.5**

### Back Screen Table Address Register

Back screen table address register is a write-only 32-bit registers, and selects the back screen color mode and table lead address. Its addresses are 1800ACH through 1800AEH. Because the value is cleared to 0, it must be set after power on or reset. Designates color mode of the back screen. Designates the lead address of the back screen table on the VRAM.

|   | 5 bit Blue Data | 5 bit Green Data | 5 bit Red Data |
| --- | --- | --- | --- |

| BKTAU | BKCLMD | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800ACH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | BKTA18 | BKTA17 | BKTA16 |

| BKTAL | BKTA15 | BKTA14 | BKTA13 | BKTA12 | BKTA11 | BKTA10 | BKTA9 | BKTA8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800AEH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | BKTA7 | BKTA6 | BKTA5 | BKTA4 | BKTA3 | BKTA2 | BKTA1 | BKTA0 |

| BKCLMD | Back Screen Color |
| --- | --- |
| 0 | Single color |
| 1 | Select per each line |

| BKTA18~BKTA16 | 1800ACH | Bit 2~0 |
| --- | --- | --- |
| BKTA 15~BKTA0 | 1800AEH | Bit 15~0 |



<!-- Page 195 -->

The actual lead VRAM address is calculated by the expression below. When the VRAM capacity is 4 Mbits, the most significant bit of the address is ignored. (Back screen table lead address) = (Back screen table address register value 19 bit) X 2H When the back screen color mode bit is set to “single color”, color data selected by the back screen table address bit is used in the entire screen. ST-58-R2



<!-- Page 196 -->

(This page is blank in the original Japanese document)



<!-- Page 197 -->

