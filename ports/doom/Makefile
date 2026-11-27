# ports/doom/Makefile -- Doom as a GILB program (README.md).
#
#   make -C ports/doom            build/DOOM.APP
#   make -C ports/doom sizes      what each file costs, by section
#
# The same tools as the SDK (sdk/Makefile): arm-none-eabi-gcc (newlib for the string functions)
# and python3, in WSL.

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size
PYTHON  := python3
SDK     := ../../sdk
ARCH    := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
CFLAGS  := $(ARCH) -Os -std=gnu99 -Wall -Wno-unused -Wno-missing-braces -Wno-pointer-sign \
           -ffunction-sections -fdata-sections -fomit-frame-pointer \
           -Igilb -Iinclude -I$(SDK)/include -DAPP_NAME='"Doom"' -DAPP_API_MIN=3   # (4: the PC keyboard, 5: sound, when there)
LDFLAGS := $(ARCH) -nostartfiles -T doom.ld -Wl,--gc-sections --specs=nano.specs
OUT     := build

# The game, without the music (opl2.c, mus.c): silent for now (gilb_cold.c has the sound stubs).
GAME := $(filter-out source/opl2.c source/mus.c,$(wildcard source/*.c))
GILB := $(wildcard gilb/*.c)            # (gilb_crt0.c: the SDK's crt0.c header, faster memcpy)
SRCS := $(GAME) $(GILB)
OBJS := $(patsubst %.c,$(OUT)/%.o,$(notdir $(SRCS)))

vpath %.c source gilb

all: $(OUT)/DOOM.APP

$(OUT)/%.o: %.c $(wildcard gilb/*.h include/*.h) Makefile
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/doom.elf: $(OBJS) doom.ld
	$(CC) $(OBJS) $(LDFLAGS) -lc -lgcc -Wl,-Map,$(OUT)/doom.map -o $@

$(OUT)/DOOM.APP: $(OUT)/doom.elf tools/pack.py
	$(PYTHON) tools/pack.py $(OUT)/doom.elf $@

objs: $(OBJS)

sizes: $(OBJS)
	@$(SIZE) -t $(OBJS) | sort -n -k4

clean:
	rm -rf $(OUT)

.PHONY: all objs sizes clean
