# Scratch probe for the city_walk plan (R3): a 1 MiB CD-only payload to time
# CD -> WRAM-L -> RAM cart transfers against.
LOAD_PROBE_DIR := $(GENERATED_DIR)/city_load_probe
LOAD_PROBE_ISO_DIR := $(LOAD_PROBE_DIR)/iso

EXAMPLE_ISO_DIR := $(LOAD_PROBE_ISO_DIR)
EXAMPLE_ISO_FILES := $(LOAD_PROBE_ISO_DIR)/PROBE.BIN

$(LOAD_PROBE_ISO_DIR)/PROBE.BIN: examples/city_load_probe/gen_payload.py
	@mkdir -p $(LOAD_PROBE_ISO_DIR)
	$(PYTHON) examples/city_load_probe/gen_payload.py $@
