AUDIO_SHOWCASE_PREFIX := $(GENERATED_DIR)/audio_showcase/audio_data
EXAMPLE_ASSETS        := $(AUDIO_SHOWCASE_PREFIX).c
EXAMPLE_HEADERS       := $(AUDIO_SHOWCASE_PREFIX).h

$(AUDIO_SHOWCASE_PREFIX).c $(AUDIO_SHOWCASE_PREFIX).h &: $(TOOLS)/generate_audio_showcase_assets.py
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/generate_audio_showcase_assets.py \
		--out-c $(AUDIO_SHOWCASE_PREFIX).c \
		--out-h $(AUDIO_SHOWCASE_PREFIX).h

# Generated assets live under build/generated, so provide the exact compile
# edge here instead of relying on the normal source-tree pattern rule.
$(AUDIO_SHOWCASE_PREFIX).o: $(AUDIO_SHOWCASE_PREFIX).c $(AUDIO_SHOWCASE_PREFIX).h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
