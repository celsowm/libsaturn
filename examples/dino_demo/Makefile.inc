# dino_demo animated T-Rex viewer.
#
# T-Rex source: examples/dino_demo/assets/trex_walking_seamless.glb.
# The GLB and its compiled model are attributed in assets/LICENSE.txt.
MODEL_GLB                := examples/$(EXAMPLE)/assets/trex_walking_seamless.glb
MODEL_OUT_PREFIX         := $(GENERATED_DIR)/$(EXAMPLE)/trex_model
MODEL_SYMBOL             := trex
MODEL_SCALE              := 1.0
MODEL_TEXTURE_SCALE      := 1.0
MODEL_PALETTE_INDEX      := 1
MODEL_MAX_TEXTURE_WIDTH  := 504
MODEL_MAX_TEXTURE_HEIGHT := 255
MODEL_SIMPLIFY           := 2400
MODEL_QUALITY            := balanced
MODEL_ANIMATION          := all
MODEL_ANIMATION_FPS      := 15
MODEL_FACE_COLORS        := off
MODEL_LIGHT_DIR          := -0.5,0.6,0.8
MODEL_AMBIENT            := 0.30
MODEL_DIFFUSE            := 0.85
MODEL_MAX_POSE_STREAM_BYTES := 786432
MODEL_HUD_RESERVE        := 64
# Adjacent triangle pairs draw as one VDP1 quad: fewer commands, and no
# texels spent on a collapsed triangle corner, which pays for the higher
# texture scale above.
MODEL_MERGE_QUADS        := on
MODEL_QUAD_MAX_FOLD_DEG  := 40
# The 46 solid-colour teeth and claws hold a third of the source triangles
# but cover a few pixels each; spend the budget on the textured body.
MODEL_MATERIAL_WEIGHTS   := 01_-_Default=0.02
# 4-bit texels with a 15-color lookup table per face: half the VRAM of one
# shared 256-color bank, and each face's colors fit its own patch.
MODEL_TEXTURE_FORMAT     := lut4
# UV seams split vertices; the Saturn only needs one per moving point, which
# halves pose data, per-frame decode and projection.
MODEL_WELD_VERTICES      := on
# main.c splits the faces between the two SH-2s; ordering faces along the
# body lets each CPU project only its half of the vertices.
MODEL_LOCALITY_ORDER     := on
# The VDP1 samples texels nearest-neighbour with no mipmapping, so a face
# baked with more texels than it covers on screen turns the skin into speckle
# noise. Cap each face at its size when the T-Rex spans ~400 pixels (default
# view ~260, max zoom ~570) and box-filter the source down to that.
MODEL_SAMPLING           := area
MODEL_TEXEL_EXTENT       := 400
# DINO_HIRES=1: 640x224 hi-res. The VDP1 framebuffer is then 8 bits/pixel,
# so lookup tables hold codes of one shared palette (0 transparent, 1 the
# HUD's white, 254 the sprite shadow code), and faces bake more texels
# for the doubled horizontal resolution.
DINO_HIRES               ?= 1
ifeq ($(DINO_HIRES),1)
MODEL_LUT_CODES          := 2-253
MODEL_TEXEL_EXTENT       := 480
CFLAGS                   += -DDINO_HIRES=1
endif
EXAMPLE_ASSETS            := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS           := $(MODEL_OUT_PREFIX).h
