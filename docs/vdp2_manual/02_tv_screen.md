(This page was blank in the original Japanese document.)



<!-- Page 29 -->

# Chapter 2 TV Screen

2.1 TV Screen Mode.................................................. 12 Special High Resolution Graphics Mode ............. 13 2.2 Interlace Mode ..................................................... 14 2.3 TV Screen Structure ............................................ 15 2.4 TV Screen Mode Register ................................... 16 2.5 External Signals and Scan Conditions ................ 19 External Signal Enable Register .................... 19 Screen Status Register .................................. 21 H Counter Register ........................................ 23 V Counter Register ........................................ 24 ST-58-R2



<!-- Page 30 -->

## 2.1 TV Screen Mode

VDP2 can display images in 31 kHz monitors as well as high-vision monitors, and in NTSC and PAL standards for TV. There are three kinds of image displaying TV screen modes: normal, high-resolution, and special monitor. Screen scan format can be selected from three types: non-interlace, single-density interlace, and doubledensity interlace. A register showing TV scan conditions is also provided.

Table 2.1 shows the TV screen modes that are selectable, the graphics mode, and the

current resolution. Furthermore, special settings are required when indicating special high-resolution graphics A and special high-resolution graphics B.

**Table 2.1 TV Screen Mode**

| TV Screen<br>Mode | Graphics Mode | Interlace Mode | Horiz X Vertical.<br>Resolution<br>(Pixels) | Restrictions During<br>Use |
| --- | --- | --- | --- | --- |
| Normal |   |   | 320 X 224 |   |
|   |   | Non-interlace | 320 X 240 |   |
|   | Normal |   | 320 X 256 | PAL standard only |
|   | Graphic A |   | 320 X 448 |   |
|   |   | Interlace | 320 X 480 |   |
|   |   |   | 320 X 512 | PAL standard only |
|   |   |   | 352 X 224 |   |
|   |   | Non-interlace | 352 X 240 |   |
|   | Normal |   | 352 X 256 | PAL standard only |
|   | Graphic B |   | 352 X 448 |   |
|   |   | Interlace | 352 X 480 |   |
|   |   |   | 352 X 512 | PAL standard only |
| Hi-Res |   |   | 640 X 224 |   |
|   |   | Non-interlace | 640 X 240 |   |
|   | Hi-Res |   | 640 X 256 | PAL standard only |
|   | Graphic A |   | 640 X 448 |   |
|   |   | Interlace | 640 X 480 |   |
|   |   |   | 640 X 512 | PAL standard only |
|   |   |   | 704 X 224 |   |
|   |   | Non-interlace | 704 X 240 |   |
|   | Hi-Res |   | 704 X 256 | PAL standard only |
|   | Graphic B |   | 704 X 448 |   |
|   |   | Interlace | 704 X 480 |   |
|   |   |   | 704 X 512 | PAL standard only |
|   |   | Non-interlace | 320 X 480 | 31kHz monitor only |
|   |   | Non-interlace | 352 X 480 | Hi-vision monitor only |
|   |   | Non-interlace | 640 X 480 | 31kHz monitor only |
|   | G | Non-interlace | 704 X 480 | Hi-vision monitor only |



<!-- Page 31 -->

### Special High-Resolution Graphics Mode

The graphics mode of special high-resolution graphics A or B displays one screen by joining the NBG0 and NBG1 screens. If the following setting is not performed, the display will not appear correctly.

- Must be able to display only NBG0 and NBG1.
- The NBG0 and NBG1 character pattern tables (or, bit map pattern) and pattern
name tables must use the exact same data.

- Must be able to reduce both NBG0 and NBG1 horizontally up to 50%.
- Set the vertical direction screen scroll values of NBG0 and NBG1 so that they
are identical.

- Set the NBG1 horizontal screen scroll value at the NBG0 horizontal screen
scroll value plus 1.

- Set both NBG0 and NBG1 horizontal coordinate increments at 2.
- Set the color RAM mode to 0.
- Set the priority numbers of NBG0 and NBG1 at the same value.
- Do not enter the line color screen.
- Special priority of both NBG0 and NBG1 should be in mode 0.
- Do not use the color calculation function.
- For registers other than those listed above, NBG0 and NBG1 settings should
be the same. ST-58-R2



<!-- Page 32 -->

## 2.2 Interlace Mode

VDP2 interlace mode (screen scan method) consists of non-interlace, single-density interlace, and double-density interlace modes. The non-interlace mode is 1 field per frame (1/60 sec.). The single-density interlace mode is 2 fields (1/30 sec.) per frame; the same image is displayed in even and odd fields. The double-density interlace mode is 2 fields (1/30 sec.) per 1 frame with separate images being displayed in even and odd fields. There is no space between scan lines in both the single-density and double-density interlace modes, but the actual resolution in the vertical direction of the single-density interlace mode is the same as the resolution in the vertical direction of the non-interlace mode. Figure 2.1 shows display methods by interlace settings.

**Figure 2.1 Display Method by Interlace Setting**

|   |   | G |
| --- | --- | --- |

| E |
| --- |



<!-- Page 33 -->

## 2.3 TV Screen Structure

In response to the TV screen mode, VDP2 outputs image signals corresponding to their respective NTSC standard or PAL standard TV, 31 kHz monitor, and highvision monitor. The TV screen is a collection of rasters constructed by vertical display intervals, vertical blank intervals (V blank interval), and their respective horizontal display intervals and horizontal blank intervals (H blank interval). The TV screen structure is shown in Figure 2.2. The location where horizontal display intervals and vertical display intervals overlap is the standard display area of the various TV formats. The set display area, where VDP2 is able to display the image, is slightly smaller than the standard display area. The border area excludes the set display area from the standard display, and can output either black or the back screen.

**Figure 2.2 TV Screen Structure**

|   | BoarderA rea<br>Setting<br>Display Area |
| --- | --- |
| A | Standard<br>Display Area |



<!-- Page 34 -->

## 2.4 TV Screen Mode Register

The TV screen mode register controls the TV screen display. It is a read/write 16 bit register and is at address 180000H. After the power on or reset, the value is cleared to 0 and therefore must be set.

- TV screen display bit : Display bit (DISP), bit 15
Controls picture display to the TV screen. Because it is in the blank condition during the display interval when this bit is 0, the VRAM can be accessed from the CPU or DMA controller at any time. The colors displayed when this bit is 0 are selected by the BDCLMD bit. Please make sure to change this bit from 0 to 1 during V blank.

- Border color mode bit (BDCLMD), bit 8
Controls colors displayed by the border area. Selects colors of all the standard display areas when the DISP bit is 0. However, after the power on or reset, if this bit is set to 1 without setting DISP bit to 1 even once, the back screen will not be correctly displayed. When the setting allows the back screen selection by line, the color displayed in the border area will become the same color as the lowermost line in the display area.

| TVMD | DISP | ~ | ~ | ~ | ~ | ~ | ~ | BDCLMD |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180000H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | LSMD1 | LSMD0 | VRESO1 | VRESO0 | ~ | HRESO2 | HRESO1 | HRESO0 |

| DISP | Process |
| --- | --- |
| 0 | Picture is not displayed on TV screen |
| 1 | Picture is displayed on TV screen |

| BDCLMD | Process |
| --- | --- |
| 0 | Displays black |
| 1 | Display back screen |



<!-- Page 35 -->

- Interlace mode bit (LSMD1, LSMD0) bits 7 and 6
Designates the interlace mode. Single-density interlace is a mode that shows the same pictures in odd and even fields; double-density interlace is a mode that shows different pictures in odd and even fields. In either case, the spaces between scan lines are not vacant. The vertical resolution for double-density interlace is twice that of non-interlace, but the vertical resolution of the actual picture for single-density interlace is the same for noninterlace. Pictures displayed in double-density interlace are vertically half the size of pictures displayed in single-density interlace or non-interlace. When the horizontal resolution (HRESO2 to HRESO0) setting is in the exclusive monitor mode, make sure to select the noninterlaced mode (00B).

- Vertical resolution bit (VRESO1, VRESO0), bit 5, 4
Designates vertical resolution when a picture is displayed on the TV screen. Increments when vertical resolution is increased, then are added to the top and bottom of the screen without changing the screen’s center. When set in the special monitor mode, the horizontal resolution (HRESO2 to HRESO0) is set to 480 lines. Settings of this bit are ignored.

| LSMD1 | LSMD0 | Process |
| --- | --- | --- |
| 0 | 0 | Non-Interlace |
| 0 | 1 | Setting not allowed |
| 1 | 0 | Single-density interlace |
| 1 | 1 | Double-density interlace |

| VRESO1 | VRESO0 | Vertical Resolution | Display Monitor |
| --- | --- | --- | --- |
| 0 | 0 | 224 Lines | NTSC or PAL format TV |
| 0 | 1 | 240 Lines | NTSC or PAL format TV |
| 1 | 0 | 256 Lines | PAL format TV |
| 1 | 1 | Not Allowed | - |



<!-- Page 36 -->

- Horizontal resolution bit (HRESO2 to HRESO0), bit 2 to 0
Selects the horizontal resolution when a picture is displayed on the TV screen. When special high-resolution graphics A or B is selected, other registers must be set as directed. See “Special High Resolution Graphics Mode” on page 13 for more information. When switching the TV mode from exclusive monitor mode to normal mode or hi-res mode, make sure to reset the VDP2.

| HRESO2 | HRESO1 | HRESO0 | Horizontal<br>Resolution | Graphic Mode | Display<br>Monitor |
| --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 320 Pixels | Normal<br>Graphic A | a |
| 0 | 0 | 1 | 352 Pixels | Normal<br>Graphic B | NTSC<br>Format or |
| 0 | 1 | 0 | 640 Pixels | Hi-Res<br>Graphic A | PAL<br>Format TV |
| 0 | 1 | 1 | 704 Pixels | Hi-Res<br>Graphic B |   |
| 1 | 0 | 0 | 320 Pixels | Exclusive Normal<br>Graphic A | 31kHz Monitor |
| 1 | 0 | 1 | 352 Pixels | Exclusive Normal<br>Graphic B | Hi-Vision Monitor |
| 1 | 1 | 0 | 640 Pixels | Exclusive Normal<br>Graphic A | 31kHz Monitor |
| 1 | 1 | 1 | 704 Pixels | Exclusive Normal<br>Graphic B | Hi-Vision Monitor |



<!-- Page 37 -->

## 2.5 External Signals and Scan Conditions

The register controlling external signals has an external signal enable register. The register displaying TV scan conditions has a screen status register, H counter register, and V counter register.

### External Signal Enable Register

The external signal enable register controls signals from the VDP2 exterior. It is a read/write 16 bit register and is at address 180002H. After the power is turned on or reset, the value is cleared to 0 and must be set.

- External latch enable bit (EXLTEN), bit 9
Selects the condition for latching the HV counter value to the HV counter register. The latched H counter value can read with the H counter register; V counter value can read with the V counter register. When reading H and V counter values through external signals such as laser guns, the bit should be set at 1. Otherwise, it should be set at 0.

- EXSYNC enable bit (EXSYEN), bit 8
Controls input to the internal synchronous circuit of the external sync signal. When synchronizing with other devices and screen displays, set to 1 and input an EXSYNC signal. The normal setting is 0.

- Display area select bit (DASEL), bit1
Designates the image display area. Valid only when the EXBGEN bit is 1.

| EXTEN | ~ | ~ | ~ | ~ | ~ | ~ | EXLTEN | EXSYEN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180002H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | ~ | ~ | DASEL | EXBGEN |

| EXLTEN | Condition |
| --- | --- |
| 0 | Latches when reading external signal enable register |
| 1 | Latches through external signal |

| EXSYEN | Process |
| --- | --- |
| 0 | Does not input external sync signal |
| 1 | Inputs external sync signal, and synchronizes TV screen display with the<br>external |



<!-- Page 38 -->

When displaying the entire standard display area, images from external screen data are displayed correctly. Images not in set display areas (sprite, scroll screen, etc.) need to be made transparent using a window because they are not displayed correctly.

- EXBG enable bit (EXBGEN), bit 0
Controls input of external screen data. Because the data becomes NBG1 screen data when inputting external screen data, the external screen settings are used for NBG1 as well.

Table 2.2 shows the register

bit for setting the external screen.

| DASEL | Process |
| --- | --- |
| 0 | Displays screen image only in the set display area |
| 1 | Displays screen in the standard display area |

| EXBGEN | Process |
| --- | --- |
| 0 | Does not input external screen data |
| 1 | Inputs external screen data |



<!-- Page 39 -->

**Table 2.2 Register for setting the external screen**

### Screen Status Register

The screen status register displays TV screen information. This read exclusive 16-bit register is at address 180004H.

| Address | Bit Number | Bit Name |   |
| --- | --- | --- | --- |
| 180020H | 9 | N1TPON | Transparent display enable |
| 180028H | 13,12 | N1CHCN1, N1CHCN0 | Character Color Count |
|   | 8 | N1W0A | W0 window area |
|   | 9 | N1W0E | W0 window enable |
|   | 10 | N1W1A | W1 window area |
| 1800D0H | 11 | N1W1E | W1 window enable |
|   | 12 | N1SWA | SW window area |
|   | 13 | N1SWE | SW window enable |
|   | 15 | N1LOG | Window logic |
| 1800E2H | 1 | N1SDEN | Shadow enable |
| 1800E4H | 6~4 | N1CAOS2~N1CAOS0 | Color RAM address offset |
| 1800E8H | 1 | N1LCEN | Line color screen insertion enable |
| 1800EAH | 3,2 | N1SPRM1, N1SPRM0 | Special priority mode |
| 1800ECH | 1 | N1CCEN | Color calculation enable |
| 1800EEH | 3,2 | N1SCCM, N1SCCM0 | Special color calculation mode |
| 1800F8H | 10~8 | N1PRIN2~N1PRIN0 | Priority number |
| 180118H | 12~8 | N1CCRT4~N1CCRT0 | Color Calculation Ratio |
| 180110H | 1 | N1COEN | Color offset enable |
| 180112H | 1 | N1COSL | Color offset select |

| TVSTAT | ~ | ~ | ~ | ~ | ~ | ~ | EXLTFG | EXSYFG |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180004H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | ~ | ~ | ~ | ~ | VBLANK | HBLANK | ODD | PAL |



<!-- Page 40 -->

- External latch flag (EXLTFG), bit 9
Through external signals, this displays whether the HV counter value is latched to the HV counter register. Clears to 0 when the screen status register reads out.

- External SYNC flag (EXSYFG), bit 8
Displays whether the internal routes through External SYNC flag are in sync. Clears to 0 when the screen status register reads out.

- Vertical blank flag (VBLANK), bit 3
Displays the vertical scan status of the TV screen.

- Horizontal blank flag (HBLANK), bit 2
Displays the horizontal scan status of the TV screen.

| EXLTFG | HV Counter Value Status |
| --- | --- |
| 0 | Not latched in register |
| 1 | Latched in register |

| EXSYFG | External Sync Status |
| --- | --- |
| 0 | Not synchronized |
| 1 | Internal circuit synchronized |

| VBLANK | Vertical Scan Status |
| --- | --- |
| 0 | During vertical scan |
| 1 | During vertical re-trace (VBLANK) |

| HBLANK | Horizontal Scan Status |
| --- | --- |
| 0 | During horizontal scan |
| 1 | During horizontal re-trace (HBLANK) |



<!-- Page 41 -->

### • Scan Field Flag : Odd/even field flag (ODD), bit 1

Scan conditions are shown when the TV screen mode is the interlace mode. The non-interlace mode is always 1.

- TV standard flags : PAL/NTSC flag (PAL), bit 0
Displays TV standards.

### H Counter Register

The H counter register shows the H counter value. This read exclusive 16-bit register is at address 180008H.

- H counter bit (HCT9 to HCT0), bits 9 to 0
Signals controlled through EXLTEN external signal enable register show the latched H counter values. The bit configuration of this bit changes according to the setting of the graphics mode, as seen in

**Table 2.3. For normal graphics of H counter values,**

the HCT0 of the least significant bit is invalid data. For special normal graphics of H counter values, the HCT9 of the most significant bit is invalid data. For the H counter value of special high-resolution graphics, the most significant bit of HCT9 becomes invalid. Because there is no bit for H0, it shows a value of 2 dot units.

| ODD | Display |
| --- | --- |
| 0 | During even field scan |
| 1 | During odd field scan |

| PAL | Display |
| --- | --- |
| 0 | NTSC standard |
| 1 | PAL standard |

| HCNT | ~ | ~ | ~ | ~ | ~ | ~ | HCT9 | HCT8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 180008H | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|   | HCT7 | HCT6 | HCT5 | HCT4 | HCT3 | HCT2 | HCT1 | HCT0 |



<!-- Page 42 -->

**Table 2.3 H counter register bit content**

### V Counter Register

The V counter register shows the V counter value. This read exclusive 16-bit register is at address 18000AH.

- V counter value bit : V counter bit (VCT9~VCT0), bit 9 to 0
