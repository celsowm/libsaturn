# Playback tuning for the recovered PS1 walk; Roar keeps its source timing.
DINO_WALK_SPEED_PERCENT ?= 140
CFLAGS += -DDINO_WALK_SPEED_PERCENT=$(DINO_WALK_SPEED_PERCENT)
DINO_PIPELINE_METRICS ?= 0
CFLAGS += -DDINO_PIPELINE_METRICS=$(DINO_PIPELINE_METRICS)
DINO_PROFILE_HUD ?= 0
CFLAGS += -DDINO_PROFILE_HUD=$(DINO_PROFILE_HUD)
DINO_PROJECTED_FACE_QUEUE ?= 1
CFLAGS += -DDINO_PROJECTED_FACE_QUEUE=$(DINO_PROJECTED_FACE_QUEUE)
DINO_BENCH_PAUSED ?= 0
CFLAGS += -DDINO_BENCH_PAUSED=$(DINO_BENCH_PAUSED)
# 1 = credit line only, no counters or help (for video capture).
DINO_SHOWCASE_HUD ?= 0
CFLAGS += -DDINO_SHOWCASE_HUD=$(DINO_SHOWCASE_HUD)
# Qualified native-memory placement; zero retains each independent Low control.
DINO_HIGH_MATERIAL_PROGRAMS ?= 1
DINO_HIGH_TEXTURE_DESCRIPTORS ?= 1
CFLAGS += -DDINO_HIGH_MATERIAL_PROGRAMS=$(DINO_HIGH_MATERIAL_PROGRAMS) \
    -DDINO_HIGH_TEXTURE_DESCRIPTORS=$(DINO_HIGH_TEXTURE_DESCRIPTORS)
# dino_demo animated T-Rex viewer.
# Keep the loading screen independently switchable for physical ODE boot triage.
DINO_LOADING_SCREEN ?= 1
CFLAGS += -DDINO_LOADING_SCREEN=$(DINO_LOADING_SCREEN)
#
# Original OBJ surface and recovered Walk: tools/bake_ps1_dino_walk.py
# --original-topology. The Blender reconstruction supplies approximate Roar.
# The former reduced mesh remains archived.
# The GLB and its compiled model are attributed in assets/LICENSE.txt.
MODEL_GLB                := examples/$(EXAMPLE)/assets/ps1_trex_saturn_original_walk.glb
MODEL_OUT_PREFIX         := $(GENERATED_DIR)/$(EXAMPLE)/trex_model
MODEL_SYMBOL             := trex
MODEL_SCALE              := 1.0
MODEL_TEXTURE_SCALE      := 1.0
MODEL_PALETTE_INDEX      := 1
MODEL_MAX_TEXTURE_WIDTH  := 504
MODEL_MAX_TEXTURE_HEIGHT := 255
MODEL_SIMPLIFY           := off
MODEL_QUALITY            := aggressive
MODEL_ANIMATION          := Walk,Roar
# Keep the original 32 Walk samples and the reconstructed 20-sample Roar.
MODEL_ANIMATION_FPS      := source
MODEL_FACE_COLORS        := off
MODEL_TEXTURE_LIGHTING   := baked-smooth
MODEL_LIGHT_DIR          := 0.48,0.70,0.52
MODEL_AMBIENT            := 0.30
MODEL_DIFFUSE            := 1.35
MODEL_MAX_POSE_STREAM_BYTES := 786432
MODEL_HUD_RESERVE        := 64
# Adjacent triangle pairs draw as one VDP1 quad: fewer commands, and no
# texels spent on a collapsed triangle corner, which pays for the higher
# texture scale above.
MODEL_MERGE_QUADS        := on
MODEL_QUAD_MAX_FOLD_DEG  := 40
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
# noise. Cap each face at its size when the T-Rex spans ~540 pixels (side
# view; the default frontal entrance has a narrower silhouette) and box-filter the source down to that.
MODEL_SAMPLING           := area
# The demo runs 640x448 double-density interlaced hi-res. The VDP1 framebuffer is then 8
# bits/pixel, so lookup tables hold codes of one shared palette (0
# transparent, 1 the HUD's white, 254 the sprite shadow code), and faces bake
# more texels for the doubled horizontal resolution.
MODEL_LUT_CODES          := 2-253
MODEL_TEXEL_EXTENT       := 540
EXAMPLE_ASSETS           := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS           := $(MODEL_OUT_PREFIX).h

# The user-selected Demo 1 reference plays as CD-DA; legacy arrangements stay
# available explicitly. The reference source is already YouTube-encoded.
DINO_MUSIC_CDDA ?= 1
DINO_MUSIC_68K ?= 1
CFLAGS += -DDINO_MUSIC_CDDA=$(DINO_MUSIC_CDDA)
ifeq ($(DINO_MUSIC_CDDA),1)
# Decode the entire reference without an additional lossy encode, on track 2.
EXAMPLE_CDDA_INPUT := examples/$(EXAMPLE)/audio-src/ps1_demo1_reference.m4a
CFLAGS += -DDINO_MUSIC_68K=0
else ifeq ($(DINO_MUSIC_68K),1)
CFLAGS += -DDINO_MUSIC_68K=1
EXAMPLE_AUDIO_STREAMS := dino_drone dino_figure dino_impact dino_signal
else
EXAMPLE_AUDIO_STREAMS := bone_and_plastic
endif
EXAMPLE_AUDIO_bone_and_plastic_SOURCE := examples/$(EXAMPLE)/audio-src/Bone_and_Plastic.ogg
EXAMPLE_AUDIO_bone_and_plastic_DISC_PATH := audio/bone_and_plastic.adpcm
EXAMPLE_AUDIO_bone_and_plastic_SAMPLE_RATE := 22050
EXAMPLE_AUDIO_bone_and_plastic_CHANNELS := 2
EXAMPLE_AUDIO_bone_and_plastic_FORMAT := ima_adpcm

# The alternative resident soundtrack uses short seamless samples from the
# original recording. The 68000 drives their levels and pitch from Sound RAM;
# playback never reads the CD after these samples are loaded at boot.
EXAMPLE_AUDIO_dino_drone_SOURCE := $(EXAMPLE_AUDIO_bone_and_plastic_SOURCE)
EXAMPLE_AUDIO_dino_drone_DISC_PATH := audio/dino_drone.s8
EXAMPLE_AUDIO_dino_drone_SAMPLE_RATE := 11025
EXAMPLE_AUDIO_dino_drone_CHANNELS := 1
EXAMPLE_AUDIO_dino_drone_FORMAT := s8
EXAMPLE_AUDIO_dino_drone_START_MS := 2000
EXAMPLE_AUDIO_dino_drone_DURATION_MS := 2800
EXAMPLE_AUDIO_dino_drone_LOOP_CROSSFADE_MS := 180

EXAMPLE_AUDIO_dino_figure_SOURCE := $(EXAMPLE_AUDIO_bone_and_plastic_SOURCE)
EXAMPLE_AUDIO_dino_figure_DISC_PATH := audio/dino_figure.s8
EXAMPLE_AUDIO_dino_figure_SAMPLE_RATE := 11025
EXAMPLE_AUDIO_dino_figure_CHANNELS := 1
EXAMPLE_AUDIO_dino_figure_FORMAT := s8
EXAMPLE_AUDIO_dino_figure_START_MS := 30000
EXAMPLE_AUDIO_dino_figure_DURATION_MS := 2400
EXAMPLE_AUDIO_dino_figure_LOOP_CROSSFADE_MS := 160

EXAMPLE_AUDIO_dino_impact_SOURCE := $(EXAMPLE_AUDIO_bone_and_plastic_SOURCE)
EXAMPLE_AUDIO_dino_impact_DISC_PATH := audio/dino_impact.s8
EXAMPLE_AUDIO_dino_impact_SAMPLE_RATE := 11025
EXAMPLE_AUDIO_dino_impact_CHANNELS := 1
EXAMPLE_AUDIO_dino_impact_FORMAT := s8
EXAMPLE_AUDIO_dino_impact_START_MS := 55000
EXAMPLE_AUDIO_dino_impact_DURATION_MS := 1800
EXAMPLE_AUDIO_dino_impact_LOOP_CROSSFADE_MS := 120

EXAMPLE_AUDIO_dino_signal_SOURCE := $(EXAMPLE_AUDIO_bone_and_plastic_SOURCE)
EXAMPLE_AUDIO_dino_signal_DISC_PATH := audio/dino_signal.s8
EXAMPLE_AUDIO_dino_signal_SAMPLE_RATE := 11025
EXAMPLE_AUDIO_dino_signal_CHANNELS := 1
EXAMPLE_AUDIO_dino_signal_FORMAT := s8
EXAMPLE_AUDIO_dino_signal_START_MS := 39000
EXAMPLE_AUDIO_dino_signal_DURATION_MS := 1600
EXAMPLE_AUDIO_dino_signal_LOOP_CROSSFADE_MS := 120
