# CD-only public-domain PCM tracks.  The generic ISO staging hook keeps these
# files out of the SH-2 executable, so the example exercises CDFS/VFS reads.
CD_JUKEBOX_DIR := $(GENERATED_DIR)/cd_streaming_jukebox
CD_JUKEBOX_ISO_DIR := $(CD_JUKEBOX_DIR)/iso
CD_JUKEBOX_AUDIO_DIR := $(CD_JUKEBOX_ISO_DIR)/audio
CD_JUKEBOX_HEADER := $(CD_JUKEBOX_DIR)/jukebox_data.h

EXAMPLE_HEADERS := $(CD_JUKEBOX_HEADER)
EXAMPLE_ISO_DIR := $(CD_JUKEBOX_ISO_DIR)
EXAMPLE_ISO_FILES := \
	$(CD_JUKEBOX_AUDIO_DIR)/ode_to_joy.pcm \
	$(CD_JUKEBOX_AUDIO_DIR)/minuet_in_g.pcm \
	$(CD_JUKEBOX_AUDIO_DIR)/greensleeves.pcm

$(EXAMPLE_ISO_FILES) $(CD_JUKEBOX_HEADER) &: tools/generate_cd_streaming_jukebox_assets.py
	@mkdir -p $(CD_JUKEBOX_AUDIO_DIR)
	$(PYTHON) tools/generate_cd_streaming_jukebox_assets.py \
		--out-dir $(CD_JUKEBOX_AUDIO_DIR) \
		--out-h $(CD_JUKEBOX_HEADER)
