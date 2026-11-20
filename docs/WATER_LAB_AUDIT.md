# Water Lab renderer and presentation audit

## VDP1 ownership

The root command at VRAM byte 0 names one of two command-table banks. Each
bank has 64 KiB of commands and 16 KiB of Gouraud tables. Command links and
Gouraud addresses use eight-byte units. The CPU builds in the other bank.
Publishing a root and changing a framebuffer are distinct operations.

In AUTO, the engine publishes a root for recurring field draws and observes
COPR before reclaiming the previous command bank. With ON_SUBMIT, publication
waits for the previous END and settled swap. The new list starts through PTMR,
and the engine requests a manual change after detecting END during VBlank.
The timer/interrupt and library polling paths can both call the swap service.

VDP1 EDSR reports current and previous transfer-end state. The SCU IST draw-
end bit also describes hardware interrupt state. Both can identify a prior
completion during a handoff. The current implementation matches COPR against
the END link of the current bank. The existing public handler executes after
the FBCR request. A probe that needs to finish readback or upload first needs
control upstream of that write, with a single owner for the request and a
separate record of which serial actually became visible.

The framebuffer window at 0x05C80000 addresses the drawing bank. In the
baseline mode that bank is 512×256 at 16 bits per pixel. The active display
covers 320×224. AUTO configures the hardware erase bounds for the active
viewport. ON_SUBMIT normally emits an equivalent viewport-clear polygon.
The auxiliary region therefore needs its own bounded erase, clip state and
guard checks. The probe must read it after END and before the swap.

[VDP1 implementation](../src/hal/vdp1/vdp1.cpp) · [Framebuffer map](sega_saturn_hardware/hard/vdp1/hon/p02_11.md) · [FBCR](sega_saturn_hardware/hard/vdp1/hon/p04_12.md) · [EDSR](sega_saturn_hardware/hard/vdp1/hon/p04_16.md)

## Painter

Captured world draws copy the material into caller-owned scene storage. The
scene uses one sort key per face, with artistic pass followed by back-to-front
depth. Projected faces and faces requiring near/screen clipping have separate
emission paths. The compact opaque RGB entry and its native lowering do not
carry effect flags. Opt-in RGB effects therefore require a general face
record. Indexed texture UVs cannot survive arbitrary clipping in the native
distorted-sprite path. The existing quad split helper keeps one pass and one
material for all pieces.

[Face painter](../src/graphics/3d/scene/faces.cpp) · [Clip and emit](../src/graphics/3d/rendering/indexed.cpp) · [Mesh mode](sega_saturn_hardware/hard/vdp1/hon/p06_33.md)

## VDP2 and transfers

The normal background layer API configures NBG0/NBG1 bitmap mode, byte base,
priority and zoom. Reserve owned VRAM and CRAM separately; test actual cycle
patterns and layer priority with the display active. Two 512×256 RGB16
bitmaps occupy the full 512 KiB. A bitmap switch must track VDP1 visibility
and leave the active copy untouched by an upload or retry.

The 160×112 auxiliary region begins at 0x05C80280. It has 320 useful bytes
per row and a 1024-byte source stride. SCU rejects direct B-bus to B-bus DMA.
Compare CPU row copies against two SCU legs using aligned Work RAM-H. Include
both lists, waits, and any conversion in the measured path. Check assignment
of the back-screen table before declaring all 512 KiB available for paired
RGB16 bitmaps.

[VDP2 layers](../include/saturn/vdp2_layers.h) · [VDP2 allocation](../include/saturn/vdp2_alloc.h) · [Color calculation](sega_saturn_hardware/hard/vdp2/hon/p12_10.md) · [SCU DMA manual](sega_saturn_hardware/hard/scu_/hon/p02_10.md) · [DMA route validation](../src/hal/scu/dma_logic.hpp)

## Reproducing the baseline

Use the example's build and Ymir capture commands. The harness exposes erase
bounds, the displayed framebuffer, VDP2 registers and work-RAM telemetry.
Probe designs for later milestones are specified in the implementation plan.

[Water Lab](../examples/water_lab/README.md) · [Water Lab plan](WATER_LAB_PLAN.md)
