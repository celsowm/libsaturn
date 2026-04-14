|   | N0LSTA7 | N0LSTA6 | N0LSTA5 | N0LSTA4 | N0LSTA3 | N0LSTA2 | N0LSTA1 | ~ |

| LSTA1U | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800A4H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | N1LSTA18 | N1LSTA17 | N1LSTA16 |

| LSTA1L | N1LSTA15 | N1LSTA14 | N1LSTA13 | N1LSTA12 | N1LSTA11 | N1LSTA10 | N1LSTA9 | N1LSTA8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800A6H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | N1LSTA7 | N1LSTA6 | N1LSTA5 | N1LSTA4 | N1LSTA3 | N1LSTA2 | N1LSTA1 | ~ |

| N0LSTA18~N0LSTA16 | 1800A0H | Bit 2~0 | For NBG0 (upper bit) |
| --- | --- | --- | --- |
| N0LSTA15~N0LSTA1 | 1800A2H | Bit 15~1 | For NBG0 (lower bit) |
| N1LSTA18~N1LSTA16 | 1800A4H | Bit 2~0 | For NBG1 (upper bit) |
| N1LSTA15~N1LSTA1 | 1800A6H | Bit 15~1 | For NBG1 (lower bit) |



<!-- Page 159 -->

### Vertical Cell Scroll Table Address Register

The vertical cell scroll table address register is a write-only 32-bit register that selects the lead address of the vertical cell scroll table, and is at addresses 18009CH to 18009EH. Because the value of the register is cleared to 0 after power on or reset, the value must be set. Designates the lead address of the vertical cell scroll table on the VRAM. The actual lead VRAM address is calculated by the expression below. When the VRAM has a 4 Mbit capacity, the address of the most significant bit is ignored.

| VCSTAU | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18009CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | VCSTA18 | VCSTA17 | VCSTA16 |

| VCSTAL | VCSTA15 | VCSTA14 | VCSTA13 | VCSTA12 | VCSTA11 | VCSTA10 | VCSTA9 | VCSTA8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18009EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCSTA7 | VCSTA6 | VCSTA5 | VCSTA4 | VCSTA3 | VCSTA2 | VCSTA1 | ~ |

| VCSTA18~VCSTA16 | 18009CH | Bit 2~0 |
| --- | --- | --- |
| VCSTA15~VCSTA1 | 18009EH | Bit 15~1 |



<!-- Page 160 -->

(This page is blank in the original Japanese document)



<!-- Page 161 -->

# Chapter 6 Rotation Scroll Screen

Introduction .............................................................................144

## 6.1 Rotation Scroll Coordinate Operation ............................... 144

## 6.2 Rotation Scroll Screen Display Control ............................ 148

RAM Control Register ...................................................148

## 6.3 Rotation Parameter Control .............................................. 151

Data Configuration of the Rotation Parameter Table .... 153 Rotation Parameter Table ............................................. 155 Rotation Parameter Read Control Register .................. 157 Rotation Parameter Table Address Register ................. 158 Rotation Read Out of the Frame Buffer ........................ 159 Rotation Parameter Change .........................................160 Rotation Parameter Mode Register .............................. 162

## 6.4 Coefficient Table Control...................................................163

Line Color Screen Data................................................. 164 Bit Configuration of Coefficient Table Data ................... 165 Coefficient Table Lead Address..................................... 165 Most Significant Bit of Coefficient Data ......................... 166 RAM Control Register ...................................................167 Coefficient Table Control Register................................. 168 Coefficient Table Address Offset Register..................... 170 ST-58-R2



<!-- Page 162 -->

## Introduction

The rotation scroll screen has two sets of parameter tables called “Rotation parameter A” and “Rotation parameter B” that can be simultaneously displayed by various parameter tables. Besides being stored as rotation parameters in VRAM, the two sets of parameters can hold various correlated coefficient tables in VRAM. There are two sets of rotation parameters, rotation parameter A and rotation parameter B, each stored in a table. RBG0 can simultaneously display one screen selected by rotation parameter A or rotation parameter B, or two screens selected by rotation parameter A and rotation parameter B. RBG1 can display only the screen designated by rotation parameter B.

Table 6.1 shows the relationship between the rotation

scroll screen and rotation parameters.

**Table 6.1 Rotation scroll screen**

Rotation parameter A and rotation parameter B can each have a coefficient table; there can be multiple displays by reading the coefficient data in each line or each dot. Using rotation parameter A, expansion-reduction rotation of sprite frame buffers can also be done.

## 6.1 Rotation Scroll Coordinate Calculation

The display screen of the rotation scroll screen, which causes rotational conversion (including parallel moving) of the viewpoint reference center point and TV screen, is a collection of points intersecting the line of vision that passes through the TV screen from the viewpoint after conversion with the fixed scroll map. Figure 6.1 shows display method of the rotation scroll screen.

| Screen | Single Display | In Relation to Rotation Parameters |
| --- | --- | --- |
| RBG0 | Allowed | Screen specified by either rotation parameter<br>A or B, or 2 screens specified by rotation<br>parameters A and B are displayed concurrentl |
| RBG1 | Not allowed (RBG0 must also be<br>displayed) | Screen specified by rotation parameter B is<br>displayed |



<!-- Page 163 -->

Z Viewpoint after conversion (Xp, Yp, Zp) Viewpoint before conversion (Px, Py, Pz) Cent ral point (Cx, Cy, Cz) Point on the screen after conversion (Xs, Ys, Zs) X Y Point on the screen bef ore Point displayed on the scroll map conversion (Sx, Sy, Sz) (X , Y, Z)

> Note: X axis runs vertical y through the page to the back.

**Figure 6.1 Rotation scroll screen display method**

From the rotation conversion formula, view coordinates and TV screen coordinates after conversion are expressed by the following equations.

### A B C

### Xp

### Px - Cx

### Cz

### Mx

### D E F

### Yp

### =

### Py - Cy

### +

### Cy

### My

### +

### G H I

### Zp

### Pz - Cz

### Cz

### Mz

### A B C

### Xs

### Sx - Cx

### Cz

### Mx

### D E F

### Ys

### =

### Sy - Cy

### Cy

### My

### +

### +

### G H I

### Zs

### Sz - Cz

### Cz

### Mz

A, B, C, D, E, F, G, H, I: Rotational matrix parameter Px, Py, Pz: View coordinate before rotational conversion Sx, Sy, Sz: TV screen coordinates before rotational conversion Cx, Cy, Cz: Rotational center coordinate Mx, My, Mz: Amount of parallel movement Xp, Yp, Zp: View coordinate after rotational conversion Xs, Ys, Zs: TV screen coordinates after rotational conversion ST-58-R2



<!-- Page 164 -->

The line of vision that passes through the TV screen after rotational conversion, from the viewpoint after rotational conversion, is expressed by the equation below. X - Xp Y - Yp Z - Zp

### Xs - Xp =

### Ys - Yp =

Zs - Zp Because the scroll map is fixed by the XY plane (Z = 0), display coordinates (X, Y) on the scroll map are found by the equation below. X = k (Xs - Xp) + Xp Y = k (Ys - Yp) + Yp However, -Zp k = Zs - Zp This “k”, called the perspective conversion coefficient, rotates only in the vertical direction of the TV screen along the X axis rotation and is fixed in the horizontal direction. Furthermore, the Y axis rotation only changes in the horizontal direction of the TV screen, and is fixed in the vertical direction. Z axis rotation is always fixed. Because the screen prior to rotational conversion is normally identical to the TV screen, Sx is the horizontal coordinate value (H counter value) in the TV screen, Sy is the vertical coordinate value (V counter value) in the TV screen, and Sz is 0. The screen coordinate value when the screen rotates in the vertical axis (SZ axis) is found by the equations below. a b 0 Sx Hcnt - Csx Csx Msx c d 0 Sy = Vcnt - Csy Csy Msy + + 0 0 1 0 0 Sz Msz a, b, c, d: TV screen rotation matrix parameter Hcnt, Vcnt: HV counter value Csx, Csy: TV screen rotation center coordinate Msx, Msy, Msz: TV screen parallel movement amount The previously mentioned expression is as shown below. Sx = Xst + DX • Hcnt + DXst • Vcnt Sy = Yst + DY • Hcnt + DYst • Vcnt Sz = Zst However, Xst = -a • Csx - b • Csy + Csx + Msx Yst = -c • Csx - d • Csy + Csy + Msy



<!-- Page 165 -->

Zst = Msz ∆X = a ∆Y = c ∆Xst = b ∆Yst = d Xst, Yst, Zst: TV screen start coordinate DX, DY: TV screen horizontal coordinate increment DXst, DYst: TV screen vertical coordinate increment Below are the calculation equations of the display coordinates (X, Y) when performing both TV screen 3 axis rotation and TV screen rotation from the equations above. X = kx (Xsp + dX • Hcnt) + Xp Y = ky (Ysp + dY • Hcnt) + Yp However, Xsp = A{(Xst + ∆Xst • Vcnt) - Px} + B{(Yst + ∆Yst • Vcnt) - Py} + C(Zst - Pz) Ysp = D{(Xst + ∆Xst • Vcnt) - Px} + E{(Yst + ∆Yst • Vcnt) - Py} + F(Zst - Pz) Xp = A(Px - Cx) + B(Py - Cy) + C(Pz - Cz) + Cx + Mx Yp = D(Px - Cx) + E(Py - Cy) + F(Pz - Cz) + Cy + My dX = A • ∆X + B • ∆Y dY = D • ∆X + E • ∆Y Xst, Yst, Zst: TV screen start coordinates ∆Xst, ∆Yst: TV screen vertical coordinate increments ∆X, ∆Y: TV screen horizontal coordinate increments A, B, C, D, E, F: Rotational matrix parameter Px, Py, Pz: View coordinates Cx, Cy, Cz: Center coordinates Mx, My: Amount of parallel movement kx, ky: Expansion reduction coefficient Hcnt, Vcnt: HV counter value VDP2 reads per line all parameters from the rotation parameter table stored on VRAM, calculates Xsp, Ysp, Xp, Yp, dX, dY used for the above calculation equation, and uses these results to find the display coordinates (X, Y) of each dot. Expansion reduction coefficients kx and ky usually use values read from the rotational parameter table. By using the coefficient table, values in all lines and dots can be changed. ST-58-R2



<!-- Page 166 -->

## 6.2 Rotation Scroll Screen Display Control

The rotation scroll screen has two surfaces, RBG0 and RBG1. When RBG1 is displayed, RBG0 must also be displayed (RBG0 appears when only one surface is displayed.) The Normal scroll screens can no longer be displayed at that time. The image data (pattern name table or bitmap pattern) being displayed in the rotation scroll screen cannot be with image data of the Normal scroll screen; neither can image data of RBG0 and RBG1 be used in common. Furthermore, image data of the rotation scroll screen must be stored in separate VRAM. Among image data, the RBG1 pattern name table is stored in VRAM-B1, and character pattern table is stored in VRAM-B0. When RBG0 needs coefficient only data with lines, the coefficient table can be stored in any VRAM bank. Image data must be stored in different banks when required with dots. The register that controls the display of the rotation scroll screen has a screen display enable register and RAM control register. The screen display enable register controls the screen display and transparency code. The register content is the same as the Normal scroll screen. See “4.1 Screen Display Control” for details.

### RAM Control Register

The RAM control register selects the VRAM bank partition, the objective for using the rotation scroll screen VRAM, and the color RAM mode. It is a read-write 16 bit register and is at address 18000EH. Because the value of the register is cleared to 0 after power on or reset, you must set the value. See “6.4 Coefficient Table Control.”

| RAMCTL | CRKTE | ~ | CRMD1 | CRMD0 | ~ | ~ | VRBMD | VRAMD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18000EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | RDBSB11 | RDBSB10 | RDBSB01 | RDBSB00 | RDBSA11 | RDBSA10 | RDBSA01 | RDBSA00 |



<!-- Page 167 -->

See “3.4 Color RAM mode.” Set the Color RAM mode to mode 1 when the CRKTE bit is 1. At that time, color data can no longer be stored because the second half of the color RAM (100800H ~ 100FFFH) is used for the coefficient table data. See “3.2 VRAM Bank Partition.” Designates the use objective of the VRAM of the rotation scroll screen. This bit is only in effect when the rotation scroll screen is displayed.

> Note: A0, A1, B0, or B1 is entered in bit name for x.

When there are no bank partitions in VRAM, the VRAM-A0 bit is used for VRAM-A, and the VRAM-B0 bit is used for VRAM-B. When coefficient data is not treated as being needed in all dots, there is no need to set the coefficient table RAM (01B). When displaying by the bit map format, do not set the pattern name table RAM (10B). VRAM cycle pattern register settings of the VRAM bank selected in RAM used for the rotational scroll are ignored. Data will not be read out when there is no image data read-out address in the selected bank. Therefore, the correct screen can no longer be displayed.

| RDBSA00, RDBSA01 | 18000EH | Bit 1,0 | For VRAM-A0 (or VRAM-A) |
| --- | --- | --- | --- |
| RDBSA10, RDBSA11 | 18000EH | Bit 3,2 | For VRAM-A1 |
| RDBSB00, RDBSB01 | 18000EH | Bit 5,4 | For VRAM-B0 (or VRAM-B) |
| RDBSB10, RDBSB11 | 18000EH | Bit 7,6 | For VRAM-B1 |

| RDBSx1 | RDBSx0 | VRAM Use |
| --- | --- | --- |
| 0 | 0 | Not used as RBG0 RAM |
| 0 | 1 | RAM for RBG0 End table |
| 1 | 0 | RAM for RGB0 Pattern Name table |
| 1 | 1 | RAM for RBG0 Character Pattern table (or Bitmap Pattern) |



<!-- Page 168 -->

When displaying RBG1, 00B must be set in bits used for VRAM-B0 and VRAM-B1. When the coefficient data read address is not the address within the selected bank, the coefficient data can not be read properly and therefore correct screen image can not be displayed. In addition, when storing the coefficient table in the color RAM, RBG0 coefficient table RAM (01B) must not be set.



<!-- Page 169 -->

## 6.3 Rotation Parameter Control

When displaying the rotation scroll screen, be sure to store the rotation parameter on which control is performed as a table in VRAM. The rotation scroll screen reads the rotation parameter tables stored in VRAM for each line. The screen is displayed according to that value. The rotation parameter is shown below.

**Table 6.2 Rotation Parameters**

A B C D E F G H I

> Note: * denotes when reading to each line.

| Rotation Parameter |   | Definition |
| --- | --- | --- |
|   | Xst | Screen upper left corner (or left edge) X coordinate |
| Screen Start Coordinate | Yst | Screen upper left corner (or left edge) Y coordinate |
|   | Zst | Screen upper left corner (or left edge) Z coordinate |
| Screen Vertical Coordinate | D Xst | Screen coordinates X increment per each line |
| Increments | D Yst | Screen coordinates Y increment per each line |
| Screen Horizontal Coordinate | D X | Screen coordinates X increment per each dot |
| Increments | D Y | Screen coordinates Y increment per each dot |
| Rotation Matrix Parameter |   | These parameters are include in 3X3 rotation matrix.<br>A B C<br>D E F<br>G H I |
|   | Px | Viewpoint X coordinate |
| Viewpoint Coordinates | Py | Viewpoint Y coordinate |
|   | Pz | Viewpoint Z coordinate |
|   | Cx | Center X coordinate |
| Center Coordinates | Cy | Center Y coordinate |
|   | Cz | Center Z coordinate |
| Amount of Horizontal | Mx | Shift in X direction for screen, viewpoint, and center |
| Shift | My | Shift in Y direction for screen, viewpoint, and center |
| Scaling Coefficients | kx | Scale coefficient of display screen in X direction |
|   | ky | Scale coefficient of display screen in Y direction |
| Coefficient Table Start Address | KAst | Table start address when using coefficients table |
| Coefficient Table Vertical<br>Address Increment | D KAst | Address increment per each line when using<br>coefficients table |
| Coefficient Table Horizontal<br>Address Increment | D KAx | Address increment when using coefficients table per<br>each dot |



<!-- Page 170 -->

Only Xst, Yst, and KAst among rotation parameters can be read by the first line of the display screen. The value of Xst, Yst, and KAst (when ∆Xst, ∆Yst, ∆X, ∆Y, ∆KAst, and ∆KAx don’t change inside one screen) are expressed by the equation below. (Screen X coordinate) = Xst + ∆Xst x (V counter value) + ∆X x (H counter value) (Screen Y coordinate) = Yst + ∆Yst x (V counter value) + ∆Y x (H counter value) (coefficient table address) = KAst + ∆KAst x (V counter value) + ∆KAx x (H counter value) Moreover, the first line can be read (in addition to Xst, Yst, and KAst) by setting the rotation parameter read control register. Values on and after the second lines of Xst, Yst, and KAst (when ∆Xst, ∆Yst, ∆X, ∆Y, ∆KAst, and ∆KAx do not change within screen one) are expressed by the equation below. (Screen X coordinate) = Xst + ∆Xst x {(V counter value) - (V counter value when Xst is read out)} + ∆X x (H counter value) (Screen Y coordinate) = Yst + ∆Yst x {(V counter value) - (V counter value when Yst is read out)} + ∆Y x (H counter value) (coefficient table address) = KAst + ∆KAst x {(V counter value) - (V counter value when KAst is read out)} + ∆KAx x (H counter value) The rotation scroll screen has two sets of parameter tables, called “Rotation Parameter A” and “Rotation Parameter B.” The display screen of RBG1 is carried out by rotation parameter B. RBG0 selects which of the two sets of parameter tables is used, and can change within the display screen. Through this, RBG0 can simultaneously display two different rotation scroll screens on one screen.



<!-- Page 171 -->

In addition, the rotation parameter table moves by storing rotation parameter tables using RBG0 and RBG1, and does not always have to store two sets of rotation parameter tables.

### Data Configuration of the Rotation Parameter Table

**Figure 6.2 shows various bit configurations of rotation parameters. Negative num-**

bers indicate by two complements. The shaded part of the bit is ignored.

> Note: Shaded areas are ignored.

**Figure 6.2 Rotation parameter data configuration**

|   | Sign | 12 bit integer part |
| --- | --- | --- |

| 10 bit fractional part | d |
| --- | --- |

| n | Sign |
| --- | --- |

| 10 bit fractional part |
| --- |

|   | Sign |
| --- | --- |

| 10 bit fractional part |
| --- |



<!-- Page 172 -->

> Note: Shaded areas are ignored.

**Figure 6.2**

|   | Sign |
| --- | --- |

| 10 bit fractional part |
| --- |

|   | Sign |   | 13 bit integer part |
| --- | --- | --- | --- |

|   | Sign | 13 bit integer part | 13 bit integer part |   |   |   | d |
| --- | --- | --- | --- | --- | --- | --- | --- |

|   | Sign | 13 bit integer part | 13 bit integer part |
| --- | --- | --- | --- |

| 10 bit fractional part |
| --- |

|   | Sign |   | 7 bit integer part |
| --- | --- | --- | --- |

| G | 16 bit integer part |
| --- | --- |

| 10 bit fractional part |
| --- |



<!-- Page 173 -->

> Note: Shaded areas are ignored

**Figure 6.2 Rotation parameter data configuration (continued)**

### Rotation Parameter Table

One set of rotation parameter tables at a size of 60H is stored in VRAM. Figure 6.3 shows the configuration of one set of tables.

|   | Sign | 9 bit integer part |
| --- | --- | --- |

| 10 bit fractional part | t |
| --- | --- |

|   | Sign | 9 bit integer part |
| --- | --- | --- |

| 10 bit fractional part | d |
| --- | --- |



<!-- Page 174 -->

**Figure 6.3 Rotation parameter table**

| Screen Start Coordinate Xst (Integer Part) |   |
| --- | --- |
| (Fractional Part) |   |
| Screen Start Coordinate Yst (Integer Part) |   |
| (Fractional Part) |   |
| Screen Start Coordinate Zst (Integer Part) |   |
| (Fractional Part) |   |
| Screen Vertical Coordinate Increment D Xst (Integer Part) |   |
| (Fractional Part) |   |
| Screen Vertical Coordinate Increment D Yst (Integer Part) |   |
| (Fractional Part) |   |
| Screen Horiz. Coordinate Increment D X (Integer Part) |   |
| (Fractional Part) |   |
| Screen Horiz. Coordinate Increment D Y (Integer Part) |   |
| (Fractional Part) |   |
| Rotation Matrix Parameter A (Integer Part) |   |
| (Fractional Part) |   |
| Rotation Matrix Parameter B (Integer Part) |   |
| (Fractional Part) |   |
| Rotation Matrix Parameter C (Integer Part) |   |
| (Fractional Part) |   |
| Rotation Matrix Parameter D (Integer Part) |   |
| (Fractional Part) |   |
| Rotation Matrix Parameter E (Integer Part) |   |
| (Fractional Part) |   |
| Rotation Matrix Parameter F (Integer Part) |   |
| (Fractional Part) |   |
| Viewpoint Coordinate Px (Integer Part) |   |
| Viewpoint Coordinate Py (Integer Part) |   |
| Viewpoint Coordinate Pz (Integer Part) |   |
|   | This data is ignored |
| Center Point Coordinate Cx (Integer Part) |   |
| Center Point Coordinate Cy (Integer Part) |   |
| Center Point Coordinate Cz (Integer Part) |   |
|   | This data is ignored |
| Horizontal Shift Mx (Integer Part) |   |
| (Fractional Part) |   |
| Horizontal Shift My (Integer Part) |   |
| (Fractional Part) |   |
| Scaling Coefficient kx (Integer Part) |   |
| (Fractional Part) |   |
| Scaling Coefficient ky (Integer Part) |   |
| (Fractional Part) |   |
| Coefficient Table Start Address KAst (Integer Part) |   |
| (Fractional Part) |   |
| Coefficient Table Vertical Address Increment D KAst (Integer Part) |   |
| (Fractional Part) |   |
| Coefficient Table Horiz. Address Increment D KAx (Integer Part) |   |
| (Fractional Part) |   |



<!-- Page 175 -->

When storing two sets of tables of rotation parameter A and rotation parameter B, store the rotation parameter A from the lead address of the rotation parameter table, then enter the 20H part of invalid data and store tables of rotation parameter B. The rotation parameter table does not always have to store two sets, but can store only the tables needed. Figure 6.4 shows the method of storing two sets of tables from rotation parameters A and B.

**Figure 6.4 How to store to the rotation parameter table VRAM**

### Rotation Parameter Read Control Register

The rotation parameter read control register is a write-only 16 bit register that indicates whether to read Xst, Yst, and KAst in each line, and is at address 1800B2H. Because the value is cleared to 0, it must be set after power on or reset.

| Rotation ParameterA Table |
| --- |
| The data in this area is not used as<br>rotation parameter |
| Rotation Parameter B Table |

| RPRCTL | ~ | ~ | ~ | ~ | ~ | RBKASTRE | RBYSTRE | RBXSTRE |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800B2H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | RAKASTRE | RAYSTRE | RAXSTRE |



<!-- Page 176 -->

Designates the coefficient table start address KAst and TV screen start coordinates Xst and Yst, and whether to read from the rotation parameter table in that line.

> Note: AX, BX, AY, BY, AKA, or BKA is entered in bit name for x.

If this bit is 1, selected parameters are read when the next rotation parameters are read. At the same time, this bit is cleared to 0. Therefore, to read parameters for each 1 line, this bit must be set to 1 for each line.

### Rotation Parameter Table Address Register

The rotation parameter table address register is a write-only 16-bit register that selects the lead address of the rotation parameter table, and is at address 1800BCH to 1800BEH. Because the value is cleared to 0, it must be set after power on or reset.

| RAXSTRE | 1800B2H | Bit 0 | For Xst of Rotation Parameter A |
| --- | --- | --- | --- |
| RBXSTRE | 1800B2H | Bit 8 | For Xst of Rotation Parameter B |
| RAYSTRE | 1800B2H | Bit 1 | For Yst of Rotation Parameter A |
| RBYSTRE | 1800B2H | Bit 9 | For Yst of Rotation Parameter B |
| RAKASTRE | 1800B2H | Bit 2 | For KAst of Rotation Parameter A |
| RBKASTRE | 1800B2H | Bit 10 | For KAst of Rotation Parameter B |

| RxSTRE | Process |
| --- | --- |
| 0 | Selected parameters are not read for that line |
| 1 | Selected parameters are read for that line |

| RPTAU | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800BCH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | RPTA18 | RPTA17 | RPTA16 |

| RPTAL | RPTA15 | RPTA14 | RPTA13 | RPTA12 | RPTA11 | RPTA10 | RPTA9 | RPTA8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800BEH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | RPTA7 | RPTA6 | RPTA5 | RPTA4 | RPTA3 | RPTA2 | RPTA1 | ~ |



<!-- Page 177 -->

Designates the lead address of rotation parameter tables. RPTA6 bit is ignored even if data is written. The bit is set at 0 for rotation parameter A, and fixed at 1 for rotation parameter B. The actual lead address of a rotation parameter table is calculated as shown in the equation below. When the VRAM size is 4 Mbit, the most significant bit of the address is ignored. For example, when 00170H or 00130H is selected, the lead address of rotation parameter A is 00260H, and the lead address of rotation parameter B is 002E0H.

### Rotation Read Out of the Frame Buffer

Rotation reading of the frame buffer is executed using the TV screen start coordinates (Xst, Yst) of rotation parameter A, TV screen vertical coordinate increments (∆Xst, ∆Yst), and TV screen horizontal coordinate increments (∆X, ∆Y). If the image selected by rotation parameter A in the rotation scroll screen is to be displayed, the entire sprite and rotation scroll screen can be made to rotate identically. If the image selected by rotation parameter B in the rotation scroll screen is to be displayed, the entire sprite and rotation scroll screen can be made to rotate separately. The frame buffer display coordinate value calculates from the TV screen starting coordinate, the display coordinate of the left end of the line calculated in each line from the TV screens vertical coordinate increment and horizontal coordinate. The bit will be discarded and calculated from the coordinate range of the frame buffer so that the display coordinate value of the line’s left end is calculated in 20 bits (code+integer10-bit +decimal part 9-bit), and the horizontal coordinate increment in a total of 12 bits (code+integer part 2-bit+decimal part 9-bit).

> Note:

| RPTA18~RPTA16 | 1800BCH | Bit 2~0 |
| --- | --- | --- |
| RPTA15~RPTA1 | 1800BEH | Bit 15~1 |



<!-- Page 178 -->

### Rotation Parameter Change

Rotation parameters table address bit (RPTA18 to RPTA1) Designates the lead address of rotation parameter tables. RBG0 indicates which of two sets of rotation parameter tables is used, and can change in part the rotation parameter on one screen display. The method of using the rotation parameter can be selected from the four rotation parameter modes below. RPTA6 bit is ignored even if data is written. The bit is set at 0 for rotation parameter A, and fixed at 1 for rotation parameter B. Mode 0: Uses rotation parameter A Mode 1: Uses rotation parameter B The actual lead address of a rotation parameter table is calculated as shown in the Mode 2: Changes the image by coefficient data read from the coefficient table of rotation equation below. When the VRAM size is 4 Mbit, the most significant bit of the parameter A address is ignored. Mode 3: Changes by the rotation parameter window. (Lead address of rotation parameter A) Modes 0 and 1 display the image obtained through each rotation parameter table set. = (rotation parameter table address register value highest 12 bit) X 100H Modes 2 and 3 display the images within one screen obtained through rotation + (rotation parameter table address register value lowest 5 bit) X 4H parameters A and B. (Lead address of rotation parameter B) Set Mode 2 to use the coefficient table of rotation parameter A by setting the RAKTE = (rotation parameter table address register value highest 12 bit) X 100H bit of the coefficient table control register to 1. The value of the most significant bit + (rotation parameter table address register value lowest 5 bit) X 4H + 80H of coefficient data read from the coefficient table displays the image obtained by rotation parameter A when it is 0, but displays the image obtained by rotation pa- For example, when 00170H or 00130H is selected, the lead address of rotation parameter B as an RBG0 image when it is 1. When set to read coefficient data used for rameter A is 00260H, and the lead address of rotation parameter B is 002E0H. rotation parameter A in each dot, two images can also be changed in each dot, but

### Rotation Read Out of the Frame Buffer

coefficient data used for rotation parameter B cannot be read in each dot. Rotation reading of the frame buffer is executed using the TV screen start coordi- Mode 3 changes two images according to the bit used in the rotation parameter nates (Xst, Yst) of rotation parameter A, TV screen vertical coordinate increments window of the rotation window control register. When the window is used as the (∆Xst, ∆Yst), and TV screen horizontal coordinate increments (∆X, ∆Y). If the image transparent process window, the part of the screen that is cut off and made transparselected by rotation parameter A in the rotation scroll screen is to be displayed, the ent displays the image obtained by rotation parameter B; the remaining part is entire sprite and rotation scroll screen can be made to rotate identically. If the image displayed as an RBG0 image obtained by rotation parameter A. selected by rotation parameter B in the rotation scroll screen is to be displayed, the entire sprite and rotation scroll screen can be made to rotate separately. The frame buffer display coordinate value calculates from the TV screen starting coordinate, the display coordinate of the left end of the line calculated in each line from the TV screens vertical coordinate increment and horizontal coordinate. The bit will be discarded and calculated from the coordinate range of the frame buffer so that the display coordinate value of the line’s left end is calculated in 20 bits (code+integer10-bit +decimal part 9-bit), and the horizontal coordinate increment in a total of 12 bits (code+integer part 2-bit+decimal part 9-bit). ST-58-R2



<!-- Page 179 -->

An example of image display from above modes 0 to 3 is shown in Figure 6.5.

**Figure 6.5 Rotation parameter change**

| Image obtained through<br>rotation parameterA |
| --- |

| Image obtained through<br>rotation parameter B |
| --- |

| Image obtained through<br>rotation parameter B<br>Image obtained<br>through rotation<br>parameterA | Image obtained through<br>rotation parameter B |
| --- | --- |
| Image obtained<br>through rotation<br>parameterA |   |



<!-- Page 180 -->

### Rotation Parameter Mode Register

The rotation parameter mode register is a write-only 16 bit register that controls rotation parameter tables used in RBG0, and is at address 1800B0H. Because the value is cleared to 0, it must be set after power on or reset. When displaying RGB0, designates which rotation parameter of A or B will be used. The value of this bit is always in effect, therefore, be careful in timing reloading. When mode 2 is selected, coefficient data cannot be read to each dot from the coefficient table for rotation parameter B while coefficient data for rotation parameter A is being read to each dot. Therefore, the designation is ignored even if a register is designated so that coefficient data is read to each dot from the coefficient table used for rotation parameter B. In mode 3, coefficient data can be read to each dot in both coefficient tables for rotation parameter A and B. Mode 0 must be set when displaying RBG1.

| RPMD | ~ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800B0H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | RPMD1 | RPMD0 |

| RPMD1 | RPMD0 | Mode | Rotation Paramete r |
| --- | --- | --- | --- |
| 0 | 0 | 0 | Rotation Parameter A |
| 0 | 1 | 1 | Rotation Parameter B |
| 1 | 0 | 2 | A screen and B screen are switched via<br>coefficient data read from rotation parameter<br>coefficient table. |
| 1 | 1 | 3 | A screen and B screen are switched via<br>rotation parameter window |



<!-- Page 181 -->

## 6.4 Coefficient Table Control

The rotation scroll screen stores parameters used in calculating display coordinates in VRAM or color RAM in a table separate from the rotation parameter table, and can express various images by reading parameters per line or per dot. This table is referred to as “coefficient table.” The timing required for the coefficient table data, depending on how display coordinates are calculated, falls under the following cases: 1. Required per line 2. Required per dot When coefficient table data is required per line, the coefficient table must be stored in VRAM. The VRAM address to read the stored coefficient table data is specified via KAst, ∆KAst, and ∆KAx in rotation parameter table and coefficient table address offset register. When coefficient table data is required per dot, the coefficient table must be stored in either VRAM or color VRAM. When stored in VRAM, at least 1 bank in RAM control register “rotation data bank selection” must be selected to become coefficient table. The VRAM address to read the stored coefficient table data is specified via KAst, ∆KAst, and ∆KAx in rotation parameter table and coefficient table address offset register. Also, when storing coefficient table in color RAM, it should be stored in the latter half of color RAM (100800H to 100FFFH). The color RAM address to read the stored coefficient table data is specified via KAst, ∆KAst, and ∆KAx in rotation parameter table. As for the address to read coefficient table data, only the lower 10 bits in the integer part of the calculated coefficient table address become valid. To select parameters for which the data read from the coefficient table are to be used, the following 4 modes (coefficient data modes) are provided: Mode 0: Used as Scale coefficient kx and ky Mode 1: Used as Scale coefficient kx Mode 2: Used as Scale coefficient ky Mode 3: Used as viewpoint coordinate Xp after rotation conversion In mode 0, kx and ky read from the rotation parameter table become invalid; data read from the coefficient table is used as kx and ky. When mode 1 is selected, ky read from the rotation parameter table is used, but data read from the coefficient table is used for kx. ST-58-R2



<!-- Page 182 -->

When mode 2 is selected, kx read from the rotation parameter table is used, but data read from the coefficient table is used for ky. When mode 3 is selected, X direction viewpoint coordinate Xp, converted rotationally as data read from the rotation parameter table, becomes invalid. Data read from the coefficient table is used for Xp.

### Line Color Screen Data

Coefficient data can be used not only as rotation parameters, but as part of line color screen data. In this case, the highest 4-bit data read from the line color screen table is added to the highest of 7-bit data that is part of coefficient data. Figure 6.6 shows line color screen data using coefficient data.

**Figure 6.6 Line color screen data using coefficient data**

When specifying mode 0 for rotation parameter mode, line color screen per rotation parameter A coefficient table is used. When specifying mode 1, line color screen per rotation parameter B coefficient table is used. When specifying mode 2, for both rotation parameter A graphics and rotation parameter B graphics, line color screen per rotation parameter A coefficient table is used. When specifying mode 3, for rotation parameter A graphics, line color screen per rotation parameter A coefficient table is used, whereas for rotation parameter B graphics, line color screen per rotation parameter B coefficient table is used. Also, when displaying RBG1, for both RBG0 and RBG1, line color screen per rotation parameter A coefficient table is used.



<!-- Page 183 -->

### Bit Configuration of Coefficient Table Data

Either “1-word” or “2-word” can be chosen as the data size on the coefficient table. The data configuration changes depending on this coefficient data size and coefficient data mode. Figure 6.7 shows the bit configuration of coefficient table data.

> Note: The MS B are sign-expanded by 3 bit s and the LS B are 0-expanded

**Figure 6.7 Bit configuration of coefficient table data**

|   | 7 bit line color screen data | Sign | 7 bit integer part |
| --- | --- | --- | --- |

|   | Sign | 4 bit integer part | 10 bit fractional part |
| --- | --- | --- | --- |

|   | 7 bit line color screen data | Sign | Integer part MSB 7 bits |
| --- | --- | --- | --- |

| Integer part LSB 8 bits | 8 bit fractional part |
| --- | --- |

|   | Sign | 12 bit integer part |
| --- | --- | --- |



<!-- Page 184 -->

### Coefficient Table Lead Address

The coefficient table lead address is obtained from the coefficient table start address (KAst integer part 16-bit) read from the coefficient table address offset register and rotation parameter table. The coefficient table vertical address increment (∆KAst integer part 9-bit) and coefficient table horizontal address increment (∆KA integer part 9-bit) are also read from the rotation parameter table. The address value of the address offset, start address and address increment change according to the data size of the coefficient table.

Table 6.3 shows the address value

showing the least significant bit of each value. For example, when 4H is 2-word (the least significant bit of integer KAst signifies the expression of the 4H address value), it can be calculated as shown below.

**Table 6.3**

### Most Significant Bit of Coefficient Data

The most significant bit of coefficient data is usually used as transparent bits; dots that used coefficient data in which this bit is 1 are forced to be transparent dots. However, when rotation parameter mode 2 is selected by RBG0, the most significant bit of data read from the coefficient table used for rotation parameter A is used for switching rotation parameters. When the most significant bit is 0, the designated image is displayed by rotation parameter A. When the most significant bit is 1, the designated image is displayed by rotation parameter B. Here, the most significant bit of coefficient data read from the coefficient table used for rotation parameter B is used as a transparent bit. The most significant bit of coefficient data for RBG1 is always used as a transparent bit.

Table 6.4 shows image processing by the most

significant bit values of coefficient data.

| Coefficient | Address Value Indicated by the LSB |   |
| --- | --- | --- |
| Data Size | Coefficient Table Address<br>Offset Register Value | KAst, D KAst, D KAx Integer<br>part value |
| 2 Words | 40000H | 4H |
| 1 Word | 20000H | 2H |



<!-- Page 185 -->

**Table 6.4 Image processing using RBG0 coefficient data MSB value**

### RAM Control Register

The RAM control register is a read/write 16-bit register that selects the VRAM bank partition, rotation scroll screen VRAM use, and color RAM mode. After power-on or reset, the value will be cleared and therefore must be set. Selects whether to store the coefficient table in the color RAM.

| Rotation<br>Parameter<br>Mode | Rotation<br>Parameter | MSB Function | MSB Value | Image Process |
| --- | --- | --- | --- | --- |
|   | A | Transparent | 0 | Displays image obtained using the<br>coefficient data |
| 0 |   |   | 1 | Forces the dot to be transparent<br>using the coefficient data |
|   | B | Not Used | - | - |
|   | A | Not Used | - | - |
| 1 | B | Transparent | 0 | Displays image obtained using the<br>coefficient data |
|   |   |   | 1 | Forces the dot to be transparent<br>using the coefficient data |
|   | A | Parameter | 0 | Displays image obtained using the<br>coefficient data |
| 2 |   | Switching | 1 | Invalidates the coefficient data an<br>displays image obtained using<br>rotation parameter B |
|   | B | Transparent | 0 | Displays image obtained using the<br>coefficient data |
|   |   |   | 1 | Forces the dot to be transparent<br>using the coefficient data |
| 3 | A, B | Transparent | 0 | Displays image obtained using the<br>coefficient data |
|   |   |   | 1 | Forces the dot to be transparent<br>using the coefficient data |

| RAMCTL | CRKTE | ~ | CRMD1 | CRMD0 | ~ | ~ | VRBMD | VRAMD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18000EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | RDBSB11 | RDBSB10 | RDBSB01 | RBBSB00 | RDBSA11 | RDBSA10 | RDBSA01 | RBBSA00 |

| CRKTE | Function |
| --- | --- |
| 0 | Coefficient table is stored in VRAM |
| 1 | Coefficient table is stored in color RAM |



<!-- Page 186 -->

Please see color RAM mode in 3.4. When CRKTE is set to 1, please set color RAM mode to 1. Here, the latter half of the color RAM (100800H-100FFFH) will be used for coefficient table data, therefore, the color data cannot be stored. Please see VRAM bank partition in 3.2. Please see rotation scroll display control in 6.2. When CRKTE is set to 1, VRAM bank 4 may not be selected to be used as coefficient table data RAM.

### Coefficient Table Control Register

The coefficient table control register is a write-only 16-bit register that controls the coefficient table, and is at address 1800B4H. Because the value is cleared to 0, it must be set after power on or reset. Designates whether to use line color screen data in coefficient data. This bit uses the corresponding coefficient table and is effective only when the data size is 2-word.

> Note: A or B is entered in the bit name for x.

| KTCTL | ~ | ~ | ~ | RBKLCE | RBKMD1 | RBKMD0 | RBKDBS | RBKTE |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800B4H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | RAKLCE | RAKMD1 | RAKMD0 | RAKDBS | RAKTE |

| RAKLCE | 1800B4H | Bit 4 | For Rotation Parameter A |
| --- | --- | --- | --- |
| RBKLCE | 1800B4H | Bit 12 | For Rotation Parameter B |

| RxKLCE | Process |
| --- | --- |
| 0 | Line color screen data within coefficient data is not used |
| 1 | Line color screen data within coefficient data is used |



<!-- Page 187 -->

Designates what parameters the coefficient data is used as. Because this bit is always in effect, be careful in rewriting timing.

> Note: A or B is entered in the bit name for x.

Designates the size of the coefficient data. This bit is in effect only when the corresponding coefficient table is used.

> Note: A or B is entered in the bit name for x.

Designates whether the coefficient table is used.

> Note: A or B is entered in the bit name for x.

| RAKMD1, RAKMD0 | 1800B4H | Bit 3,2 | For Rotation Parameter A |
| --- | --- | --- | --- |
| RBKMD1, RBKMD0 | 1800B4H | Bit 11,10 | For Rotation Parameter B |

| RxKMD1 | RxKMD0 | Mode | Coefficient Data Function |
| --- | --- | --- | --- |
| 0 | 0 | 0 | Use as scale coefficient kx, ky |
| 0 | 1 | 1 | Use as scale coefficient kx |
| 1 | 0 | 2 | Use as scale coefficient ky |
| 1 | 1 | 3 | Use as viewpoint Xp after rotation conversion |

| RAKDBS | 1800B4H | Bit 1 | For Rotation Parameter A |
| --- | --- | --- | --- |
| RBKDBS | 1800B4H | Bit 9 | For Rotation Parameter B |

| RxKDBS | Coefficient Data Size |
| --- | --- |
| 0 | 2 Words |
| 1 | 1 Word |

| RAKTE | 1800B4H | Bit 0 | For Rotation Parameter A |
| --- | --- | --- | --- |
| RBKTE | 1800B4H | Bit 8 | For Rotation Parameter B |

| RxKTE | Process |
| --- | --- |
| 0 | Do not use coefficient table |
| 1 | Use coefficient table |



<!-- Page 188 -->

### Coefficient Table Address Offset Register

Coefficient table address offset register is a write-only 16-bit register that designates the coefficient table lead address offset value, and is at address 1800B6H. Because the value is cleared to 0, it must be set after power on or reset. Designates the lead address offset value of the coefficient table stored in the rotation parameter table. These bits are added to the highest coefficient table start address (KAst) read from the rotation parameter table. The actual lead address of the coefficient table changes according to the size of the coefficient data, and is calculated by the expression below. When VRAM size is 4 Mbits, the most significant bit of the address is ignored. When the coefficient data size is 2 word: When the coefficient data size is 1 word:

| KTAOF | ~ | ~ | ~ | ~ | ~ | RBKTAOS2 | RBKTAOS1 | RBKTAOS0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1800B6H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | RAKTAOS2 | RAKTAOS1 | RAKTAOS0 |

| RAKTAOS2~RAKTAOS0 | 1800B6H | Bit 2~0 | For Rotation Parameter A |
| --- | --- | --- | --- |
| RBKTAOS2~RBKTAOS0 | 1800B6H | Bit 10~8 | For Rotation Parameter B |



<!-- Page 189 -->

