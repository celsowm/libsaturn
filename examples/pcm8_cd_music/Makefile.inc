# CD-resident PCM8 music: the track is a signed 8-bit interleaved stereo file in the ISO, read by the
# disc scheduler into the two seamless PCM8 streams. The CC0 source is shared with audio_showcase's cache.
PCM8_CD_SRC := $(GENERATED_DIR)/audio_showcase/cc0_cache/title.wav

$(PCM8_CD_SRC):
	@mkdir -p $(dir $@)
	curl -fsSL -A libsaturn-asset-fetch -o $@ https://opengameart.org/sites/default/files/title_1.wav

EXAMPLE_AUDIO_STREAMS := pcm8_cd_music
EXAMPLE_AUDIO_pcm8_cd_music_SOURCE := $(PCM8_CD_SRC)
EXAMPLE_AUDIO_pcm8_cd_music_DISC_PATH := audio/music.pcm8
EXAMPLE_AUDIO_pcm8_cd_music_SAMPLE_RATE := 22050
EXAMPLE_AUDIO_pcm8_cd_music_CHANNELS := 2
EXAMPLE_AUDIO_pcm8_cd_music_FORMAT := s8
EXAMPLE_ISO_DIR := $(GENERATED_DIR)/$(EXAMPLE)/audio
