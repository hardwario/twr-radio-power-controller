SDK_DIR ?= sdk
VERSION ?= vdev
LED_STRIP_COUNT ?= 144
LED_STRIP_TYPE ?= 4
LED_STRIP_SWAP_RG ?= 0

CFLAGS += -D'VERSION="${VERSION}"'
CFLAGS += -D'LED_STRIP_COUNT=$(LED_STRIP_COUNT)'
CFLAGS += -D'LED_STRIP_TYPE=$(LED_STRIP_TYPE)'
CFLAGS += -D'LED_STRIP_SWAP_RG=$(LED_STRIP_SWAP_RG)'

-include sdk/Makefile.mk

.PHONY: all
all: sdk
	@$(MAKE) -s debug

.PHONY: sdk
sdk:
	@if [ ! -f $(SDK_DIR)/Makefile.mk ]; then echo "Initializing Git submodules..."; git submodule update --init; fi

.PHONY: update
update: sdk
	@echo "Updating Git submodules..."; git submodule update --remote --merge
