# Chapter 14 Shadow Function

Introduction ....................................................................256 14.1 Shadow Process ..................................................256 Normal Shadow ................................................256 MSB Shadow ....................................................258 Shadow Control Register ..................................259 ST-58-R2



<!-- Page 274 -->

## Introduction

This function projects a shadow on a sprite or scroll screen by using a sprite. There are two types of sprite shadows: the Normal shadow and the MSB shadow. The MSB shadow is used when the sprite is type 2 through 7, and is used when the sprite shadow priority is highest. The shadow function is processed after the color calculation and color offset functions. The shadow function is shown in Figure 14.1. Frame Buffer Data Scroll Screen Output Image Transparent + = Shadow Sprite Normal Sprite

**Figure 14.1 Shadow Function**

## 14.1 Shadow Process

When the sprite priority of the Normal shadow or MSB shadow is highest, the shadow process makes the sprite transparent and divides in half the brightness of the part of the sprite in the top image.

### Normal Shadow

With Normal shadow sprite, the number of bits to be determined by the sprite type changes with dot color data of the least significant bit in the designated sprite type at 0, and all other dot color data at 1. Sprite data of a Normal shadow designates the color control word such that all bits of dot color data remaining from the dot color code are 1, with only the least significant bit at 0. Also, sprite data of a Normal shadow is created by writing sprite characters of the dot color code whose remaining bits are 1 to the frame buffer. In Figure 14.1, a shadow cannot be projected because it has already been directed to the frame buffer. As a result, write to the frame buffer first. For more about color control word, see “VDP1 User’s Manual.” The scroll screen and back screen, which project a shadow by the Normal shadow sprite, can designate in all screens.



<!-- Page 275 -->

A Normal shadow is shown in Figure 14.2 and sprite data of a Normal shadow is shown in Figure 14.3. Normal Shadow Sprite Normal Shadow Transparent Transparent Frame Buffer Before Write Frame Buffer After Write

**Figure 14.2**

Sprite data write of a Normal shadow

- When Sprite Type 0~3, 5
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 1 1 1 1 1 1 1 1 1 0

- When Sprite Type 4, 6
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 1 1 1 1 1 1 1 1 0

- When Sprite Type 7
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 1 1 1 1 1 1 1 0

- When Sprite Type C~F
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 1 1 1 1 1 1 0

- When Sprite Type 8
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 1 1 1 1 1 0

- When Sprite Type 9~B
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 1 1 1 1 0

> Note: The bits in the shaded areas are used in judging the normal shadow.

**Figure 14.3 Sprite data of a Normal shadow**

ST-58-R2



<!-- Page 276 -->

### MSB Shadow

MSB shadow is enabled only when sprite types are type 2 through 7, with sprite data MSB at 1. Depending on the value of 15 bits other than MSB, there are two types of shadow: sprite shadow and the transparent shadow. The sprite shadow MSB is 1 and all the values of 15 bits, other than the MSB, not 0. The transparent shadow MSB is 1, with all the values of 15 bits, other than the MSB, at 0. When dot color data satisfies Normal shadow conditions, it is judged to be a Normal shadow even if sprite shadow conditions are satisfied. MSB shadow sprite data is created by changing only MSB to 1 in the form of an MSB shadow sprite for frame buffer data that the VDP1 has already written to. (See “MSB On” in the “VDP1 User’s Manual.”) A shadow is added to the sprite character when all frame buffer data bits before the MSB changes are not 0; i.e., when a normal sprite that has already been written becomes a sprite shadow. A shadow is added for a scroll screen priority that is one less than the sprite of the transparent shadow when all frame buffer data bits before the MSB is changed are 0; i.e., a transparent shadow will result when transparent. Scroll screen and back screen that add shadows by sprites of the transparent shadow sprites can be selected on each screen. The MSB shadow can not be used when using the sprite window. For more details about the sprite window, see “8.1 Window Area.” The sprite shadow and transparent shadow are shown in Figure 14.4. Sprite data of the MSB shadow is shown in

**Figure 14.5.**

MSB Shadow Sprite Sprite Shadow Transparent Shadow Transparent Transparent Frame Buffer before changing MSB Frame Buffer after changing MSB

**Figure 14.4 Sprite shadow and transparent shadow**



<!-- Page 277 -->

Sprite Shadow Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 X X X X X X X X X X X X X X X The Xed bits could be either 0 or 1 as long as the dot color data in the selected sprite type does not meet the conditions of Normal Shadow. Transparent Shadow Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0

**Figure 14.5 Sprite data of MSB shadow**

### Shadow Control Register

The shadow control register is a write-only 16-bit register that designates whether to use the shadow function for the scroll screen and back screen, and is located at address 1800E2H. Because the value is cleared to 0 after power on or reset, the value must be set. 15 14 13 12 11 10 9 8 SDCTL ~ ~ ~ ~ ~ ~ ~ TPSDSL 1800E2H 7 6 5 4 3 2 1 0 ~ ~ BKSDEN R0SDEN N3SDEN N2SDEN N1SDEN N0SDEN Shadow enable bit (N0SDEN, N1SDEN, N2SDEN, N3SDEN, R0SDEN, BKSDEN) This bit determines in sprites of the Normal shadow and transparent shadow whether to use the shadow function for the scroll screen and back screen. N0SDEN 1800E2H Bit 0 For NBG0 (or RBG1) N1SDEN 1800E2H Bit 1 For NBG1 (or EXBG) N2SDEN 1800E2H Bit 2 For NBG2 N3SDEN 1800E2H Bit 3 For NBG3 R0SDEN 1800E2H Bit 4 For RBG0 BKSDEN 1800E2H Bit 5 For Back The sprite of a sprite shadow always uses the shadow function for itself. ST-58-R2



<!-- Page 278 -->

xxSDEN Process 0 Does not use shadow function (shadow not added) 1 Uses shadow function (shadow added)

> Note: N0, N1, N2, N3, R0, or BK is entered in bit name for xx.

Transparent shadow select bit (TPSDSL), bit 8 Determines whether to activate the sprite of the transparent shadow. TPSDSL Process 0 Disables transparent shadow sprite 1 Enables transparent shadow sprite When the sprite of a transparent shadow is nullified, a shadow will no longer be projected on a screen by the transparent shadow sprite.



<!-- Page 279 -->

