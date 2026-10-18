# VDP1 User's Manual Supplement

VDP1 User’s Manual Supplement — SEGA ST-013-SP1-052794, May 27, 1994.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=2)

Physical PDF pages 2–14. Text extraction; consult the PDF for figures.

## Bookmarks

- [Table of Contents](#pdf-page-6)
- [Introduction](#pdf-page-7)
- [1.0 Differences in Functions](#pdf-page-8)
- [1.1 Functions of Version 0](#pdf-page-8)
- [1.2 Functions Added to Version 1](#pdf-page-9)
- [2.0 Using Version 0](#pdf-page-11)
- [2.1 High Speed Shrink (HSS)](#pdf-page-11)
- [2.2 Pre-Clipping Disable (Pclp)](#pdf-page-11)
- [3.0 Manual Changes](#pdf-page-12)
- [3.1 Description of Changes](#pdf-page-12)
- [3.2 Detailed Descriptions](#pdf-page-13)

## PDF page 2

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=2)

```text
                TM















                                           VDP1
                                      User's Manual
                                        Supplement

                                        Doc. #ST-13-SP1-052794
           SEGA Confidential
                                         © 1994 SEGA.  All Rights Reserved.
```

## PDF page 3

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=3)

```text
                                   READER CORRECTION/COMMENT SHEET


          Keep us updated!
           If you should come across any incorrect or outdated information while reading through the attached
          document, or come up with any questions or comments, please let us know so that we can make the
          required changes in subsequent revisions.  Simply fill out all information below and return this form to
          the Developer Technical Support Manager at the address below.   Please make more copies of this form if
          more space is needed.  Thank you.

          General Information:
          Your Name                                                                                                              Phone

          Document number                                                                                                 DateST-13-SP1-052794

          Document name           VDP1 User's Manual Supplement

          Corrections:

           Chpt.    pg. #     Correction























          Questions/comments:






                                                   Where to send your corrections:

                        Fax:       (415) 802-3963                             Mail:     SEGA OF AMERICA
                                  Attn:  Manager,                                       Attn:  Manager,
                                  Developer Technical Support                           Developer Technical Support
                                                                                        275 Shoreline Dr.  Ste 500
                        SEGA ConfidentialRedwood City, CA  94065
```

## PDF page 4

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=4)

```text
                                                REFERENCES



     In translating/creating this document, certain technical words and/or phrases were interpreted
     with the assistance of the technical literature listed below.


     1.     Dictionary of Science and Engineering, 350,000 words, 3rd Edition
          Inter Press
          Tokyo, Japan
          1990

     2.     Computer Dictionary
          Kyoritsu Publishing Co., LTD.
          Tokyo, Japan
          1978

     3.     IBM Dictionary of Computing
          McGraw-Hill, Inc.
          New York, New York
          1994


                    SEGA Confidential
```

## PDF page 5

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=5)

```text
Revision History

       Revision 7.1               December 6, 1993
                               •  Minor corrections
                               •  Total 155 pages

       First Edition              March 20, 1994
                               •  Added even/odd coordinate selection bit (EOS) to
                                  frame buffer change mode register (FBCR)
                               •  Added high speed shrink (HSS) and pre-clipping
                                  disable (Pclp) to draw (paint) mode word (+04H) in
                                  the command table.
                               •  Total 163 pages

       Supplement              March 31, 1994
                               •  Explained functions for version 0 of the VDP1 device.
                               •  The following functions added to version 1 are not
                                  included in version 0:
                                  >The frame buffer change mode register (FBCR) does
                                  not have an even/odd coordinate selection bit (EOS)
                                  function.
                                  >The draw (paint) mode word (+04H) in the command
                                  table does not high speed shrink (HSS) and pre-clip-
                                  ping disable (Pclp) functions.
                               •  Total 7 pages.


              SEGA Confidential
```

## PDF page 6

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=6)

```text
Table of Contents


Introduction ........................................................................................................................i
              Confirmation of Version Number ......................................................................... i

1.0    Differences in Functions ...........................................................................................1
              1.1   Functions of Version 0 ...............................................................................1
                             Even/Odd Coordinate Selection Bit (EOS) .......................................... 1
                             High Speed Shrink (HSS) .................................................................... 1
                             Pre-Clipping Disable (Pclp) ................................................................. 1
              1.2    Functions Added to Version 1....................................................................2
                             Even/Odd Coordinate Selection Bit (EOS) .......................................... 2
                             High Speed Shrink (HSS) .................................................................... 2
                             Pre-Clipping Disable (Pclp) ................................................................. 3

2.0    Using Version 0 ........................................................................................................4
              2.1    High Speed Shrink (HSS).......................................................................... 4
              2.2    Pre-Clipping Disable (Pclp) .......................................................................4

3.0    Manual Changes ......................................................................................................5
              3.1    Description of Changes .............................................................................5
                             Version Number (VER) ........................................................................5
                             Even/Odd Coordinate Selection Bit (EOS) .......................................... 5
                             High Speed Shrink (HSS) .................................................................... 5
                             Pre-Clipping Disable (Pclp) ................................................................. 5

              3.2    Detailed Descriptions ................................................................................6


                       SEGA Confidential
```

## PDF page 7

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=7)

```text
Introduction

This manual contains additional explanations of version 0 of the VDP1 device.  It
explains the functions of version 0 while noting the differences between the func-
tions of version 1 and the functions of version 0 of the VDP1 device. Refer to VDP1
User’s Manual, 1st edition, regarding version 1.
This supplement is valid only for version 0 of the VDP1 device.  Refer to the appro-
priate manual regarding VDP1 devices other than version 0.

Confirmation of Version Number

First, confirm the version number. Use the method described below to confirm the
version number.

Version Number
The version number (VER, version number) is handled by bits 15 to 12 of the mode
register (MODR, 100016H), thus the version number of the VDP1 device is indicated
by four bits. Bits 15 to 12 indicate the version number when the mode register is read
out. When the value is 0 (0000B), the version number of the VDP1 device is 0.



               SEGA Confidential

                                                  i
```

## PDF page 8

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=8)

```text
1.0    Differences in Functions


1.1   Functions of Version 0
The differences in the functions of version 0 from the functions of version 1 are ex-
plained below. These functions are not included in the version 0 VDP1 device. Refer
to the VDP1 User’s Manual, 1st edition (version 1).

Even/Odd Coordinate Select Bit (EOS): bit 4
In version 1, an even/odd coordinate selection bit (EOS) was added at bit 4 of the
frame buffer change mode register (FBCR, 100002H). However, in version 0, this bit
does not have that function.

When writing a value to the frame buffer change mode register (FBCR) in version 0,
set bit 4 to “0”. Version 0 does not have the bit for high speed shrink (HSS) in the
draw (paint) mode word (CMDPMOD) or that function.

High Speed Shrink (HSS)
In version 1, high speed shrink (HSS) is added at bit 12 of the draw (paint) mode
word (CMDPMOD, +04H). However, version 0 does not have this function at this bit.
When drawing (painting) a scaled sprite or distorted sprite in version 0, set bit 12 of
the draw (paint) mode word (CMDPMOD) in the command table to “0”.

Pre-Clipping Disable (Pclp)
In version 1, pre-clipping disable (Pclp) was added at bit 11 of the draw (paint) mode
word (CMDPMOD, +04H). However, version 0 does not have this function at this bit.
When a draw (paint) command is used in version 0, set bit 11 of the draw (paint)
mode word (CMDPMOD) in the command table to “0”.

   SEGA Confidential


 VDP 1 User's Manual Supplement                                                                                                                1
```

## PDF page 9

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=9)

```text
     1.2   Functions Added to Version 1
     For reference, the functions of version 1 which were added after version 0 are ex-
     plained below. These functions are not included in version 0 VDP1 devices. The
     newly added functions are as follows. For more information, refer to each item.

     Even/Odd Coordinate Select Bit (EOS): bit 4
     An even/odd coordinate selection bit (EOS) was added at bit 4 of the frame buffer
     change mode register (FBCR, 100002H).
     This bit becomes valid when “1” is specified for high speed shrink in drawing
     (painting) a scaled sprite or a distorted sprite. When HSS = 1 is specified, lines with
     respect to which the drawing is scaled down are drawn by sampling only pixels at
     even or odd coordinates in the original picture data. The sampling of pixels at even
     or odd coordinates is specified by this bit.
     If EOS = 0, then pixels at even coordinates are sampled, and if EOS = 1, then pixels at
     odd coordinates are sampled.
     When HSS = 0, this bit is not referenced.

     High Speed Shrink (HSS)
     High speed shrink was added at bit 12 of the draw (paint) mode word (CMDPMOD,
     +04H).
     When scaling down and drawing a scaled sprite or a distorted sprite, this bit is used
     to select whether precedence is given to speed or to accuracy.  This is valid only for
     the drawing (painting) commands of scaled sprites and distorted sprites. Leave it
     fixed at “0” for all other drawing commands.
     When “1” is specified for this bit, lines with respect to which the drawing is scaled
     down are drawn by sampling only the even or odd pixels of the original picture
     data. The selection of even coordinates or odd coordinates is done with the even/
     odd coordinate selection bit (EOS) of the frame buffer change mode register (FBCR).
     When “0” is specified, the original picture data are sampled irrespective of even or
     odd coordinates at any magnification ratio.  Use “0” to give precedence to accuracy
     over speed, and use “1” to give precedence to speed.
                     SEGA Confidential

       2
```

## PDF page 10

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=10)

```text
Pre-Clipping Disable (Pclp)
Pre-clipping disable (Pclp) has been added at bit 11 of the draw (paint) mode word
(CMDPMOD, 04H).  This bit specifies whether pre-clipping is performed or dis-
abled. When “0” is specified, pre-clipping is performed. When “1” is specified, pre-
clipping is not performed.
One drawing (painting) command comprises a group of several lines, and the re-
spective lines comprise a number of dots. Each dot is drawn (painted) based on
clipping area (drawing [painting] area) information specified by the CPU.
A line that is completely removed from the drawing (painting) area—and an entire 1
line drawing (painting) is not required—can be detected in advance, and drawing
(painting) efficiency can be increased by not allowing the drawing (painting) to be
started. Also, when one end of 1 line is outside the drawing (painting) area, effi-
ciency can be improved by starting drawing (painting) from inside the drawing
(painting) area (that line is limited to vertical and horizontal directions).
The VDP1 normally performs this detection, but in the case of small elements whose
points are in the (A)—(B) or (D)—(C) direction, the overhead required for that detec-
tion (up to five CPU clock cycles for one line) becomes conspicuous and can lower
the drawing (painting) efficiency.
In the case of large elements that extend greatly out of the drawing (painting) area, it
is more efficient to perform pre-clipping.  This bit is only valid for drawing (paint-
ing) commands.  Fix it to “0” for other commands.


   SEGA Confidential


VDP 1 User's Manual Supplement                                                                                                                3
```

## PDF page 11

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=11)

```text
     2.0     Using Version 0

     Use version 0 as described below.

     2.1   High Speed Shrink (HSS)
     When drawing (painting) scaled sprites and distorted sprites reduced in version 1, it
     is possible to specify high speed shrink in order to give precedence to speed.
     Version 0 does not have this function. In order to give precedence to speed when
     scaled sprites and distorted sprites are scaled down and drawn (painted), a method
     other than one that scales down and draws (paints) scaled sprites and distorted
     sprites is required.
     For example, a method that draws (paints) using scaled sprites or that registers
     reduced scaled sprites or distorted sprites and draws (paints) them without reduc-
     tion must be used.  If the drawing (painting) speed is slow in version 0, confirm
     whether or not a scaled sprite or distorted sprite is being scaled down and drawn.

     2.2   Pre-Clipping Disable (Pclp)
     When pre-clipping is not required in version 1, drawing (painting) speed can be
     raised by specifying pre-clipping disable.
     Version 0 does not have this function. When drawing (painting) small horizontal or
     vertical lines, it is necessary to employ a method that reduces the overhead required
     to detect whether drawing (painting) with pre-clipping is necessary.
     For example, measures must be taken by making several small elements large when
     drawing (painting) them or exchanging left and right for up and down and increas-
     ing the size when drawing (painting) them.  If the drawing (painting) speed is slow
     in version 0, confirm whether or not many small elements are being drawn
     (painted).

                     SEGA Confidential

       4
```

## PDF page 12

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=12)

```text
3.0    Manual Changes

The manual for version 1 can be changed into the manual for version 0 by making
the following changes.

3.1   Description of Changes
The items changed include explanations of the three bits added and their functions.
The manual can be used for version 0 by deleting the explanations of these func-
tions.

Version Number (VER)
The value of the version number (VER, bits 15-12) of the mode register (MODR,
100016H) is “0” (0000B).

Even/Odd Coordinate Selection Bit (EOS)
Information regarding the even/odd coordinate selection bit (EOS, bit 4) of the
frame buffer change mode register (FBCR, 100002H) is deleted. Bit 4 of FBCR is set
to “0”.  Information regarding the even/odd coordinate selection bit (EOS, bit 7) of
the mode  register (MODR, 100016H) is also deleted. Bit 7 of MODR becomes unde-
fined.

High Speed Shrink (HSS)
Information regarding high speed shrink (HSS, bit 12) of the draw (paint) mode
word (CMDPMOD, +04) is deleted.  Bit 12 of CMDPMOD is set to “0” for scaled
sprites and distorted sprites.  Information regarding HSS = 1 in the explanation of
end code disable is deleted.  The information regarding HSS = 0 remains valid.

Pre-Clipping Disable (Pclp)
Information regarding pre-clipping disable (Pclp, bit 11) of the draw (paint) mode
word (CMDPMOD, +04) is deleted. Bit 11 of FBCR is set to “0” in the draw (paint)
command table.

   SEGA Confidential


 VDP 1 User's Manual Supplement                                                                                                                5
```

## PDF page 13

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=13)

```text
       3.2   Detailed Descriptions
       The items changed for version 0 and their descriptions are given in the table below.
       V0 in the table refers to version 0 of the VDP1 device.

       Table 3.1  Changes and Their Descriptions
         Page         Line                                 Item                                                                 Description
                                  (Chapter 4  System Registers)
           35            11         System Registers                              Delete change timing of even/odd coordinate selection bit
                                                                               (EOS).
                                                                               This bit is not defined in version 0.
           38            Top      Frame buffer change mode                     Delete even/odd coordinate selection bit (EOS).
                      figure      register                                     Set bit 4 to "0".
                                                                               This bit is not defined in V0.
           44            —         Even/odd coordinate selection bit     Delete explanation of function of even/odd coordinate
                                                                               selection bit (EOS).
                                                                               Set bit to "0".
                                                                               This bit is not defined in V0.
           57         Figure      Mode register                                    Delete even/odd coordinate selection bit (EOS).
                                                                               Bit 7 is undefined.
                                                                               This bit is not defined in V0.
                        11         Version number                                The value of bits 15-12 is "0" (0000B).
                        15         Even/odd coordinate selection bit     Delete even/odd coordinate selection bit (EOS).
                                  (Chapter 5  Tables)
           66         Figure      Command table                                Delete HSS (high speed shrink) from bit 12 and Pclp (pre-
                       5.5                                                     clipping) from bit 11 of the draw (paint) mode word
                                                                               (CMDPMOD, +04H).
                                                                               Set both bits 11 and 12 to "0".
                                                                               These bits are not defined in V0.
                                  (Chapter 6  Command Tables)
           79         Figure      CMDPMOD                                       Delete HSS (high speed shrink) from bit 12 and Pclp (pre-
                         7                                                     clipping disable) from bit 11.
                        10                                                     Set both bits 12 and 11 to "0".
                                                                               These bits are not defined in V0.
           81           —         High speed shrink                             Delete the explanation of the function of high speed shrink
           82           —                                                      (HSS).
                                                                               Set bit 12 to "0".
                                                                               This bit is not defined in V0.
           83            —         Pre-clipping disable                          Delete the explanation of the function of pre-clipping
                                                                               (Pclp).
                                                                               Set bit 11 to "0".
                                                                               This bit is not defined in V0.
           86            Top      End code disable                              Delete end code processing when HSS is "1".
                      table                                                    HSS is not defined in V0.
                                                                               Set HSS to "0".


                                                                                                                          (Continued on next page.)
                              SEGA Confidential

          6
```

## PDF page 14

[Original page](https://antime.kapsi.fi/sega/files/ST-013-SP1-052794.pdf#page=14)

```text
Table 3.1  Changes and Their Descriptions (continued)
  Page         Line                                 Item                                                                 Description
                          (Chapter 7  Commands)
   118         Figure     Normal sprite draw (paint)                    Delete Pclp from CMDPMOD.
                17        command                                       Delete Pclp definition.
                                                                        Set bit 11 of CMDPMOD to "0".
   120        Figure      Scaled sprite draw (paint)                    Delete HSS and Pclp from CMDPMOD.
   121           5        command                                       Delete definition of HSS and Pclp.
                          (Specification of two coordinate              Set bits 12 and 11 of CMDPMOD to "0".
                          points)
   122         Figure     Scaled sprite draw (paint)
                16        command
                          (Specification of zoom point)
   124         Figure     Distorted sprite draw (paint)
                19        command
   126         Figure     Polygon draw (paint) command        Delete Pclp from CMDPMOD.
                13                                                      Delete definition of Pclp.
                                                                        Set bit 11 of CMDPMOD to "0".
   128         Figure     Polyline draw (paint) command
                13
   130         Figure     Line draw (paint) command
                13
                          (Chapter 8  Quick Reference)
   135        Figure      Frame buffer change mode                      Delete even/odd coordinate selection bit (EOS).
                          register                                      Set bit 4 of MODR to "0".
              Bottom      Even/odd coordinate selection bit     Delete even/odd coordinate selection bit (EOS).
               table                                                    Set bit 4 of MODR to "0".
   139        Figure      Mode register                                    Delete even/odd coordinate selection bit (EOS).
                                                                        Bit 7 of MODR is undefined.
                 4          Even/odd coordinate selection bit     Delete even/odd coordinate selection bit (EOS).
   142        Figure      Command table                                Delete HSS and Pclp from CMDPMOD.
                                                                        Set both bits 11 and 12 of CMDPMOD to "0".
   146        Figure      Draw (paint) mode word                    Delete HSS (high speed shrink) from bit 12 and Pclp (pre-
                                                                        clipping disable) from bit 11.
                                                                        Set both bits 12 and 11 to "0".
                 2          High speed shrink                             Delete high speed shrink (HSS).
                                                                        Set bit 12 of CMDPMOD to "0".
                 3          Pre-clipping disable                          Delete pre-clipping (Pclp).
                                                                        Set bit 11 of CMDPMOD to "0".
   147          Top       End code disable                              Delete end code processing when HSS is "1".
               table                                                    HSS is not defined in V0.
                                                                        Set HSS to "0".
   152        Bottom      Normal sprite draw (paint)                    Delete Pclp from CMDPMOD.
               table      command                                       Set bit 11 of CMDPMOD to "0".
   153        Figure      Scaled sprite draw (paint)                    Delete HSS and Pclp from CMDPMOD.
                          command
   154          Top       Distorted sprite draw (paint)                 Set bits 12 and 11 of CMDPMOD to "0".
               figure     command
              Bottom      Polygon draw (paint) command        Delete Pclp from CMDPMOD.
               figure                                                   Set bit 11 of CMDPMOD to "0".
   155          Top       Polyline draw (paint) command
               figure
              Bottom      Line draw (paint) command
               figure
                          (Index)
   161       right 12     Index                                                 Delete "even/odd coordinate selection."
             right 33                                                   Delete "high speed shrink."
   162       right 42                                                   Delete "pre-clipping".
     SEGA Confidential


 VDP 1 User's Manual Supplement                                                                                                                7
```
