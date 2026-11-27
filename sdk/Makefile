# sdk/Makefile -- build GILB programs (.APP files; sdk/README.md).
#
#   make -C sdk                       every example, into sdk/build/
#   make -C sdk APP=hello             one of them
#
# A program of your own: copy examples/hello to examples/<name>, then make -C sdk APP=<name>
# (NAME= sets the name the player's list shows; default: the folder's name. API_MIN= the API version
# it needs, default 1: an older GILB refuses it. sdk/API.md has what each version adds).
# Needs arm-none-eabi-gcc and python3 (WSL / Linux / macOS), the same as the firmware.

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
PYTHON  := python3
ARCH    := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
CFLAGS  := $(ARCH) -Os -Wall -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -Iinclude
LDFLAGS := $(ARCH) -nostdlib -nostartfiles -T app.ld -Wl,--gc-sections
OUT     := build

APPS := $(if $(APP),$(APP),$(notdir $(wildcard examples/*)))

all: $(foreach a,$(APPS),$(OUT)/$(shell echo $(a) | tr a-z A-Z | cut -c1-8).APP)

define app_rule
$(OUT)/$(shell echo $(1) | tr a-z A-Z | cut -c1-8).APP: $(wildcard examples/$(1)/*.c) crt0.c app.ld include/gilb_app.h tools/gilb_app.py
	@mkdir -p $(OUT)/$(1)
	$(CC) $(CFLAGS) -DAPP_NAME='"$(if $(NAME),$(NAME),$(1))"' -DAPP_API_MIN=$(if $(API_MIN),$(API_MIN),1) crt0.c $(wildcard examples/$(1)/*.c) $(LDFLAGS) -lgcc \
	  -Wl,-Map,$(OUT)/$(1)/$(1).map -o $(OUT)/$(1)/$(1).elf
	$(OBJCOPY) -O binary $(OUT)/$(1)/$(1).elf $(OUT)/$(1)/$(1).bin
	$(PYTHON) tools/gilb_app.py $(OUT)/$(1)/$(1).bin $$@
endef
$(foreach a,$(APPS),$(eval $(call app_rule,$(a))))

clean:
	rm -rf $(OUT)

.PHONY: all clean
