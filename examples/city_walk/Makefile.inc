# city_walk: the city never lives in the executable. tools/city_chunker.py turns
# the vendored, Draco-compressed GLB into CITY.BIN, which the generic ISO
# staging hook puts on the disc; the program streams it CD -> RAM cart ->
# work RAM. The first build decodes Draco with npx (needs Node.js) and builds
# the block model and bakes its facade textures (~1 minute).
CITY_GEN_DIR := $(GENERATED_DIR)/city_walk
CITY_ISO_DIR := $(CITY_GEN_DIR)/iso
CITY_GLB := examples/city_walk/assets/low_poly_city.glb
CITY_TOOLS := tools/city_chunker.py \
	tools/model_pipeline/draco.py \
	tools/model_pipeline/chunking.py \
	tools/model_pipeline/emit_bin.py \
	tools/model_pipeline/gltf.py \
	tools/model_pipeline/model.py \
	tools/model_pipeline/blocks.py \
	tools/model_pipeline/facades.py \
	tools/model_pipeline/foliage.py \
	tools/model_pipeline/face_colors.py

EXAMPLE_HEADERS := $(CITY_GEN_DIR)/city_data.h
EXAMPLE_ISO_DIR := $(CITY_ISO_DIR)
EXAMPLE_ISO_FILES := $(CITY_ISO_DIR)/CITY.BIN

$(CITY_ISO_DIR)/CITY.BIN $(CITY_GEN_DIR)/city_data.h &: $(CITY_TOOLS) $(CITY_GLB)
	@mkdir -p $(CITY_ISO_DIR)
	$(PYTHON) tools/city_chunker.py --input $(CITY_GLB) \
		--out-bin $(CITY_ISO_DIR)/CITY.BIN --out-h $(CITY_GEN_DIR)/city_data.h \
		--cache-dir $(CITY_GEN_DIR) --incremental \
		--lod-caps 384:176,144:64,48:24 --face-budget 600 --ground-upd 1,1 \
		--light-dir=-0.5,0.6,0.8 --ambient 0.42 --diffuse 0.60 \
		--max-archive-bytes 4718592 --report $(CITY_GEN_DIR)/city.report.json
