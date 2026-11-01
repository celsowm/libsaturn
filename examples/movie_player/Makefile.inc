# The movie goes on the disc, not into the executable: it is read from the CD while it plays.
# assets/movies/libsaturn_intro.mov is made by tools/satmovie.py from the LibSaturn intro video.
MOVIE_PLAYER_ISO_DIR := $(GENERATED_DIR)/movie_player/iso
EXAMPLE_ISO_DIR := $(MOVIE_PLAYER_ISO_DIR)
EXAMPLE_ISO_FILES := $(MOVIE_PLAYER_ISO_DIR)/LSINTRO.MOV

$(MOVIE_PLAYER_ISO_DIR)/LSINTRO.MOV: assets/movies/libsaturn_intro.mov
	@mkdir -p $(MOVIE_PLAYER_ISO_DIR)
	cp $< $@
