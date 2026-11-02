# Compressed CC0 music: compact ADX CD example with caller-owned PCM buffers.
ADX_COMPACT_SRC := $(GENERATED_DIR)/audio_showcase/cc0_cache/title.wav

$(ADX_COMPACT_SRC):
	@mkdir -p $(dir $@)
	curl -fsSL -A libsaturn-asset-fetch -o $@ https://opengameart.org/sites/default/files/title_1.wav

EXAMPLE_AUDIO_STREAMS := adx_compact_cd
EXAMPLE_AUDIO_adx_compact_cd_SOURCE := $(ADX_COMPACT_SRC)
EXAMPLE_AUDIO_adx_compact_cd_DISC_PATH := audio/music.adxbin
EXAMPLE_AUDIO_adx_compact_cd_SAMPLE_RATE := 22050
EXAMPLE_AUDIO_adx_compact_cd_CHANNELS := 2
EXAMPLE_AUDIO_adx_compact_cd_FORMAT := adx
EXAMPLE_ISO_DIR := $(GENERATED_DIR)/$(EXAMPLE)/audio
