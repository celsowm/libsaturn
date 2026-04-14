|   | ~ | N0CHCN2 | N0CHCN1 | N0CHCN0 | N0BMSZ1 | N0BMSZ0 | N0BMEN | N0CHSZ |

| CHCTLB | ~ | R0CHCN2 | R0CHCN1 | R0CHCN0 | ~ | R0BMSZ | R0BMEN | R0CHSZ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18002AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N3CHCN | N3CHSZ | ~ | ~ | N2CHCN | N2CHSZ |

| N0CHCN2~N0CHCN0 | 180028H | Bit 6~4 | For NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1CHCN1,N1CHCN0 | 180028H | Bit 13,12 | For NBG1 (or EXBG) |
| N2CHCN | 18002AH | Bit 1 | For NBG2 |
| N3CHCN | 18002AH | Bit 5 | For NBG3 |
| R0CHCN2~R0CHCN0 | 18002AH | Bit 14~12 | For RBG0 |

| N0CHCN2 | N0CHCN1 | N0CHCN0 | TV Screen Mode |   |   | Color |
| --- | --- | --- | --- | --- | --- | --- |
|   |   |   | Normal | Hi-Res | Exclusive<br>Monitor | Format |
| 0 | 0 | 0 | 16 colors | 16 colors | 16 colors | Palette Format |
| 0 | 0 | 1 | 256 colors | 256 colors | 256 colors | Palette Format |
| 0 | 1 | 0 | 2048 colors | 2048 colors | 2048 colors | Palette Format |
| 0 | 1 | 1 | 32,786 colors | 32,786 colors | 32,786 colors | RGB Format |
| 1 | 0 | 0 | 16,770,000<br>colors | Setting not<br>allowed | Setting not<br>allowed | RGB Format |
| 1 | 0 | 1 | Setting not allowed (Please do not set.) |   |   |   |
| 1 | 1 | 0 | Setting not allowed (Please do not set.) |   |   |   |
| 1 | 1 | 1 | Setting not allowed (Please do not set.) |   |   |   |



<!-- Page 79 -->

> Note:

> Note:

Depending on the color count of NBG0 and NBG1, the scroll screen that cannot be displayed will appear. When NBG0 is set at 2048 or 32,768 colors, NBG2 can no longer be displayed. When NBG0 is set at 16,770,000 colors, NBG1 to NBG3 can no longer be displayed. When NBG1 is set at 2048 or 32,768 colors, NBG3 can no longer be displayed.

| N1CHCN1 | N1CHCN0 | TV Screen Mode |   |   | Color Format |
| --- | --- | --- | --- | --- | --- |
|   |   | Normal | Hi-Res | Exclusive<br>Monitor | l |
| 0 | 0 | 16 colors | 16 colors | 16 colors | Palette Format |
| 0 | 1 | 256 colors | 256 colors | 256 colors | Palette Format |
| 1 | 0 | 2048 colors | 2048 colors | 2048 colors | Palette Format |
| 1 | 1 | 32,786 colors | 32,786 colors | 32,786 colors | RGB Format |

| NnCHCN0 | TV Screen Mode |   |   | Color Format |
| --- | --- | --- | --- | --- |
|   | Normal | Hi-Res | Exclusive<br>Monitor |   |
| 0 | 16 colors | 16 colors | 16 colors | Palette Format |
| 1 | 256 colors | 256 colors | 256 colors | Palette Format |

| R0CHCN2 | R0CHCN1 | R0CHCN0 | TV Screen Mode |   |   | Color |
| --- | --- | --- | --- | --- | --- | --- |
|   |   |   | Normal | Hi-Res | Exclusive<br>Monitor | Format |
| 0 | 0 | 0 | 16 colors | 16 colors | Cannot Display | Palette Format |
| 0 | 0 | 1 | 256 colors | 256 colors | Cannot Display | Palette Format |
| 0 | 1 | 0 | 2048 colors | 2048 colors | Cannot Display | Palette Format |
| 0 | 1 | 1 | 32,786 colors | 32,786 colors | Cannot Display | RGB Format |
| 1 | 0 | 0 | 16,770,000<br>colors | Setting not<br>allowed | Cannot Display | RGB Format |
| 1 | 0 | 1 | Setting not allowed (Please do not set.) |   |   |   |
| 1 | 1 | 0 | Setting not allowed (Please do not set.) |   |   |   |
| 1 | 1 | 1 | Setting not allowed (Please do not set.) |   |   |   |



<!-- Page 80 -->

Designates the bit map size of each screen when display is in a bit map format.

> Note: 0 or 1 is entered in bit name for n.

Designates whether to display the scroll screen in a bit map format.

> Note: N0, N1, or R0 is entered in bit name for xx.

| N0BMSZ1,N0BMSZ0 | 180028H | Bit 3,2 | For NBG0 |
| --- | --- | --- | --- |
| N1BMSZ1,N1BMSZ0 | 180028H | Bit 11,10 | For NBG1 |
| R0BMSZ | 18002AH | Bit 10 | For RBG0 |

| NnBMSZ1 | NnBMSZ0 | Bitmap Size |
| --- | --- | --- |
| 0 | 0 | 512 H dots X 256 V dots |
| 0 | 1 | 512 H dots X 512 V dots |
| 1 | 0 | 1024 H dots X 256 V dots |
| 1 | 1 | 1024 H dots X 512 V dots |

| ROBMSZ | Bitmap Size |
| --- | --- |
| 0 | 512 H dots X 256 V dots |
| 1 | 512 H dots X 512 V dots |

| N0BMEN | 180028H | Bit 1 | For NBG0 |
| --- | --- | --- | --- |
| N1BMEN | 180028H | Bit 9 | For NBG1 |
| R0BMEN | 18002AH | Bit 9 | For RBG0 |

| xxBMEN | Screen Display Format |
| --- | --- |
| 0 | Cell Format |
| 1 | Bitmap Format |



<!-- Page 81 -->

Designates the character size when the scroll screen is in a cell format.

> Note: N0, N1, N2, N3, or R0 is entered in bit name for xx.

| N0CHSZ | 180028H | Bit 0 | For NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1CHSZ | 180028H | Bit 8 | For NBG1 |
| N2CHSZ | 18002AH | Bit 0 | For NBG2 |
| N3CHSZ | 18002AH | Bit 4 | For NBG3 |
| ROCHSZ | 18002AH | Bit 8 | For RBG0 |

| xxCHSZ | Character Pattern Size |
| --- | --- |
| 0 | 1 H Cell X 1 V Cell |
| 1 | 2 H Cells X 2 V Cells |



<!-- Page 82 -->

## 4.6

## Pattern Name Table (Page)

Pattern name table (or page) stores the method of arrangement when the character pattern is in a square the size of a 64 X 64 cell in the VRAM. It also arranges pattern name data in table form and stores it in VRAM. Pattern name data selects the lead address of the character pattern stored in VRAM and the control information for each character pattern. Pattern name data in a pattern name table is in one-word or two-word. When in one-word, auxiliary data of the least significant 10 bits of the pattern name control register is added to make up for insufficient bits.

### Pattern Name Table Data Configuration

The boundary stored in VRAM and VRAM capacity required in a pattern name table of 64 X 64 cells (1 page) change depending on the pattern name data size (word count) and character size. The capacity and data configuration of pattern name tables are shown in

**Table 4.4 and Figure 4.8.**

**Table 4.4 Pattern name table capacity and page boundary of one page**

| Pattern Name Data<br>Size | Character Size | Contents of 1<br>Page | Boundary During<br>VRAM Storage |
| --- | --- | --- | --- |
| 1 Word | 1 H Cell X 1 V Cell | 8192 Bytes | 2000H |
|   | 2 H Cells X 2 V Cells | 2048 Bytes | 800H |
| 2 Words | 1 H Cell X 1 V Cell | 16,384 Bytes | 4000H |
|   | 2 H Cells X 2 V Cells | 4096 Bytes | 1000H |



<!-- Page 83 -->

**Figure 4.8 Data configuration of pattern name tables**

| Character Pattern 0-0 Pattern Name Data |
| --- |
| Character Pattern 0-1 Pattern Name Data |

|   |   | +0004 |
| --- | --- | --- |
| +0000 | +0002 |   |
| +0080 |   |   |
|   | +0082 | +0084 |

| +007A |   |   |
| --- | --- | --- |
|   | +007C | +007E |
| +00FA | +00FC | +00FE |

| +1F00 |   |   |
| --- | --- | --- |
|   | +1F02 | +1F04 |
| +1F80 |   |   |
|   | +1F82 | +1F84 |

| +1F7A | +1F7C | +1F7E |
| --- | --- | --- |
| +1FFA |   |   |
|   | +1FFC | +1FFE |



<!-- Page 84 -->

**Figure 4.8 Data configuration of pattern name tables (continued)**

| Character Pattern 0-0 Pattern Name Data |
| --- |
| Character Pattern 0-1 Pattern Name Data |

| +000 | +002 | +004 |
| --- | --- | --- |
| +040 | +042 | +044 |

| +03A | +03C | +03E |
| --- | --- | --- |
| +07A | +07C | +07E |

| +780 | +782 | +784 |
| --- | --- | --- |
| +7C0 | +7C2 | +7C4 |

| +7BA | +7BC | +7BE |
| --- | --- | --- |
| +7FA | +7FC | +7FE |



<!-- Page 85 -->

**Figure 4.8 Data configuration of pattern name tables (continued)**

| Character Pattern 0-0 Pattern Name Data (Most significant word) |
| --- |
| Character Pattern 0-0 Pattern Name Data (Least significant word) |
| Character Pattern 0-1 Pattern Name Data (Most significant word) |

| Character Pattern 63-63 Pattern Name Data (Most significant word) |
| --- |
| Character Pattern 63-63 Pattern Name Data (Least significant word) |

| +0000<br>+0002 |   | +0008<br>+000A |
| --- | --- | --- |
| +0000 | +0004 |   |
| +0002 | +0006 |   |
| +0100<br>+0102 |   | +0108<br>+010A |
| +0100 | +0104 |   |
| +0102 | +0106 |   |

| +00F4 | +00F8 | +00FC |
| --- | --- | --- |
| +00F6 | +00FA | +00FE |
| +01F4<br>+01F6 | +01F8 |   |
|   |   | +01FC |
|   | +01FA | +01FE |

| +3E00<br>+3E02 |   | +3E08 |
| --- | --- | --- |
|   | +3E04 |   |
|   | +3E06 | +3E0A |
| +3F00 | +3F04 | +3F08<br>+3F0A |
| +3F02 | +3F06 |   |

| +3EF4 | +3EF8 | +3EFC |
| --- | --- | --- |
| +3EF6 | +3EFA | +3EFE |
| +3FF4<br>+3FF6 | +3FF8 | +3FFC |
|   | +3FFA | +3FFE |



<!-- Page 86 -->

**Figure 4.8 Data configuration of pattern name tables (continued)**

| Character Pattern 0-0 Pattern Name Data (Most significant word) |
| --- |
| Character Pattern 0-0 Pattern Name Data (Least significant word) |
| Character Pattern 0-1 Pattern Name Data (Most significant word) |

| Character Pattern 31-31 Pattern Name Data (Most significant word) |
| --- |
| Character Pattern 31-31 Pattern Name Data (Least significant word) |

| +000<br>+002 | +004<br>+006 | +008<br>+00A |
| --- | --- | --- |
| +080<br>+082 | +084<br>+086 | +088<br>+08A |

| +074<br>+076 |   | +078<br>+07A | +07C<br>+07E |   |
| --- | --- | --- | --- | --- |
|   | +074 |   |   | +07C |
|   | +076 |   |   | +07E |
| +0F4<br>+0F6 |   | +0F8<br>+0FA | +0FC<br>+0FE |   |
|   | +0F4 |   |   | +0FC |
|   | +0F6 |   |   | +0FE |

| +000 |
| --- |
| +002 |

| +004 |
| --- |
| +006 |

| +008 |
| --- |
| +00A |

| +078 |
| --- |
| +07A |

| +080 |
| --- |
| +082 |

| +084 |
| --- |
| +086 |

| +088 |
| --- |
| +08A |

| +0F8 |
| --- |
| +0FA |

| +F00<br>+F02 |   |   | +F04<br>+F06 | +F08<br>+F0A |
| --- | --- | --- | --- | --- |
|   | +F80 |   | +F84<br>+F86 | +F88<br>+F8A |
|   | +F82 |   |   |   |

| +F74<br>+F76 |   | +F78<br>+F7A | +F7C<br>+F7E |   |
| --- | --- | --- | --- | --- |
|   | +F74 |   |   |   |
|   | +F76 |   |   |   |
| +FF4<br>+FF6 |   | +FF8<br>+FFA | +FFC<br>+FFE |   |
|   | +FF6 |   |   | FFE |

| +F04 |
| --- |
| +F06 |

| +F08 |
| --- |
| +F0A |

| +F78 |
| --- |
| +F7A |



<!-- Page 87 -->

### Pattern Name Data

Pattern name data is composed of the following four fields, for a total of 26 bits.

- Character number
15 bits

- Palette number
7 bits

- Special function bits
2 bits

- Reverse function bits
2 bits The character number designates the address of the character pattern (VRAM). The palette number designates the address of the palette (color RAM) used by the character. The special function bits designate whether that character will use the special function. The reverse function bits designate whether to use the up-down reverse or left-right reverse functions. The size of the pattern name data in the pattern name table can select either 1-word or 2-word. Because all required pattern name data cannot be designated when 1 word is selected, it is supplemented by auxiliary data of the least significant 10 bits of the pattern name control register. The composition of pattern name data changes depending on character size, character number color, and the character number auxiliary mode. The character number auxiliary mode designates the number of bits per character number when the pattern name table size in the pattern name table is 1-word, and whether that character can use the reverse function.

Table 4.5 shows the

character number auxiliary mode. Figure 4.9 shows the configuration of 2-word pattern name data, and Figure 4.10 shows the configuration of 1 word pattern name data.

**Table 4.5 Character number auxiliary mode**

.

> Note: Shaded bits are ignored

**Figure 4.9 Bit configuration when the pattern name data is 2 word**

| Character Number<br>Auxiliary Mode | Process |
| --- | --- |
| 0 | Character number that can be specified in pattern name data is 10 bits.<br>Flip function can be specified in character units. |
| 1 | Character number that can be specified in pattern name data is 12 bits.<br>Flip function cannot be used. |

|   |   |   |   |   |   |   |   |   | Palette Number |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   |   | PR | CC |   |   |   |   |   | 6 | 5 | 4 | 3 | 2 | 1 | 0 |

|   | Character Number |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |



<!-- Page 88 -->

Table 4.6 shows the bit configuration when the pattern name data is 1 word.

**Table 4.6 Bit configuration when pattern name data is 1 word.**

> Note:

| Character<br>Size | Character<br>Color Count | Auxiliary<br>Mode | Character<br>Number | Palette<br>Number | Special<br>Function | Flip Function |
| --- | --- | --- | --- | --- | --- | --- |
| 1 X 1 | 16 | 0 | 15*1 | 7 | 2 | 2 |
| 1 X 1 | 16 | 1 | 15*2 | 7 | 2 | - |
| 1 X 1 | other than 16 | 0 | 15*1 | 3 | 2 | 2 |
| 1 X 1 | other than 16 | 1 | 15*2 | 3 | 2 | - |
| 2 X 2 | 16 | 0 | 15*3 | 7 | 2 | 2 |
| 2 X 2 | 16 | 1 | 15*4 | 7 | 2 | - |
| 2 X 2 | other than 16 | 0 | 15*3 | 3 | 2 | 2 |
| 2 X 2 | other than 16 | 1 | 15*4 | 3 | 2 | - |
| 2 Words |   |   | 15 | 7 | 2 | 2 |



<!-- Page 89 -->

> Note: Both vertical and horizont al flip function bits are set to 0.

> Note: Shaded bit s are ignored

> Note: Shaded bit is ignored

> Note: Shaded bits are ignored

**Figure 4.10 Configuration when pattern name data is one word**

| Palette Number |   |   |   |   |   | Character Number |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 3 | 2 | 1 | 0 |   |   | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |

|   |   | Palette No. |   |   | Character Number |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PR | CC | 6 | 5 | 4 | 14 | 13 | 12 | 11 | 10 |

| Palette Number |   |   |   | Character Number |   |   |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 3 | 2 | 1 | 0 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |

|   |   | Palette No. |   |   | Character Number |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PR | CC | 6 | 5 | 4 | 14 | 13 | 12 |

| Palette Number |   |   |   |   |   | Character Number |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   | 6 | 5 | 4 |   |   | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |

|   |   | Palette No. |   |   | Character Number |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PR | CC |   |   |   | 14 | 13 | 12 | 11 | 10 |



<!-- Page 90 -->

> Note: Both vertical and horizont al flip function bits are set to 0.

> Note: Shaded bits are ignored

> Note: Shaded bits are ignored

**Figure 4.10 Configuration when pattern name data is one word (continued)**

| Palette Number |   |   |   | Character Number |   |   |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   | 6 | 5 | 4 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |

|   |   | Palette No. |   |   | Character Number |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PR | CC |   |   |   | 14 | 13 | 12 |

| Palette Number |   |   |   |   |   | Character Number |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 3 | 2 | 1 | 0 |   |   | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 |

|   |   | Palette No. |   |   | Character Number |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PR | CC | 6 | 5 | 4 | 14 | 13 | 12 | 1 | 0 |

| Palette Number |   |   |   | Character Number |   |   |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 3 | 2 | 1 | 0 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 |

|   |   | Palette No. |   |   | Character Number |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PR | CC | 6 | 5 | 4 | 14 |   |   | 1 | 0 |



<!-- Page 91 -->

> Note: Shaded bit is ignored

> Note: Shaded bit s are ignored

> Note: Bot h vert ical and horizontal flip function bits are set to 0.

**Figure 4.10 Configuration when pattern name data is one word (continued)**

| Palette Number |   |   |   |   |   | Character Number |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   | 6 | 5 | 4 |   |   | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 |

|   |   | Palette No. |   |   | Character Number |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PR | CC |   |   |   | 14 | 13 | 12 | 1 | 0 |

| Palette Number |   |   |   | Character Number |   |   |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   | 6 | 5 | 4 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 |

|   |   | Palette No. |   |   | Character Number |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PR | CC |   |   |   | 14 |   |   | 1 | 0 |



<!-- Page 92 -->

### Character Number

The character number is 15-bit data, and designates the address of the character pattern being displayed in that position. The boundary of the character pattern from this character number is always 20H. Moreover, when the VRAM size is 4M bits, the most significant bit of the character number (bit 14) is not used.

### Palette Number

The palette number is 7-bit data, and designates the address of the color palette used in the character pattern being displayed in that position. This data can be used only when the color format is the palette format, not the RGB format. The palette number is added to the dot color code of the character pattern. Because there is a total of 11 bits of dot color data, the bits that are used change depending on the character color number. Figure 4.11 shows the configuration of 11-bit dot color data.

**Figure 4.11 Dot color data by character color number**

### Special Function Bit

The special function bit is 2-bit data, and designates whether to use the special function for the character pattern being displayed at that position. The special function bit has a special priority bit that controls the priority number, and a special color calculation bit that controls color operation. See “11.2 Special Priority Function” for more about the special priority bit, and “12.3 Special Color Calculation Function” for more about the special color calculation bit.

| Palette Number |   |   |   |   |   |   | Dot Color Code |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 6 | 5 | 4 | 3 | 2 | 1 | 0 | 3 | 2 | 1 | 0 |

|   | Palette No. |   |   |   | Dot Color Code |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 6 |   | 5 | 4 |   | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |

| Dot Color Code |   |   |   |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |



<!-- Page 93 -->

### Reverse (Flip) Function Bit

The reverse function bit is 2-bit data, and designates whether to use the reverse function for the character pattern being displayed at that position. The reverse function bit has a top-bottom reverse bit that reverses the top and bottom of a character pattern, and a left-right reverse bit that reverses left and right. The reverse function bit is shown in

**Table 4.7, and a reverse display of a character pattern in**

shown in Figure 4.12.

**Table 4.7 Reverse Function Bit**

**Figure 4.12 Reverse display of character patterns**

| Vertical Flip Bit | Horizontal Flip Bit | Process |
| --- | --- | --- |
| 0 | 0 | Cannot flip vertically or horizontally |
| 0 | 1 | Horizontal flipping only |
| 1 | 0 | Vertical flipping only |
| 1 | 1 | Can flip both vertically and horizontally |



<!-- Page 94 -->

### Pattern Name Control Register

The pattern name control register assigns pattern name data size, character number supplement mode, and pattern name supplement data. This register is a write only 16-bit register located in addresses 180030H to 180038H. Because the value of the register is cleared to 0 after the power is turned on or reset, the value must be set. Designates the pattern name data size when displaying in the cell format.

| PNCN0 | N0PNB | N0CNSM | ~ | ~ | ~ | ~ | N0SPR | N0SCC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180030H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N0SPLT6 | N0SPLT5 | N0SPLT4 | N0SCN4 | N0SCN3 | N0SCN2 | N0SCN1 | N0SCN0 |

| PNCN1 | N1PNB | N1CNSM | ~ | ~ | ~ | ~ | N1SPR | N1SCC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180032H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N1SPLT6 | N1SPLT5 | N1SPLT4 | N1SCN4 | N1SCN3 | N1SCN2 | N1SCN1 | N1SCN0 |

| PNCN2 | N2PNB | N2CNSM | ~ | ~ | ~ | ~ | N2SPR | N2SCC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180034H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N2SPLT6 | N2SPLT5 | N2SPLT4 | N2SCN4 | N2SCN3 | N2SCN2 | N2SCN1 | N2SCN0 |

| PNCN3 | N3PNB | N3CNSM | ~ | ~ | ~ | ~ | N3SPR | N3SCC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180036H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N3SPLT6 | N3SPLT5 | N3SPLT4 | N3SCN4 | N3SCN3 | N3SCN2 | N3SCN1 | N3SCN0 |

| PNCR | R0PNB | R0CNSM | ~ | ~ | ~ | ~ | R0SPR | R0SCC |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180038H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | R0SPLT6 | R0SPLT5 | R0SPLT4 | R0SCN4 | R0SCN3 | R0SCN2 | R0SCN1 | R0SCN0 |

| N0PNB | 180030H | Bit 15 | For NBG0 (or RBG 1) |
| --- | --- | --- | --- |
| N1PNB | 180032H | Bit 15 | For NBG1 |
| N2PNB | 180034H | Bit 15 | For NBG2 |
| N3PNB | 180036H | Bit 15 | For NBG3 |
| R0PNB | 180038H | Bit 15 | For RBG0 |



<!-- Page 95 -->

> Note: N0, N1, N3, or R0 is entered in bit name for xx.

Designates the character number supplement mode when the pattern name data size in the pattern name table is 1-word.

> Note: N0, N1, N2, N3, or R0 is entered in bit name for xx.

Designates the pattern name supplement data as the special priority bit when the pattern name data size is 1-word. See “11.2 Special Color Priority Function” for how this bit is used.

| xxPNB | Pattern Name Data Size |
| --- | --- |
| 0 | 2 Words |
| 1 | 1 Word |

| N0CNSM | 180030H | Bit 14 | For NBG0 (or RBG 1) |
| --- | --- | --- | --- |
| N1CNSM | 180032H | Bit 14 | For NBG1 |
| N2CNSM | 180034H | Bit 14 | For NBG2 |
| N3CNSM | 180036H | Bit 14 | For NBG3 |
| R0CNSM | 180038H | Bit 14 | For RBG0 |

| xxCNSM | Character Number<br>Auxiliary Mode | Process |
| --- | --- | --- |
| 0 | 0 | Character number in pattern name data is 10 bits<br>Flip function can be selected in character units. |
| 1 | 1 | Character number in pattern name data is 12 bits<br>Flip function cannot be used. |

| N0SPR | 180030H | Bit 9 | For NBG0 (or RBG 1) |
| --- | --- | --- | --- |
| N1SPR | 180032H | Bit 9 | For NBG1 |
| N2SPR | 180034H | Bit 9 | For NBG2 |
| N3SPR | 180036H | Bit 9 | For NBG3 |
| R0SPR | 180038H | Bit 9 | For RBG0 |



<!-- Page 96 -->

The special color calculation bit is designated as pattern name supplement data when the pattern name data size is 1-word. See “12.2 Special Color Calculation Function” to learn how this bit is used. Designates the palette number bit as pattern name supplement data when the pattern name data size is 1-word. Three bits are added to the palette number bit of the pattern name data for the supplementary palette number bit. Designates the character number bit as the pattern name supplement data when the pattern name data size is 1-word. Five bits are added to the palette number bit of the pattern name data for the supplementary palette number bit.

| N0SCC | 180030H | Bit 8 | For NBG0 (or RBG 1) |
| --- | --- | --- | --- |
| N1SCC | 180032H | Bit 8 | For NBG1 |
| N2SCC | 180034H | Bit 8 | For NBG2 |
| N3SCC | 180036H | Bit 8 | For NBG3 |
| R0SCC | 180038H | Bit 8 | For RBG0 |

| N0SPLT6~N0SPLT4 | 180030H | Bit 7~5 | For NBG0 (or RBG 1) |
| --- | --- | --- | --- |
| N1SPLT6~N1SPLT4 | 180032H | Bit 7~5 | For NBG1 |
| N2SPLT6~N2SPLT4 | 180034H | Bit 7~5 | For NBG2 |
| N3SPLT6~N3SPLT4 | 180036H | Bit 7~5 | For NBG3 |
| R0SPLT6~R0SPLT4 | 180038H | Bit 7~5 | For RBG0 |

| N0SCN4~N0SCN0 | 180030H | Bit 4~0 | For NBG0 (or RBG 1) |
| --- | --- | --- | --- |
| N1SCN4~N1SCN0 | 180032H | Bit 4~0 | For NBG1 |
| N2SCN4~N2SCN0 | 180034H | Bit 4~0 | For NBG2 |
| N3SCN4~N3SCN0 | 180036H | Bit 4~0 | For NBG3 |
| R0SCN4~R0SCN0 | 180038H | Bit 4~0 | For RBG0 |



<!-- Page 97 -->

## 4.7 Planes

Plane arranges the pattern name table (page) in sizes of 1 x 1, 2 x 1, or 2 x 2. Size is designated in its respective register.

### Plane Size

When the plane consists of more than one pattern name table (page), the pattern name table used by one plane should be linked to VRAM and stored. Figure 4.13 shows the relationship of the pattern name table arranged by plane size (number of plane page) and pattern name table.

**Figure 4.13 Arrangement of pattern name table by plane size**

| Page 0 |
| --- |
| Page 1 |
| Page 2 |
| Page 3 |
| Page 4 |

| Page 0 |
| --- |
| f |

| Page 0 | Page 1 |
| --- | --- |

| Page 0 | Page 1 |
| --- | --- |
| Page 2 | Page 3 |



<!-- Page 98 -->

### Plane Size Register

The plane size register controls the plane size and setting of the screen-over process of the rotation scroll surface. This register is a write only 16-bit register located at address 18003AH. Because the value of the register is cleared to 0 after the power is turned on or reset, the value must be set. Designates the plane size (number of pages) of each scroll screen.

> Note: N0, N1, N2, N3, RA, or RB is entered in bit name for xx.

When the reduction display is set up to a factor of 1/4 in NBG0 and NBG1, do not set the plane size of that screen to 2 H pages x 2 V pages.

| PLSZ | RBOVR1 | RBOVR0 | RBPLSZ1 | RBPLSZ0 | RAOVR1 | RAOVR0 | RAPLSZ1 | RAPLSZ0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18003AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N3PLSZ1 | N3PLSZ0 | N2PLSZ1 | N2PLSZ0 | N1PLSZ1 | N1PLSZ0 | N0PLSZ1 | N0PLSZ0 |

| N0PLSZ1, N0PLSZ0 | 18003AH | Bit 1,0 | For NBG0 |
| --- | --- | --- | --- |
| N1PLSZ1, N1PLSZ0 | 18003AH | Bit 3,2 | For NBG1 |
| N2PLSZ1, N2PLSZ0 | 18003AH | Bit 5,4 | For NBG2 |
| N3PLSZ1, N3PLSZ0 | 18003AH | Bit 7,6 | For NBG3 |
| RAPLSZ1, RAPLSZ0 | 18003AH | Bit 9,8 | For Rotation Parameter A |
| RBPLSZ1, RBPLSZ0 | 18003AH | Bit 13,12 | For Rotation Parameter B |

| xxPLSZ1 | xxPLSZ0 | Plane Size |
| --- | --- | --- |
| 0 | 0 | 1 H Page X 1 V Page |
| 0 | 1 | 2 H Pages X 1 V Page |
| 1 | 0 | Invalid (Do not set.) |
| 1 | 1 | 2 H Pages X 2 V Pages |



<!-- Page 99 -->

Designates control (screen-over process) when the display coordinate value exceeds the display area in the rotation scroll surface.

> Note: A or B is entered in bit name for x.

When the rotation scroll surface is in bit map, the character pattern designated by the screen-over pattern name register must not be set to repeat process. With the rotation scroll surface in bit map, and when the length of the bit map is 256 dots, if the display area is set to 0 ≤ X < 512 and 0 ≤ Y < 512 and all the outer area is set to be transparent, two of the same images will be displayed for each 256 V dots.

| RAOVR1, RAOVR0 | 18003AH | Bit 11,10 | For Rotation Parameter A |
| --- | --- | --- | --- |
| RBOVR1, RBOVR0 | 18003AH | Bit 15,14 | For Rotation Parameter B |

| RxOVR1 | RxOVR0 | Screen Over Process |
| --- | --- | --- |
| 0 | 0 | Outside the display area, the image set in the display area is repeate |
| 0 | 1 | Outside the display area, the character pattern specified by screen ov<br>pattern name register is repeated. (Only when the rotation scroll<br>surface is in cell format.) |
| 1 | 0 | Outside the display area, the scroll screen is transparent, |
| 1 | 1 | Set the display area as 0£ X£ 512, 0£ Y£ 512 regardless of plane size<br>bitmap size and make that area transparent. |



<!-- Page 100 -->

## 4.8

## Maps

Maps are square patterns consisting of 2 x 2 or 4 x 4 planes. A map of a Normal scroll screen consists of a 2 x 2 plane, and a map of a rotation scroll surface consists of a 4 x 4 plane. The method of arranging the plane is made by selecting the pattern name table lead address in various plane registers.

### Map Selection Register

Maps are organized into four planes (normal scroll screen) or 16 planes (rotation scroll surface). Each screen has for each plane number a 6-bit map register to select the pattern name table lead address for various planes. It also has a map offset register of three bits added to the highest map register. The total 9-bit map selection register changes the bit used and the register displaying the address value, depending on the pattern name data size and character size. Figure 4.14 shows the relationship of the map register and map offset register.

**Figure 4.14 Map selection register**

Table 4.8 shows the address values of register and bits that are used for the map

selection register by the pattern name data size and character size.

| Map Offset Register |   |   |
| --- | --- | --- |
| 8 | 7 | 6 |

| Map RegisterA |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- |
| 5 | 4 | 3 | 2 | 1 | 0 |

| Map Register B |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- |
| 5 | 4 | 3 | 2 | 1 | 0 |

| Map Offset Register |   |   | Map Register |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |



<!-- Page 101 -->

**Table 4.8 Address value of map designated register by setting**

> Note: When the VRAM capacity is set at 4M bits, the most significant bit among the bits used is not

| Plane Size | Pattern Name<br>Data Size | Character Size | Bits and Addresses |
| --- | --- | --- | --- |
|   | 1 Word | 1 H Cell X 1 V Cell | (Value of bit 6~0) X 2000H |
| 1 H page X |   | 2 H Cells X 2 V Cells | (Value of bit 8~0) X 800H |
| 1 V page | 2 Words | 1 H Cell X 1 V Cell | (Value of bit 5~0) X 4000H |
|   |   | 2 H Cells X 2 V Cells | (Value of bit 7~0) X 1000H |
|   | 1 Word | 1 H Cell X 1 V Cell | (Value of bit 6~1) X 4000H |
| 2 H pages X |   | 2 H Cells X 2 V Cells | (Value of bit 8~1) X 1000H |
| 1 V page | 2 Words | 1 H Cell X 1 V Cell | (Value of bit 5~1) X 8000H |
|   |   | 2 H Cells X 2 V Cells | (Value of bit 7~1) X 2000H |
|   | 1 Word | 1 H Cell X 1 V Cell | (Value of bit 6~2) X 8000H |
| 2 H pages X |   | 2 V Cells X 2 V Cells | (Value of bit 8~2) X 2000H |
| 2 V pages | 2 Words | 1 H Cell X 1 V Cell | (Value of bit 5~2) X 10000H |
|   |   | 2 H Cells X 2 V Cells | (Value of bit 7~2) X 4000H |



<!-- Page 102 -->

### Map Size

Map size (number of planes in the map) will change depending on if the screen is a normal scroll screen or rotation scroll surface. The normal scroll screen has a map 2 H planes X 2 V planes in each screen. The rotation scroll surface has a map 4 H planes X 4 V planes in both of rotation parameters A and B. Figure 4.15 shows the plane arrangements of different map sizes.

**Figure 4.15 Map size**

| Plane | Plane |
| --- | --- |
| Plane | Plane |

| Plane | Plane | Plane | Plane |
| --- | --- | --- | --- |
| Plane | Plane | Plane | Plane |
| Plane | Plane | Plane | Plane |
| Plane | Plane | Plane | Plane |



<!-- Page 103 -->

When NBG0 and NBG1 enable bits (N0ZMQT and N1ZMQT) are set to allow reduction up to a factor of 1/4, the map size of NBG0 and NBG1 become normal. A set screen plane size, that can be reduced up to 1/4 should not be 2 H pages X 2 V pages. Figure 4.16 shows the map size by the reduction setting.

**Figure 4.16 Plane arrangement of map by reduction settings**

### Map Offset Register

The map offset register designates the map offset value. This is a write-only 16-bit register, with addresses located at 18003CH to 18003EH. Because the value of the register is cleared to 0 after the power is turned on or reset, the value must be set.

| Plane A<br>For NBG0 | Plane B<br>For NBG0 |
| --- | --- |
| Plane C<br>For NBG0 | Plane D<br>For NBG0 |
| Plane A<br>For NBG2 | Plane B<br>For NBG2 |
| Plane C<br>For NBG2 | Plane D<br>For NBG2 |

| Plane A<br>For NBG1 | Plane B<br>For NBG1 |
| --- | --- |
| Plane C<br>For NBG1 | Plane D<br>For NBG1 |
| Plane A<br>For NBG3 | Plane B<br>For NBG3 |
| Plane C<br>For NBG3 | Plane D<br>For NBG3 |

| MPOFN | ~ | N3MP8 | N3MP7 | N3MP6 | ~ | N2MP8 | N2MP7 | N2MP6 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18003CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | N1MP8 | N1MP7 | N1MP6 | ~ | N0MP8 | N0MP7 | N0MP6 |

| MPOFR | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18003EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | RBMP8 | RBMP7 | RBMP6 | ~ | RAMP8 | RAMP7 | RAMP6 |



<!-- Page 104 -->

When the scroll screen display format is the cell format, the map offset value of 3 bits is added to the highest 6 bits of the map register. This designates the bit map pattern boundary when in the bit map format. Boundary address of the bit map pattern is shown below: (boundary address value of the bit map pattern) = (map offset register value 3 bit) x 20000H.

| N0MP8~N0MP6 | 18003CH | Bit 2~0 | For NBG0 |
| --- | --- | --- | --- |
| N1MP8~N1MP6 | 18003CH | Bit 6~4 | For NBG1 |
| N2MP8~N2MP6 | 18003CH | Bit 10~8 | For NBG2 |
| N3MP8~N3MP6 | 18003CH | Bit 14~12 | For NBG3 |
| RAMP8~RAMP6 | 18003EH | Bit 2~0 | For Rotation Parameter A |
| RBMP8~RBMP6 | 18003EH | Bit 6~4 | For Rotation Parameter B |



<!-- Page 105 -->

### Normal Scroll Screen Map Register

Normal scroll screen map register designates the lead address of the pattern name table of each plane when the normal scroll screen is displayed in the cell format. This register is a write-only 16-bit register, with addresses located at 180040H to 18004EH. Because the value of the register is cleared to 0 after the power is turned on or reset, the value must be set.

| MPABN0 | ~ | ~ | N0MPB5 | N0MPB4 | N0MPB3 | N0MPB2 | N0MPB1 | N0MPB0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180040H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N0MPA5 | N0MPA4 | N0MPA3 | N0MPA2 | N0MPA1 | N0MPA0 |

| MPCDN0 | ~ | ~ | N0MPD5 | N0MPD4 | N0MPD3 | N0MPD2 | N0MPD1 | N0MPD0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180042H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N0MPC5 | N0MPC4 | N0MPC3 | N0MPC2 | N0MPC1 | N0MPC0 |

| MPABN1 | ~ | ~ | N1MPB5 | N1MPB4 | N1MPB3 | N1MPB2 | N1MPB1 | N1MPB0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180044H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N1MPA5 | N1MPA4 | N1MPA3 | N1MPA2 | N1MPA1 | N1MPA0 |

| MPCDN1 | ~ | ~ | N1MPD5 | N1MPD4 | N1MPD3 | N1MPD2 | N1MPD1 | N1MPD0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180046H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N1MPC5 | N1MPC4 | N1MPC3 | N1MPC2 | N1MPC1 | N1MPC0 |

| MPABN2 | ~ | ~ | N2MPB5 | N2MPB4 | N2MPB3 | N2MPB2 | N2MPB1 | N2MPB0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180048H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N2MPA5 | N2MPA4 | N2MPA3 | N2MPA2 | N2MPA1 | N2MPA0 |

| MPCDN2 | ~ | ~ | N2MPD5 | N2MPD4 | N2MPD3 | N2MPD2 | N2MPD1 | N2MPD0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18004AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N2MPC5 | N2MPC4 | N2MPC3 | N2MPC2 | N2MPC1 | N2MPC0 |

| MPABN3 | ~ | ~ | N3MPB5 | N3MPB4 | N3MPB3 | N3MPB2 | N3MPB1 | N3MPB0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18004CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N3MPA5 | N3MPA4 | N3MPA3 | N3MPA2 | N3MPA1 | N3MPA0 |

| MPCDN3 | ~ | ~ | N3MPD5 | N3MPD4 | N3MPD3 | N3MPD2 | N3MPD1 | N3MPD0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18004EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | N3MPC5 | N3MPC4 | N3MPC3 | N3MPC2 | N3MPC1 | N3MPC0 |



<!-- Page 106 -->

The lead address for the pattern name table is designated for each plane, when the Normal scroll screen is displayed by the cell format.

| N0MPA5~N0MPA0 | 180040H | Bit 5~0 | For NBG0 Plane A |
| --- | --- | --- | --- |
| N0MPB5~N0MPB0 | 180040H | Bit 13~8 | For NBG0 Plane B |
| N0MPC5~N0MPC0 | 180042H | Bit 5~0 | For NBG0 Plane C |
| N0MPD5~N0MPD0 | 180042H | Bit 13~8 | For NBG0 Plane D |
| N1MPA5~N1MPA0 | 180044H | Bit 5~0 | For NBG1 Plane A |
| N1MPB5~N1MPB0 | 180044H | Bit 13~8 | For NBG1 Plane B |
| N1MPC5~N1MPC0 | 180046H | Bit 5~0 | For NBG1 Plane C |
| N1MPD5~N1MPD0 | 180046H | Bit 13~8 | For NBG1 Plane D |
| N2MPA5~N2MPA0 | 180048H | Bit 5~0 | For NBG2 Plane A |
| N2MPB5~N2MPB0 | 180048H | Bit 13~8 | For NBG2 Plane B |
| N2MPC5~N2MPC0 | 18004AH | Bit 5~0 | For NBG2 Plane C |
| N2MPD5~N2MPD0 | 18004AH | Bit 13~8 | For NBG2 Plane D |
| N3MPA5~N3MPA0 | 18004CH | Bit 5~0 | For NBG3 Plane A |
| N3MPB5~N3MPB0 | 18004CH | Bit 13~8 | For NBG3 Plane B |
| N3MPC5~N3MPC0 | 18004EH | Bit 5~0 | For NBG3 Plane C |
| N3MPD5~N3MPD0 | 18004EH | Bit 13~8 | For NBG3 Plane D |



<!-- Page 107 -->

### Rotation Scroll Surface Map Register

The Rotation Scroll Surface Map Register designates the lead address of the pattern name table arranged in each plane by rotation parameters A and B. When a writeonly 16-bit register, with addresses located at 180050H to 18006EH. Because the value of the register is cleared to 0 after the power is turned on or reset, the value must be set.

| MPABRA | ~ | ~ | RAMPB5 | RAMPB4 | RAMPB3 | RAMPB2 | RAMPB1 | RAMPB0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180050H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RAMPA5 | RAMPA4 | RAMPA3 | RAMPA2 | RAMPA1 | RAMPA0 |

| MPCDRA | ~ | ~ | RAMPD5 | RAMPD4 | RAMPD3 | RAMPD2 | RAMPD1 | RAMPD0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180052H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RAMPC5 | RAMPC4 | RAMPC3 | RAMPC2 | RAMPC1 | RAMPC0 |

| MPEFRA | ~ | ~ | RAMPF5 | RAMPF4 | RAMPF3 | RAMPF2 | RAMPF1 | RAMPF0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180054H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RAMPE5 | RAMPE4 | RAMPE3 | RAMPE2 | RAMPE1 | RAMPE0 |

| MPGHRA | ~ | ~ | RAMPH5 | RAMPH4 | RAMPH3 | RAMPH2 | RAMPH1 | RAMPH0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180056H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RAMPG5 | RAMPG4 | RAMPG3 | RAMPG2 | RAMPG1 | RAMPG0 |

| MPIJRA | ~ | ~ | RAMPJ5 | RAMPJ4 | RAMPJ3 | RAMPJ2 | RAMPJ1 | RAMPJ0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180058H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RAMPI5 | RAMPI4 | RAMPI3 | RAMPI2 | RAMPI1 | RAMPI0 |

| MPKLRA | ~ | ~ | RAMPL5 | RAMPL4 | RAMPL3 | RAMPL2 | RAMPL1 | RAMPL0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18005AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RAMPK5 | RAMPK4 | RAMPK3 | RAMPK2 | RAMPK1 | RAMPK0 |

| MPMNRA | ~ | ~ | RAMPN5 | RAMPN4 | RAMPN3 | RAMPN2 | RAMPN1 | RAMPN0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18005CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RAMPM5 | RAMPM4 | RAMPM3 | RAMPM2 | RAMPM1 | RAMPM0 |

| MPOPRA | ~ | ~ | RAMPP5 | RAMPP4 | RAMPP3 | RAMPP2 | RAMPP1 | RAMPP0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18005EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RAMPO5 | RAMPO4 | RAMPO3 | RAMPO2 | RAMPO1 | RAMPO0 |



<!-- Page 108 -->

| MPABRB | ~ | ~ | RBMPB5 | RBMPB4 | RBMPB3 | RBMPB2 | RBMPB1 | RBMPB0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180060H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RBMPA5 | RBMPA4 | RBMPA3 | RBMPA2 | RBMPA1 | RBMPA0 |

| MPCDRB | ~ | ~ | RBMPD5 | RBMPD4 | RBMPD3 | RBMPD2 | RBMPD1 | RBMPD0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180062H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RBMPC5 | RBMPC4 | RBMPC3 | RBMPC2 | RBMPC1 | RBMPC0 |

| MPEFRB | ~ | ~ | RBMPF5 | RBMPF4 | RBMPF3 | RBMPF2 | RBMPF1 | RBMPF0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180064H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RBMPE5 | RBMPE4 | RBMPE3 | RBMPE2 | RBMPE1 | RBMPE0 |

| MPGHRB | ~ | ~ | RBMPH5 | RBMPH4 | RBMPH3 | RBMPH2 | RBMPH1 | RBMPH0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180066H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RBMPG5 | RBMPG4 | RBMPG3 | RBMPG2 | RBMPG1 | RBMPG0 |

| MPIJRB | ~ | ~ | RBMPJ5 | RBMPJ4 | RBMPJ3 | RBMPJ2 | RBMPJ1 | RBMPJ0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180068H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RBMPI5 | RBMPI4 | RBMPI3 | RBMPI2 | RBMPI1 | RBMPI0 |

| MPKLRB | ~ | ~ | RBMPL5 | RBMPL4 | RBMPL3 | RBMPL2 | RBMPL1 | RBMPL0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18006AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RBMPK5 | RBMPK4 | RBMPK3 | RBMPK2 | RBMPK1 | RBMPK0 |

| MPMNRB | ~ | ~ | RBMPN5 | RBMPN4 | RBMPN3 | RBMPN2 | RBMPN1 | RBMPN0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18006CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RBMPM5 | RBMPM4 | RBMPM3 | RBMPM2 | RBMPM1 | RBMPM0 |

| MPOPRB | ~ | ~ | RBMPP5 | RBMPP4 | RBMPP3 | RBMPP2 | RBMPP1 | RBMPP0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18006EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | RBMPO5 | RBMPO4 | RBMPO3 | RBMPO2 | RBMPO1 | RBMPO0 |



<!-- Page 109 -->

Map bit (for rotation scroll): Map bit (RAMPA5 to RAMPA0, RAMPB5 to RAMPB0, RAMPC5 to RAMPC0, RAMPD5 to RAMPD0, RAMPE5 to RAMPE0, RAMPF5 to RAMPF0, RAMPG5 to RAMPG0, RAMPH5 to RAMPH0, RAMPI5 to RAMPI0, RAMPJ5 to RAMPJ0, RAMPK5 to RAMPK0, RAMPL5 to RAMPL0, RAMPM5 to RAMPM0, RAMPN5 to RAMPN0, RAMPO5 to RAMPO0, RAMPP5 to RAMPP0, RBMPA5 to RBMPA0, RBMPB5 to RBMPB0, RBMPC5 to RBMPC0, RBMPD5 to RBMPD0, RBMPE5 to RBMPE0, RBMPF5 to RBMPF0, RBMPG5 to RBMPG0, RBMPH5 to RBMPH0, RAMPI5 to RBMPI0, RBMPJ5 to RBMPJ0, RBMPK5 to RBMPK0, RBMPL5 to RBMPL0, RBMPM5 to RBMPM0, RBMPN5 to RBMPN0, RBMPO5 to RBMPO0, RBMPP5 to RBMPP0) When a rotation scroll surface is displayed in the cell format, it designates the lead address of the pattern name table being arranged in each plane . ST-58-R2



<!-- Page 110 -->

| RAMPA5~RAMPA0 | 180050H | Bit 5~0 | Rotation Parameter A for Screen Plane A |
| --- | --- | --- | --- |
| RAMPB5~RAMPB0 | 180050H | Bit 13~8 | Rotation Parameter A for Screen Plane B |
| RAMPC5~RAMPC0 | 180052H | Bit 5~0 | Rotation Parameter A for Screen Plane C |
| RAMPD5~RAMPD0 | 180052H | Bit 13~8 | Rotation Parameter A for Screen Plane D |
| RAMPE5~RAMPE0 | 180054H | Bit 5~0 | Rotation Parameter A for Screen Plane E |
| RAMPF5~RAMPF0 | 180054H | Bit 13~8 | Rotation Parameter A for Screen Plane F |
| RAMPG5~RAMPG0 | 180056H | Bit 5~0 | Rotation Parameter A for Screen Plane G |
| RAMPH5~RAMPH0 | 180056H | Bit 13~8 | Rotation Parameter A for Screen Plane H |
| RAMPI5~RAMPI0 | 180058H | Bit 5~0 | Rotation Parameter A for Screen Plane I |
| RAMPJ5~RAMPJ0 | 180058H | Bit 13~8 | Rotation Parameter A for Screen Plane J |
| RAMPK5~RAMPK0 | 18005AH | Bit 5~0 | Rotation Parameter A for Screen Plane K |
| RAMPL5~RAMPL0 | 18005AH | Bit 13~8 | Rotation Parameter A for Screen Plane L |
| RAMPM5~RAMPM0 | 18005CH | Bit 5~0 | Rotation Parameter A for Screen Plane M |
| RAMPN5~RAMPN0 | 18005CH | Bit 13~8 | Rotation Parameter A for Screen Plane N |
| RAMPO5~RAMPO0 | 18005EH | Bit 5~0 | Rotation Parameter A for Screen Plane O |
| RAMPP5~RAMPP0 | 18005EH | Bit 13~8 | Rotation Parameter A for Screen Plane P |
| RBMPA5~RBMPA0 | 180060H | Bit 5~0 | Rotation Parameter B for Screen Plane A |
| RBMPB5~RBMPB0 | 180060H | Bit 13~8 | Rotation Parameter B for Screen Plane B |
| RBMPC5~RBMPC0 | 180062H | Bit 5~0 | Rotation Parameter B for Screen Plane C |
| RBMPD5~RBMPD0 | 180062H | Bit 13~8 | Rotation Parameter B for Screen Plane D |
| RBMPE5~RBMPE0 | 180064H | Bit 5~0 | Rotation Parameter B for Screen Plane E |
| RBMPF5~RBMPF0 | 180064H | Bit 13~8 | Rotation Parameter B for Screen Plane F |
| RBMPG5~RBMPG0 | 180066H | Bit 5~0 | Rotation Parameter B for Screen Plane G |
| RBMPH5~RBMPH0 | 180066H | Bit 13~8 | Rotation Parameter B for Screen Plane H |
| RBMPI5~RBMPI0 | 180068H | Bit 5~0 | Rotation Parameter B for Screen Plane I |
| RBMPJ5~RBMPJ0 | 180068H | Bit 13~8 | Rotation Parameter B for Screen Plane J |
| RBMPK5~RBMPK0 | 18006AH | Bit 5~0 | Rotation Parameter B for Screen Plane K |
| RBMPL5~RBMPL0 | 18006AH | Bit 13~8 | Rotation Parameter B for Screen Plane L |
| RBMPM5~RBMPM0 | 18006CH | Bit 5~0 | Rotation Parameter B for Screen Plane M |
| RBMPN5~RBMPN0 | 18006CH | Bit 13~8 | Rotation Parameter B for Screen Plane N |
| RBMPO5~RBMPO0 | 18006EH | Bit 5~0 | Rotation Parameter B for Screen Plane O |
| RBMPP5~RBMPP0 | 18006EH | Bit 13~8 | Rotation Parameter B for Screen Plane P |



<!-- Page 111 -->

## 4.9 Bit Maps

When displaying the bit map format, select from sizes, 512 H dots x 256 V dots, 512 H dots x 512 V dots, 1024 H dots x 256 V dots, or 1024 H dots X 512 V dots. All dot bit map pattern data is stored in the VRAM.

### Bit Map Size

Different types of bit map sizes can be selected by the normal scroll screen and rotation scroll surface. When a high-resolution graphics mode that is greater than 512 H pixels is selected in a Normal scroll screen, if a 512 H dot bit map size is selected, the same picture is repeated in the horizontal direction. Furthermore, when the vertical resolution selects the exclusive monitor mode or double-density interlace mode with more than 256 pixels when a 256 V dot bit map size is selected, the same picture is repeated in the vertical direction.

Table 4.9 shows bit map sizes.

**Table 4.9 Bit map size**

### Bit Map Color Number

The color format for displaying the bit map format screen combines the bit map palette number and dot color code within bit map pattern data. It has a palette format, which designates the color RAM address, and RGB format that directly designates display RGB data.

Table 4.10 shows, in various color formats the bit map

surface per color number and the bit number per dot of the bit map pattern data. Furthermore, the bit map color count is set to the character color count bit of the character control register.

| Screen | Bitmap Size Selections |
| --- | --- |
|   | 512 H dots X 256 V dots |
| Normal | 512 H dots X 512 V dots |
| Scroll Screen | 1024 H dots X 256 V dots |
|   | 1024 H dots X 512 V dots |
| Rotation | 512 H dots X 256 V dots |
| Scroll Screen | 512 H dots X 512 V dots |



<!-- Page 112 -->

**Table 4.10 Bit map color count**

> Note:

| Color Format | Bitmap Color Count | Bitmap Pattern Data Bit Count For 1<br>Dot |
| --- | --- | --- |
|   | 16 colors | 4 bits |
| Palette | 256 colors | 8 bits |
|   | 2048 colors | 16 bits (only use lower 11 bits) |
| RGB | 32,768 colors | 16 bits |
|   | 16,770,000 colors | 32 bits (only use MSB and lower 24 bits |



<!-- Page 113 -->

### Bit Map Pattern

The required VRAM capacity in a 1-bit map pattern surface depends upon the bit map size and bit map color count (bit map pattern data size). Changes in the data configuration of each bit map pattern stored in VRAM are identical. The bit map size and bit map color count can be set to exceed the VRAM capacity, but the same picture would be repeated in the vertically.

Table 4.11 shows bit map pattern capaci-

ties and Figure 4.17 shows the bit map pattern configuration. The boundary that stores bit map patterns in the VRAM is 20000H, and is independent of the bit map size and the bit map color count. The designation is performed in the map offset register.

**Table 4.11 Bit map pattern capacity per 1 surface**

| Bitmap Size | Bitmap Pattern<br>Data Size | Bitmap Color Count | Size per Surface |   |
| --- | --- | --- | --- | --- |
|   | 4 bits/dot | 16 colors | 64K bytes | (512K bits) |
| 512 H dots X | 8 bits/dot | 256 colors | 128K bytes | (1M bits) |
| 256 V dots | 16 bits/dot | 2048 colors, 32,768 colors | 256K bytes | (2M bits) |
|   | 32 bits/dot | 16,770,000 colors | 512K bytes | (4M bits) |
|   | 4 bits/dot | 16 colors | 128K bytes | (1M bits) |
| 512 H dots X | 8 bits/dot | 256 colors | 256K bytes | (2M bits) |
| 512 V dots | 16 bits/dot | 2048 colors, 32,768 colors | 512K bytes | (4M bits) |
|   | 32 bits/dot | 16,770,000 colors | 1024K bytes | (8M bits) |
|   | 4 bits/dot | 16 colors | 128K bytes | (1M bits) |
| 1024 H dots X | 8 bits/dot | 256 colors | 256K bytes | (2M bits) |
| 256 V dots | 16 bits/dot | 2048 colors, 32,768 colors | 512K bytes | (4M bits) |
|   | 32 bits/dot | 16,770,000 colors | 1024K bytes | (8M bits) |
| 1024 H dots X | 4 bits/dot | 16 colors | 256K bytes | (2M bits) |
| 512 V dots | 8 bits/dot | 256 colors | 512K bytes | (4M bits) |
|   | 16 bits/dot | 2048 colors, 32,768 colors | 1024K bytes | (8M bits) |



<!-- Page 114 -->

**Figure 4.17 Bit map pattern configuration**

| Dot 0-0 | Dot 0-1 | Dot 0-2 | Dot 0-3 |
| --- | --- | --- | --- |
| Dot 0-4 | Dot 0-5 | Dot 0-6 | Dot 0-7 |

| Dot 255-508 | Dot 255-509 | Dot 255-510 | Dot 255-511 |
| --- | --- | --- | --- |

| +00 | 00 | +00 | 01 |
| --- | --- | --- | --- |
| +01 | 00 | +01 | 01 |

| +0 | 0FE | +00 | FF |
| --- | --- | --- | --- |
| +0 | 1FE | +01 | FF |

| +00 | 00 |
| --- | --- |

| +00 | 01 |
| --- | --- |

| +0 | 0FE |
| --- | --- |

| +00 | FF |
| --- | --- |

| +01 | 00 |
| --- | --- |

| +01 | 01 |
| --- | --- |

| +0 | 1FE |
| --- | --- |

| +01 | FF |
| --- | --- |

| +FE | 00 | +FE | 01 |
| --- | --- | --- | --- |
| +FF | 00 | +FF | 01 |

| +F | EFE | +FE | FE |
| --- | --- | --- | --- |
| +F | FFE | +FF | FF |

| +FE | 00 |
| --- | --- |

| +FE | 01 |
| --- | --- |

| +F | EFE |
| --- | --- |

| +FE | FE |
| --- | --- |

| +FF | 00 |
| --- | --- |

| +FF | 01 |
| --- | --- |

| +F | FFE |
| --- | --- |

| +FF | FF |
| --- | --- |



<!-- Page 115 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 | Dot 0-1 |
| --- | --- |
| Dot 0-2 | Dot 0-3 |

| Dot 255-510 | Dot 255-511 |
| --- | --- |

| +00000 |   | +00002 |   |
| --- | --- | --- | --- |
|   | +00001 | +00002 | +00003 |
| +00200 |   | +00202 |   |
|   | +00201 | +00202 | +00203 |

| +001FC | +001FD | +001FE | +001FF |
| --- | --- | --- | --- |
| +003FC | +003FD | +003FE | +003FF |

| +1FC00 | +1FC01 | +1FC02 | +1FC03 |
| --- | --- | --- | --- |
| +1FE00 | +1FE01 | +1FE02 | +1FE03 |

| +1FDFC | +1FDFD | +1FDFE | +1FDFF |
| --- | --- | --- | --- |
| +1FFFC | +1FFFD | +1FFFE | +1FFFF |



<!-- Page 116 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 |
| --- |
| Dot 0-1 |

|   | +00002 |   | +00006 |
| --- | --- | --- | --- |
| +00000 | +00002 | +00004 | +00006 |
|   | +00402 |   | +00406 |
| +00400 | +00402 | +00404 | +00406 |

| +003F8 | +003FA | +003FC | +003FE |
| --- | --- | --- | --- |
| +007F8 | +007FA | +007FC | +007FE |

| +3F800 | +3F802 | +3F804 | +3F806 |
| --- | --- | --- | --- |
| +3FC00 | +3FC02 | +3FC04 | +3FC06 |

| +3FBF8 | +3FBFA | +3FBFC | +3FBFE |
| --- | --- | --- | --- |
| +3FFF8 | +3FFFA | +3FFFC | +3FFFE |



<!-- Page 117 -->

**Figure 4.17 Bit map pattern configuration (continue)**

| Dot 0-0 (upper word) |
| --- |
| Dot 0-0 (lower word) |
| Dot 0-1 (upper word) |

| Dot 255-511 (upper word) |
| --- |
| Dot 255-511 (lower word) |

| +00000 | +00004 | +00008 |   |
| --- | --- | --- | --- |
|   |   |   | +0000C |
| +00800 | +00804 | +00808 |   |
|   |   |   | +0080C |

| +007F0 | +007F4 | +007F8 | +007FC |
| --- | --- | --- | --- |
| +00FF0 | +00FF4 | +00FF8 | +00FFC |

| +7F000 | +7F004 | +7F008 | +7F00C |
| --- | --- | --- | --- |
| +7F800 | +7F804 | +7F808 | +7F80C |

| +7F7F0 | +7F7F4 | +7F7F8 | +7F7FC |
| --- | --- | --- | --- |
| +7FFF0 | +7FFF4 | +7FFF8 | +7FFFC |



<!-- Page 118 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 | Dot 0-1 | Dot 0-2 | Dot 0-3 |
| --- | --- | --- | --- |
| Dot 0-4 | Dot 0-5 | Dot 0-6 | Dot 0-7 |

| Dot 511-508 | Dot 511-509 | Dot 511-510 | Dot 511-511 |
| --- | --- | --- | --- |

| +00 | 000 | +000 | 01 |
| --- | --- | --- | --- |
| +00 | 100 | +001 | 01 |

| +00 | 0FE | +000 | FF |
| --- | --- | --- | --- |
| +00 | 1FE | +001 | FF |

| +00 | 000 |
| --- | --- |

| +000 | 01 |
| --- | --- |

| +00 | 0FE |
| --- | --- |

| +000 | FF |
| --- | --- |

| +00 | 100 |
| --- | --- |

| +001 | 01 |
| --- | --- |

| +00 | 1FE |
| --- | --- |

| +001 | FF |
| --- | --- |

| +1F | E00 | +1F | E01 |
| --- | --- | --- | --- |
| +1F | F00 | +1FF | 01 |

| +1F | EFE | +1F | EFF |
| --- | --- | --- | --- |
| +1F | FFE | +1F | FFF |

| +1F | E00 |
| --- | --- |

| +1F | E01 |
| --- | --- |

| +1F | EFE |
| --- | --- |

| +1F | EFF |
| --- | --- |

| +1F | F00 |
| --- | --- |

| +1FF | 01 |
| --- | --- |

| +1F | FFE |
| --- | --- |

| +1F | FFF |
| --- | --- |



<!-- Page 119 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 | Dot 0-1 |
| --- | --- |
| Dot 0-2 | Dot 0-3 |

| Dot 511-510 | Dot 511-511 |
| --- | --- |

| +00000 |   | +00002 |   |
| --- | --- | --- | --- |
|   | +00001 | +00002 | +00003 |
| +00200 |   | +00202 |   |
|   | +00201 | +00202 | +00203 |

| +001FC | +001FD | +001FE | +001FF |
| --- | --- | --- | --- |
| +003FC | +003FD | +003FE | +003FF |

| +3FC00 | +3FC01 | +3FC02 | +3FC03 |
| --- | --- | --- | --- |
| +3FE00 | +3FE01 | +3FE02 | +3FE03 |

| +3FDFC | +3FDFD | +3FDFE | +3FDFF |
| --- | --- | --- | --- |
| +3FFFC | +3FFFD | +3FFFE | +3FFFF |



<!-- Page 120 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 |
| --- |
| Dot 0-1 |

| +00000 | +00002 | +00004 | +00006 |
| --- | --- | --- | --- |
| +00400 | +00402 | +00404 | +00406 |

| +003F8 | +003FA | +003FC | +003FE |
| --- | --- | --- | --- |
| +007F8 | +007FA | +007FC | +007FE |

| +7F800 | +7F802 | +7F804 | +7F806 |
| --- | --- | --- | --- |
| +7FC00 | +7FC02 | +7FC04 | +7FC06 |

| +7FBF8 | +7FBFA | +7FBFC | +7FBFE |
| --- | --- | --- | --- |
| +7FFF8 | +7FFFA | +7FFFC | +7FFFE |



<!-- Page 121 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 (upper word) |
| --- |
| Dot 0-0 (lower word) |
| Dot 0-1 (upper word) |

| Dot 511-511 (upper word) |
| --- |
| Dot 511-511 (lower word) |

|   | +00004 |   |   |
| --- | --- | --- | --- |
| +00000 | +00004 | +00008 | +0000C |
|   | +00804 |   |   |
| +00800 | +00804 | +00808 | +0080C |

| +007F0 | +007F4 | +007F8 | +007FC |
| --- | --- | --- | --- |
| +00FF0 | +00FF4 | +00FF8 | +00FFC |

| +FF000 | +FF004 | +FF008 | +FF00C |
| --- | --- | --- | --- |
| +FF800 | +FF804 | +FF808 | +FF80C |

| +FF7F0 | +FF7F4 | +FF7F8 | +FF7FC |
| --- | --- | --- | --- |
| +FFFF0 | +FFFF4 | +FFFF8 | +FFFFC |



<!-- Page 122 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 | Dot 0-1 | Dot 0-2 | Dot 0-3 |
| --- | --- | --- | --- |
| Dot 0-4 | Dot 0-5 | Dot 0-6 | Dot 0-7 |

| Dot 255-1020 | Dot 255-1021 | Dot 255-1022 | Dot 255-1023 |
| --- | --- | --- | --- |

| +00 | 000 | +000 | 01 |
| --- | --- | --- | --- |
| +00 | 200 | +002 | 01 |

| +00 | 1FE | +001 | FF |
| --- | --- | --- | --- |
| +00 | 3FE | +003 | FF |

| +00 | 000 |
| --- | --- |

| +000 | 01 |
| --- | --- |

| +00 | 1FE |
| --- | --- |

| +001 | FF |
| --- | --- |

| +00 | 200 |
| --- | --- |

| +002 | 01 |
| --- | --- |

| +00 | 3FE |
| --- | --- |

| +003 | FF |
| --- | --- |

| +1F | C00 | +1FC | 01 |
| --- | --- | --- | --- |
| +1F | E00 | +1FE | 01 |

| +1F | DFE | +1FD | FF |
| --- | --- | --- | --- |
| +1F | FFE | +1FF | FF |

| +1F | C00 |
| --- | --- |

| +1FC | 01 |
| --- | --- |

| +1F | DFE |
| --- | --- |

| +1FD | FF |
| --- | --- |

| +1F | E00 |
| --- | --- |

| +1FE | 01 |
| --- | --- |

| +1F | FFE |
| --- | --- |

| +1FF | FF |
| --- | --- |



<!-- Page 123 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 | Dot 0-1 |
| --- | --- |
| Dot 0-2 | Dot 0-3 |

| Dot 255-1022 | Dot 255-1023 |
| --- | --- |

| +00000 | +00001 | +00002 | +00003 |
| --- | --- | --- | --- |
| +00400 | +00401 | +00402 | +00403 |

| +003FC | +003FD | +003FE | +003FF |
| --- | --- | --- | --- |
| +007FC | +007FD | +007FE | +007FF |

| +3F800 | +3F801 | +3F802 | +3F803 |
| --- | --- | --- | --- |
| +3FC00 | +3FC01 | +3FC02 | +3FC03 |

| +3FBFC | +3FBFD | +3FBFE | +3FBFF |
| --- | --- | --- | --- |
| +3FFFC | +3FFFD | +3FFFE | +3FFFF |



<!-- Page 124 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 |
| --- |
| Dot 0-1 |

| +00000 | +00002 | +00004 | +00006 |
| --- | --- | --- | --- |
| +00800 | +00802 | +00804 | +00806 |

| +007F8 | +007FA | +007FC | +007FE |
| --- | --- | --- | --- |
| +00FF8 | +00FFA | +00FFC | +00FFE |

| +7F000 | +7F002 | +7F004 | +7F006 |
| --- | --- | --- | --- |
| +7F800 | +7F802 | +7F804 | +7F806 |

| +7F7F8 | +7F7FA | +7F7FC | +7F7FE |
| --- | --- | --- | --- |
| +7FFF8 | +7FFFA | +7FFFC | +7FFFE |



<!-- Page 125 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 (upper word) |
| --- |
| Dot 0-0 (lower word) |
| Dot 0-1 (upper word) |

| Dot 255-1023 (upper word) |
| --- |
| Dot 255-1023 (lower word) |

|   | +00004 |   |   |
| --- | --- | --- | --- |
| +00000 | +00004 | +00008 | +0000C |
|   | +01004 |   |   |
| +01000 | +01004 | +01008 | +0100C |

| +00FF0 | +00FF4 | +00FF8 | +00FFC |
| --- | --- | --- | --- |
| +01FF0 | +01FF4 | +01FF8 | +01FFC |

| +FE000 | +FE004 | +FE008 | +FE00C |
| --- | --- | --- | --- |
| +FF000 | +FF004 | +FF008 | +FF00C |

| +FEFF0 | +FEFF4 | +FEFF8 | +FEFFC |
| --- | --- | --- | --- |
| +FFFF0 | +FFFF4 | +FFFF8 | +FFFFC |



<!-- Page 126 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 | Dot 0-1 | Dot 0-2 | Dot 0-3 |
| --- | --- | --- | --- |
| Dot 0-4 | Dot 0-5 | Dot 0-6 | Dot 0-7 |

| Dot 511-1020 | Dot 511-1021 | Dot 511-1022 | Dot 511-1023 |
| --- | --- | --- | --- |

| +00 | 000 | +000 | 01 |
| --- | --- | --- | --- |
| +00 | 200 | +002 | 01 |

| +00 | 1FE | +001 | FF |
| --- | --- | --- | --- |
| +00 | 3FE | +003 | FF |

| +00 | 000 |
| --- | --- |

| +000 | 01 |
| --- | --- |

| +00 | 1FE |
| --- | --- |

| +001 | FF |
| --- | --- |

| +00 | 200 |
| --- | --- |

| +002 | 01 |
| --- | --- |

| +00 | 3FE |
| --- | --- |

| +003 | FF |
| --- | --- |

| +3F | C00 | +3FC | 01 |
| --- | --- | --- | --- |
| +3F | E00 | +3F | E01 |

| +3F | DFE | +3FD | FF |
| --- | --- | --- | --- |
| +3F | FFE | +3FF | FF |

| +3F | C00 |
| --- | --- |

| +3FC | 01 |
| --- | --- |

| +3F | DFE |
| --- | --- |

| +3FD | FF |
| --- | --- |

| +3F | E00 |
| --- | --- |

| +3F | E01 |
| --- | --- |

| +3F | FFE |
| --- | --- |

| +3FF | FF |
| --- | --- |



<!-- Page 127 -->

**Figure 4.17 Bit map pattern configuration (continued)**

| Dot 0-0 | Dot 0-1 |
| --- | --- |
| Dot 0-2 | Dot 0-3 |

| Dot 511-1022 | Dot 511-1023 |
| --- | --- |

|   | +00001 |   | +00003 |
| --- | --- | --- | --- |
| +00000 |   | +00002 | +00003 |
|   | +00401 |   | +00403 |
| +00400 |   | +00402 | +00403 |

| +003FC | +003FD | +003FE | +003FF |
| --- | --- | --- | --- |
| +007FC | +007FD | +007FE | +007FF |

| +7F800 | +7F801 | +7F802 | +7F803 |
| --- | --- | --- | --- |
| +7FC00 | +7FC01 | +7FC02 | +7FC03 |
