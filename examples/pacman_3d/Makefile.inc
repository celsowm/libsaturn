# pacman_3d - no external assets; the maze is flat-shaded polygons.
EXAMPLE_COMMON_SRCS = examples/common/pacman_game.c

# Starfield for the VDP2 NBG0 layer behind the board. Regenerate the source
# PNG from the original photo with tools/make_starfield.py; it is committed
# so the build does not need the photo.
EXAMPLE_INPUT        := examples/pacman_3d/assets/stars.png
EXAMPLE_RESIZE       := 256 256
EXAMPLE_ASSET_PREFIX := $(GENERATED_DIR)/pacman_3d/stars
EXAMPLE_ASSETS       := $(EXAMPLE_ASSET_PREFIX).c
EXAMPLE_HEADERS      := $(EXAMPLE_ASSET_PREFIX).h
