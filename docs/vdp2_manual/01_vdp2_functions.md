
Introduction .......................................................... 2 1.1 System Configuration .................................. 2 1.2 Address Map ............................................... 3 VRAM .................................................... 3 Color RAM ............................................. 3 Register ................................................. 4 1.3 Scroll Function ............................................ 5 Display Screen ...................................... 5 Scroll Screen ......................................... 6 Line Screen .......................................... 7 Windows .......................................... 7 1.4 Priority Function .......................................... 8 Priority Function .................................... 8 Color Calculation Function .................... 8 Color Offset Function ............................ 8 Shadow Function................................... 9 ST-58-R2



<!-- Page 20 -->

## Introduction

VDP2 has a scroll and priority function. The scroll function defines the scroll screen, moves the screen up, down, right, left, and rotates the screen. The priority function prioritizes the display of multiple scroll screens, sprites, and external screens. It also processes the images in operations such as color calculation and color offset.

## 1.1 System Configuration

VDP2 is connected to 4 Mbit or 8 Mbit VRAM and contains 32K bits of color RAM. Image data is defined in the VRAM and color RAM from the CPU via the SCU. Image display controlling information is set by each register in the same way. Data defined by VRAM is read according to the setting of the register, then becomes the image data of each scroll screen. Image data of each scroll screen and sprite image data received from VDP1, as well as the external image data received from outside, become image display data. Display priority is decided by the register setting. When display image data is in a palette format, color data defined in the color RAM according to that value is read and displayed. When display image data is in the RGB format, it is shown as is. In this way, the acquired display color data is output to the display device. The VDP2 system configuration is shown in Figure 1.1. External Screen VDP1 Circuitry (OPTION) VDP2 Register Display Device CPU SCU Color RAM VRAM

**Figure 1.1 System Configuration**



<!-- Page 21 -->

## 1.2 Address Map

In order to define pattern name tables and character pattern data, VDP2 is connected to two VRAMs. VDP2 contains 32K bits of color RAM for defining color data, and together with internal registers control VRAM. Figure 1.2 shows VDP2 controlled VRAM, color RAM, and register address maps.

**Figure 1.2 Address Map**

### VRAM

VRAM stores scroll screen image data and data tables needed in each function. Read access by VDP2 is always given priority over read/write access through the CPU or DMA controller. Consequently, the wait cycle enters the CPU or DMA controller through the access timing. Access through the CPU or DMA controller is possible in units of byte, word, and long word.

### Color RAM

Color RAM stores color data of sprites and scroll screens. It also defines the enable bit of the color calculation function as it applies to the most significant bit when necessary. Read/write access from the CPU or DMA controller is possible, but the image may be disturbed by the access timing. Access through the CPU or DMA controller is possible only in word units and long word units. Access in bytes is not allowed.

| VRAM |
| --- |
| COLOR-RAM |
| REGISTER |



<!-- Page 22 -->

### Register

Registers set each VDP2 function. Because the values of most registers are cleared to 0 after power on or reset, the values must be set. Read/write access from the CPU or DMA controller is always possible, but the image may be poor due to the access timing. Access by the CPU or DMA controller is possible only in word units and long word units. Access in bytes is not allowed.



<!-- Page 23 -->

## 1.3 Scroll Function

The VDP2 scroll function has a scroll screen and a window.

### Display Screen

The TV screen mode has the following characteristics.

**Table 1.1 TV Screen Mode**

The scroll screen which can be displayed has the following characteristics.

**Table 1.2 Scroll Screen**

| TV Screen<br>Mode | Graphic Mode | Horizontal<br>Resolution<br>(Pixels) | Vertical<br>Resolution<br>(Pixels) | Display Device |
| --- | --- | --- | --- | --- |
| Normal | Normal<br>Graphic A | 320 | 224 | NTSC Format |
|   | Normal<br>Graphic B | 352 | 240 | or |
| Hi-Res | Hi-Res<br>Graphic A | 640 | 256 | PAL Format |
|   | Hi-Res<br>Graphic B | 704 | selection | TV |
|   | Exclusive Normal<br>Graphic A | 320 | 480 | 31kHz Monitor |
| Exclusive | Exclusive Norma l<br>Graphic B | 352 | 480 | Hi-Vision Monitor |
| Monitor | Exclusive Hi-Res<br>Graphic A | 640 | 480 | 31kHz Monitor |
|   | Exclusive Hi-Res<br>Graphic B | 704 | 480 | Hi-Vision Monitor |

| Scroll Screen Name |   | Name | Remarks |
| --- | --- | --- | --- |
|   | Normal Scroll 0 | NBG0 | Can move up/down/left |
| Normal | Normal Scroll 1 | NBG1 | right. Can scale |
| Scroll Screen | Normal Scroll 2 | NBG2 | Can move up/down/left |
|   | Normal Scroll 3 | NBG3 | right. |
| Rotation | Rotation Scroll 0 | RBG0 | Can scale/rotate |
| Scroll Screen | Rotation Scroll 1 | RBG1 |   |
| Line | Line Color Screen | LNCL | Used only in color<br>calculations |
| Screen | Back Screen | BACK | Displayed only when<br>other screens are not<br>displayed |
| Expandable Screen | External Input Screen | EXBG | Screen input externally |



<!-- Page 24 -->

The following windows exist:

**Table 1.3 Windows**

### Scroll Screen

The functions of the scroll screen are listed in the table below.

**Table 1.4 Scroll Screen Function**

> Note:

Normal scroll screen changes the number of screens that can be displayed through each setting.

| Window Name | Name | Remarks |
| --- | --- | --- |
| Normal Window | W0 | Line Window allowed |
|   | W1 |   |
| Sprite Window | SW | Sprite Character Window |

| Function | Normal Scroll Screen |   |   |   | Rotation Scroll Screen |   |
| --- | --- | --- | --- | --- | --- | --- |
|   | NBG0 | NBG1 | NBG2 | NBG3 | RBG0 | RBG1 |
| Character<br>Color Count | 16 colors<br>256 colors<br>2048 colors<br>32,768 colors<br>16,770,000 colors<br>selection | 16 colors<br>256 colors<br>2048 colors<br>32,768 colors<br>selection | 16 colors<br>256 colors<br>selection | 16 colors<br>256 colors<br>selection | 16 colors<br>256 colors<br>2048 colors<br>32,768 colors<br>16,770,000 colors<br>selection | 16 colors<br>256 colors<br>2048 colors<br>32,768 colors<br>16,770,000 colors<br>selection |
| Character<br>Size | 1 Cell H x 1 Cell V; 2 Cells H x 2 Cells V |   |   |   |   |   |
| Pattern Name<br>Data Size | 1 Word, 2 Words selection |   |   |   |   |   |
| Plane Size | 1 H x 1 V 1 Pages; 2 H x 1 V 1 Pages; 2 H x 2 V Pages |   |   |   |   |   |
| Plane Count | 4 | 4 | 4 | 4 | 16 | 16 |
| Bitmap<br>Display | Display<br>Allowed | Display<br>Allowed | Display Not<br>Allowed | Display Not<br>Allowed | Display<br>Allowed | Display Not<br>Allowed |
| Bitmap Size | 512 H x 256 V Dots<br>512 H x 512 V Dots<br>1024 H x 256 V Dots<br>1024 H x 512 V Dots<br>selection |   | None |   | 512 H X 256 V<br>Dots<br>512 H X 512 V<br>selection | None |
| Scale<br>Function | 1/4~256 ratio |   | None |   | Any Ratio |   |
| Rotation<br>Function | None |   |   |   | Yes |   |
| Line Scroll<br>Function | Yes | Yes | No | No | No |   |
| Vertical Cell<br>Scroll Function | Yes | Yes | No | No | No |   |
| Mosaic<br>Process<br>Function | Yes |   |   |   | Yes (Horizontal Direction Only) |   |



<!-- Page 25 -->

The normal scroll screen can be displayed simultaneously with one rotation scroll screen. If two rotation scroll screens are displayed, the normal scroll screen cannot be displayed (the register that sets RBG1 is used for NBG0). When an external input screen is displayed, NBG1 cannot be displayed. The register setting the external input screen can be used for NBG1.

### Line Screen

The line color screen works for color calculation and on other screens. It can indicate whether the entire screen consists of one color, or if there is a color for each line, but it cannot display characters. The back screen is displayed when all other screens are transparent. The entire screen is displayed in one color, or a color can be selected for each line, but characters cannot be displayed.

### Windows

A rectangular window can be selected by using the two screen coordinate value points in the upper left and lower right corners of a normal window. The sprite window is a window based on sprite characters. There are three types of windows that can be used and stacked individually for each screen: the “transparent control window” designates the transparent area; the “color calculation window” designates the area in which color calculation is not performed; the “rotation parameter window” changes screens by two rotation parameters. ST-58-R2



<!-- Page 26 -->

## 1.4 Priority Function

There are four types of VDP2 priority functions: priority function, color calculation function, color offset function, and shadow function.

### Priority Function

The display priority of the sprite and scroll screen is decided by a 3-bit priority number. The sprite priority number can be set at a maximum value of 8, one of which is designated by character units. The scroll screen priority number is usually designated by surface units. When the special priority function is used, character units and dot units can change the scroll screen priority number.

### Color Calculation Function

By adding color data of multiple screens, the color calculation function produces an effect in which the back screen can be seen through the front screen. It is normally performed by two screen images, the top image and the second image, but up to four screens can be peformed when the expanded color calculation function is used. Surface units determine whether the color calculation is performed. Sprites can be selected by character units through sprite color calculation condition settings. When a scroll screen uses the special color calculation function, sprites can be selected by character units and dot units. The color calculation ratio of the top and second images can be selected from 32 steps. Sprites can set a maximum of 8 color calculation ratios, among which one can be selected by character units. The scroll screen is selected by surface units. When the Gradation function is used, one selected screen can be gradated horizontally and displayed.

### Color Offset Function

The color offset function is used for displaying the offset value calculation (subtraction) for color data, and for fade in and fade out purposes. The color offset function can be specified by surface unit. Up to two color offset values can be selected for each RGB, one of which can be specified by surface units.



<!-- Page 27 -->

### Shadow Function

The shadow function adds shadow to the shapes of sprite characters on each screen. There are two types of sprite shadow: normal shadow by data, and MSB shadow. The normal shadow can only add a shadow to the scroll screen. The MSB shadow can add a shadow to scroll screens and to sprites. ST-58-R2



<!-- Page 28 -->

