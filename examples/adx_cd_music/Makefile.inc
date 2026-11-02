# ADX compressed stereo track resident on CD, decoded on SH-2 to native SCSP PCM16.
# The CC0 source is shared with audio_showcase / pcm8_cd_music.
ADX_CD_SRC := $(GENERATED_DIR)/audio_showcase/cc0_cache/title.wav

$(ADX_CD_SRC):
	@mkdir -p $(dir $@)
	curl -fsSL -A libsaturn-asset-fetch -o $@ https://opengameart.org/sites/default/files/title_1.wav

EXAMPLE_AUDIO_STREAMS := adx_cd_music
EXAMPLE_AUDIO_adx_cd_music_SOURCE := $(ADX_CD_SRC)
EXAMPLE_AUDIO_adx_cd_music_DISC_PATH := audio/music.adxbin
EXAMPLE_AUDIO_adx_cd_music_SAMPLE_RATE := 22050
EXAMPLE_AUDIO_adx_cd_music_CHANNELS := 2
EXAMPLE_AUDIO_adx_cd_music_FORMAT := adx
EXAMPLE_ISO_DIR := $(GENERATED_DIR)/$(EXAMPLE)/audio
