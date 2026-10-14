OPTICAL_TEST_PROFILE ?= minimal
ifneq ($(filter $(OPTICAL_TEST_PROFILE),minimal full),$(OPTICAL_TEST_PROFILE))
$(error OPTICAL_TEST_PROFILE must be minimal or full)
endif
EXAMPLE_INCFLAGS += -DOET_FULL=$(if $(filter full,$(OPTICAL_TEST_PROFILE)),1,0)
OET_BUILD_ID ?= $(shell git rev-parse --short=8 HEAD 2>/dev/null || echo 00000000)
EXAMPLE_INCFLAGS += -DOET_BUILD_ID=0x$(OET_BUILD_ID)
EXAMPLE_INCFLAGS += -I$(GENERATED_DIR)/$(EXAMPLE)
EXAMPLE_ISO_DIR := $(GENERATED_DIR)/$(EXAMPLE)/disc
EXAMPLE_ISO_FILES := $(EXAMPLE_ISO_DIR)/ODETEST.BIN
EXAMPLE_HEADERS := $(GENERATED_DIR)/$(EXAMPLE)/fixture.h
$(EXAMPLE_ISO_FILES) $(EXAMPLE_HEADERS) &: tools/generate_optical_fixture.py
	$(PYTHON) tools/generate_optical_fixture.py --out $(EXAMPLE_ISO_FILES) --header $(EXAMPLE_HEADERS)
