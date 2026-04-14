# VDP2 User's Manual

Converted from `vdp2_sega_manual.pdf`.

This Markdown preserves extracted text and adds tables converted from the PDF. Figure captions are preserved as text.


<!-- Page 1 -->

### General Notice

When using this document, keep the following in mind: 1. This document is confidential. By accepting this document you acknowledge that you are bound by the terms set forth in the non-disclosure and confidentiality agreement signed separately and /in the possession of SEGA. If you have not signed such a non-disclosure agreement, please contact SEGA immediately and return this document to SEGA. 2. This document may include technical inaccuracies or typographical errors. Changes are periodically made to the information herein; these changes will be incorporated in new versions of the document. SEGA may make improvements and/or changes in the product(s) and/or the program(s) described in this document at any time. 3. No one is permitted to reproduce or duplicate, in any form, the whole or part of this document without SEGA’S written permission. Request for copies of this document and for technical information about SEGA products must be made to your authorized SEGA Technical Services representative. 4. No license is granted by implication or otherwise under any patents, copyrights, trademarks, or other intellectual property rights of SEGA Enterprises, Ltd., SEGA of America, Inc., or any third party. 5. Software, circuitry, and other examples described herein are meant merely to indicate the characteristics and performance of SEGA’s products. SEGA assumes no responsibility for any intellectual property claims or other problems that may result from applications based on the examples describe herein. 6. It is possible that this document may contain reference to, or information about, SEGA products (development hardware/software) or services that are not provided in countries other than Japan. Such references/information must not be construed to mean that SEGA intends to provide such SEGA products or services in countries other than Japan. Any reference of a SEGA licensed product/program in this document is not intended to state or simply that you can use only SEGA’s licensed products/programs. Any functionally equivalent hardware/software can be used instead. 7. SEGA will not be held responsible for any damage to the user that may result from accidents or any other reasons during operation of the user’s equipment, or programs according to this document. NOTE: A reader's comment/correction form is provided with this document. Please address comments to : SEGA of America, Inc., Developer Technical Support (att. Evelyn Merritt) 150 Shoreline Drive, Redwood City, CA 94065 SEGA may use or distribute whatever information you supply in any way it believes appropriate without incurring any obligation to you. (6/27/95- 002)



<!-- Page 2 -->

TM

## VDP2

## User's Manual

## Version 1.1

Doc. #ST-58-R2-060194 © 1994 SEGA. All Rights Reserved.



<!-- Page 3 -->

## READER CORRECTION/COMMENT SHEET

Where to send your corrections:

| Chpt. | pg. # | Correction |
| --- | --- | --- |
|   |   | f |
|   |   | n |
|   |   | o |
|   |   | C |
|   |   | A |

|   | Where to send your corrections:<br>Fax: (415) 802-3963 Mail: SEGA OF AMERICA<br>Attn: Manager, Attn: Manager,<br>Developer Technical Support Developer Technical Support<br>275 Shoreline Dr. Ste 500<br>Redwood City, CA 94065 |
| --- | --- |



<!-- Page 4 -->

## REFERENCES

In translating/creating this document, certain technical words and/or phrases were interpreted with the assistance of the technical literature listed below. 1. KenKyusha New Japanese-English Dictionary 1974 Edition 2. Nelson’s Japanese-English Character Dictionary 2nd revised version 3. Microsoft Computer Dictionary 4. Japanese-English Computer Terms Dictionary Nichigai Associates 4th version



<!-- Page 5 -->

## Preface

This manual describes the VDP2 (Video Display Processor 2) and how to use it. The VDP2 controls the scroll screen control and the display priority control.

## Manual Notations

Notations within this manual are described below.

### Binary, hexadecimal

Binary notation has a B attached at the end (as in 100B); however, B may be omitted when binary notation is obvious. Hexadecimal notation has an H attached at the end (as in 00H and FFH).

### Units

1 Kbyte is 1024 bytes. 1 Mbit is 1024 Kbits, or 1,048,576 bits.

### MSB, LSB

The structure of byte and word shows the MSB (most significant bit) on the left and LSB (least significant bit) on the right.

### An undefined bit

A bit not defined by the register is shown as a dash. A “0” should be written into an undefined bit of the register. Bits not defined by data of tables defined by VRAM are shown as shaded. As a rule, a 0 should be written, providing that the undefined bit is ignored.

### Byte, word, bit

Bits, as in digits of 0 and 1, are the lowest unit of data. A byte consists of 8 bits. A word consists of 2 bytes, and begins from an even address.

### Boundary

A boundary defines data from an address divisible by a selected value. For example, data for a 20H-byte boundary is defined at addresses beginning from 20H, 40H, and so on. A word is a 2-byte boundary.

### Address

All addresses defined by VDP2 are relative addresses within VDP2. The first address of VDP2 begins from 5E00000H. For example, VRAM is at 000000H address of the relative address, and begins from 5E00000H address of the absolute address. The TV screen mode register is at 180000H address of the relative address, and is set at address 5F80000H of the absolute address.



<!-- Page 6 -->

## Manual Structure

The main items described in each chapter are as follows.

**Table 1.**

Chapter Name Contents

# Chapter 1 VDP2 Functions

VDP2 Functions

# Chapter 2 TV Screen

TV Screen Mode, Normal, Hi-Res, Exclusive Monitor, Interlace Mode, External Signal, H - Counter, V-Counter, Exclusive Hi-Res Setting

# Chapter 3 RAM

VRAM Size, Address Map, VRAM, Color RAM, Register, VRAM Change, VRAM Bank Partition, VRAM Access Method, Color RAM Mode

# Chapter 4 Scroll Screen

Cell, Character Color Count, Transparent Dot, Character Pattern, Pattern Name Table, Special Function Bit, Reverse Function Bit, Page, Plane, Map, Bitmap, Screen-Over Process, Mosaic Process

# Chapter 5 Normal Scroll Screen

Screen Scroll, Scaling, Line Scroll, Vertical Cell Scroll Coordinates

# Chapter 6 Rotation Scroll Screen

Rotation Scroll Increment, Rotation Scroll Screen Display, Rotation Parameter Coefficient Table

# Chapter 7 Line Screen

Line Color Screen, Back Screen

# Chapter 8 Window

Normal Rectangular Window, Normal Line Window, Sprite Window

# Chapter 9 Sprite Data

Sprite Type, Sprite Color Mode, Priority, Color Calculation

# Chapter 10 Dot Color Data

Palette Format, RGB Format, Sprite Dot, Scroll Dot, Special Function Code

# Chapter 11 Priority Function

Priority Number, Line Color Screen Insertion

# Chapter 12 Color Calculation

Color Calculation, Extended Color Calculation, Special Color Calculation, Gradation Calculation

# Chapter 13 Color Offset Function

Color Offset

# Chapter 14 Shadow Function

Normal Shadow, MSB Shadow

# Chapter 15 How To Use VDP2

Operation Flow Chart, How to use RAM, Bit Structure

| Chapter Name | Contents |
| --- | --- |
| Chapter 1 VDP2 Functions | VDP2 Functions |
| Chapter 2 TV Screen | TV Screen Mode, Normal, Hi-Res, Exclusive<br>Monitor, Interlace Mode, External Signal, -H<br>Counter, V-Counter, Exclusive Hi-Res Setting |
| Chapter 3 RAM | VRAM Size, Address Map, VRAM, Color RAM<br>Register, VRAM Change, VRAM Bank<br>Partition, VRAM Access Method, Color RAM<br>Mode |
| Chapter 4 Scroll Screen | Cell, Character Color Count, Transparent Dot,<br>Character Pattern, Pattern Name Table,<br>Special Function Bit, Reverse Function Bit,<br>Page, Plane, Map, Bitmap, Screen-Over<br>Process, Mosaic Process |
| Chapter 5 Normal Scroll Screen | Screen Scroll, Scaling, Line Scroll, Vertical Ce<br>Scroll Coordinates |
| Chapter 6 Rotation Scroll Screen | Rotation Scroll Increment, Rotation Scroll<br>Screen Display, Rotation Parameter<br>Coefficient Table |
| Chapter 7 Line Screen | Line Color Screen, Back Screen |
| Chapter 8 Window | Normal Rectangular Window, Normal Line<br>Window, Sprite Window |
| Chapter 9 Sprite Data | Sprite Type, Sprite Color Mode, Priority, Color<br>Calculation |
| Chapter 10 Dot Color Data | Palette Format, RGB Format, Sprite Dot, Scro<br>Dot, Special Function Code |
| Chapter 11 Priority Function | Priority Number, Line Color Screen Insertion |
| Chapter 12 Color Calculation | Color Calculation, Extended Color Calculation<br>Special Color Calculation, Gradation<br>Calculation |
| Chapter 13 Color Offset Function | Color Offset |
| Chapter 14 Shadow Function | Normal Shadow, MSB Shadow |
| Chapter 15 How To Use VDP2 | Operation Flow Chart, How to use RAM, Bit<br>Structure |



<!-- Page 7 -->

**Table 2.**

| Function | Details |   |   | Chapter |   |
| --- | --- | --- | --- | --- | --- |
| Overview | ~ |   |   | 1 | VDP2 Functions |
|   | TV Screen Configuration, Designate Display<br>Area, Boarder Area |   |   | 2.1 | TV Screen Configuration |
| TV Screen | TV Screen Mode, Normal, Hi-Res, Exclusive<br>Monitor |   |   | 2.2 | TV Screen Mode |
|   | Interlace, Non-interlace, Single-Density Interlace,<br>Double-Density Interlace |   |   | 2.3 | Interlace Mode |
|   | Address Map |   |   | 3.1 | Address Map |
|   |   | Size |   | 3.1 | Address Map |
| RAM | VRAM | Change |   | 3.2 | VRAM Change |
|   |   | Bank Partition |   | 3.3 | VRAM Bank Partition |
|   |   | Access During Display |   | 3.4 | How to Access VRAM<br>During Display |
|   | Color RAM Mode |   |   | 3.5 | Color RAM Mode |
|   |   | Screen Display |   | 4.1 | Screen Display Control |
|   |   |   | Character<br>Color Count<br>Bitmap Color<br>Count | 4.3 | Cell |
|   |   | Color | Palette Format<br>Dot Color Data | 10.1 | Palette Format Dot Color<br>Data |
|   | Normal Scroll<br>Screen |   | RGB Format<br>Dot Color Data | 10.2 | RGB Format Dot Color Dat |
| Scroll Screen | Rotation Scroll<br>Screen |   | Color RAM<br>Mode | 3.5 | Color RAM Mode |
|   |   |   | Cell | 4.3 | Cell |
|   |   |   | Character<br>Pattern | 4.4 | Character Pattern |
|   |   | Cell Format | Pattern Name<br>Table (Page) | 4.6 | Pattern Name Table (Page |
|   |   |   | Plane | 4.7 | Plane |
|   |   |   | Map | 4.8 | Map |
|   |   | Bitmap Format |   | 4.9 | Bitmap |
|   |   | Display Area, Screen-Over |   | 4.10 | Display Area |
|   |   | Mosaic Process |   | 4.11 | Mosaic Process |
|   |   | Screen Scroll Function |   | 5.1 | Screen Scroll Function |
|   |   | Scale Function |   | 5.2 | Scale Function |
|   |   | Line Scroll Function, Vertical<br>Cell Scroll Function |   | 5.3 | Line & Vertical Cell Scroll<br>Function |



<!-- Page 8 -->

**Table 2.**

| Function | Details |   | Chapter |   |
| --- | --- | --- | --- | --- |
|   |   | Coordinates Calculation | 6.1 | Rotation Scroll Coordinates<br>Calculation |
|   | Rotation | Display Control | 6.2 | Rotation Scroll Screen<br>Display Control |
|   | Scroll Screen | Rotation Parameter Control | 6.3<br>8 | Rotation Parameter Control<br>Window |
| Scroll Screen |   | Coefficient Control | 6.4 | Coefficient Table Control |
|   | Line Screen | Line Color Screen | 7.1<br>6.4<br>11.3 | Line Color Screen<br>Coefficient Table Control<br>Line Color Screen Insertion |
|   |   | Back Screen | 7.2 | Back Screen |
| Window | Normal Rectangular Window, Normal Line<br>Window, Sprite Window, Window Effective Area |   | 8<br>9.1 | Window<br>Sprite Data |
|   | Sprite Data, Type, Color Mode |   | 9.1 | Sprite Data |
|   | Priority and Color Calculation |   | 9.2 | Priority and Color<br>Calculation |
| Sprite | Sprite Window |   | 8<br>9.1 | Window<br>Sprite Data |
|   | Dot Color | Palette Format | 10.1 | Palette Format Dot Color<br>Data |
|   | Data | RGB Format | 10.2 | RGB Format Dot Color Dat |
|   |   | Color RAM Mode | 3.5 | Color RAM Mode |
|   | Priority Function |   | 11.1<br>9.2 | Priority Function<br>Priority and Color<br>Calculation |
| Priority | Special Priority Function |   | 11.2<br>10.3 | Special Priority Function<br>Special Function Code |
|   | Line Color Screen Insertion |   | 11.3<br>7.1 | Line Color Screen Insertion<br>Line Color Screen |
|   |   | Color Calculation Function,<br>Extended Color Calculation<br>Function | 12.1<br>7.1 | Color Calculation Function<br>Line Color Screen |
| Image Process | Color | Special Color Calculation<br>Function | 12.3<br>10.3 | Special Color Calculation<br>Function<br>Special Function Code |
|   |   | Gradation Calculation Function | 12.2 | Gradation Calculation<br>Function |
|   |   | Color Calculation Window | 8 | Window |
|   | Color Offset Function |   | 13 | Color Offset Function |
|   | Shadow | Normal Shadow, MSB Shadow | 14<br>9.1 | Shadow Function<br>Sprite Data |



<!-- Page 9 -->

## Table of Contents

Preface............................................................................................................................ i Manual Notation .................................................................................................. i Manual Structure ............................................................................................... iii List of Figures................................................................................................................ xi List of Tables ............................................................................................................... xiv

# Chapter 1 VDP2 Functions ........................................................................................... 1

Introduction.........................................................................................................2

## 1.1 System Configuration ................................................................................... 2

## 1.2 Address Map ................................................................................................3

VRAM .....................................................................................................3 Color RAM ..............................................................................................3 Register ..................................................................................................4

## 1.3 Scroll Function..............................................................................................5

Display Screen .......................................................................................5 Scroll Screen ..........................................................................................6 Line Screen ............................................................................................ 7 Windows .................................................................................................7

## 1.4 Priority Function ........................................................................................... 8

Priority Function......................................................................................8 Color Calculation Function .....................................................................8 Color Offset Function..............................................................................8 Shadow Function....................................................................................9

# Chapter 2 TV Screen .................................................................................................. 11

## 2.1 TV Screen Mode ........................................................................................12

Special High Resolution Graphic Mode................................................13

## 2.2 Interlace Mode............................................................................................14

## 2.3 TV Screen Structure ...................................................................................15

## 2.4 TV Screen Mode Register ..........................................................................16

## 2.5 External Signals and Scan Conditions .......................................................19

External Signal Enable Register...........................................................19 Screen Status Register ........................................................................21 H Counter Register...............................................................................23 V Counter Register ...............................................................................24

# Chapter 3 RAM ...........................................................................................................25

Introduction.......................................................................................................26

## 3.1 Address Map ..............................................................................................26

VRAM Size Register .............................................................................28

## 3.2 VRAM Bank Partitioning.............................................................................29

RAM Control Register ..........................................................................29

## 3.3 Accessing VRAM During Display Interval ..................................................31

VRAM Access During Display Interval ................................................. 31 Image Data Access ..............................................................................32 Vertical Cell Scroll Table Data Access ..................................................35 Read/Write Access by the CPU............................................................35 VRAM Cycle Pattern Selection Process...............................................37



<!-- Page 10 -->

VRAM Cycle Pattern Register .............................................................. 39

## 3.4 Color RAM Mode ........................................................................................43

RAM Control Register .......................................................................... 45

# Chapter 4 Scroll Screen ..............................................................................................47

## 4.1 Screen Display Control ..............................................................................48

Screen Display Enable Register ...........................................................48

## 4.2 Scroll Screen Structure ..............................................................................50

Cell Format.......................................................................................................50 Bit Map Format .................................................................................................52

## 4.3 Cell .............................................................................................................53

Character Color Number ...................................................................... 53 Cell Data Configuration ........................................................................ 53 Transparent Dots .................................................................................. 57 RGB Format Dot Data .......................................................................... 58

## 4.4 Character Patterns .....................................................................................59

Character Size and Cell Arrangement.................................................. 59

## 4.5 Character Control Register ........................................................................ 60

## 4.6 Pattern Name Table (Page)........................................................................ 64

Pattern Name Table Data Configuration............................................... 64 Pattern Name Data ...............................................................................69 Character Number ................................................................................ 74 Palette Number.....................................................................................74 Special Function Bit ..............................................................................74 Reverse (Flip) Function Bit ...................................................................75 Pattern Name Control Register ............................................................ 76

## 4.7 Planes ........................................................................................................79

Plane Size ............................................................................................ 79 Plane Size Register ..............................................................................80

## 4.8 Maps...........................................................................................................82

Map Selection Register ........................................................................ 82 Map Size...............................................................................................84 Map Offset Register..............................................................................85 Normal Scroll Screen Map Register ..................................................... 87 Rotation Scroll Surface Map Register .................................................. 89

## 4.9 Bit Maps .....................................................................................................93

Bit Map Size .........................................................................................93 Bit Map Color Number .......................................................................... 93 Bit Map Pattern .....................................................................................95 Bit Map Palatte Number ..................................................................... 111 Special Function Bit ............................................................................ 111 Bit Map Palatte Number Register ....................................................... 112

## 4.10 Display Area ........................................................................................... 114

Display Area ....................................................................................... 114 Screen-Over Process ......................................................................... 115 Display-Over Pattern Name ............................................................... 115 Screen-Over Pattern Name Register ................................................. 116

## 4.11 Mosaic Process ...................................................................................... 117

Mosaic Control Register ..................................................................... 118



<!-- Page 11 -->

# Chapter 5 Normal Scroll Screen ...............................................................................121

Introduction.....................................................................................................122

## 5.1 Screen Scroll Function .............................................................................122

Screen Scroll Value Register..............................................................123

## 5.2 Expansion/Contraction Function ..............................................................126

Coordinate Increment Register ..........................................................127 Reduction Enable Register.................................................................129

## 5.3 Line and Vertical Cell Scroll Function.......................................................131

Line Scroll Function ............................................................................131 Vertical Cell Scroll Function ................................................................134 Line and Vertical Cell Scroll Control Register..................................... 137 Line Scroll Table Address Register.....................................................140 Vertical Cell Scroll Table Address Register ........................................ 141

# Chapter 6 Rotation Scroll Screen .............................................................................143

Introduction.....................................................................................................144

## 6.1 Rotation Scroll Coordinate Operation ......................................................144

## 6.2 Rotation Scroll Screen Display Control ....................................................148

RAM Control Register ........................................................................148

## 6.3 Rotation Parameter Control .....................................................................151

Data Configuration of the Rotation Parameter Table..........................153 Rotation Parameter Table...................................................................155 Rotation Parameter Read Control Register........................................ 157 Rotation Parameter Table Address Register ...................................... 158 Rotation Read Out of the Frame Buffer ..............................................159 Rotation Parameter Change...............................................................160 Rotation Parameter Mode Register ....................................................162

## 6.4 Coefficient Table Control ..........................................................................163

Line Color Screen Data ......................................................................164 Bit Configuration of Coefficient Table Data.........................................165 Coefficient Table Lead Address ..........................................................165 Most Significant Bit of Coefficient Data ..............................................166 RAM Control Register ........................................................................167 Coefficient Table Control Register ......................................................168 Coefficient Table Address Offset Register ..........................................170

# Chapter 7 Line Screen ..............................................................................................171

Introduction.....................................................................................................172

## 7.1 Line Color Screen ....................................................................................172

Line Color Screen Table Address Register.........................................174

## 7.2 Back Screen .............................................................................................175

Back Screen Table Address Register ................................................. 176

# Chapter 8 Windows...................................................................................................179

## 8.1 Window Area ............................................................................................180

Normal Rectangular Window..............................................................180 Window Position Register ..................................................................181 Normal Line Window ..........................................................................184 Line Window Table Address Register ................................................. 186 Sprite Window ....................................................................................187



<!-- Page 12 -->

Sprite Control Register .......................................................................188 Window’s Active Area for the Screen..................................................189

## 8.2 Window Process.......................................................................................190

Window Control Register....................................................................193

# Chapter 9 Sprite Data ...............................................................................................199

## 9.1 Sprite Data ...............................................................................................200

Sprite Types........................................................................................200 Sprite Color Mode...............................................................................203

## 9.2 Priority and Color Calculation ...................................................................204

Priority Number Selection ...................................................................204 Color Calculation Enable Conditions ..................................................205 Color Calculation Ratio Selection .......................................................206 Sprite Control Register .......................................................................207 Priority Number Register ....................................................................209 Color Calculation Ratio Registers ......................................................210

# Chapter 10 Pixels......................................................................................................213

Introduction.....................................................................................................214

## 10.1 Palette Format Pixels .............................................................................214

Sprite Dot Pixels .................................................................................214 Scroll Dot Pixels .................................................................................216 Color RAM Address Offset Register ...................................................217

## 10.2 RGB Format Pixels ................................................................................218

Sprite Pixels........................................................................................218 Scroll Pixels ........................................................................................218

## 10.3 Special Function Code ...........................................................................220

Special Function Code Select Register ..............................................221 Special Function Code Register .........................................................222

# Chapter 11 Priority Function......................................................................................223

Introduction.....................................................................................................224

## 11.1 Priority Function......................................................................................224

Priority Number...................................................................................224 Priority Number Register ....................................................................225

## 11.2 Special Priority Function.........................................................................227

Special Priority Mode Register ...........................................................229

## 11.3 Insertion of Line Color Screen ...............................................................230

Line Color Screen Enable Register ....................................................231

# Chapter 12 Color Calculations ...................................................................................233

Introduction.....................................................................................................234

## 12.1 Color Calculation Function .....................................................................234

Normal Color Calculation ...................................................................234 Extended Color Calculation Function ................................................. 236

## 12.2 Gradation Calculation Function ..............................................................238

Color Calculaton Control Register ......................................................240 Color Calculation Ratio Register ........................................................243

## 12.3 Special Color Calculation Function ........................................................245

Special Color Calculation Mode Register ...........................................247



<!-- Page 13 -->

# Chapter 13 Color Offset Function .............................................................................249

Introduction.....................................................................................................250

## 13.1 Color Offset Selection ............................................................................250

Color Offset Enable Register ..............................................................251 Color Offset Select Register ...............................................................252 Color Offset Register ..........................................................................253

# Chapter 14 Shadow Function ...................................................................................255

Introduction.....................................................................................................256

## 14.1 Shadow Process ....................................................................................256

Normal Shadow ..................................................................................256 MSB Shadow ......................................................................................258 Shadow Control Register ...................................................................259

# Chapter 15 How to Use VDP2 ..................................................................................261

## 15.1 Operation Flow .......................................................................................262

## 15.2 How to Use RAM ....................................................................................264

## 15.3 Bit Configuration Map.............................................................................267

# Chapter 16 Quick Reference ....................................................................................295

## 16.1 Register Map ..........................................................................................296

## 16.2 Register Bit List ......................................................................................315

## 16.3 Register Bit Functions ............................................................................328

## 16.4 Table List ................................................................................................389



<!-- Page 14 -->

## Table of Figures

# Chapter 1 VDP2 Functions

Figure 1.1 System Configuration........................................................................ 2 Figure 1.2 Address Map .....................................................................................3

# Chapter 2 TV Screen

Figure 2.1 Display Method by Interlace Setting ............................................... 14 Figure 2.2 TV Screen Structure........................................................................ 15

# Chapter 3 RAM

Figure 3.1 Different Capacities of VRAM Address Map ................................... 27 Figure 3.2 VRAM Cycle Pattern Register......................................................... 32 Figure 3.3 Access Selection Limits of Pattern Name Table Data ..................... 33

**Figure 3.4 Example of Character Pattern Data Read Access**

Selection .........................................................................................34 Figure 3.5 Access Select Limits of Vertical Cell Scroll Table Data ................... 35

**Figure 3.6 CPU Read/Write Access Selection when**

VRAM is not Divided into Two Bank ............................................... 36

**Figure 3.7 CPU Read/Write Access Selection when**

VRAM is Divided into Two Banks ................................................... 37 Figure 3.8 VRAM Cycle Pattern Selection ....................................................... 39 Figure 3.9 Color Data Configuration on Color RAM......................................... 44 Figure 3.10 Color Data of the Color RAM ........................................................ 45

# Chapter 4 Scroll Screen

Figure 4.1 Scroll Screen Configuration of Cell Format..................................... 50

**Figure 4.2 Scroll Screen Configuration of Cell Format and**

Corresponding Data Settings ......................................................... 51 Figure 4.3 Scroll Screen Configuration of Bit Map Format............................... 52 Figure 4.4 Relationship of bit map format scroll screen and data settings....... 52 Figure 4.5 Configuration of Cells by Character Color Count ............................ 54 Figure 4.6 RGB Format Dot Data .....................................................................58 Figure 4.7 Cell Arrangement by Character Size............................................... 59 Figure 4.8 Data Configuration of Pattern Name Tables ................................... 65 Figure 4.9 Bit Configuration when Pattern Name Data is 2 Word.................... 69 Figure 4.10 Configuration when Pattern Name Data is 1 Word ....................... 71 Figure 4.11 Dot Color Data by Character Number of Colors............................ 74 Figure 4.12 Reverse Display of Character Patterns......................................... 75 Figure 4.13 Arrangement of Pattern Name Table by Plane Size...................... 79 Figure 4.14 Map Selection Register ................................................................. 82 Figure 4.15 Map Size .......................................................................................84 Figure 4.16 Plane Arrangement of Map by Reduction Settings ....................... 85 Figure 4.17 Bit Map Pattern Configuration ....................................................... 96 Figure 4.18 Dot Color Data by Bit Map Number of Colors ............................. 111 Figure 4.19 Mosaic Pattern ............................................................................ 117



<!-- Page 15 -->

# Chapter 5 Normal Scroll Screen

Figure 5.1 Screen Scroll Value Bit Configuration ...........................................122 Figure 5.2 Configuration of Coordinate Increment Register........................... 126 Figure 5.3 Line Scroll Function ......................................................................131 Figure 5.4 Bit Configuration of Line Scroll Table Data ...................................132 Figure 5.5 Line Scroll Table ............................................................................133 Figure 5.6 Vertical Cell Scroll Function ..........................................................134 Figure 5.7 Data Configuration on Vertical Cell Scroll Table ........................... 135 Figure 5.8 Vertical Cell Scroll Table................................................................136

# Chapter 6 Rotation Scroll Screen

Figure 6.1 Rotation Scroll Screen Display Method ........................................ 145 Figure 6.2 Rotation Parameter data Configuration ........................................ 153 Figure 6.3 Rotation Parameter Table .............................................................156 Figure 6.4 How to Store to the Rotation Parameter Table VRAM .................. 157 Figure 6.5 Rotation Parameter Change ......................................................... 161 Figure 6.6 Line Color Screen Data Using Coefficient Data ............................ 164 Figure 6.7 Bit Configuration of Coefficient Table Data ...................................165

# Chapter 7 Line Screen

Figure 7.1 Line Screen ...................................................................................172 Figure 7.2 Configuration of Line Color Screen Table ..................................... 173 Figure 7.3 Bit Configuration of Line Color Screen Table Data ....................... 173 Figure 7.4 Configuration of Back Screen Table ..............................................175 Figure 7.5 Bit Configuration of Back Screen Table Data ................................176

# Chapter 8 Windows

Figure 8.1 Normal Rectangle Window ...........................................................180 Figure 8.2 Normal Line Window .....................................................................184 Figure 8.3 Bit Configuration of Normal Line Window Table Data ................... 184 Figure 8.4 Configuration of Normal Line Window Table .................................185 Figure 8.5 Sprite Window ...............................................................................187 Figure 8.6 Active Area of Windows ................................................................189 Figure 8.7 Window Process ...........................................................................191

# Chapter 9 Sprite Data

Figure 9.1 Sprite Types ..................................................................................201

# Chapter 10 Dot Color Data

Figure 10.1 Palette Format Sprite Dot Color Data .........................................215 Figure 10.2 Sprite Color RAM Address ..........................................................215 Figure 10.3 Palette Format Scroll Dot Color Data..........................................216 Figure 10.4 Scroll Color RAM Address ..........................................................216 Figure 10.5 RGB Format Sprite Dot Color Data.............................................218 Figure 10.6 RGB Format Scroll Dot Color Data .............................................219 Figure 10.7 Dot Color Code Corresponding to Special Function Code ......... 220



<!-- Page 16 -->

# Chapter 11 Priority Function

Figure 11.1 Priority Function ..........................................................................224 Figure 11.2 Line Color Screen Insertion .........................................................230

# Chapter 12 Color Operations

Figure 12.1 Color Calculation Function .........................................................234 Figure 12.2 Color Calculation Ratio Mode .....................................................236 Figure 12.3 Expand Color Operation Function...............................................237 Figure 12.4 Gradation Calculation Function...................................................239

# Chapter 13 Color Offset Function

Figure 13.1 Color Offset Data ........................................................................250

# Chapter 14 Window Function

Figure 14.1 Shadow Function ........................................................................256 Figure 14.2 Sprite Data Write of a Normal Shadow ....................................... 257 Figure 14.3 Sprite Data of a Normal Shadow ................................................ 257 Figure 14.4 Sprite Shadow and Transparent Shadow ...................................258 Figure 14.5 Sprite Data of MSB Shadow .......................................................259



<!-- Page 17 -->

## List of Tables

# Chapter 1 VDP2 Functions

**Table 1.1 TV Screen Mode................................................................................. 5**

**Table 1.2 Scroll Screen .....................................................................................5**

**Table 1.3 Windows .............................................................................................6**

**Table 1.4 Scroll Screen Function ....................................................................... 6**

# Chapter 2 TV Screen

**Table 2.1 TV Screen Mode...............................................................................12**

**Table 2.2 Register for Setting the External Screen ..........................................21**

**Table 2.3 H Counter Register Bit Content ........................................................24**

**Table 2.4 V Counter Register Bit Content ........................................................24**

# Chapter 3 RAM

**Table 3.1 Data Defined in VRAM .....................................................................26**

**Table 3.2 Access Numbers of Required Pattern Name Table Data**

during 1 Cycle ..................................................................................33

**Table 3.3 Character Pattern Data (Bit Map Pattern Data)**

Read Access Number ......................................................................34

**Table 3.4 Character Pattern Data Read Access Selection Limits .................... 34**

**Table 3.5 Access Command.............................................................................40**

# Chapter 4 Scroll Screen

**Table 4.1 Character Color Count and Dot Data Size ....................................... 53**

**Table 4.2 Cell Data Configuration ....................................................................53**

**Table 4.3 Transparent Dot Data Values ...........................................................58**

**Table 4.4 Pattern Name Table Capacity Page Boundary of One Page............ 64**

**Table 4.5 Character Number Auxiliary Mode....................................................69**

**Table 4.6 Bit Configuration when Pattern Name Table is 1 Word .................... 70**

**Table 4.7 Reverse Function Bit ........................................................................75**

**Table 4.8 Address Value of Map Designation Register by Setting ................... 83**

**Table 4.9 Bit Map Size .....................................................................................93**

**Table 4.10 Bit Map Color Count .......................................................................94**

**Table 4.11 Bit Map Pattern Capacity per Surface ...........................................95**

**Table 4.12 Normal Scroll Screen Display Area .............................................. 114**

**Table 4.13 Rotation Scroll Screen Display Area ............................................. 114**

# Chapter 5 Normal Scroll Screen

**Table 5.1 Horizontal Coordinate Increment and Reduction Setting ...............128**

**Table 5.2 Display Screen Limits by Setting of Reduction Enable Bit ............. 130**

# Chapter 6 Rotation Scroll Screen

**Table 6.1 Rotation Scroll Screen ...................................................................144**

**Table 6.2 Rotation Parameters.......................................................................151**

**Table 6.3 Lease significant bit of Coefficient Parameter**

Data Showing the Address Value Separate from Coefficient Parameter Data Size .....................................................166

**Table 6.4 Image Processing using RGB0 Coefficient Data MGB Value......... 167**



<!-- Page 18 -->

# Chapter 8 Windows

**Table 8.1 Bit Content of Window Position Register for Horizontal**

Coordinates. ....................................................................................182

**Table 8.2 Bit Content of Window Position Register for Vertical**

Coordinates. ....................................................................................183

# Chapter 9 Sprite Data

**Table 9.1 Shared Bits .....................................................................................200**

**Table 9.2 Selection of Sprite Priority Number Register ..................................204**

**Table 9.3 Selection of Sprite Color Calculation Ratio Register ...................... 206**

# Chapter 11 Priority Function

**Table 11.1 Priority when the Priority Numbers are Equal ............................... 225**

**Table 11.2 Special Priority Function by Mode ................................................ 228**

# Chapter 12 Color Operation

**Table 12.1 Color Operation Function when in High Resolution**

Mode or Special Monitor Mode ....................................................236

**Table 12.2 Expanded Color Calculation Ratio................................................ 238**

**Table 12.3 Special Color Calculation Mode ...................................................246**

# Chapter 15 Method of Using VDP2

**Table 15.1 Register Connected with Data Defined in VRAM .........................265**



<!-- Page 19 -->

# Chapter 1 VDP2 Functions
