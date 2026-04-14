# Chapter 15 How to Use VDP2

15.1 Operation Flow......................................................... 262 15.2 How to Use RAM ...................................................... 264 15.3 Bit Configuration Map ............................................. 267 ST-58-R2



<!-- Page 280 -->

## 15.1 Operation Flow

Below is an overview of the steps for defining and setting VDP2 data. Step 1 Set the TV screen mode.

- 
Select normal graphics, high-resolution graphic, or exclusive monitor

- 
Set interlace mode

- 
Set vertical and horizontal resolution Step 2 Select the scroll screen to be used.

- 
Normal scroll screen (NBG0, NBG1, NBG2, NBG3)

- 
Rotation scroll screen (RBG0, RBG1)

- 
External input screen (EXBG)

- 
Line color screen (LNCL)

- 
Back screen (BACK) Step 3 Select the functions of each screen.

- 
Select cell format or bit map format (A) Cell format

- 
Character color count

- 
Character size

- 
Pattern name data size

- 
Plane size

- 
Scaling function

- 
Line scroll function

- 
Vertical cell scroll function

- 
Mosaic process function (B) Bit map format

- 
Bit map color count

- 
Bit map size

- 
Scaling function

- 
Rotation function

- 
Mosaic process function Step 4 Select the color RAM mode. Step 5 Select the window to be used. When using the Normal window, store the line window table in VRAM.



<!-- Page 281 -->

### Step 6 Calculate the size of VRAM to determine whether the tables can be stored

there.

- 
VRAM size

- 
Character pattern (number and size)

- 
Pattern name table (number and size)

- 
Bit map pattern (number and size)

- 
Line scroll table

- 
Vertical cell scroll table

- 
Rotation parameter table

- 
Coefficient table

- 
Line color screen table

- 
Back screen table

- 
Line window table Step 7 Select VRAM use.

- 
VRAM bank partition

- 
VRAM access method Step 8 Create character pattern and pattern name table.

- 
Select the character number supplement mode.

- 
Set reverse function bit. Creates a bit map pattern when in the bit map format. Step 9 Create other VRAM tables. Step 10 Define priority and color calculation in terms of sprite data. Step 11 Set special functions.

- 
Special function code

- 
Special priority function

- 
Extended color calculation function

- 
Gradation calculation function

- 
Color offset function

- 
Shadow function (Normal, MSB) Step 12 Reset the screen, redefine and reset VRAM and registers in terms of story. ST-58-R2



<!-- Page 282 -->

## 15.2 How to Use RAM

When using VDP2, data is defined and set in VRAM, color RAM, and the register.

### • VRAM

Data defined in VRAM differs depending on the scroll screen to be used, screen format, screen size, and the image process functions to be used. Data is defined in VRAM according to the register setting; defined addresses are set in the various address registers.

Table 15.1 shows the main register connected with data defined in

VRAM.



<!-- Page 283 -->

**Table 15.1 Register connected with data defined in VRAM**

Data Definition Register Setting Data Pattern Name Control Register Pattern Name Data Size, (180030H~180038H) Character Number Supplementary Mode, Pattern Name Supplementary Data Plane size register (18003AH) Plane size when in displaying in cell format Pattern name Character pattern lead Normal scroll screen map Pattern Name Data lead table address register (180040H~18004EH) address for each plane Rotation scroll screen map Pattern Name Data lead register (180050H~18006EH) address for each plane Map offset register 3 bits map offset value (18003CH~18003EH) attached to map register upper bits Character Dot data of cell Character control register Character color count, pattern (180028H, 18002AH) character size Bitmap pattern Bitmap pattern data Character control register Character color count (bitmap (180028H, 18002AH) color count), bitmap size, bitmap enable Map offset register Boundary address of bitmap (18003CH~18003EH) pattern Line scroll Horizontal and vertical Line scroll table address Line scroll table lead address table screen scroll value, register (1800A0H~1800A6H) horizontal coordinate Line & vertical cell scroll Scroll configuration control increment control register (18009AH) data Vertical cell Vertical screen scroll Vertical cell scroll table Vertical cell scroll table lead scroll table value address register (18009CH, address 18009EH) Line & vertical cell scroll Scroll configuration control control register (18009AH) data RAM control register VRAM use per rotation scroll Rotation Parameter, (18000EH) screen Rotation Coefficient Table Rotation parameter table Parameter table lead address parameter Related Registers address register (1800BCH, table 1800BEH) Rotation parameter mode Rotation parameter mode register (1800B0H) setting Coefficient table data RAM control register VRAM use per rotation scroll (Zoom Coefficients kx, (18000EH) screen Coefficient ky, and start point Coefficient table control Coefficient data mode, data table coordinate Xp after register (1800B4H) size of coefficient data, rotation conversion) coefficient table enable Coefficient table address Coefficient table lead address offset register (1800B6H) offset value Line color Color RAM address Line color screen table Line color screen color mode, screen table address register (1800A8H, line color screen table lead 1800AAH) address Back screen RGB color data Back screen table address Back screen color mode, back table register (1800ACH, screen table lead address 1800AEH) Line window Horizontal start point Line window table address Line window enable, line table coordinate, horizontal register window table lead address end point coordinate (1800D8H~1800DEH) ST-58-R2



<!-- Page 284 -->

### • Color RAM Definition

Defines the sprite of the palette format and scroll screen color data. Color data stored in color RAM is in RGB format and has three modes. Mode selection designates in the color RAM mode (CRMD1, CRMD0, bits 13 and 12) of the RAM control register (18000EH). The most significant bit of color data stored in color RAM is the enable bit when the special color calculation mode is mode 3. Color is calculated in dot units for dots using color data when the most significant bit color data is 1, the special color calculation mode is mode 3, the color format is the palette format, and the color calculation enable bit is 1.

### • Color RAM Reference

Color RAM is referred from character patterns, bit map patterns, and line color screen table data. The color RAM address is expressed by 11 bits. When the character color count or bit map color count is 16-color, the 7-bit palette number added to the host becomes 11 bits; when 256-color, the 3-bit palette number added to the host becomes 11 bits. The palette number of the character pattern is in pattern name data and supplement data of the pattern name control register; the palette number of the bit map pattern is in the bit map palette number register.

### • Register

The register is, as a rule, a write-only, 16-bit register that designates the VDP2 function. One function may extend in several registers, and several functions may be arrange in one register. Set registers corresponding to the functions to be used when needed.



<!-- Page 285 -->

## 15.3 Bit Configuration Map

Every register bit register is related to various other bits. The bit map configuration of separate scroll screens and separate priority functions is shown in the bit map configuration below. TV Screen Mode TV Screen Display (DISP, 180000H, bit 15) 0 : Does not display picture on TV screen 1 : Displays picture Boarder Color Mode (BDCLMD, 180000H, bit 8) 0 : Displays Black 1 : Displays Back Screen Interlace mode (LSMD, 180000H, bit 7~6) 00 : Non-interlace 10 : Single-density Interlace 11 : Double-density Interlace Vertical Resolution (VRESO, 180000H, bit 5~4) 00 : 224 Lines (NTSC format or PAL format TV) 01 : 240 Lines (NTSC format or PAL format TV) 10 : 256 Lines (PAL format TV) Horizontal Resolution (HRESO, 180000H, bit 2~0) 000 : 320 Pixels (Normal Graphics A, NTSC format or PAL format TV) 001 : 352 Pixels (Normal Graphics B, NTSC format or PAL format TV) 010 : 640 Pixels (Hi-Res Graphics A, NTSC format or PAL format TV) 011 : 704 Pixels (Hi-Res Graphics B, NTSC format or PAL format TV) 100 : 320 Pixels (Exclusive Normal Graphics A, 31kHz monitor) 101 : 352 Pixels (Exclusive Normal Graphics B, Hi-Vision monitor) 110 : 640 Pixels (Exclusive Hi-Res Graphics A, 31kHz monitor) 111 : 704 Pixels (Exclusive Hi-Res Graphics B, Hi-Vision monitor) External Signal Enable External Latch Enable (EXLTEN, 180002H, bit 9) 0 : Latch when External Enable Register is read 1 : Latches via external signal External Synchronization Enable (EXSYEN, 180002H, bit 8) 0 : Does not input External Sync. Signal 1 : Inputs External Sync. Signal and synchronizes TV screen display externally Image Display Area Select (DASEL, 180002H, bit 1) 0 : Displays image in set display area only 1 : Displays image in specified display area only External Screen Enable (EXBGEN, 180002H, bit 0) 0 : Does not input External Screen Data 1 : Inputs External Screen Data ST-58-R2



<!-- Page 286 -->

Screen Status External Latch Flag (EXLTFG, 180004H, bit 9) 0 : Register is not latched (Register will be cleared when status is read) 1 : HV Counter Value is latched in register External Synchronization Flag (EXSYFG, 180004H, bit 8) 0 : Does not synchronize (Register will be cleared when status is read) 1 : Internal Circuitry Synchronized V-Blank (VBLANK, 180004H, bit 3) 0 : Scans during vertical display 1 : Scans during vertical retrace (VBLANK) H-Blank ( HBLANK, 180004H, bit 2) 0 : Scans during horizontal display 1 : Scans during horizontal retrace (HBLANK) Scan Field Flag (ODD, 180004H, bit 1) 0 : Scans during even fields 1 : Scans during odd fields TV Format Flag (PAL, 180004H, bit 0) 0 : NTSC Format 1 : PAL Format H-Counter Value (HCT, 180008H, bit 9~0) V-Counter Value (VCT, 18000AH, bit 9~0)



<!-- Page 287 -->

RAM VRAM VRAM Size (VRAMSZ, 180006H, bit 15) 0 : 4 Mbit 1 : 8 Mbit VRAM Change Enable (VRAMCE, 18000CH, bit 8) 0 : Does not use change function (uses both VRAM-A and VRAM-B as display VRAM) 1 : Uses change function (uses either VRAM-A or VRAM-B as display VRAM) VRAM Select (VRAMSL, 18000CH, bit 0) 0 : Uses VRAM-A for CPU VRAM 1 : Uses VRAM-B for CPU VRAM VRAM Mode (VRAMD, 18000EH, bit 8) VRAM Mode (VRBMD, 18000EH, bit 9) 0 : Does not make 2 bank partitions 1 : Makes 2 bank partitions VRAM Cycle Pattern (For VRAM-A0 (or VRAM-A)) (VCPnA0, 180010H, 180012H) VRAM Cycle Pattern (For VRAM-A1) (VCPnA1, 180014H, 180016H) VRAM Cycle Pattern (For VRAM-B0 (or VRAM-B)) (VCPnB0, 180018H, 18001AH) VRAM Cycle Pattern (For VRAM-B1) (VCPnB1, 18001CH, 18001EH) For Timing T0 (VCP0xx, 18001yH, bit 15~12) For Timing T1 (VCP1xx, 18001yH, bit 11~8) For Timing T2 (VCP2xx, 18001yH, bit 7~4) For Timing T3 (VCP3xx, 18001yH, bit 3~0) For Timing T4 (VCP4xx, 18001zH, bit 15~12) For Timing T5 (VCP5xx, 18001zH, bit 11~8) For Timing T6 (VCP6xx, 18001zH, bit 7~4) For Timing T7 (VCP7xx, 18001zH, bit 3~0) 0000 : NBG0 Pattern Name Data Read 0001 : NBG1 Pattern Name Data Read 0010 : NBG2 Pattern Name Data Read 0011 : NBG3 Pattern Name Data Read 0100 : NBG0 Character Pattern Data Read 0101 : NBG1 Character Pattern Data Read 0110 : NBG2 Character Pattern Data Read 0111 : NBG3 Character Pattern Data Read 1100 : Vertical Cell Scroll Table Data Read for NBG0 1101 : Vertical Cell Scroll Table Data Read for NBG1 1110 : CPU Read/Write 1111 : No Access Color RAM Color RAM Mode (CRMD, 18000EH, bit 13~12) 00 : Mode 0 : RGB, each 5 bits; 1024 color settings 01 : Mode 1 : RGB, each 5 bits; 2048 color settings 10 : Mode 2 : RGB, each 5 bits; 1024 color settings ST-58-R2



<!-- Page 288 -->

Scroll Screen Normal Scroll Screen NBG0 NBG1 NBG2 NBG3 Rotation Scroll Screen RBG0 RBG1 External Input Screen : EXBG Line Screen Line Color Screen : LNCL Back Screen : BACK Normal Scroll Screen (NBG0) Transparent Display Enable (N0TPON, 180020H, bit 8) 0 : Enables Transparent Code (Transparent coded dots become transparent) 1 : Disables Transparent Code (Transparent coded dots are displayed according to their data value) Screen Display Enable (N0ON, 180020H, bit 0) 0 : Cannot Display (Does not access VRAM) 1 : Can Display Character Color Count (N0CHCN, 180028H, bit 6~4) 000 : 16 colors (Palette Format) 001 : 256 colors (Palette Format) 010 : 2048 colors (Palette Format) 011 : 32,768 colors (RGB Format) 100 : 16,770,000 colors (RGB Format) Bitmap Enable (N0BMEN, 180028H, bit 1) 0 : Display in Cell Format See Cell Format (NBG0) 1 : Display in Bitmap Format See Bitmap Format (NBG0) Mosaic Enable (N0MZE, 180022H, bit 0) 0 : Does not execute Mosaic Process 1 : Executes Mosaic Process Mosaic Size (MZSZx, 180022H) Horizontal Mosaic Size (MZSZH, 180022H, bit 11~8) Vertical Mosaic Size (MZSZV, 180022H, bit 15~12) (Continued)



<!-- Page 289 -->

Cell Format (NBG0) Character Size (N0CHSZ, 180028H, bit 0) 0 : 1 H Cell X 1 V Cell 1 : 2 H Cells X 2 V Cells Pattern Name Data Size (N0PNB, 180030H, bit 15) 0 : 2 Words 1 : 1 Word 1 Word (Pattern Name Data Size) Character Number Supplementary Mode (N0CNSM, 180030H, bit 14) 0 : Character number in pattern name data is 10 bits; reverse function can be selected in character units 1 : Character number in pattern name data is 12 bits; reverse function cannot be selected. Special Priority (N0SPR, 180030H, bit 9) (For Pattern Name Supplementary Data) Special Color Calculation (N0SCC, 180030H, bit 8) (For Pattern Name Supplementary Data) Supplementary Palette Number (N0SPLT, 180030H, bit 7~5) Supplementary Character Number (N0SCN, 180030H, bit 4~0) Plane Size (N0PLSZ, 18003AH, bit 1~0) 00 : 1 H Page X 1 V Page 01 : 2 H Pages X 1 V Page 10 : 2 H Pages X 2 V Pages Map Offset (N0MP, 18003CH, bit 2~0) Map (N0MPx, 180040H~180042H) For Plane A (N0MPA, 180040H, bit 5~0) For Plane B (N0MPB, 180040H, bit 13~8) For Plane C (N0MPC, 180042H, bit 5~0) For Plane D (N0MPD, 180042H, bit 13~8) Bitmap Format (NBG0) Bitmap Size (N0BMSZ, 180028H, bit 3~2) 00 : 512 H Dots X 256 V Dots 01 : 512 H Dots X 512 V Dots 10 : 1024 H Dots X 256 V Dots 11 : 1024 H Dots X 512 V Dots Special Priority (N0BMPR, 18002CH, bit 5) Special Color Calculation (N0BMCC, 18002CH, bit 4) Supplementary Palette Number (N0BMP, 18002CH, bit 2~0) Map Offset (N0MP, 18003CH, bit 2~0) ST-58-R2



<!-- Page 290 -->

Normal Scroll Screen (NBG0) (continued) Screen Scroll Value (N0SCx, 180070H~180076H) For Horizontal Direction (N0SCX, 180070H, bit 10~180072H, bit 8) For Vertical Direction (N0SCY, 180074H, bit 10~180076H, bit 8) Coordinate Increment (N0ZMx, 180078H~18007EH) For Horizontal Direction (N0ZMX, 180078H, bit 2~18007AH, bit 8) For Vertical Direction (N0ZMY, 18007CH, bit 2~18007EH, bit 8) Reduction Enable (N0ZMQT, N0ZMHF, 180098H, bit 1~0) 00 : Reduction can not be displayed horizontally 01 : Reduction can be displayed up to 1/2 horizontally 10 : Reduction can be displayed up to 1/4 horizontally 11 : Reduction can be displayed up to 1/4 horizontally Line Scroll Space (N0LSS, 18009AH, bit 5~4) 00 : Every 1 Line (Non-interlace, Double-density Interlace), Every 2 Lines (Single-density Interlace) 01 : Every 2 Lines (Non-interlace, Double-density Interlace), Every 4 Lines (Single-density Interlace) 10 : Every 4 Line (Non-interlace, Double-density Interlace), Every 8 Lines (Single-density Interlace) 11 : Every 8 Line (Non-interlace, Double-density Interlace), Every 16 Lines (Single-density Interlace) Line Zoom Enable (N0LZMX, 18009AH, bit 3) 0 : Does not scale horizontally in line units 1 : Scales horizontally in line units Line Scroll Enable (N0LSCY, 18009AH, bit 2) (For Vertical Screen Scroll Values) 0 : Does not scroll vertically in line units 1 : Scrolls vertically in line units Line Scroll Enable (N0LSCX, 18009AH, bit 1) (For Horizontal Screen Scroll Values) 0 : Does not scroll horizontally in line units 1 : Scrolls horizontally in line units Vertical Cell Scroll Enable (N0VCSC, 18009AH, bit 0) 0 : No vertical cell scroll 1 : Allows vertical cell scroll Line Scroll Table Address (N0LSTA, 1800A0H, bit 2~1800A2H, bit 1) Vertical Cell Scroll Table Address (VCSTA, 18009CH, bit 2~18009EH, bit 1)



<!-- Page 291 -->

Normal Scroll Screen (NBG1) Transparent Display Enable (N1TPON, 180020H, bit 9) 0 : Turns on Transparent Code (Transparent coded dots become transparent) 1 : Turns off Transparent Code (Transparent coded dots are displayed as per their data value) Screen Display Enable (N1ON, 180020H, bit 1) 0 : Cannot Display (Cannot access VRAM during display) 1 : Can Display Character Color Count (N1CHCN, 180028H, bit 13~12) 00 : 16 colors (Palette Format) 01 : 256 colors (Palette Format) 10 : 2048 colors (Palette Format) 11 : 32,768 colors (RGB Format) Bitmap Enable (N1BMEN, 180028H, bit 9) 0 : Display in Cell Format See Cell Format (NBG1) 1 : Display in Bitmap Format See Bitmap Format (NBG1) Mosaic Enable (N1MZE, 180022H, bit 1) 0 : Does not execute Mosaic Process 1 : Executes Mosaic Process Mosaic Size (MZSZx, 180022H) Horizontal Mosaic Size (MZSZH, 180022H, bit 11~8) Vertical Mosaic Size (MZSZV, 180022H, bit 15~12) (Continued) ST-58-R2



<!-- Page 292 -->

Cell Format (NBG1) Character Size (N1CHSZ, 180028H, bit 8) 0 : 1 H Cell X 1 V Cell 1 : 2 H Cells X 2 V Cells Pattern Name Data Size (N1PNB, 180032H, bit 15) 0 : 2 Words 1 : 1 Word 1 Word (Pattern Name Data Size) Character Number Supplementary Mode (N1CNSM, 180032H, bit 14) 0 : Character number in pattern name data is 10 bits; reverse function can be selected in character units 1 : Character number in pattern name data is 12 bits; reverse function cannot be selected. Special Priority (N1SPR, 180032H, bit 9) (For Pattern Name Supplementary Data) Special Color Calculation (N1SCC, 180032H, bit 8) (For Pattern Name Supplementary Data) Supplementary Palette Number (N1SPLT, 180032H, bit 7~5) Supplementary Character Number (N1SCN, 180032H, bit 4~0) Plane Size (N1PLSZ, 18003AH, bit 3~2) 00 : 1 H Page X 1 V Page 01 : 2 H Pages X 1 V Page 10 : 2 H Pages X 2 V Pages Map Offset (N1MP, 18003CH, bit 6~4) Map (N1MPx, 180044H~180046H) For Plane A (N1MPA, 180044H, bit 5~0) For Plane B (N1MPB, 180044H, bit 13~8) For Plane C (N1MPC, 180046H, bit 5~0) For Plane D (N1MPD, 180046H, bit 13~8) Bitmap Format (NBG1) Bitmap Size (N1BMSZ, 180028H, bit 11~10) 00 : 512 H Dots X 256 V Dots 01 : 512 H Dots X 512 V Dots 10 : 1024 H Dots X 256 V Dots 11 : 1024 H Dots X 512 V Dots Special Priority (N1BMPR, 18002CH, bit 13) Special Color Calculation (N1BMCC, 18002CH, bit 12) Supplementary Palette Number (N1BMP, 18002CH, bit 10~8) Map Offset (N1MP, 18003CH, bit 6~4)



<!-- Page 293 -->

Normal Scroll Screen (NBG1) (Continued) Screen Scroll Value (N1SCx, 180080H~180086H) For Horizontal Direction (N1SCX, 180080H, bit 10~180082H, bit 8) For Vertical Direction (N1SCY, 180084H, bit 10~180086H, bit 8) Coordinate Increment (N1ZMx, 180088H~18008EH) For Horizontal Direction (N1ZMX, 180088H, bit 2~18008AH, bit 8) For Vertical Direction (N1ZMY, 18008CH, bit 2~18008EH, bit 8) Reduction Enable (N1ZMQT, N1ZMHF, 180098H, bit 9~8) 00 : Reduction can not be displayed horizontally 01 : Reduction can be displayed up to 1/2 horizontally 10 : Reduction can be displayed up to 1/4 horizontally 11 : Reduction can be displayed up to 1/4 horizontally Line Scroll Space (N1LSS, 18009AH, bit 13~12) 00 : Every 1 Line (Non-interlace, Double-density Interlace), Every 2 Lines (Single-density Interlace) 01 : Every 2 Lines (Non-interlace, Double-density Interlace), Every 4 Lines (Single-density Interlace) 10 : Every 4 Line (Non-interlace, Double-density Interlace), Every 8 Lines (Single-density Interlace) 11 : Every 8 Line (Non-interlace, Double-density Interlace), Every 16 Lines (Single-density Interlace) Line Zoom Enable (N1LZMX, 18009AH, bit 11) 0 : No zoom horizontally in line units 1 : Allows zoom horizontally in line units Line Scroll Enable (N1LSCY, 18009AH, bit 10) (For Vertical Screen Scroll Values) 0 : No zoom vertically in line units 1 : Allows zoom vertically in line units Line Scroll Enable (N1LSCX, 18009AH, bit 9) (For Horizontal Screen Scroll Values) 0 : No scroll horizontally in line units 1 : Allows scroll horizontally in line units Vertical Cell Scroll Enable (N1VCSC, 18009AH, bit 8) 0 : No vertical cell scroll 1 : Allows vertical cell scroll Line Scroll Table Address (N1LSTA, 1800A4H, bit 2~1800A6H, bit 1) Vertical Cell Scroll Table Address (VCSTA, 18009CH, bit 2~18009EH, bit 1) ST-58-R2



<!-- Page 294 -->

Normal Scroll Screen (NBG2) Transparent Display Enable (N2TPON, 180020H, bit 10) 0 : Turns on Transparent Code (Transparent coded dots become transparent) 1 : Turns off Transparent Code (Transparent coded dots are displayed as per their data value) Screen Display Enable (N2ON, 180020H, bit 2) 0 : Cannot Display (Cannot access VRAM during display) 1 : Can Display Character Color Count (N2CHCN, 18002AH, bit 1) 0 : 16 colors (Palette Format) 1 : 256 colors (Palette Format) (Display in Cell Format) See Cell Format (NBG2) Mosaic Enable (N2MZE, 180022H, bit 2) 0 : Does not execute Mosaic Process 1 : Executes Mosaic Process Mosaic Size (MZSZx, 180022H) Horizontal Mosaic Size (MZSZH, 180022H, bit 11~8) Vertical Mosaic Size (MZSZV, 180022H, bit 15~12) Screen Scroll Value (N2SCx, 180090H~180092H) For Horizontal Direction (N2SCX, 180090H, bit 10~0) For Vertical Direction (N2SCY, 180092H, bit 10~0)



<!-- Page 295 -->

Cell Format (NBG2) Character Size (N2CHSZ, 18002AH, bit 0) 0 : 1 H Cell X 1 V Cell 1 : 2 H Cells X 2 V Cells Pattern Name Data Size (N2PNB, 180034H, bit 15) 0 : 2 Words 1 : 1 Word 1 Word (Pattern Name Data Size) Character Number Supplementary Mode (N2CNSM, 180034H, bit 14) 0 : Character number in pattern name data is 10 bits; reverse function can be selected in character units 1 : Character number in pattern name data is 12 bits; reverse function cannot be selected Special Priority (N2SPR, 180034H, bit 9) (For Pattern Name Supplementary Data) Special Color Calculation (N2SCC, 180034H, bit 8) (For Pattern Name Supplementary Data) Supplementary Palette Number (N2SPLT, 180034H, bit 7~5) Supplementary Character Number (N2SCN, 180034H, bit 4~0) Plane Size (N2PLSZ, 18003AH, bit 5~4) 00 : 1 H Page X 1 V Page 01 : 2 H Pages X 1 V Page 10 : 2 H Pages X 2 V Pages Map Offset (N2MP, 18003CH, bit 10~8) Map (N2MPx, 180048H~18004AH) For Plane A (N2MPA, 180048H, bit 5~0) For Plane B (N2MPB, 180048H, bit 13~8) For Plane C (N2MPC, 18004AH, bit 5~0) For Plane D (N2MPD, 18004AH, bit 13~8) ST-58-R2



<!-- Page 296 -->

Normal Scroll Screen (NBG3) Transparent Display Enable (N3TPON, 180020H, bit 11) 0 : Turns on Transparent Code (Transparent coded dots become transparent) 1 : Turns off Transparent Code (Transparent coded dots are displayed as per their data value) Screen Display Enable (N3ON, 180020H, bit 3) 0 : Cannot Display (Cannot access VRAM during display) 1 : Can Display Character Color Count (N3CHCN, 18002AH, bit 5) 0 : 16 colors (Palette Format) 1 : 256 colors (Palette Format) (Display in Cell Format) See Cell Format (NBG3) Mosaic Enable (N3MZE, 180022H, bit 3) 0 : Does not execute Mosaic Process 1 : Executes Mosaic Process Mosaic Size (MZSZx, 180022H) Horizontal Mosaic Size (MZSZH, 180022H, bit 11~8) Vertical Mosaic Size (MZSZV, 180022H, bit 15~12) Screen Scroll Value (N3SCx, 180094H~180096H) For Horizontal Direction (N3SCX, 180094H, bit 10~0) For Vertical Direction (N3SCY, 180096H, bit 10~0)



<!-- Page 297 -->

Cell Format (NBG3) Character Size (N3CHSZ, 18002AH, bit 4) 0 : 1 H Cell X 1 V Cell 1 : 2 H Cells X 2 V Cells Pattern Name Data Size (N3PNB, 180036H, bit 15) 0 : 2 Words 1 : 1 Word 1 Word (Pattern Name Data Size) Character Number Supplementary Mode (N3CNSM, 180036H, bit 14) 0 : Character number in pattern name data is 10 bits; reverse function can be selected in character units 1 : Character number in pattern name data is 12 bits; reverse function cannot be selected Special Priority (N3SPR, 180036H, bit 9) (For Pattern Name Supplementary Data) Special Color Calculation (N3SCC, 180036H, bit 8) (For Pattern Name Supplementary Data) Supplementary Palette Number (N3SPLT, 180036H, bit 7~5) Supplementary Character Number (N3SCN, 180036H, bit 4~0) Plane Size (N3PLSZ, 18003AH, bit 7~6) 00 : 1 H Page X 1 V Page 01 : 2 H Pages X 1 V Page 10 : 2 H Pages X 2 V Pages Map Offset (N3MP, 18003CH, bit 14~12) Map (N3MPx, 18004CH~18004EH) For Plane A (N3MPA, 18004CH, bit 5~0) For Plane B (N3MPB, 18004CH, bit 13~8) For Plane C (N3MPC, 18004EH, bit 5~0) For Plane D (N3MPD, 18004EH, bit 13~8) ST-58-R2



<!-- Page 298 -->

Rotation Scroll Screen (RBG0) Transparent Display Enable (R0TPON, 180020H, bit 12) 0 : Turns on Transparent Code (Transparent coded dots become transparent) 1 : Turns off Transparent Code (Transparent coded dots are displayed as per their data value) Screen Display Enable (R0ON, 180020H, bit 4) 0 : Cannot Display (Cannot access VRAM during display) 1 : Can Display Character Color Count (R0CHCN, 18002AH, bit 14~12) 000 : 16 colors (Palette Format) 001 : 256 colors (Palette Format) 010 : 2048 colors (Palette Format) 011 : 32,768 colors (RGB Format) 100 : 16,770,000 colors (RGB Format) Bitmap Enable (R0BMEN, 18002AH, bit 9) 0 : Display in Cell Format See Cell Format (RBG0) 1 : Display in Bitmap Format See Bitmap Format (RBG0) Mosaic Enable (R0MZE, 180022H, bit 4) 0 : Does not execute Mosaic Process 1 : Executes Mosaic Process Mosaic Size (MZSZx, 180022H) Horizontal Mosaic Size (MZSZH, 180022H, bit 11~8) Vertical Mosaic Size (MZSZV, 180022H, bit 15~12) (Continued)



<!-- Page 299 -->

Cell Format (RBG0) Character Size (R0CHSZ, 18002AH, bit 8) 0 : 1 H Cell X 1 V Cell 1 : 2 H Cells X 2 V Cells Pattern Name Data Size (R0PNB, 180038H, bit 15) 0 : 2 Words 1 : 1 Word 1 Word (Pattern Name Data Size) Character Number Supplementary Mode (R0CNSM, 180038H, bit 14) 0 : Character number in pattern name data is 10 bits; reverse function can be selected in character units 1 : Character number in pattern name data is 12 bits; reverse function cannot be selected Special Priority (R0SPR, 180038H, bit 9) (For Pattern Name Supplementary Data) Special Color Calculation (R0SCC, 180038H, bit 8) (For Pattern Name Supplementary Data) Supplementary Palette Number (R0SPLT, 180038H, bit 7~5) Supplementary Character Number (R0SCN, 180038H, bit 4~0) Rotation Parameter Mode (RPMD, 1800B0H, bit 1~0) 00 : Mode 0 : Use for Rotation Parameter A See Rotation Parameter A (RBG0) 01 : Mode 1 : Use for Rotation Parameter B See Rotation Parameter B (RBG0) ST-58-R2



<!-- Page 300 -->

For Rotation Parameter A (RBG0) Plane Size (RAPLSZ, 18003AH, bit 9~8) 00 : 1 H Page X 1 V Page 01 : 2 H Pages X 1 V Page 10 : 2 H Pages X 2 V Pages Screen Over Process (RAOVR, 18003AH, bit 11~10) 00 : Outside the display area it repeats the image set in the display area 01 : Outside the display area it repeats the image set in the Screen Over Pattern Register 10 : Everything outside the display area is transparent 11 : Sets display area to 0 ≤ x < 512 and 0 ≤ y <512 regardless of the plane size or bitmap size. Everything outside the display area is transparent. Screen Over Pattern Name (RAOPN, 1800B8H, bit 15~0) Map Offset (RAMP, 18003EH, bit 2~0) Map (RAMPx, 180050H~18005EH) For Plane A (RAMPA, 180050H, bit 5~0) For Plane B (RAMPB, 180050H, bit 13~8) For Plane C (RAMPC, 180052H, bit 5~0) For Plane D (RAMPD, 180052H, bit 13~8) For Plane E (RAMPE, 180054H, bit 5~0) For Plane F (RAMPF, 180054H, bit 13~8) For Plane G (RAMPG, 180056H, bit 5~0) For Plane H (RAMPH, 180056H, bit 13~8) For Plane I (RAMPI, 180058H, bit 5~0) For Plane J (RAMPJ, 180058H, bit 13~8) For Plane K (RAMPK, 18005AH, bit 5~0) For Plane L (RAMPL, 18005AH, bit 13~8) For Plane M (RAMPM, 18005CH, bit 5~0) For Plane N (RAMPN, 18005CH, bit 13~8) For Plane O (RAMPO, 18005EH, bit 5~0) For Plane P (RAMPP, 18005EH, bit 13~8)



<!-- Page 301 -->

For Rotation Parameter B (RBG0) Plane Size (RBPLSZ, 18003AH, bit 13~12) 00 : 1 H Page X 1 V Page 01 : 2 H Pages X 1 V Page 10 : 2 H Pages X 2 V Pages Screen Over Process (RBOVR, 18003AH, bit 15~14) 00 : Outside the display area it repeats the image set in the display area 01 : Outside the display area it repeats the image set in the Screen Over Pattern Register 10 : Everything outside the display area is transparent 11 : Sets display area to 0 ≤ x < 512 and 0 ≤ y <512 regardless of the plane size or bitmap size. Everything outside the display area is transparent. Screen Over Pattern Name (RBOPN, 1800BAH, bit 15~0) Map Offset (RBMP, 18003EH, bit 6~4) Map (RBMPx, 180060H~18006EH) For Plane A (RBMPA, 180060H, bit 5~0) For Plane B (RBMPB, 180060H, bit 13~8) For Plane C (RBMPC, 180062H, bit 5~0) For Plane D (RBMPD, 180062H, bit 13~8) For Plane E (RBMPE, 180064H, bit 5~0) For Plane F (RBMPF, 180064H, bit 13~8) For Plane G (RBMPG, 180066H, bit 5~0) For Plane H (RBMPH, 180066H, bit 13~8) For Plane I (RBMPI, 180068H, bit 5~0) For Plane J (RBMPJ, 180068H, bit 13~8) For Plane K (RBMPK, 18006AH, bit 5~0) For Plane L (RBMPL, 18006AH, bit 13~8) For Plane M (RBMPM, 18006CH, bit 5~0) For Plane N (RBMPN, 18006CH, bit 13~8) For Plane O (RBMPO, 18006EH, bit 5~0) For Plane P (RBMPP, 18006EH, bit 13~8) Bitmap Format (RBG0) Bitmap Size (R0BMSZ, 18002AH, bit 10) 0 : 512 H Dots X 256 V Dots 1 : 512 H Dots X 512 V Dots Special Priority (R0BMPR, 18002EH, bit 5) Special Color Calculation (R0BMCC, 18002EH, bit 4) Supplementary Palette Number (R0BMP, 18002EH, bit 2~0) Rotation Parameter Mode (RPMD, 1800B0H, bit 1~0) 00 : Mode 0 : Use for Rotation Parameter A 01 : Mode 1 : Use for Rotation Parameter B Map Offset (For Rotation Parameter A) (RAMP, 18003EH, bit 2~0) Map Offset (For Rotation Parameter B) (RBMP, 18003EH, bit 6~4) ST-58-R2



<!-- Page 302 -->

Rotation Scroll Screen (RBG0) (Continued) Rotation Data Bank Setting (RDBSxx, 18000EH) For VRAM-A0 (or VRAM-A) (RDBSA0, 18000EH, bit 1~0) For VRAM-A1 (RDBSA1, 18000EH, bit 3~2) For VRAM-B0 (or VRAM-B) (RDBSB0, 18000EH, bit 5~4) For VRAM-B1 (RDBSB1, 18000EH, bit 7~6) 00 : Not used as RAM for RBG0 01 : RAM for RBG0 Coefficient Data Table 10 : RAM for RBG0 Pattern Name Table 11 : RAM for RBG0 Character Pattern Table (or Bitmap Pattern) Parameter Read Enable (RxxxSTRE, 1800B2H) For Rotation Parameter A Xst (RAXSTRE, 1800B2H, bit 0) For Rotation Parameter B Xst (RBXSTRE, 1800B2H, bit 8) For Rotation Parameter A Yst (RAYSTRE, 1800B2H, bit 1) For Rotation Parameter B Yst (RBYSTRE, 1800B2H, bit 9) For Rotation Parameter A KAst (RAKASTRE, 1800B2H, bit 2) For Rotation Parameter B KAst (RBKASTRE, 1800B2H, bit 10) 0 : Selected parameter is not read per line 1 : Selected parameter is read per line Rotation Parameter Table Address (RPTA, 1800BCH, bit 2~1800BEH, bit 1) Rotation Parameter Mode (RPMD, 1800B0H, bit 1~0) 00 : Mode 0 : Rotation Parameter A 01 : Mode 1 : Rotation Parameter B 10 : Mode 2 : A image and B image are switched according to coefficient data read from rotation parameter A coefficient table 11 : Mode 3 : A image and B image are switched according to rotation parameter window Coefficient Line Color Enable (RxKLCE, 1800B4H) For Rotation Parameter A (RAKLCE, 1800B4H, bit 4) For Rotation Parameter B (RBKLCE, 1800B4H, bit 12) 0 : Line color screen data in coefficient data is not used 1 : Line color screen data in coefficient data is used Coefficient Data Mode (RxKMD, 1800B4H) For Rotation Parameter A (RAKMD, 1800B4H, bit 3~2) For Rotation Parameter B (RBKMD, 1800B4H, bit 11~10) 00 : Mode 0 : Used as zoom coefficients kx and ky 01 : Mode 1 : Used as zoom coefficient kx 10 : Mode 2 : Used as zoom coefficient ky 11 : Mode 3 : Used as viewpoint coordinate Xp after conversion Coefficient Data Size (RxKDBS, 1800B4H) For Rotation Parameter A (RAKDBS, 1800B4H, bit 1) For Rotation Parameter B (RBKDBS, 1800B4H, bit 9) 0 : 2 Words 1 : 1 Word Coefficient Table Enable (RxKTE, 1800B4H) For Rotation Parameter A (RAKTE, 1800B4H, bit 0) For Rotation Parameter B (RBKTE, 1800B4H, bit 8) 0 : Does not use Coefficient Table 1 : Uses Coefficient Table Coefficient Table Address Offset (RxKTAOS, 1800B6H) For Rotation Parameter A (RAKTAOS, 1800B6H, bit 2~0) For Rotation Parameter B (RBKTAOS, 1800B6H, bit 10~8)



<!-- Page 303 -->

Rotation Scroll Screen (RBG1) Transparent Display Enable (For NBG0) (N0TPON, 180020H, bit 8) 0 : Turns on Transparent Code (Transparent coded dots become transparent) 1 : Turns off Transparent Code (Transparent coded dots are displayed as per their data value) Screen Display Enable (R1ON, 180020H, bit 5) 0 : Cannot Display (Cannot access VRAM during display) 1 : Can Display Character Color Count (For NBG0) (N0CHCN, 180028H, bit 6~4) 000 : 16 colors (Palette Format) 001 : 256 colors (Palette Format) 010 : 2048 colors (Palette Format) 011 : 32786 colors (RGB Format) 100 : 16,770,000 colors (RBG Format) (Display in Cell Format) See Cell Format (RBG1) Mosaic Enable (For NBG0) (N0MZE, 180022H, bit 0) 0 : Does not execute Mosaic Process 1 : Executes Mosaic Process Mosaic Size (MZSZx, 180022H) Horizontal Mosaic Size (MZSZH, 180022H, bit 11~8) Vertical Mosaic Size (MZSZV, 180022H, bit 15~12) ST-58-R2



<!-- Page 304 -->

Cell Format (RBG1) Character Size (For NBG0) (N0CHSZ, 180028H, bit 0) 0 : 1 H Cell X 1 V Cell 1 : 2 H Cells X 2 V Cells Pattern Name Data Size (For NBG0) (N0PNB, 180030H, bit 15) 0 : 2 Words 1 : 1 Word 1 Word (Pattern Name Data Size) Character Number Supplementary Mode (For NBG0) (N0CNSM, 180030H, bit 14) 0 : Character number in pattern name data is 10 bits, reverse function can be selected per character unit 1 : Character number in pattern name data is 12 bits, reverse function cannot be selected Special Priority (For NBG0) (N0SPR, 180030H, bit 9) (For Pattern Name Supplementary Data) Special Color Calculation (For NBG0) (N0SCC, 180030H, bit 8) (For Pattern Name Supplementary Data) Supplementary Palette Number (For NBG0) (N0SPLT, 180030H, bit 7~5) Supplementary Character Number (For NBG0) (N0SCN, 180030H, bit 4~0) (For use of Rotation Parameter B) See Rotation Parameter B (RBG1) For Rotation Parameter B (RBG1) Plane Size (RBPLSZ, 18003AH, bit 13~12) 00 : 1 H Page X 1 V Page 01 : 2 H Pages X 1 V Page 10 : 2 H Pages X 2 V Pages Screen Over Process (RBOVR, 18003AH, bit 15~14) 00 : Outside the display area it repeats the image set in the display area 01 : Outside the display area it repeats the image set in the Screen Over Pattern Register 10 : Everything outside the display area is transparent 11 : Sets display area to 0 ≤ x < 512 and 0 ≤ y <512 regardless of the plane size or bitmap size. Everything outside the display area is transparent. Screen Over Pattern Name (RBOPN, 1800BAH, bit 15~0) Map Offset (RBMP, 18003EH, bit 6~4) Map (RBMPx, 180060H~18006EH) For Plane A (RBMPA, 180060H, bit 5~0) For Plane B (RBMPB, 180060H, bit 13~8) For Plane C (RBMPC, 180062H, bit 5~0) For Plane D (RBMPD, 180062H, bit 13~8) For Plane E (RBMPE, 180064H, bit 5~0) For Plane F (RBMPF, 180064H, bit 13~8) For Plane G (RBMPG, 180066H, bit 5~0) For Plane H (RBMPH, 180066H, bit 13~8) For Plane I (RBMPI, 180068H, bit 5~0) For Plane J (RBMPJ, 180068H, bit 13~8) For Plane K (RBMPK, 18006AH, bit 5~0) For Plane L (RBMPL, 18006AH, bit 13~8) For Plane M (RBMPM, 18006CH, bit 5~0) For Plane N (RBMPN, 18006CH, bit 13~8) For Plane O (RBMPO, 18006EH, bit 5~0) For Plane P (RBMPP, 18006EH, bit 13~8)



<!-- Page 305 -->

Line Color Screen (LNCL) Line Color Screen Color Mode (LCCLMD, 1800A8H, bit 15) 0 : Single Color 1 : Select per line Line Color Screen Table Address (LCTA, 1800A8H, bit 2~1800AAH, bit 0) Back Screen (BACK) Back Screen Color Mode (BKCLMD, 1800ACH, bit 15) 0 : Single Color 1 : Set each line Back Screen Table Address (BKTA, 1800ACH, bit 2~1800AEH, bit 0) Window Normal Rectangular Window W0 W1 Normal Line Window Sprite Window : SW Normal Rectangular Window Window Position (For Horizontal Coordinate) (WxxX, 1800C0H~1800CCH) W0 Start Point Coordinate (W0SX, 1800C0H, bit 9~0) W0 End Point Coordinate (W0EX, 1800C4H, bit 9~0) W1 Start Point Coordinate (W1SX, 1800C8H, bit 9~0) W1 End Point Coordinate (W1EX, 1800CCH, bit 9~0) Window Position (For Vertical Coordinate) (WxxY, 1800C2H~1800CEH) W0 Start Point Coordinate (W0SY, 1800C2H, bit 8~0) W0 End Point Coordinate (W0EY, 1800C6H, bit 8~0) W1 Start Point Coordinate (W1SY, 1800CAH, bit 8~0) W1 End Point Coordinate (W1EY, 1800CEH, bit 8~0) Normal Line Window Line Window Enable (WxLWE, 1800D8H~1800DCH) For W0 (W0LWE, 1800D8H, bit 15) For W1 (W1LWE, 1800DCH, bit 15) 0 : Does not set normal window to line window 1 : Sets normal window to line window Line Window Table Address (WxLWTA, 1800D8H~1800DEH) For W0 (W0LWTA, 1800D8H, bit 2~1800DAH, bit 1) For W1 (W1LWTA, 1800DCH, bit 2~1800DEH, bit 1) Sprite Window Sprite Window Enable (SPWINEN, 1800E0H, bit 4) 0 : Does not use Sprite Window 1 : Uses Sprite Window ST-58-R2



<!-- Page 306 -->

Window Control Window Logic (xxLOG, 1800D0H~1800D6H) For NBG0 of Transparent Process Window (or RBG1) (N0L0G, 1800D0H, bit 7) For NBG1 of Transparent Process Window (or EXBG) (N1L0G, 1800D0H, bit 15) For NBG2 of Transparent Process Window (N2L0G, 1800D2H, bit 7) For NBG3 of Transparent Process Window (N3L0G, 1800D2H, bit 15) For RBG0 of Transparent Process Window (R0L0G, 1800D4H, bit 7) For Sprite of Transparent Process Window (SPL0G, 1800D4H, bit 15) For Rotation Parameter Window (RPL0G, 1800D6H, bit 7) For Color Calculation Window (CCL0G, 1800D6H, bit 15) 0 : OR 1 : AND Window Enable (For W0) (xxW0E, 1800D0H~1800D6H) For NBG0 of Transparent Process Window (or RBG1) (N0W0E, 1800D0H, bit 1) For NBG1 of Transparent Process Window (or EXBG) (N1W0E, 1800D0H, bit 9) For NBG2 of Transparent Process Window (N2W0E, 1800D2H, bit 1) For NBG3 of Transparent Process Window (N3W0E, 1800D2H, bit 9) For RBG0 of Transparent Process Window (R0W0E, 1800D4H, bit 1) For Sprite of Transparent Process Window (SPW0E, 1800D4H, bit 9) For Rotation Parameter Window (RPW0E, 1800D6H, bit 1) For Color Calculation Window (CCW0E, 1800D6H, bit 9) 0 : Does not use W0 Window 1 : Uses W0 Window Window Enable (For W1) (xxW1E, 1800D0H~1800D6H) For NBG0 of Transparent Process Window (or RBG1) (N0W1E, 1800D0H, bit 3) For NBG1 of Transparent Process Window (or EXBG) (N1W1E, 1800D0H, bit 11) For NBG2 of Transparent Process Window (N2W1E, 1800D2H, bit 3) For NBG3 of Transparent Process Window (N3W1E, 1800D2H, bit 11) For RBG0 of Transparent Process Window (R0W1E, 1800D4H, bit 3) For Sprite of Transparent Process Window (SPW1E, 1800D4H, bit 11) For Rotation Parameter Window (RPW1E, 1800D6H, bit 3) For Color Calculation Window (CCW1E, 1800D6H, bit 11) 0 : Does not use W1 Window 1 : Uses W1 Window Window Enable (For SW) (xxSWE, 1800D0H~1800D6H) For NBG0 of Transparent Process Window (or RBG1) (N0SWE, 1800D0H, bit 5) For NBG1 of Transparent Process Window (or EXBG) (N1SWE, 1800D0H, bit 13) For NBG2 of Transparent Process Window (N2SWE, 1800D2H, bit 5) For NBG3 of Transparent Process Window (N3SWE, 1800D2H, bit 13) For RBG0 of Transparent Process Window (R0SWE, 1800D4H, bit 5) For Sprite of Transparent Process Window (SPSWE, 1800D4H, bit 13) For Color Calculation Window (CCSWE, 1800D6H, bit 13) 0 : Does not use SW Window 1 : Uses SW Window (Continued)



<!-- Page 307 -->

Window Control (Continued) Window Area (For W0) (xxW0A, 1800D0H~1800D6H) For NBG0 of Transparent Process Window (or RBG1) (N0W0A, 1800D0H, bit 0) For NBG1 of Transparent Process Window (or EXBG) (N1W0A, 1800D0H, bit 8) For NBG2 of Transparent Process Window (N2W0A, 1800D2H, bit 0) For NBG3 of Transparent Process Window (N3W0A, 1800D2H, bit 8) For RBG0 of Transparent Process Window (R0W0A, 1800D4H, bit 0) For Sprite of Transparent Process Window (SPW0A, 1800D4H, bit 8) For Rotation Parameter Window (RPW0A, 1800D6H, bit 0) For Color Calculation Window (CCW0A, 1800D6H, bit 8) 0 : Activates Inside of W0 Window 1 : Activates Outside of W0 Window Window Area (For W1) (xxW1A, 1800D0H~1800D6H) For NBG0 of Transparent Process Window (or RBG1) (N0W1A, 1800D0H, bit 2) For NBG1 of Transparent Process Window (or EXBG) (N1W1A, 1800D0H, bit 10) For NBG2 of Transparent Process Window (N2W1A, 1800D2H, bit 2) For NBG3 of Transparent Process Window (N3W1A, 1800D2H, bit 10) For RBG0 of Transparent Process Window (R0W1A, 1800D4H, bit 2) For Sprite of Transparent Process Window (SPW1A, 1800D4H, bit 10) For Rotation Parameter Window (RPW1A, 1800D6H, bit 2) For Color Calculation Window (CCW1A, 1800D6H, bit 10) 0 : Activates Inside of W1 Window 1 : Activates Outside of W1 Window Window Area (For SW) (xxSWA, 1800D0H~1800D6H) For NBG0 of Transparent Process Window (or RBG1) (N0SWA, 1800D0H, bit 4) For NBG1 of Transparent Process Window (or EXBG) (N1SWA, 1800D0H, bit 12) For NBG2 of Transparent Process Window (N2SWA, 1800D2H, bit 4) For NBG3 of Transparent Process Window (N3SWA, 1800D2H, bit 12) For RBG0 of Transparent Process Window (R0SWA, 1800D4H, bit 4) For Sprite of Transparent Process Window (SPSWA, 1800D4H, bit 12) For Color Calculation Window (CCSWA, 1800D6H, bit 12) 0 : Activates Inside of SW Window 1 : Activates Outside of SW Window ST-58-R2



<!-- Page 308 -->

Sprite Sprite Color Calculation Condition (SPCCCS, 1800E0H, bit 13~12) 00 : When (Priority Number) ≤ (Color Calculation Condition Number) 01 : When (Priority Number) = (Color Calculation Condition Number) 10 : When (Priority Number) ≥ (Color Calculation Condition Number) 11 : When the MSB of the Color Data is 1 Sprite Color Calculation Condition Number (SPCCN, 1800E0H, BIT 10~8) Sprite Color Mode (SPCLMD, 1800E0H, bit 5) 0 : All sprite data is palette format only 1 : Sprite data is a combination of palette format and RGB format Sprite Window Enable (SPWINEN, 1800E0H, bit 4) 0 : Does not use Sprite Window 1 : Uses Sprite Window Sprite Type (SPTYPE, 1800E0H, bit 3~0) Priority Number (For Sprite) (SxPRIN, 1800F0H~1800F6H) For Sprite Register 0 (S0PRIN, 1800F0H, bit 2~0) For Sprite Register 1 (S1PRIN, 1800F0H, bit 10~8) For Sprite Register 2 (S2PRIN, 1800F2H, bit 2~0) For Sprite Register 3 (S3PRIN, 1800F2H, bit 10~8) For Sprite Register 4 (S4PRIN, 1800F4H, bit 2~0) For Sprite Register 5 (S5PRIN, 1800F4H, bit 10~8) For Sprite Register 6 (S6PRIN, 1800F6H, bit 2~0) For Sprite Register 7 (S7PRIN, 1800F6H, bit 10~8) Color Calculation Ratio (For Sprite) (SxCCRT, 180100H~180106H) For Sprite Register 0 (S0CCRT, 180100H, bit 4~0) For Sprite Register 1 (S1CCRT, 180100H, bit 12~8) For Sprite Register 2 (S2CCRT, 180102H, bit 4~0) For Sprite Register 3 (S3CCRT, 180102H, bit 12~8) For Sprite Register 4 (S4CCRT, 180104H, bit 4~0) For Sprite Register 5 (S5CCRT, 180104H, bit 12~8) For Sprite Register 6 (S6CCRT, 180106H, bit 4~0) For Sprite Register 7 (S7CCRT, 180106H, bit 12~8)



<!-- Page 309 -->

Dot Color Data Color RAM Address Offset ( xxCAOS, 1800E4H~1800E6H) For NBG0 (or RBG1) ( N0CAOS, 1800E4H, bit 2~0) For NBG1 (or EXBG) ( N1CAOS, 1800E4H, bit 6~4) For NBG2 ( N2CAOS, 1800E4H, bit 10~8) For NBG3 ( N3CAOS, 1800E4H, bit 14~12) For RBG0 ( R0CAOS, 1800E6H, bit 2~0) For Sprite ( SPCAOS, 1800E6H, bit 4~0) Special F unction Code Select ( xxSFCS, 180024H) For NBG0 (or RBG1) ( N0SFCS, 180024H, bit 0) For NBG1 (or EXBG) ( N1SFCS, 180024H, bit 1) For NBG2 ( N2SFCS, 180024H, bit 2) For NBG3 ( N3SFCS, 180024H, bit 3) For RBG0 ( R0SFCS, 180024H, bit 4) 0 : Activates Special Function Code A 1 : Activates Special Function Code B Special F unction Code ( SFCDxx, 180026H) For Special Function Code A ( SFCDAx, 180026H, bit 7~0) For Special Function Code B ( SFCDBx, 180026H, bit 15~8) SFCDx0 : When Dot Color Code's lower 4 bits are 0H or 1H SFCDx1 : When Dot Color Code's lower 4 bits are 2H or 3H SFCDx2 : When Dot Color Code's lower 4 bits are 4H or 5H SFCDx3 : When Dot Color Code's lower 4 bits are 6H or 7H SFCDx4 : When Dot Color Code's lower 4 bits are 8H or 9H SFCDx5 : When Dot Color Code's lower 4 bits are AH or BH SFCDx6 : When Dot Color Code's lower 4 bits are CH or DH SFCDx7 : When Dot Color Code's lower 4 bits are EH or FH 0 : Does not use Special Function 1 : Uses Special Function ST-58-R2



<!-- Page 310 -->

Priority Line Color Screen Insert Enable (xxLCEN, 1800E8H) For NBG0 (or RBG1) (N0LCEN, 1800E8H, bit 0) For NBG1 (or EXBG) (N1LCEN, 1800E8H, bit 1) For NBG2 (N2LCEN, 1800E8H, bit 2) For NBG3 (N3LCEN, 1800E8H, bit 3) For RBG0 (R0LCEN, 1800E8H, bit 4) For Sprite (SPLCEN, 1800E8H, bit 5) 0 : Does not insert line color screen when the corresponding screen is the top image 1 : Inserts line color screen when the corresponding screen is the top image Special Priority Mode (xxSPRM, 1800EAH) For NBG0 (or RBG1) (N0SPRM, 1800EAH, bit 1~0) For NBG1 (or EXBG) (N1SPRM, 1800EAH, bit 3~2) For NBG2 (N2SPRM, 1800EAH, bit 5~4) For NBG3 (N3SPRM, 1800EAH, bit 7~6) For RBG0 (R0SPRM, 1800EAH, bit 9~8) 00 : Mode 0 : Selects number LSB per screen 01 : Mode 1 : Selects number LSB per character 10 : Mode 2 : Selects number LSB per dot Special Priority Number (For Scroll Screen) (xxPRIN, 1800F8H~1800FCH) For NBG0 (or RBG1) (N0PRIN, 1800F8H, bit 2~0) For NBG1 (or EXBG) (N1PRIN, 1800F8H, bit 10~8) For NBG2 (N2PRIN, 1800FAH, bit 2~0) For NBG3 (N3PRIN, 1800FAH, bit 10~8) For RBG0 (R0PRIN, 1800FCH, bit 2~0)



<!-- Page 311 -->

Color Calculation Gradation Calculation Enable (BOKEN, 1800ECH, bit 15) 0 : Does not use Gradation Calculation Function 1 : Uses Gradation Calculation Function Gradation Screen Number (BOKN, 1800ECH, bit 14~12) 000 : Sprite 001 : RBG0 010 : NBG0 or RBG1 100 : NBG1 or EXBG 101 : NBG2 110 : NBG3 Expanded Color Calculation Enable (EXCCEN, 1800ECH, bit 10) 0 : Does not use Expanded Color Calculation 1 : Uses Expanded Color Calculation Color Calculation Ratio Mode (CCRTMD, 1800ECH, bit 9) 0 : Mode 0 : In the case of color calculation, select per top image side 1 : Mode 1 : In the case of color calculation, select per second image side Color Calculation Mode (CCMD, 1800ECH, bit 8) 0 : Mode 0 : Add according to the register value color calculation ratio 1 : Mode 1 : Add as is Color Calculation Enable (xxCCEN, 1800ECH) For NBG0 (or RBG1) (N0CCEN, 1800ECH, bit 0) For NBG1 (or EXBG) (N1CCEN, 1800ECH, bit 1) For NBG2 (N2CCEN, 1800ECH, bit 2) For NBG3 (N3CCEN, 1800ECH, bit 3) For RBG0 (R0CCEN, 1800ECH, bit 4) For LNCL (LCCCEN, 1800ECH, bit 5) For Sprite (SPCCEN, 1800ECH, bit 6) 0 : Does not do Color Calculation 1 : Does Color Calculation Special Color Calculation Mode (xxSCCM, 1800EEH) For NBG0 (or RBG1) (N0SCCM, 1800EEH, bit 1~0) For NBG1 (or EXBG) (N1SCCM, 1800EEH, bit 3~2) For NBG2 (N2SCCM, 1800EEH, bit 5~4) For NBG3 (N3SCCM, 1800EEH, bit 7~6) For RBG0 (R0SCCM, 1800EEH, bit 9~8) 00 : Mode 0 : Select color calculation enable per screen 01 : Mode 1 : Select color calculation enable per character 10 : Mode 2 : Select color calculation enable per dot 11 : Mode 3 : Select color calculation enable per MSB of color data Color Calculation Ratio (For Scroll Screen) (xxCCRT, 180108H~18010EH) For NBG0 (or RBG1) (N0CCRT, 180108H, bit 4~0) For NBG1 (or EXBG) (N1CCRT, 180108H, bit 12~8) For NBG2 (N2CCRT, 18010AH, bit 4~0) For NBG3 (N3CCRT, 18010AH, bit 12~8) For RBG0 (R0CCRT, 18010CH, bit 4~0) For LNCL (LCCCRT, 18010EH, bit 4~0) For BACK (BKCCRT, 18010EH, bit 12~8) ST-58-R2



<!-- Page 312 -->

Color Offset Color Offset Enable (xxCOEN, 180110H) For NBG0 (or RBG1) (N0COEN, 180110H, bit 0) For NBG1 (or EXBG) (N1COEN, 180110H, bit 1) For NBG2 (N2COEN, 180110H, bit 2) For NBG3 (N3COEN, 180110H, bit 3) For RBG0 (R0COEN, 180110H, bit 4) For BACK (BKCOEN, 180110H, bit 5) For Sprite (SPCOEN, 180110H, bit 6) 0 : Does not use Color Offset Function 1 : Uses Color Offset Function Color Offset Select (xxCOSL, 180112H) For NBG0 (or RBG1) (N0COSL, 180112H, bit 0) For NBG1 (or EXBG) (N1COSL, 180112H, bit 1) For NBG2 (N2COSL, 180112H, bit 2) For NBG3 (N3COSL, 180112H, bit 3) For RBG0 (R0COSL, 180112H, bit 4) For BACK (BKCOSL, 180112H, bit 5) For Sprite (SPCOSL, 180112H, bit 6) 0 : Uses value of Color Offset A 1 : Uses value of Color Offset B Color Offset Value (COxxx, 180114H~18011EH) For Color Offset A Red Data (COARD, 180114H, bit 8~0) For Color Offset A Green Data (COAGR, 180116H, bit 8~0) For Color Offset A Blue Data (COABL, 180118H, bit 8~0) For Color Offset B Red Data (COARD, 18011AH, bit 8~0) For Color Offset B Green Data (COAGR, 18011CH, bit 8~0) For Color Offset B Blue Data (COABL, 18011EH, bit 8~0) Shadow Function Shadow Enable (xxSDEN, 1800E2H) For NBG0 (or RBG1) (N0SDEN, 1800E2H, bit 0) For NBG1 (or EXBG) (N1SDEN, 1800E2H, bit 1) For NBG2 (N2SDEN, 1800E2H, bit 2) For NBG3 (N3SDEN, 1800E2H, bit 3) For RBG0 (R0SDEN, 1800E2H, bit 4) For BACK (BKSDEN, 1800E2H, bit 5) 0 : Does not use Shadow Function (Does not add shadow) 1 : Uses Shadow Function (Adds shadow) Transparent Shadow Select (TPSDSL, 1800E2H) 0 : Disables Transparent Shadow Sprite 1 : Enables Transparent Shadow Sprite



<!-- Page 313 -->

