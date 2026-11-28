# All backends and the scene driver share this optimization level.
EXAMPLE_INCFLAGS += -O3
EXAMPLE_HEADERS += $(GENERATED_DIR)/heterogeneous_lighting_ports.h
EXAMPLE_HEADERS += $(GENERATED_DIR)/heterogeneous_lighting_stream.h

$(GENERATED_DIR)/heterogeneous_lighting_ports.h: $(EXAMPLE_DIR)/lighting.dsp tools/scu_dsp.py
	@mkdir -p $(@D)
	$(PYTHON) tools/scu_dsp.py asm $< --c $@ --name kLightingPorts

$(GENERATED_DIR)/heterogeneous_lighting_stream.h: $(EXAMPLE_DIR)/lighting.dsp tools/scu_dsp.py
	@mkdir -p $(@D)
	$(PYTHON) tools/scu_dsp.py asm $< --c $@ --name kLightingStream -D STREAM
