# Skybridge's pig is a user-modified derivative of AlexEsfell's
# "Low-poly Pig" (CC BY 4.0). Source data are stored losslessly as 7
# UTF-8 zlib/base64 parts because the GitHub connector cannot upload
# binaries. tools/unpack_model_asset.py verifies their full SHA256 before
# handing a real .glb to the EXISTING LibSaturn polygon/animation converter.
#
# The normalized GLB joins separately animated rigid mesh nodes into one
# skin, preserving original Walk/Idle/Jump animations and face colors.
# See assets/PIG_ATTRIBUTION.md for author, modification and source hashes.
PIG_SOURCE_PART_PREFIX := examples/$(EXAMPLE)/assets/pig.source.zlib.b64.part
PIG_SOURCE_PARTS := $(addprefix $(PIG_SOURCE_PART_PREFIX),01 02 03 04 05 06 07)
MODEL_GLB := $(GENERATED_DIR)/$(EXAMPLE)/pig_source.glb
$(MODEL_GLB): $(PIG_SOURCE_PARTS) $(TOOLS)/unpack_model_asset.py
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/unpack_model_asset.py \
		--prefix $(PIG_SOURCE_PART_PREFIX) --parts 7 \
		--sha256 335dfb81cd8774ceb6c7db5eacd068c180c979d55f097f38da7d190169fe3e69 \
		--out $@

# This invokes tools/import_model.py --target saturn with all existing
# simplification, silhouette and animation metric gates, NOT custom pig
# topology handwritten into the example.
MODEL_OUT_PREFIX := $(GENERATED_DIR)/$(EXAMPLE)/pig_model
MODEL_SYMBOL := skybridge_pig
MODEL_SCALE := 4.3
MODEL_PALETTE_INDEX := 7
MODEL_SIMPLIFY := auto
MODEL_QUALITY := balanced
MODEL_ANIMATION := Walk,Idle,Jump
MODEL_ANIMATION_FPS := 12
MODEL_FACE_COLORS := on
MODEL_LIGHT_DIR := -0.5,0.6,0.8
MODEL_AMBIENT := 0.48
MODEL_DIFFUSE := 0.55
EXAMPLE_ASSETS := $(MODEL_OUT_PREFIX).c
EXAMPLE_HEADERS := $(MODEL_OUT_PREFIX).h
