Signals controlled through EXLTEN external signal enable register show the latched V counter values. The bit configuration of this register changes according to the settings of the TV screen mode, as shown in

**Table 2.4. The V counter values for**

single density interlace of the normal and high resolution modes show V counter values in their various even and odd fields. The V counter values for double density interlace of normal and high resolution modes show the odd fields when 0 and even field when the least significant bit of VCT0 is 1. VCT1~VCT9 show the V counter values in their respective fields.

**Table 2.4 V counter register bit content**

| Graphic<br>Mode | HCT9 | HCT8 | HCT7 | HCT6 | HCT5 | HCT4 | HCT3 | HCT2 | HCT1 | HCT0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Normal | H8 | H7 | H6 | H5 | H4 | H3 | H2 | H1 | H0 | Invalid |
| Hi-Res | H9 | H8 | H7 | H6 | H5 | H4 | H3 | H2 | H1 | H0 |
| Exclusive<br>Normal | Invalid | H8 | H7 | H6 | H5 | H4 | H3 | H2 | H1 | H0 |
| Exclusive<br>Hi-Res | Invalid | H9 | H8 | H7 | H6 | H5 | H4 | H3 | H2 | H1 |

| VCNT | ~ | ~ | ~ | ~ | ~ | ~ | VCT9 | VCT8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18000AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCT7 | VCT6 | VCT5 | VCT4 | VCT3 | VCT2 | VCT1 | VCT0 |

| TV Screen<br>(Interlace) Mode | VCT9 | VCT8 | VCT7 | VCT6 | VCT5 | VCT4 | VCT3 | VCT2 | VCT1 | VCT0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Normal Hi-Res<br>(Non-Interlace,<br>Single-Density<br>Interlace) | V8 | V7 | V6 | V5 | V4 | V3 | V2 | V1 | V0 | Invalid |
| Normal Hi-Res<br>(Double-Density<br>Interlace) | V8 | V7 | V6 | V5 | V4 | V3 | V2 | V1 | V0 | 0: Odd fields<br>1: Even fields |
| Exclusive<br>Monitor | V9 | V8 | V7 | V6 | V5 | V4 | V3 | V2 | V1 | V0 |



<!-- Page 43 -->

# Chapter 3 RAM

Introduction...................................................................... 26

## 3.1 Address Map ............................................................. 26

VRAM Size Register............................................... 28

## 3.2 VRAM Bank Partitioning............................................ 29

RAM Control Register ............................................ 29

## 3.3 Accessing VRAM During Display Interval ................. 31

VRAM Access During Display Interval ................... 31 Image Data Access ................................................ 32 Vertical Cell Scroll Table Data Access .................... 35 Read/Write Access by the CPU.............................. 35 VRAM Cycle Pattern Selection Process................. 37 VRAM Cycle Pattern Register ................................ 39

## 3.4 Color RAM Mode ....................................................... 43

RAM Control Register ............................................ 45 ST-58-R2



<!-- Page 44 -->

## Introduction

VDP2 is connected to special VRAM for defining pattern name tables, character patterns, and so on. VRAM has two divisions called VRAM-A and VRAM-B, each having equal capacity. VRAM-A and VRAM-B can each be divided into two banks, called bank 0 and bank 1. Banks divided with four equal capacities are called VRAM-A0, VRAM-A1, VRAM-B0, and VRAM-B1. VRAM data is defined in table 3.1. Also contained is color RAM for defining the color data of scroll screens and sprites.

**Table 3.1 Data defined in VRAM**

## 3.1 Address Map

VDP2 can be applied to two types of VRAM: 4 Mbit and 8 Mbit. Programs created for systems using a 4 Mbit VRAM can also be used in systems using 8 Mbit VRAM, but programs created for systems using an 8 Mbit VRAM cannot be used in systems using 4 Mbit VRAM.

| Data that must be defined when<br>display format is cell (format) | Data that must be defined when<br>display format is bitmap (format) | Data defined as necessary |
| --- | --- | --- |
| Pattern name table data<br>Character pattern data | Bitmap pattern data | dLine scroll table data<br>Vertical cell scroll table data<br>Rotation parameter table data<br>Coefficient table data<br>Line color screen table data<br>Back screen table data<br>Line window table data |



<!-- Page 45 -->

The address map changes according to VRAM capacity being used in the system, as shown in Figure 3.1.

**Figure 3.1 Different Capacities of VRAM Address Map**

| VRAM-A0 |
| --- |
| VRAM-A1 |
| VRAM-B0 |
| VRAM-B1 |

| VRAM-A0 |
| --- |
| VRAM-A1 |
| VRAM-B0 |
| VRAM-B1 |



<!-- Page 46 -->

### VRAM Size Register

The VRAM size register indicates the VRAM capacity to be used in the system. It is a read/write 16-bit register and is at the 180006H address. Bits 3 to 0 are exclusively for read only. Because the value of bit15 (VRAMSZ) is cleared to 0 after the power is turned on or reset, it must be reset. Indicates the VRAM capacity used in the system. This bit must be set before data is written to VRAM. Shows the VDP2 version number; the first is 0.

| VRSIZE | VRAMSZ | ~ | ~ | ~ | ~ | ~ | ~ | ~ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180006H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | VER3 | VER2 | VER1 | VER0 |

| VRAMSZ | VRAM Size |
| --- | --- |
| 0 | 4 Mbit |
| 1 | 8 Mbit |



<!-- Page 47 -->

## 3.2 VRAM Bank Partitioning

VDP2 can access VRAM-A0, VRAM-A1, VRAM-B0, and VRAM-B1 at the same time when both VRAM-A and VRAM-B are divided in half. As a result, more image data can be obtained at once, a higher number of scroll screens can be displayed simultaneously, and a screen with multiple colors can be displayed. However, there are limitations when selecting of VRAM read/write access through the CPU during the display. Therefore, don’t partition the VRAM into two areas when accessing (read/ write) through the CPU during the display. Normally, accessing can be efficiently done if divided into two areas.

### RAM Control Register

RAM control register selects VRAM bank partitions with the objective of using the rotation scroll screen VRAM as well as the color RAM mode. It is a read/write 16-bit register and is at the 18000EH address. Also, because the value is cleared to 0, it must be set after the power is turned on or reset. See “6.4 Coefficient Table Control.” See “3.4 Color RAM Mode.” Set the Color RAM mode to mode 1 when the CRKTE bit is 1. At that time, color data can no longer be stored because the second half of the color RAM (100800H ~ 100FFFH) is used for the coefficient table data. Controls VRAM bank partitions.

| RAMCTL | CRKTE | ~ | CRMD1 | CRMD0 | ~ | ~ | VRBMD | VRAMD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18000EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | RDBSB11 | RDBSB10 | RDBSB01 | RDBSB00 | RDBSA11 | RDBSA10 | RDBSA01 | RDBSA00 |

| VRAMD | 18000EH | Bit 8 | For VRAM-A |
| --- | --- | --- | --- |
| VRBMD | 18000EH | Bit 9 | For VRAM-B |



<!-- Page 48 -->

> Note: Enter A or B into bit name for x.

See “6.2 Rotation Scroll Screen Display Control”. When the CRKTE bit is 1, do not designate to allow the 4 banks of VRAM to be used as RAM for the coefficient table data.

| VRxMD | Process |
| --- | --- |
| 0 | Do not partition in 2 banks |
| 1 | Partition in 2 banks |



<!-- Page 49 -->

## 3.3 Accessing VRAM During Display Interval

### VRAM Access During Display Interval

VDP2 synchronizes scroll screen data with the TV scan and displays them while reading from VRAM. VRAM access during display repeats the cycle as four or eight access operating units (1 cycle). When the TV screen mode is the Normal mode, 1 cycle accesses eight times. Also, 1 cycle is accessed four times when in the highresolution or special monitor mode. Below are the ten types of VRAM accesses performed in one cycle: (1) Normal scroll screen pattern name data read access. (2) Normal scroll screen character pattern data read access or bit map pattern data read access. (3) NBG0, NBG1 vertical cell scroll table data read access. (4) Read/Write access through the CPU. (5) Does not access. (6) RGB0 pattern name data read access. (7) RGB0 character pattern data read access or bit map pattern data read access. (8) RGB0 coefficient table data read access. (9) RGB1 pattern name data read access. (10) RGB1 character pattern data read access. The timing during the 1 cycle when the above (1) through (5) are performed must be selected for each bank of VRAM-A0, VRAM-A1, VRAM-B0, and VRAM-B1. This selection is performed by writing the values of 4 bits, called access commands, to the VRAM cycle pattern register. Access Commands correspond to the several types of VRAM access. Each VRAM access in the above items (6) through (8) occupies a full one cycle, therefore, for one bank only one type may be selected. This is accomplished by writing the value corresponding to each VRAM access type to the RAM control register rotation data bank select bit. The setting of the bank VRAM cycle pattern register, which select (6) through (8) VRAM access, will become invalid. Each VRAM access in the above items (9) and (10) occupies a full one cycle. (9) is fixed in VRAM-B1 and (10) in VRAM-B0. While items (9) and (10) are selected automatically with the display of RGB1, the setting of the VRAM-B0 and VRAM-B1 VRAM cycle pattern registers will become invalid. ST-58-R2



<!-- Page 50 -->

The VRAM cycle pattern register has registers that correspond to the following banks: VRAM-A0, VRAM-A1, VRAM-B0, VRAM-B1. When the VRAM is not divided into two partitions, the VRAM-A0 register is used for VRAM-A, and the VRAM-B0 register is used for VRAM-B. Registers for VRAM-A1 and VRAM-B1 are not used. Registers that correspond to the various banks are separated into eight (T0 to T7) access timings. Access is performed in order, beginning from VRAM access, showing the access command selected in the T0 bit. T0 to T7 are in effect when the TV screen is in Normal mode, but only T0 to T3 are in effect for the high-resolution or special monitor mode; T4 to T7 are ignored. Figure 3.2 shows the VRAM cycle pattern register used during 1 cycle.

**Figure 3.2 VRAM Cycle Pattern Register**

Be sure to set “do not access” for the remaining access time after selecting the VRAM access required in the display. If the VRAM access address selected in the VRAM cycle pattern register is not the address in the selected bank, access won’t be done and the correct screen will not be displayed.

### Image Data Access

The required image data must be read from VRAM for normal scroll screens (NBG0 to NBG3) to be displayed. When the display format is the cell format, the required image data is pattern name data and character pattern data. When in a bit map format, the necessary image data is bit map pattern data . The VRAM access number for obtaining this image data during 1 cycle is decided by the conditions.



<!-- Page 51 -->

Pattern name data read access during 1 cycle must be set to a maximum of two banks, one being either VRAM-A0 or VRAM-B0, and the other being VRAM-A1 or VRAM-B1. When the VRAM is not divided into two partitions, the VRAM-A0 register is used as VRAM-A, and the VRAM-B0 register is used as VRAM-B; therefore, one or the other must be set. Any access timing may be selected if within the register’s effective range in all TV screen modes. The access number must be the same as the number as determined by conditions, but the related timing does not need to be selected. The pattern name data read access number is shown in

**Table 3.2. The pattern name**

data read access selection limits are shown in Figure 3.3.

**Table 3.2 Access numbers of required pattern name table data during 1 cycle**

**Figure 3.3 Access selection limits of pattern name table data**

As a rule, the character pattern data read access during 1 cycle can select any timing from four banks. However, the timing that can be selected through pattern name data access timing is limited. Only when the pattern name data access of NBG0 and NBG1 are selected in T0 can select various character pattern data read accesses through the timings of any of the four banks be selected with are no limits. The access number must be selected so that it is the same as the number as determined by the conditions. The related timing does not need to be selected. Character pattern data read access numbers are shown in

**Table 3.3. Character pattern data read access**

selection limits are shown in

**Table 3.4.**

| Item | NBG0~NBG3 |   |   |
| --- | --- | --- | --- |
| Reduction setting | x1 | x1/2 | x1/4 |
| Number of VRAM<br>accesses required<br>during 1 cycle | 1 | 2 | 4 |

|   |   |   |   | n |
| --- | --- | --- | --- | --- |
|   |   |   | o |   |



<!-- Page 52 -->

**Table 3.3 Character pattern data (bit map pattern data) read access number**

**Table 3.4 Character pattern data read access selection limits**

When the reduction setting is one, all of the character pattern data read access must observe selection limits if the character pattern data read access is to be two or greater. If the reduction setting is 1/2 or 1/4, the required access number when the reduction setting is 1 (one time for 16 colors and two times for 256 colors) must observe selection limits through one time pattern name data read access. Figure 3.4 shows character pattern data read access selection limits when the pattern name data read access is selected in T1 and T3, with 256 colors and 1/2 reduction.

> Note: Charact er Pattern Data Read Access must be select ed twice in each selectable

**Figure 3.4 Example of character pattern data read access selection**

| Item | NBG0~NBG3 |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Character<br>Color Count | 16 |   |   | 256 |   | 2048 | 32,768 | 16,770,000 |
| Reduction<br>setting | 1 | 1/2 | 1/4 | 1 | 1/2 | 1 | 1 | 1 |
| Number of<br>VRAM<br>accesses<br>required<br>during 1 cycle | 1 | 2 | 4 | 2 | 4 | 4 | 4 | 8 |

| Item | TV Screen | Pattern Name Table Data Access Timing |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   | Mode | T0 | T1 | T2 | T3 | T4 | T5 | T6 | T7 |
| Timing that<br>can select | Normal | T0~T2,<br>T4~T7 | T0~T3,<br>T5~T7 | T0~T3,<br>T6~T7 | T0~T3,<br>T7 | T0~T3 | T1~T3 | T2,<br>T3 | T3 |
| character<br>pattern data<br>access | Hi-Res,<br>exclusive<br>monitor | T0~T2 | T1~T3 | T0, T2,<br>T3 | T0, T1,<br>T3 | - | - | - | - |

|   |   |   |   |   |   | C |
| --- | --- | --- | --- | --- | --- | --- |



<!-- Page 53 -->

### Vertical Cell Scroll Table Data Access

### When using the vertical cell scroll function in NBG0 and NBG1 {Translator’s Note:

The original document reads NB1, we believe this is an error.}, vertical cell scroll table data must also be read. Vertical cell scroll table data read access must be performed for one surface during 1 cycle. Vertical cell scroll table data read access for NBG0 must be selected in T0 or T1 timing. NBG1 vertical cell scroll table data read access must be selected within the timing of T0 to T2. Also, access for NBG0 and NBG1 must be by the same bank and NBG0 access must be selected first. When specifying the same vertical cell scroll table data read access against multiple banks, make sure to specify the same access timing.

**Figure 3.5 shows access selection limits of vertical cell scroll table data.**

> Note: For NB G0 and NBG1 access timing, NBG0 access must be first selected in

**Figure 3.5 Access select limits of vertical cell scroll table data**

### Read/Write Access by the CPU

When performing read/write access to the VRAM by the CPU during the screen display interval, the timing must be set to the VRAM cycle pattern register. VDP2 waits for the selected timing in the CPU read/write access when VRAM access is requested by the CPU, and approves access only in that timing. When read/write access is not requested by the CPU, nothing will be performed for VRAM, even for selected timings. Moreover, during read access through the CPU, the wait cycle will enter the CPU until it is able to read. The write access wait cycle will not be entered if the two word write access is at least two times.

|   |   |   |   |   |   |   | i |
| --- | --- | --- | --- | --- | --- | --- | --- |
|   |   |   |   |   |   | f |   |
|   |   |   |   |   |   | n |   |



<!-- Page 54 -->

VRAM access by the CPU can be selected only in units of access to VRAM-A or VRAM-B, and can not be selected in bank units. When selecting VRAM access by the CPU for the VRAM without two partitions, you should select the CPU read/write access command in the VRAM cycle pattern register of the timing performing the access. Selecting an access command that does not access in place of the CPU read/write access command is the same as before. In the screen display enable register, when the access command (pattern name data read, character pattern data read, or bit map pattern data read) used for a screen not set to be displayed is also set, it becomes the CPU read/write access. See “4.1 Screen Display Control” about the screen display enable register. When selecting an access command for not to access or CPU read/write with respect to every access timing of the VRAM that is not partitioned into two areas, the CPU access will then be always allowed during display period. This allows to use one of the VRAMs as an auxiliary work RAM. In addition, by switching the VRAM used in the image display as a frame buffer, the image can be displayed while being rewritten at a high speed.

**Figure 3.6 illustrates the VRAM cycle pattern register selection if CPU read/write**

access is being performed in T2 and T4 when VRAM-A is not partitioned.

**Figure 3.6 CPU Read/Write Access Selection when VRAM is not Divided into Two Bank**

When setting the CPU read/write access for the VRAM that is partitioned into two areas, the CPU read/write access command must be set in the VRAM cycle pattern register of both bank 0 and bank 1 of the timing performing access. Further, in the registers of both bank 0 and bank 1 of the timing before the set CPU read/write access command timing, the access command that won’t access must be selected. However, when selecting CPU read/write access in linked timing, only the timing before the lead of the linked access timing may be selected.

**Figure 3.7 illustrates the selection of the VRAM cycle pattern register when perform-**

ing CPU read/write access linked to T4 and T5 while VRAM-B is divided into two partitions.

| CPU Read/<br>Write | Other<br>Access<br>Commands | No<br>Access | Other<br>Access<br>Commands |
| --- | --- | --- | --- |



<!-- Page 55 -->

**Figure 3.7 CPU Read/Write Access Selection when VRAM is Divided into Two Banks**

### VRAM Cycle Pattern Selection Process

Selection process to the VRAM cycle pattern register is listed below. 1. Decide the TV screen mode. 2. Decide whether to partition the VRAM into two segments. 3. Decide the number of character colors of the scroll screen being displayed and the reduction setting. Also, decide whether to use the vertical cell scroll function. 4. Decide the VRAM bank that will store the required image data (patternname data, character pattern data, bit map pattern data) for all scroll screens. Decide the VRAM bank for storing vertical cell scroll table data when the vertical cell scroll function is used. 5. Decide whether to read/write access through the CPU. 6. To observe selection limits of various access timings, select access command in the VRAM cycle pattern register.

| NoA ccess | CPU Read/<br>Write | CPU Read/<br>Write | Other<br>Access<br>Commands |
| --- | --- | --- | --- |
| NoA ccess | CPU Read/<br>Wrtie | CPU Read/<br>Wrtie | Other<br>Access<br>Commands |



<!-- Page 56 -->

An example of VRAM cycle pattern register selection is shown in Figure 3.8.

**Figure 3.8 VRAM Cycle Pattern Selection**

| Screen Name | Character Colors | Reduction<br>Setting | Vertical Cell Scroll<br>Function |
| --- | --- | --- | --- |
| NBG0 | 256 Colors | x1/2 | Do not use |
| NBG1 | 256 Colors | x1 | Use |
| NBG3 | 256 Colors | x1 | - |

| Screen Name | Pattern Name | Character Pattern | Vertical Cell Scroll Table |
| --- | --- | --- | --- |
| NBG0 | A0 | B0,B1 | - |
| NBG1 | A0,A1 | B0,B1 | A0 |
| NBG3 | A1 | A1,B0 | - |

| N1CE | N0PN | N1PN | N0PN | NA | CPU | CPU | NA |
| --- | --- | --- | --- | --- | --- | --- | --- |
| N3PN | NA | N1PN | NA | NA | CPU | CPU | N3CG |
| N0CG | N0CG | N1CG | N1CG | NA | N0CG | N0CG | N3CG |
| N0CG | N0CG | N1CG | N1CG | NA | N0CG | N0CG | NA |



<!-- Page 57 -->

### VRAM Cycle Pattern Register

The VRAM cycle pattern register controls the VRAM access during the display interval. It is a 16-bit write only register with addresses from 180010H to 18001EH. Because the value is cleared to 0 after the power is turned on or reset, it must be reset.

| CYCA0L | VCP0A03 | VCP0A02 | VCP0A01 | VCP0A00 | VCP1A03 | VCP1A02 | VCP1A01 | VCP1A00 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180010H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCP2A03 | VCP2A02 | VCP2A01 | VCP2A00 | VCP3A03 | VCP3A02 | VCP3A01 | VCP3A00 |

| CYCA0U | VCP4A03 | VCP4A02 | VCP4A01 | VCP4A00 | VCP5A03 | VCP5A02 | VCP5A01 | VCP5A00 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180012H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCP6A03 | VCP6A02 | VCP6A01 | VCP6A00 | VCP7A03 | VCP7A02 | VCP7A01 | VCP7A00 |

| CYCA1L | VCP0A13 | VCP0A12 | VCP0A11 | VCP0A10 | VCP1A13 | VCP1A12 | VCP1A11 | VCP1A10 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180014H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCP2A13 | VCP2A12 | VCP2A11 | VCP2A10 | VCP3A13 | VCP3A12 | VCP3A11 | VCP3A10 |

| CYCA1U | VCP4A13 | VCP4A12 | VCP4A11 | VCP4A10 | VCP5A13 | VCP5A12 | VCP5A11 | VCP5A10 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180016H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCP6A13 | VCP6A12 | VCP6A11 | VCP6A10 | VCP7A13 | VCP7A12 | VCP7A11 | VCP7A10 |

| CYCB0L | VCP0B03 | VCP0B02 | VCP0B01 | VCP0B00 | VCP1B03 | VCP1B02 | VCP1B01 | VCP1B00 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180018H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCP2B03 | VCP2B02 | VCP2B01 | VCP2B00 | VCP3B03 | VCP3B02 | VCP3B01 | VCP3B00 |

| CYCB0U | VCP4B03 | VCP4B02 | VCP4B01 | VCP4B00 | VCP5B03 | VCP5B02 | VCP5B01 | VCP5B00 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18001AH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCP6B03 | VCP6B02 | VCP6B01 | VCP6B00 | VCP7B03 | VCP7B02 | VCP7B01 | VCP7B00 |

| CYCB1L | VCP0B13 | VCP0B12 | VCP0B11 | VCP0B10 | VCP1B13 | VCP1B12 | VCP1B11 | VCP1B10 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18001CH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCP2B13 | VCP2B12 | VCP2B11 | VCP2B10 | VCP3B13 | VCP3B12 | VCP3B11 | VCP3B10 |

| CYCB1U | VCP4B13 | VCP4B12 | VCP4B11 | VCP4B10 | VCP5B13 | VCP5B12 | VCP5B11 | VCP5B10 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18001EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | VCP6B13 | VCP6B12 | VCP6B11 | VCP6B10 | VCP7B13 | VCP7B12 | VCP7B11 | VCP7B10 |



<!-- Page 58 -->

Table 3.5 shows access command that corresponds to the content of the VRAM

access during 1 cycle.

**Table 3.5 Access command**

> Note:

Sets the access command of VRAM access that performs in VRAM-A0 (or VRAM-A) timing T0 to T7.

| Access Command Value |   |   |   | VRAM Access |
| --- | --- | --- | --- | --- |
| VCPnxx3 | VCPnxx2 | VCPnxx1 | VCPnxx0 |   |
| 0 | 0 | 0 | 0 | NBG0 Pattern Name Data Read |
| 0 | 0 | 0 | 1 | NBG1 Pattern Name Data Read |
| 0 | 0 | 1 | 0 | NBG2 Pattern Name Data Read |
| 0 | 0 | 1 | 1 | NBG3 Pattern Name Data Read |
| 0 | 1 | 0 | 0 | NBG0 Character Pattern Data Read |
| 0 | 1 | 0 | 1 | NBG1 Character Pattern Data Read |
| 0 | 1 | 1 | 0 | NBG2 Character Pattern Data Read |
| 0 | 1 | 1 | 1 | NBG3 Character Pattern Data Read |
| 1 | 0 | 0 | 0 | Setting not allowed |
| 1 | 0 | 0 | 1 | Setting not allowed |
| 1 | 0 | 1 | 0 | Setting not allowed |
| 1 | 0 | 1 | 1 | Setting not allowed |
| 1 | 1 | 0 | 0 | NBG0 Vertical Cell Scroll Table Data Read |
| 1 | 1 | 0 | 1 | NBG1 Vertical Cell Scroll Table Data Read |
| 1 | 1 | 1 | 0 | CPU Read/Write |
| 1 | 1 | 1 | 1 | No Access |



<!-- Page 59 -->

Sets the access command of the VRAM access that performs in VRAM-A1 timing T0 to T7. When VRAM is not partitioned in two, the value of this register is ignored. Sets the access command of VRAM access that performs in VRAM-B0 (or VRAM-B) timing T0 to T7.

| VCP0A00~VCP0A03 | 180010H | Bit 12~15 | VRAM-A0 (or VRAM-A) Timing for T0 |
| --- | --- | --- | --- |
| VCP1A00~VCP1A03 | 180010H | Bit 8~11 | VRAM-A0 (or VRAM-A) Timing for T1 |
| VCP2A00~VCP2A03 | 180010H | Bit 4~7 | VRAM-A0 (or VRAM-A) Timing for T2 |
| VCP3A00~VCP3A03 | 180010H | Bit 0~3 | VRAM-A0 (or VRAM-A) Timing for T3 |
| VCP4A00~VCP4A03 | 180012H | Bit 12~15 | VRAM-A0 (or VRAM-A) Timing for T4 |
| VCP5A00~VCP5A03 | 180012H | Bit 8~11 | VRAM-A0 (or VRAM-A) Timing for T5 |
| VCP6A00~VCP6A03 | 180012H | Bit 4~7 | VRAM-A0 (or VRAM-A) Timing for T6 |
| VCP7A00~VCP7A03 | 180012H | Bit 0~3 | VRAM-A0 (or VRAM-A) Timing for T7 |

| VCP0A10~VCP0A13 | 180014H | Bit 12~15 | VRAM-A1 Timing for T0 |
| --- | --- | --- | --- |
| VCP1A10~VCP1A13 | 180014H | Bit 8~11 | VRAM-A1 Timing for T1 |
| VCP2A10~VCP2A13 | 180014H | Bit 4~7 | VRAM-A1 Timing for T2 |
| VCP3A10~VCP3A13 | 180014H | Bit 0~3 | VRAM-A1 Timing for T3 |
| VCP4A10~VCP4A13 | 180016H | Bit 12~15 | VRAM-A1 Timing for T4 |
| VCP5A10~VCP5A13 | 180016H | Bit 8~11 | VRAM-A1 Timing for T5 |
| VCP6A10~VCP6A13 | 180016H | Bit 4~7 | VRAM-A1 Timing for T6 |
| VCP7A10~VCP7A13 | 180016H | Bit 0~3 | VRAM-A1 Timing for T7 |



<!-- Page 60 -->

Sets the access command of VRAM access that performs in VRAM-B1 timing T0 to T7. When VRAM is not partitioned into two areas, the value of this register is ignored.

| VCP0B00~VCP0B03 | 180018H | Bit 12~15 | VRAM-B0 (or VRAM-B) Timing for T0 |
| --- | --- | --- | --- |
| VCP1B00~VCP1B03 | 180018H | Bit 8~11 | VRAM-B0 (or VRAM-B) Timing for T1 |
| VCP2B00~VCP2B03 | 180018H | Bit 4~7 | VRAM-B0 (or VRAM-B) Timing for T2 |
| VCP3B00~VCP3B03 | 180018H | Bit 0~3 | VRAM-B0 (or VRAM-B) Timing for T3 |
| VCP4B00~VCP4B03 | 18001AH | Bit 12~15 | VRAM-B0 (or VRAM-B) Timing for T4 |
| VCP5B00~VCP5B03 | 18001AH | Bit 8~11 | VRAM-B0 (or VRAM-B) Timing for T5 |
| VCP6B00~VCP6B03 | 18001AH | Bit 4~7 | VRAM-B0 (or VRAM-B) Timing for T6 |
| VCP7B00~VCP7B03 | 18001AH | Bit 0~3 | VRAM-B0 (or VRAM-B) Timing for T7 |

| VCP0B10~VCP0B13 | 18001CH | Bit 12~15 | VRAM-B1 Timing for T0 |
| --- | --- | --- | --- |
| VCP1B10~VCP1B13 | 18001CH | Bit 8~11 | VRAM-B1 Timing for T1 |
| VCP2B10~VCP2B13 | 18001CH | Bit 4~7 | VRAM-B1 Timing for T2 |
| VCP3B10~VCP3B13 | 18001CH | Bit 0~3 | VRAM-B1 Timing for T3 |
| VCP4B10~VCP4B13 | 18001EH | Bit 12~15 | VRAM-B1 Timing for T4 |
| VCP5B10~VCP5B13 | 18001EH | Bit 8~11 | VRAM-B1 Timing for T5 |
| VCP6B10~VCP6B13 | 18001EH | Bit 4~7 | VRAM-B1 Timing for T6 |
| VCP7B10~VCP7B13 | 18001EH | Bit 0~3 | VRAM-B1 Timing for T7 |



<!-- Page 61 -->

## 3.4 Color RAM Mode

With 32 Kbits (2 Kword) of color RAM, color data that is stored is used for all scroll screens and palette format sprites. The color data selects and stores either RGB-5 bit (15 bit data) or RGB-8 bit (24 bit data). In addition, when dividing it into 16K bits (1K word) and storing various color data of the same type, the expansion color calculation function can also be used. There are three methods for storing color data in color RAM: (1) Mode 0: RGB in each of 5 bits for a total of 15 bits, 1024 color settings (2) Mode 1: RGB in each of 5 bits for a total of 15 bits, 2048 color settings (3) Mode 2: RGB in each of 8 bits for a total of 24 bits, 1024 color settings Because color data must be set to RGB-8 bit when it is output, a 0 will be added to the lowest 3 bits if RGB-5 bit color data is stored in the color RAM, . When the special color calculation mode is set to mode 3, the most significant bit of color RAM data becomes the color calculation enable bit. See “12.3 Special Color Calculation Function” about the special color calculation mode. ST-58-R2



<!-- Page 62 -->

**Figure 3.9 shows the color data configuration of the color RAM.**

> Note: The MSB CC is enable bit when special color calculation mode is mode 3.

> Note: The MSB CC is enable bit when special color calculation mode is mode 3.

**Figure 3.9 Color data configuration on the color RAM**

| CC | 5 Bti Blue Data | 5 Bit Green Data | 5 Bit Red Data |
| --- | --- | --- | --- |

| 5 Bti Blue Data | 0 0 0 |
| --- | --- |

| 5 Bit Green Data | 0 0 0 |
| --- | --- |

| 5 Bit Red Data | 0 0 0 |
| --- | --- |

| CC |   | 8 Bti Blue Data |
| --- | --- | --- |

| 8 Bit Green Data | 8 Bti Red Data |
| --- | --- |



<!-- Page 63 -->

Color data written to the color RAM is illustrated in Figure 3.10.

**Figure 3.10 Color Data of the Color RAM**

### RAM Control Register

The RAM control register selects the bank partitions of the VRAM, the purpose of using the rotation scroll screen of VRAM, and the color RAM mode. It is a read/ write 16-bit register and is at the 18000EH address. Also, because the value is cleared to 0, it must be set after the power is turned on or reset. See “6.4 Coefficient Table Control.”

| 16 bit X 1024 Colors |
| --- |
| 16 bit X 1024 Colors |

| RAMCTL | CRKTE | ~ | CRMD1 | CRMD0 | ~ | ~ | VRBMD | VRAMD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18000EH | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | RDBSB11 | RDBSB10 | RDBSB01 | RDBSB00 | RDBSA11 | RDBSA10 | RDBSA01 | RDBSA00 |



<!-- Page 64 -->

Selects the color RAM mode. See “3.4 Color RAM mode.” Set the Color RAM mode to mode 1 when the CRKTE bit is 1. At that time, color data can no longer be stored because the second half of the color RAM (100800H ~ 100FFFH) is used for the coefficient table data. Saving color data to the color RAM must be done after thes bits have been set. When mode 0 is set, data written to the first half of the color RAM will be written to the second half at the same time. VRAM mode bit (VRBMD and VRAMD), bits 9 and 8. (See “ 3.2 VRAM Bank Partition.”) Designates the use objective of the VRAM of the rotation scroll screen. This bit is only in effect when the rotation scroll screen is displayed. (See “6.2 Rotation Scroll Screen Display Control.”)

| CRMD1 | CRMD0 | Mode | Process |
| --- | --- | --- | --- |
| 0 | 0 | 0 | RGB each 5 bits, 1024 color settings |
| 0 | 1 | 1 | RGB each 5 bits, 2048 color settings |
| 1 | 0 | 2 | RGB each 8 bits, 1024 color settings |
| 1 | 1 | - | Setting not allowed |



<!-- Page 65 -->

# Chapter 4 Scroll Screen

## 4.1 Screen Display Control ..............................................................................48

Screen Display Enable Register ...........................................................48

## 4.2 Scroll Screen Structure ..............................................................................50

Cell Format ........................................................................................... 50 Bit Map Format .....................................................................................52

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

Mosaic Control Register ..................................................................... 118 ST-58-R2



<!-- Page 66 -->

## 4.1

## Screen Display Control

The scroll screen selects screens not displayed by controlling VRAM access used in the display of each screen, and can also indicate whether to invalidate the dot color code (transparency code) in each screen, which are the transparent dots of the screen being displayed.

### Screen Display Enable Register

The screen display enable register controls the screen display and transparency code. With a write-only 16-bit register, its address is 180020H. Because the value of the register is cleared to 0 after power on or reset, the value must be set. Designates whether to nullify the transparency code. For more specifics about the transparency code see Transparent Dots in section “4.3 Cell.”

> Note: N0, N1, N2, N3, or R0 is entered into bit name for xx.

| BGON | ~ | ~ | ~ | R0TPON | N3TPON | N2TPON | N1TPON | N0TPON |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180020H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | R1ON | R0ON | N3ON | N2ON | N1ON | N0ON |

| N0TPON | 180020H | Bit 8 | For NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1TPON | 180020H | Bit 9 | For NBG1 (or EXBG) |
| N2TPON | 180020H | Bit 10 | For NBG2 |
| N3TPON | 180020H | Bit 11 | For NBG3 |
| R0TPON | 180020H | Bit 12 | For RBG0 |

| xxTPON | Process |
| --- | --- |
| 0 | Validates transparency code (transparency code dots become transparent) |
| 1 | Invalidates transparency code (transparency code dots are displayed according to d<br>values) |



<!-- Page 67 -->

Designates whether to display each scroll screen.

> Note: N0, N1, N2, N3, R0, or R1 is entered into bit name xx.

When the screen access command (which has a 0 bit) is set in the VRAM cycle pattern register, the access command is ignored and the VRAM access for displaying the screen will not be performed. When R0ON is 0, do not set R1ON at 1. When both R0ON and R1ON are 1, the normal scroll screen can no longer be displayed. At this time, VRAM-B0 is fixed in RAM used for RBG1 character pattern tables; and VRAM-B1 is fixed in RAM used for RBG1 pattern name tables. When a specific screens can no longer be displayed by register settings, the screen bit should be set to 0. For example, when both R0ON and R1ON are 1, set the N0ON, N1ON, N2ON, N3ON bits at 0. See section “6.2 Rotation Scroll Surface Display Control” for more about rotation scroll surfaces.

| N0TPON | 180020H | Bit 8 | For NBG0 (or RBG1) |
| --- | --- | --- | --- |
| N1TPON | 180020H | Bit 9 | For NBG1 (or EXBG) |
| N2TPON | 180020H | Bit 10 | For NBG2 |
| N3TPON | 180020H | Bit 11 | For NBG3 |
| R0TPON | 180020H | Bit 12 | For RBG0 |

| xxTPON | Process |
| --- | --- |
| 0 | Validates transparency code (transparency code dots become transparent) |
| 1 | Invalidates transparency code (transparency code dots are displayed according to d<br>values) |



<!-- Page 68 -->

## 4.2 Scroll Screen Structure

The scroll screen has two screen formats, the cell format and the bit map format.

### Cell Format

The cell format scroll screen is composed of picture pattern “cells” that are 8 H dots by 8 V dots; cells are arranged in 1 H X 1 V or 2 H X 2 V to form “character patterns.” A “page” is an arrangement of character patterns in 32 H X 32 V or 64 H X 64 V. A “plane” is an arrangement of pages 1 H X 1 V, 2 H X 1 V, or 2 H X 2 V. A “map” is an arrangement of planes 2 H X 2 V (for normal scroll screens), or 4 H X 4 V (for rotation scroll surface). Figure 4.1 shows the cell format configuration of the scroll screen.

**Figure 4.1 Scroll screen configuration of the cell format**

Dot color data stored as character pattern tables in VRAM becomes cell data. Color data is composed of 4, 8, 16, or 32-bit character color. Character pattern data is cell data arranged in one or four pieces. Page data is character pattern name data (address of character pattern table) stored as a pattern name table. Page data arranged in one, two, or four pieces is a plane. The map selects the lead address of the pattern name table in the map register and map offset register. Figure 4.2 shows the configuration of a cell format of the scroll screen and corresponding data settings.

|   | d |
| --- | --- |
| i |   |



<!-- Page 69 -->

> Note: Character pattern and pla ne size vary depen din g on the reg ister setting; ma p size varies

**Figure 4.2**

| Plane A | Plane B |
| --- | --- |
| Plane C | Plane D |

| Top Address of Plane A PNT |
| --- |
| Top Address of Plane B PNT |
| Top Address of Plane C PNT |
| Top Address of Plane D PNT |

| Page 0 | Page 1 |
| --- | --- |
| Page 2 | Page 3 |

| CP0 Pattern Name Data |   |
| --- | --- |
| CP1023 Pattern Name Data |   |
| CP0 Pattern Name Data |   |
| CP1023 Pattern Name Data |   |
| CP0 Pattern Name Data |   |
|   | i |
| CP1023Pattern Name Data |   |
| CP0 Pattern Name Data |   |
| CP1023 Pattern Name Data |   |

| Cell 0 | Cell 1 |
| --- | --- |
| Cell 2 | Cell 3 |

| Dot 0 Color Data |
| --- |
| Dot 63 Color Data |
| Dot 0 Color Data |
| Dot 63 Color Data |
| Dot 0 Color Data |
| Dot 63 Color Data |
| Dot 0 Color Data |
| Dot 63 Color Data |



<!-- Page 70 -->

### Bit Map Format

The scroll screen of the bit map format is composed of a bit map pattern 512 H (or 1024) dots and 256 V (or 512) dots. When a screen is displayed by the bit map format, the size of the bit map must be set in the register and the set size of the bit map pattern must be stored in VRAM. Figure 4.3 shows the scroll screen configuration of the bit map format. Figure 4.4 shows the relationship of the register and the scroll screen of the bit map format.

**Figure 4.3 Scroll screen configuration of the bit map format**

**Figure 4.4 Relationship of bit map format scroll screen and data settings**

| Dot 0 Color Data |
| --- |
| C |



<!-- Page 71 -->

## 4.3 Cell

The cell is a picture pattern 8 H dots by 8 V dots, and is stored in VRAM. The character color count (number of colors per one cell) can be selected from among 16, 256, 2048, 32,768, or 16,777,216 colors. The amount of RAM required in the size of each dot color data and in data of one cell changes according to the color count.

### Character Color Number

There are two color formats for displaying characters: the palette format and the RGB format. The palette format treats display color data as color RAM address data selected by the dot color code within cell data, and palette number within pattern name data. The RGB format treats cell data as display color data.

**Table 4.1 shows**

the character color count and the number of bits per dot at that time in the various color formats.

**Table 4.1 Character color count and dot data size**

> Note: In color RAM modes 0 and 2, 2048-color becomes 1024-color.

### Cell Data Configuration

The data configuration of each cell stored in a character pattern table changes according to the bit count of one dot. The boundary when stored in VRAM is 20H and has no relationship to the bit count of one dot. Cell data configuration is shown in

**Table 4.2 and Figure 4.5.**

**Table 4.2 Cell Data Configuration**

| Color Format | Character Color Count | Bit Count for 1 Dot |
| --- | --- | --- |
|   | 16 colors | 4 bits |
| Palette Format | 256 colors | 8 bits |
|   | 2048 colors | 16 bits (Only use lower 11 bits) |
| RGB Format | 32,768 colors | 16 bits |
|   | 16,770,000 colors | 32 bits (Only use MSB and lower 24 bits) |

| Bit Count for 1 Dot | Cell Data | Boundary |
| --- | --- | --- |
| 4 bits/dot | 32 bytes/cell | 20H byte |
| 8 bits/dot | 64 bytes/cell | 20H byte |
| 16 bits/dot | 128 bytes/cell | 20H byte |
| 32 bits/dot | 256 bytes/cell | 20H byte |



<!-- Page 72 -->

**Figure 4.5 Data configuration of cells by character color count**

| Dot 0-0 Data | Dot 0-1 Data | Dot 0-2 Data | Dot 0-3 Data |
| --- | --- | --- | --- |
| Dot 0-4 Data | Dot 0-5 Data | Dot 0-6 Data | Dot 0-7 Data |

| Dot 7-4 Data | Dot 7-5 Data | Dot 7-6 Data | Dot 7-7 Data |
| --- | --- | --- | --- |

| +0 |   | 0 | +0 |   | 1 | +0 |   | 2 | + |   | 03 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   | +0 | 0 |   | +0 | 1 |   | +0 | 2 |   | + | 03 |
| +0 |   | 4 | +0 |   | 5 | +0 |   | 6 | + |   | 07 |
|   | +0 | 4 |   | +0 | 5 |   | +0 | 6 |   | + | 07 |
| +0 |   | 8 | +0 |   | 9 | +0 |   | A | +0 |   |   |
|   | +0 | 8 |   | +0 | 9 |   | +0 | A |   | +0 | B |
| +0 |   | C | +0 |   | D | +0 |   | E | +0 |   | F |
|   | +0 | C |   | +0 | D |   | +0 | E |   | +0 | F |
| +1 |   | 0 | +1 |   | 1 | +1 |   | 2 | + |   | 13 |
|   | +1 | 0 |   | +1 | 1 |   | +1 | 2 |   | + | 13 |
| +1 |   | 4 | +1 |   | 5 | +1 |   | 6 | + |   | 17 |
|   | +1 | 4 |   | +1 | 5 |   | +1 | 6 |   | + | 17 |
| +1 |   | 8 | +1 |   | 9 | +1 |   |   | +1 |   | B |
|   | +1 | 8 |   | +1 | 9 |   | +1 |   |   | +1 | B |
| +1 |   | C | +1 |   | D | +1 |   | E | +1 |   | F |
|   | +1 | C |   | +1 | D |   | +1 | E |   | +1 | F |



<!-- Page 73 -->

**Figure 4.5 Data configuration of cells by character color count (continued)**

| Dot 0-0 Data | Dot 0-1 Data |
| --- | --- |
| Dot 0-2 Data | Dot 0-3 Data |

| Dot 7-6 Data | Dot 7-7 Data |
| --- | --- |

| +00 | +01 | +02 | +03 | +04 | +05 | +06 | +07 |
| --- | --- | --- | --- | --- | --- | --- | --- |
|   |   |   |   |   | i |   |   |
| +08 | +09 | +0A | +0B | +0C | +0D | +0E | +0F |
|   |   |   |   | f |   |   |   |
| +10 | +11 | +12 | +13 | +14 | +15 | +16 | +17 |
| +18 | +19 | +1A | +1B | +1C | +1D | +1E | +1F |
|   |   |   | n |   |   |   |   |
| +20 | +21 | +22 | +23 | +24 | +25 | +26 | +27 |
| +28 | +29 | +2A | +2B | +2C | +2D | +2E | +2F |
| +30 | +31 | +32 | +33 | +34 | +35 | +36 | +37 |
| +38 | +39 | +3A | +3B | +3C | +3D | +3E | +3F |



<!-- Page 74 -->

**Figure 4.5 Data configuration of cells by character color count (continued)**

| Dot 0-0 Data |
| --- |
| Dot 0-1 Data |

| +00 |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- |
|   | +02 | +04 | +06 | +08 | +0A | +0C | +0E |
| +10 | +12 | +14 | +16 | +18 | +1A | +1C | +1E |
| +20 | +22 | +24 | +26 | +28 | +2A | +2C | +2E |
| +30 | +32 | +34 | +36 | +38 | +3A | +3C | +3E |
|   |   |   |   |   |   |   | n |
| +40 | +42 | +44 | +46 | +48 | +4A | +4C | +4E |
| +50 | +52 | +54 | +56 | +58 | +5A | +5C | +5E |
| +60 | +62 | +64 | +66 | +68 | +6A | +6C | +6E |
| +70 | +72 | +74 | +76 | +78 | +7A | +7C | +7E |



<!-- Page 75 -->

**Figure 4.5 Data configuration of cells by character color count (continued)**

### Transparent Dots

Dot color code, which are transparent dots (transparency code), changes according to the color format. When the color format is the palette format, the transparent dot applies when all bits per one dot is 0; when the RGB format, the transparent dot applies when the most significant bit of the dot data is 0. When in the palette format, lead color data of the palette corresponds to the transparency code; therefore, it normally cannot be used. If the transparency code is nullified, this color data can be used. Control is done by the screen display enable register.

Table 4.3 shows the transparent dot data values.

| Dot 0-0 Data (Most signfiicant word) |
| --- |
| Dot 0-0 Data (Least significant word) |
| Dot 0-1 Data (Most signfiicant word) |

| Dot 7-7 Data (Most signfiicant word) |
| --- |
| Dot 7-7 Data (Least significant word) |

| +00 |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- |
|   | +04 | +08 | +0C | +10 | +14 | +18 | +1C |
|   |   |   |   |   | +34 | +38 |   |
| +20 | +24 | +28 | +2C | +30 |   |   | +3C |
|   |   |   |   |   | +54 | +58 |   |
| +40 | +44 | +48 | +4C | +50 |   |   | +5C |
|   |   |   |   |   | +74 | +78 |   |
| +60 | +64 | +68 | +6C | +70 |   |   | +7C |
|   |   |   |   |   | +94 | +98 |   |
| +80 | +84 | +88 | +8C | +90 |   |   | +9C |
| +A0 | +A4 | +A8 | +AC | +B0 | +B4 | +B8 | +BC |
|   | o |   |   |   |   |   |   |
| +C0 | +C4 | +C8 | +CC | +D0 | +D4 | +D8 | +DC |
| +E0 | +E4 | +E8 | +EC | +F0 | +F4 | +F8 | +FC |



<!-- Page 76 -->

**Table 4.3 Transparent dot data values**

### RGB Format Dot Data

When the color format is the RGB format, the character color count can be selected from two groups: 32,768 colors and 16,770,000 colors. 16,770,000 colors are designated by RGB 8-bit; but 32,768 colors designate the higher 5 bits within RGB 8-bit, and the lower 3 bits are set to 0. The transparency bit designates whether it is a transparent dot. The most significant bit is a transparent dot when 0. In the screen display enable register, when the transparency code is indicated as invalid, the transparent bit is ignored. Figure 4.6 shows the dot data of the RGB format.

**Figure 4.6 RGB format dot data**

| Color Format | Character Color<br>Count | Bit Count for 1 Dot | Transparency Code |
| --- | --- | --- | --- |
|   | 16 colors | 4 bits/dot | 0H (4 bit) |
| Palette Format | 256 colors | 8 bits/dot | 00H (8 bit) |
|   | 2048 colors | 16 bits/dot | 000H (lower 11 bits) |
| RGB Format | 32,768 colors | 16 bits/dot | MSB (bit 15) is 0 |
|   | 16,770,000 colors | 32 bits/dot | MSB (bit 31) is 0 |

|   | Blue Data |   |   |   |   | Green Data |   |   |   |   | Red Data |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   | 7 | 6 | 5 | 4 | 3 | 7 | 6 | 5 | 4 | 3 | 7 | 6 | 5 | 4 | 3 |

|   |   |   |   |   |   |   |   | Blue Data |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|   |   |   |   |   |   |   |   | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |

| Green Data |   |   |   |   |   |   |   | Red Data |   |   |   |   |   |   |   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |



<!-- Page 77 -->

## 4.4 Character Patterns

Character patterns are perfect squares composed of 1 or 4 cells; the size is specified in their respective registers.

### Character Size and Cell Arrangement

When the character pattern is composed of four cells, the data of a cell that is used in the same character pattern must be linked to and stored in a character pattern table. The relationship of cell arrangement by character size (cell number of character pattern) and character pattern table is shown in Figure 4.7.

**Figure 4.7 Cell Arrangement by Character Size**

| Cell Data 0 |
| --- |
| Cel lData 1 |
| Cell Data 2 |
| Cell Data 3 |
| Cell Data 4 |

| Cel lData 0 |
| --- |

| Cel lData 0 | Cell Data 1 |
| --- | --- |
| Cell Data 2 | Cell Data 3 |



<!-- Page 78 -->

## 4.5

## Character Control Register

The character control register selects cell and bit map formats, the number of character (bit map) colors, and the size of the character pattern or bit map. This register is a write only 16-bit register located in addresses 180028H to 18002AH. Because the value of the register is cleared to 0 after the power is turned on or reset, the value must be set. Designates the character color count of each screen, and the bit map color count when displaying by the bit map format.

> Note:

| CHCTLA | ~ | ~ | N1CHCN1 | N1CHCN0 | N1BMSZ1 | N1BMSZ0 | N1BMEN | N1CHSZ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180028H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
