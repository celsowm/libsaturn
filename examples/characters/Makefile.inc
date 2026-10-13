CHARACTER_GEN := $(GENERATED_DIR)/character_examples
EXAMPLE_EXTRA_SRCS := examples/characters/demo.c examples/characters/combat.c
EXAMPLE_ASSETS := $(addprefix $(CHARACTER_GEN)/,fighter_high.c fighter_medium.c fighter_low.c fighter_assets.c)
EXAMPLE_HEADERS := $(addprefix $(CHARACTER_GEN)/,fighter_high.h fighter_medium.h fighter_low.h fighter_assets.h)
EXAMPLE_INCFLAGS += -I$(CHARACTER_GEN) -Iexamples/characters
ifeq ($(CHARACTER_DEMO_DISC_ASSETS),1)
EXAMPLE_INCFLAGS += -DCHARACTER_DEMO_DISC_ASSETS=1
endif
EXAMPLE_ISO_DIR := $(CHARACTER_GEN)/disc
EXAMPLE_ISO_FILES := $(addprefix $(CHARACTER_GEN)/disc/,high_blue.ska high_red.ska medium_blue.ska medium_red.ska low_blue.ska low_red.ska)
