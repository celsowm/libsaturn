PCM8_PREFIX := $(GENERATED_DIR)/pcm8_stream/pcm8_data
EXAMPLE_ASSETS  := $(PCM8_PREFIX).c
EXAMPLE_HEADERS := $(PCM8_PREFIX).h

# The CC0 source WAV is shared with audio_showcase's cache.
$(PCM8_PREFIX).c $(PCM8_PREFIX).h &: $(TOOLS)/generate_pcm8_stream_assets.py
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/generate_pcm8_stream_assets.py \
		--out-c $(PCM8_PREFIX).c --out-h $(PCM8_PREFIX).h \
		--cache-dir $(GENERATED_DIR)/audio_showcase/cc0_cache

$(PCM8_PREFIX).o: $(PCM8_PREFIX).c $(PCM8_PREFIX).h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
