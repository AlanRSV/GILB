# GILB (ReChord) — Echo Mini custom firmware build system
#
# Target: ARM Cortex-M3 (Thumb-2), Rockchip RKnanoC. Two cores:
#   core A — the UI, drivers, file system, library and player control (firmware/, firmware/ap.mk)
#   core B — the decoders (firmware/coreb/), one image a codec family
#
#   make cfw-ap    core A (build/cfw/), its size budget (tools/ap_size.py)
#   make cfw-img   the update image (build/HIFIEC90.IMG on the 8 GB board)
#   make sim       the desktop simulator (sim/)
#   make host-tests, make test-tags
#
# The SDK-era pipelines (the stock UI with an SDK core B, the boot-experiment images) were archived on
# 2026-11-25: archive/Makefile.legacy, archive/firmware/.

ifeq ($(OS),Windows_NT)
    PYTHON ?= py -3
else
    PYTHON ?= python3
endif

# Portable dir create / remove (the previous recipes shelled out to PowerShell,
# which broke builds on Linux/macOS). Python is already a required dependency.
MKDIR_P := $(PYTHON) -c "import os,sys; os.makedirs(sys.argv[1], exist_ok=True)"
RMDIR_R := $(PYTHON) -c "import shutil,sys; shutil.rmtree(sys.argv[1], ignore_errors=True)"

CROSS_COMPILE ?= arm-none-eabi-
CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE    := $(CROSS_COMPILE)size

BUILD_DIR := build

# Header-change tracking: core A's objects' .d files.
-include $(shell find build/cfw -name "*.d" 2>/dev/null)

include firmware/ap.mk

# ---- CFW AP (fw1) clean-room image: milestone 1, boots to OUR screen ----
# Distinct from the SDK A_CORE pipeline above (build-ap/link-ap/pack-fw1):
# this links ONLY firmware/{startup,ui,drivers,services,theme,libc}/*.c.
CFW_BUILD_DIR := build/cfw
CFW_OBJS := $(patsubst %.c,$(CFW_BUILD_DIR)/objs/%.o,$(AP_SRCS))
# LTO (on by default since 2026-11-24, +768 B of resident code room; LTO=0 turns it off):
# link-time optimisation for the objects firmware_ap.ld never names. Those it places by name, the
# files wholly in an overlay (*spectrum.o, *lib_*.o, ...) and ap_startup.o (its naked functions'
# literal pools must stay in reach), stay as they are: an LTO build's code comes out of new objects,
# which those patterns would not match. Mostly resident files put their overlay functions in by a
# section tag instead (OVL_TEXT, services/ovl.h), so they are optimised too (2026-11-24: +1.7 KB
# more). The link compiles each function as its own unit (-flto-partition=max): a string literal is
# emitted once per unit, in the section of the first function there to use it, so one shared by a
# resident and an overlay function would otherwise land in the overlay (the link fails: NOCROSSREFS);
# the linker still merges the resident copies. Make does not see a flag change: switching LTO on or
# off needs a clean build (rm -rf build/cfw). LTO=0 is for ruling LTO out of a fault: its code left
# under AP_MIN_FREE (1948 B free, 2026-11-24); since the layouts it does not fit FW_CODE at all (2026-11-25:
# 964 B over even with AP_MIN_FREE=0), so ruling LTO out now needs a feature left out. -flto=auto compiles the
# link's units on every core (2026-11-25: a clean core-A build 32 s -> 13 s, the same code).
LTO ?= 1
ifeq ($(LTO),1)
LD_NAMED := $(shell grep -oE '\*[a-z_0-9/]+\.o' $(AP_LDSCRIPT) | sed 's|.*[*/]||' | sort -u)
# (and lib_*.o, the library's overlay by wildcard; watchdog.o, its ISR called from inline asm;
# bootguard.o, so the .noinit crash record keeps its place across an update)
LTO_OBJS := $(foreach o,$(filter-out $(addprefix %/,$(LD_NAMED) watchdog.o bootguard.o),$(CFW_OBJS)),$(if $(findstring /lib_,$(o)),,$(o)))
$(LTO_OBJS): AP_CFLAGS += -flto
AP_LDFLAGS += -flto=auto -flto-partition=max -Os -ffunction-sections -fdata-sections
else
$(CFW_OBJS): AP_CFLAGS += -DOVL_STR_RESIDENT
endif
CFW_ELF := $(CFW_BUILD_DIR)/rechord_ap.elf

# ---- core B (decoder) images: one per codec family, linked at 0. They live in the
# firmware slot (make_fw1 --module), core A loads the one a track needs. ----
COREB_DIR  := build/coreb
COREB_RT   := b_startup b_main b_libc b_selftest b_msg b_eq eq eq_design
COREB_MAD  := $(patsubst %,libmad/%,bit fixed frame huffman layer12 layer3 stream synth timer)
COREB_ALAC := $(patsubst %,alac/%,ag_dec dp_dec ALACBitUtilities alac_dec)
COREB_FAAD := $(patsubst firmware/coreb/%.c,%,$(wildcard firmware/coreb/faad/*.c))
COREB_TREMOR := $(patsubst %,tremor/%,block codebook floor0 floor1 info mapping0 mdct registry res012 sharedbook synthesis vorbisfile window)
COREB_OGG := ogg/framing ogg/bitwise
COREB_OPUS := $(patsubst firmware/coreb/%.c,%,$(wildcard firmware/coreb/opus/celt/*.c firmware/coreb/opus/silk/*.c \
                 firmware/coreb/opus/silk/fixed/*.c firmware/coreb/opus/src/*.c firmware/coreb/opus/opusfile/*.c))
COREB_WAVPACK := $(patsubst %,wavpack/%,common_utils decorr_utils entropy_utils open_utils read_words unpack unpack_utils unpack_floats)
COREB_DEMAC := $(patsubst %,demac/%,decoder entropy predictor filter_16_11 filter_64_11 filter_32_10 filter_256_13 filter_1280_15)
COREB_WMA  := $(patsubst %,wma/%,wmadeci wmafixed mdct fft-ffmpeg mdct_lookup ffmpeg_bitstream)
COREB_WMAPRO := $(patsubst %,wma/pro/%,wmaprodec wma mdct_tables) $(patsubst %,wma/%,mdct fft-ffmpeg mdct_lookup ffmpeg_bitstream)
COREB_WMALL := wma/lossless/wmalldec wma/pro/wma wma/ffmpeg_bitstream
COREB_FAMILIES := main aac ogg opus wv dsd ape wma wmapro wmall
COREB_FAM_main := b_codecs_main b_flac b_hifi b_hifi_probe b_pcm b_mp3 b_alac $(COREB_MAD) $(COREB_ALAC) b_hifi_fn b_hifi_alac
COREB_FAM_aac  := b_codecs_aac b_aac $(COREB_FAAD)
COREB_FAM_ogg  := b_codecs_ogg b_vorbis $(COREB_TREMOR) $(COREB_OGG)
COREB_FAM_opus := b_codecs_opus b_opus $(COREB_OPUS) $(COREB_OGG)
COREB_FAM_wv   := b_codecs_wv b_wavpack $(COREB_WAVPACK)
COREB_FAM_dsd  := b_codecs_dsd b_dsd dsd/dsd_dec
COREB_FAM_ape  := b_codecs_ape b_ape $(COREB_DEMAC) b_hifi b_hifi_fn b_hifi_ape
COREB_FAM_wma  := b_codecs_wma b_wma b_asf asf $(COREB_WMA)
COREB_FAM_wmapro := b_codecs_wmapro b_wmapro b_asf asf $(COREB_WMAPRO)
COREB_FAM_wmall := b_codecs_wmall b_wmall b_asf asf $(COREB_WMALL)
# stack reserve per image (bytes): FAAD2 nests up to ~93 KiB of locals (HE-AAC v2)
COREB_STACK_main := 0x5000
COREB_STACK_aac  := 0x18000
COREB_STACK_ogg  := 0x4000
COREB_STACK_opus := 0x8000
COREB_STACK_wv   := 0x4000
COREB_STACK_dsd  := 0x2000
COREB_STACK_ape  := 0x1000
COREB_STACK_wma  := 0x2000
COREB_STACK_wmapro := 0x2000
COREB_STACK_wmall := 0x2000
COREB_CFLAGS := $(AP_ARCH_FLAGS) -O2 -Wall -Wno-unused-function -ffreestanding \
                -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -MMD -MP \
                -Ifirmware -Ifirmware/coreb -Ifirmware/coreb/libmad -DOPT_ACCURACY -DNDEBUG
COREB_FAAD_DEFS := -DFIXED_POINT -DLC_ONLY_DECODER -DHAVE_STRING_H=1 -DHAVE_STDINT_H=1 -DSTDC_HEADERS=1 \
                   -DPACKAGE_VERSION='"2.11.4"' -Ifirmware/coreb/faad -w
$(COREB_DIR)/faad/%.o: COREB_CFLAGS += $(COREB_FAAD_DEFS) -Os
$(COREB_DIR)/faad/common.o: COREB_CFLAGS += -Dfaad_malloc=faad_malloc_unused -Dfaad_free=faad_free_unused
# the hot files (docs/HIFI_PLAN.md: the FFT/IMDCT, the filter bank, the output, SBR's DCT-IV, QMF,
# HF generation and adjustment, PS) at -O2: the same integer code, fewer instructions; the rest stays -Os
# (core B's 128 KiB image)
COREB_FAAD_O2 := cfft mdct filtbank specrec output sbr_dct sbr_qmf sbr_hfgen sbr_hfadj ps_dec
$(patsubst %,$(COREB_DIR)/faad/%.o,$(COREB_FAAD_O2)): COREB_CFLAGS += -O2
$(COREB_DIR)/b_aac.o: COREB_CFLAGS += $(COREB_FAAD_DEFS)
$(COREB_DIR)/alac/%.o: COREB_CFLAGS += -Ifirmware/coreb/alac -w
$(COREB_DIR)/alac/alac_dec.o: COREB_CFLAGS += -DALAC_RECHORD_HIFI    # (coreb/b_hifi_alac.c)
COREB_OPUS_DEFS := -DOPUS_BUILD -DFIXED_POINT -DDISABLE_FLOAT_API -DVAR_ARRAYS -DOP_FIXED_POINT \
                   -DOP_DISABLE_FLOAT_API -DOP_DISABLE_HTTP -Ifirmware/coreb/opus/include -Ifirmware/coreb/opus/celt \
                   -Ifirmware/coreb/opus/silk -Ifirmware/coreb/opus/silk/fixed -Ifirmware/coreb/opus/src \
                   -Ifirmware/coreb/opus/opusfile -Ifirmware/coreb/ogg -w
$(COREB_DIR)/opus/%.o $(COREB_DIR)/b_opus.o: COREB_CFLAGS += $(COREB_OPUS_DEFS)
COREB_WV_DEFS := -DNO_TAGS -DNO_SEEKING -Ifirmware/coreb/wavpack -w
$(COREB_DIR)/wavpack/%.o $(COREB_DIR)/b_wavpack.o: COREB_CFLAGS += $(COREB_WV_DEFS)
$(COREB_DIR)/tremor/%.o $(COREB_DIR)/ogg/%.o $(COREB_DIR)/b_vorbis.o: COREB_CFLAGS += -Ifirmware/coreb/tremor -Ifirmware/coreb/ogg -w -Dalloca=__builtin_alloca
$(COREB_DIR)/b_alac.o: COREB_CFLAGS += -Ifirmware/coreb/alac
$(COREB_DIR)/dsd/%.o: COREB_CFLAGS += -O3
# the EQ (coreb/eq.h): its filters (eq.c) at -O2, the design and the glue, run once a curve, at -Os
$(COREB_DIR)/eq_design.o $(COREB_DIR)/b_eq.o $(COREB_DIR)/b_msg.o: COREB_CFLAGS += -Os
$(COREB_DIR)/demac/%.o: COREB_CFLAGS += -Wno-unused-variable
$(COREB_DIR)/demac/decoder.o: COREB_CFLAGS += -DDEMAC_RECHORD_HIFI   # (coreb/b_hifi_ape.c)
# Rockbox libwma / libwmapro (fixed point, C paths: Rockbox's ARM asm is ARM-mode), FFmpeg's
# WMA Lossless decoder made standalone (integer). No strict
# aliasing: the vendored code mixes int32_t (long on newlib) and int pointers.
$(COREB_DIR)/wma/%.o $(COREB_DIR)/b_wma.o $(COREB_DIR)/b_wmapro.o $(COREB_DIR)/b_wmall.o: COREB_CFLAGS += -Ifirmware/coreb/wma -fno-strict-aliasing -Wno-pointer-sign
$(COREB_DIR)/wma/lossless/%.o: COREB_CFLAGS += -Wno-parentheses
# the ASF packet parser is core A's file (audio/asf.c), built into the image too
$(COREB_DIR)/asf.o: firmware/audio/asf.c Makefile
	@$(MKDIR_P) $(dir $@)
	$(CC) $(COREB_CFLAGS) -c $< -o $@
$(COREB_DIR)/%.o: firmware/coreb/%.c Makefile
	@$(MKDIR_P) $(dir $@)
	$(CC) $(COREB_CFLAGS) -c $< -o $@
define COREB_IMAGE
$(COREB_DIR)/$(1).elf: $(patsubst %,$(COREB_DIR)/%.o,$(COREB_RT) $(COREB_FAM_$(1))) firmware/coreb/coreb.ld
	$$(CC) $$(AP_ARCH_FLAGS) -nostdlib -T firmware/coreb/coreb.ld -Wl,--gc-sections,--no-warn-rwx-segments \
	    -Wl,--defsym=B_STACK=$(COREB_STACK_$(1)) \
	    $$(filter %.o,$$^) -lgcc -o $$@
$(COREB_DIR)/$(1).bin: $(COREB_DIR)/$(1).elf
	$$(OBJCOPY) -O binary $$< $$@
endef
$(foreach f,$(COREB_FAMILIES),$(eval $(call COREB_IMAGE,$(f))))
COREB_BINS := $(patsubst %,$(COREB_DIR)/%.bin,$(COREB_FAMILIES))
COREB_MODULES := $(foreach f,$(COREB_FAMILIES),--module b$(f)=$(COREB_DIR)/$(f).bin)
-include $(wildcard $(COREB_DIR)/*.d $(COREB_DIR)/*/*.d)

cfw-ap: $(CFW_ELF)
	@echo "CFW AP ELF: $(CFW_ELF)"

# BOARD (and REVERSE_LISTS) are compile flags the dependency files don't see: a stamp per
# combination makes switching either rebuild every object.
CFW_BOARD_STAMP := $(CFW_BUILD_DIR)/.board-$(BOARD)$(if $(filter 1,$(REVERSE_LISTS)),-rl)
$(CFW_BOARD_STAMP):
	@$(MKDIR_P) $(dir $@)
	@rm -f $(CFW_BUILD_DIR)/.board-*
	@touch $@

$(CFW_BUILD_DIR)/objs/%.o: %.c $(CFW_BOARD_STAMP)
	@$(MKDIR_P) $(dir $@)
	$(CC) $(AP_CFLAGS) -c $< -o $@

# Core A: one link with its overlays (firmware/services/ovl.h). __ovl_abi, the ABI every overlay
# header carries and core A checks, is a hash of everything linked (a resident image and overlays
# from different links never pair up). Then: the budget (tools/ap_size.py: the build fails when
# FW_CODE has less than AP_MIN_FREE free; the breakdown in ap_size.txt), the resident image the
# packer takes (the overlays stripped: they are modules, not part of what the ROM loads), and each
# overlay as a module blob (tools/ovl_pack.py sets its length and CRC).
CFW_RES_ELF := $(CFW_BUILD_DIR)/rechord_ap_res.elf
CFW_MAP := $(CFW_BUILD_DIR)/rechord_ap.map
ovl_name = $(word 1,$(subst :, ,$(1)))
ovl_mod = $(word 2,$(subst :, ,$(1)))
CFW_OVL_MODULES := $(foreach o,$(AP_OVLS),--module $(call ovl_mod,$(o))=$(CFW_BUILD_DIR)/$(call ovl_mod,$(o)).bin)

$(CFW_ELF): $(CFW_OBJS) $(AP_LDSCRIPT) tools/ap_size.py tools/ovl_pack.py
	@$(MKDIR_P) $(dir $@)
	$(CC) $(AP_LDFLAGS) -Wl,--defsym=__ovl_abi=$$(cat $(CFW_OBJS) $(AP_LDSCRIPT) | cksum | cut -d' ' -f1) \
	    -Wl,-Map,$(CFW_MAP) $(CFW_OBJS) $(AP_LDLIBS) -o $@
	$(PYTHON) tools/ap_size.py $@ --map $(CFW_MAP) --min-free $(AP_MIN_FREE) --fonts $(TEXT_FONTS) \
	    --out $(CFW_BUILD_DIR)/ap_size.txt || { rm -f $@; exit 1; }
	$(OBJCOPY) $(foreach o,$(AP_OVLS),-R .ovl_$(call ovl_name,$(o))) $@ $(CFW_RES_ELF)
	$(PYTHON) tools/ovl_pack.py $@ $(addprefix --ovl ,$(AP_OVLS)) --out $(CFW_BUILD_DIR) || { rm -f $@; exit 1; }

# Our fw1 + stock section_3 (until the BB module milestone), CRC recomputed.
# HARD GATE in tools/pack_fw1.py: no scatter row below 0x03004000 (ROM-owned
# RAM - the 2026-08-25 brick). Debrick: docs/DEBRICK-LINUX.md.
# The update file the board's stock updater takes (firmware/drivers/board.h):
# 512 MB MINIVnnn.IMG, 8 GB HIFIECnn.IMG. 900/90 sort above any stock version.
UPDATE_IMG := $(if $(filter 8g,$(BOARD)),HIFIEC90.IMG,MINIV900.IMG)

# The baked fonts (tools/gen_font.py in the project repo), loaded at boot by
# firmware/drivers/text_load.c into the fonts window (firmware_ap.ld).
TEXT_FONTS := firmware/drivers/fonts/rechord_fonts.bin
TEXT_FONTS_MAX := 99840
# The stock IMG the 33 MB intermediate is spliced into, and the 5 MB reference whose envelope
# the update file takes (tools/make_fw1.py --stock / --ref). Paths may contain spaces.
STOCK_IMG ?= stock/3.7.0/ECHO MINI V3.7.0/HIFIEC37.IMG
REF_IMG   ?= community/refcfw/HIFIEC90.IMG
# FONT_IMG (optional): stock's bitmap fonts copied into the image. Nothing reads them since GILB's fonts
# module; images shared with others leave them out (they are Snowsky's, 2026-11-25).
cfw-img: $(CFW_ELF) $(COREB_BINS) $(TEXT_FONTS)
	@test $$(wc -c < $(TEXT_FONTS)) -le $(TEXT_FONTS_MAX) || \
	    { echo "$(TEXT_FONTS) is over the fonts window ($(TEXT_FONTS_MAX) B, firmware_ap.ld)"; exit 1; }
	$(PYTHON) tools/make_fw1.py $(CFW_RES_ELF) -o build/ReChord_AP.IMG --stock "$(STOCK_IMG)" --ref "$(REF_IMG)" \
	    $(if $(FONT_IMG),--fonts "$(FONT_IMG)") $(COREB_MODULES) \
	    --module fonts=$(TEXT_FONTS) $(CFW_OVL_MODULES)
	cp build/ReChord_AP5M.IMG build/$(UPDATE_IMG)
	@echo "  update file for the $(BOARD) board: build/$(UPDATE_IMG)"

.DEFAULT_GOAL := cfw-ap
.PHONY: cfw-ap cfw-img host-tests test-tags sim clean
HOSTCC ?= cc

# Host tests of the service layer (fs/settings/log/input), the display
# driver, and the FAT32 write path of fs_target.c (own image builder and
# checker, power-cut sweep, screenshot BMP) -- no hardware, no root.
host-tests:
	CC=$(HOSTCC) sh firmware/services/tests/run_tests.sh
	CC=$(HOSTCC) sh firmware/drivers/tests/run_tests.sh
	@mkdir -p build/host
	$(HOSTCC) -std=c99 -Wall -Werror -Ifirmware/theme -o build/host/test_theme firmware/theme/tests/test_theme.c \
	  firmware/theme/theme.c firmware/theme/themes_builtin.c firmware/theme/theme_file.c
	build/host/test_theme > build/host/test_theme.txt || { cat build/host/test_theme.txt; exit 1; }
	@tail -1 build/host/test_theme.txt
	$(HOSTCC) -std=c99 -Wall -Werror -Ifirmware/ui -Ifirmware -Ifirmware/drivers -o build/host/test_dirty_rects \
	  firmware/ui/tests/test_dirty_rects.c firmware/ui/ui.c
	build/host/test_dirty_rects
	HOSTCC=$(HOSTCC) $(PYTHON) tools/test_fs_write.py
	@mkdir -p build/host
	$(HOSTCC) -O2 -Wall -Ifirmware -o build/host/test_fw_powercut firmware/services/test_fw_powercut.c \
	  firmware/services/fw_update.c firmware/services/scratch.c
	build/host/test_fw_powercut
	$(HOSTCC) -O2 -Wall -Werror -D__udivmoddi4=my_udivmoddi4 -o build/host/test_udiv64 \
	  firmware/libc/tests/test_udiv64.c firmware/libc/udiv64.c
	build/host/test_udiv64

# Tag reader (firmware/services/tags.c): fixtures, expected fields, rd() cost,
# fuzz under ASan/UBSan. FUZZ=<iterations> SEED=<n> GAPLESS=<dir> optional.
test-tags:
	CC=$(HOSTCC) firmware/services/tests/run_tags.sh $(FUZZ) $(SEED)

# The desktop simulator: the firmware's UI and player in a window (sim/README.md). Needs SDL2.
sim:
	$(MAKE) -C sim BOARD=$(BOARD)

clean:
	@$(RMDIR_R) build
