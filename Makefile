# Makefile to rebuild MK64 split image
#include ${KOS_BASE}/Makefile.rules

include util.mk

include safe_gcc.mk

HAVE_CDI4DC := $(shell which cdi4dc > /dev/null 2>&1 && echo "yes" || echo "no")

# Default target
default: all

# Preprocessor definitions

# uncomment to put brake/back on the X button
#DEFINES := BUTTON_SWAP_X=1

DEFINES := 

#==============================================================================#
# Build Options                                                                #
#==============================================================================#

# Configure the build options with 'make SETTING=value' (ex. make VERSION=eu).
# Run 'make clean' prior to changing versions.

# Build for the N64 (turn this off for ports)
TARGET_N64 ?= 0
TARGET_DC := 1
ifeq ($(TARGET_DC), 1)
  ENABLE_OPENGL ?= 1
  NON_MATCHING := 1
endif

# COMPILER - selects the C compiler to use
#   ido - uses the SGI IRIS Development Option compiler, which is used to build
#         an original matching N64 ROM
#   gcc - uses the GNU C Compiler
COMPILER ?= kos-cc

# Add debug tools with 'make DEBUG=1' and modify the macros in include/debug.h
# Adds crash screen enhancement and activates debug mode
# Run make clean first
DEBUG ?= 0

# Compile with GCC
GCC ?= 1

# VERSION - selects the version of the game to build
#  us     - builds the 1997 North American version
#  eu.v10 - builds the 1997 1.0 PAL version
#  eu.v11 - builds the 1997 1.1 PAL version
VERSION ?= us

ifeq      ($(VERSION),us)
  DEFINES += VERSION_US=1
  GRUCODE   ?= f3dex_old
else ifeq ($(VERSION),eu.v10)
  DEFINES += VERSION_EU=1 VERSION_EU_V10=1
  GRUCODE   ?= f3dex_old
else ifeq ($(VERSION),eu.v11)
  DEFINES += VERSION_EU=1 VERSION_EU_V11=1
  GRUCODE   ?= f3dex_old
endif

ifeq ($(DEBUG),1)
  DEFINES += DEBUG=1
  COMPARE ?= 0
endif

TARGET := mk64.$(VERSION)

BASEROM := baserom.$(VERSION).z64



# GRUCODE - selects which RSP microcode to use.
#   f3dex_old - default, version 0.95. An early version of F3DEX.
#   f3dex     - latest version of F3DEX, used on iQue and Lodgenet.
#   f3dex2    - F3DEX2, currently unsupported.
# Note that 3/4 player mode uses F3DLX
$(eval $(call validate-option,GRUCODE,f3dex_old f3dex f3dex2))

ifeq ($(GRUCODE),f3dex_old) # Fast3DEX 0.95
  DEFINES += F3DEX_GBI=1 F3D_OLD=1 _LANGUAGE_C=1
else ifeq ($(GRUCODE),f3dex) # Fast3DEX 1.23
  DEFINES += F3DEX_GBI=1 F3DEX_GBI_SHARED=1
else ifeq ($(GRUCODE),f3dex2) # Fast3DEX2
  DEFINES += F3DEX_GBI_2=1 F3DEX_GBI_SHARED=1
endif

ifeq      ($(COMPILER),ido)
  #MIPSISET := -mips2
else ifeq ($(COMPILER),gcc)
  DEFINES += AVOID_UB=1 NON_MATCHING=1
  NON_MATCHING := 1
  VERSION_ASFLAGS := --defsym AVOID_UB=1 
#  MIPSISET     := -mips3
endif



# NON_MATCHING - whether to build a matching, identical copy of the ROM
#   1 - enable some alternate, more portable code that does not produce a matching ROM
#   0 - build a matching ROM
NON_MATCHING ?= 0
$(eval $(call validate-option,NON_MATCHING,0 1))

ifeq ($(TARGET_N64),0)
  NON_MATCHING := 1
endif

ifeq ($(NON_MATCHING),1)
  DEFINES += NON_MATCHING=1 AVOID_UB=1
  VERSION_ASFLAGS := --defsym AVOID_UB=1 
  COMPARE := 0
endif

# GCC define is to link gcc's std lib.
ifeq ($(GCC), 1)
    DEFINES += AVOID_UB=1 GCC=1
endif

# COMPARE - whether to verify the SHA-1 hash of the ROM after building
#   1 - verifies the SHA-1 hash of the selected version of the game
#   0 - does not verify the hash
COMPARE ?= 1
$(eval $(call validate-option,COMPARE,0 1))



# Whether to hide commands or not
VERBOSE ?= 1
ifeq ($(VERBOSE),0)
  V := @
endif

# Whether to colorize build messages
COLOR ?= 1

ifeq ($(OS),Windows_NT)
    DETECTED_OS=windows
    # Set Windows temporary directory to its environment variable
    export TMPDIR=$(TEMP)
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        DETECTED_OS=linux
    else ifeq ($(UNAME_S),Darwin)
        DETECTED_OS=macos
    endif
endif

# display selected options unless 'make clean' or 'make distclean' is run
ifeq ($(filter clean distclean,$(MAKECMDGOALS)),)
  $(info ==== Build Options ====)
  $(info Version:        $(VERSION))
  $(info Microcode:      $(GRUCODE))
  $(info Target:         $(TARGET))
  ifeq ($(COMPARE),1)
    $(info Compare ROM:    yes)
  else
    $(info Compare ROM:    no)
  endif
  ifeq ($(NON_MATCHING),1)
    $(info Build Matching: no)
  else
    $(info Build Matching: yes)
  endif
  $(info =======================)
endif

#==============================================================================#
# Universal Dependencies                                                       #
#==============================================================================#

TOOLS_DIR := tools

# (This is a bit hacky, but a lot of rules implicitly depend
# on tools and assets, and we use directory globs further down
# in the makefile that we want should cover assets.)
ifeq ($(DETECTED_OS),windows)
# because python3 is a command to trigger windows store, and python on windows it's just called python
  ifneq ($(PYTHON),)
  else ifneq ($(call find-command,python),)
    PYTHON := python
  else ifneq ($(call find-command,python3),)
    PYTHON := python3
  endif
else
  PYTHON ?= python3
endif

DUMMY != $(PYTHON) --version || echo FAIL
ifeq ($(DUMMY),FAIL)
  $(error Unable to find python)
endif

ifeq ($(filter clean distclean print-%,$(MAKECMDGOALS)),)
   # Make tools if out of date
  DUMMY != make -C $(TOOLS_DIR)
  ifeq ($(DUMMY),FAIL)
    $(error Failed to build tools)
  endif
  $(info Building ROM...)

  # Make sure assets exist
  NOEXTRACT ?= 0
  ifeq ($(NOEXTRACT),0)
    DUMMY != $(PYTHON) extract_assets.py $(VERSION) >&2 || echo FAIL
    ifeq ($(DUMMY),FAIL)
      $(error Failed to extract assets)
    endif
  endif
endif



#==============================================================================#
# Target Executable and Sources                                                #
#==============================================================================#

BUILD_DIR_BASE := build
# BUILD_DIR is location where all build artifacts are placed
BUILD_DIR      := $(BUILD_DIR_BASE)/$(VERSION)
ELF            := mario-kart.elf
LD_SCRIPT      := mk64.ld
ASSET_DIR      := assets
BIN_DIR        := bin
DATA_DIR       := data
INCLUDE_DIRS   := include

# Directories containing source files
#SRC_ASSETS_DIR := assets/code/ceremony_data assets/code/startup_logo assets/code/data_800E45C0 assets/code/data_800E8700 assets/code/common_data
SRC_DIRS       := src src/data src/buffers src/racing src/ending src/audio src/debug src/os src/os/math courses src/compression src/dcaudio src/gfx assets/code/data_800E45C0 assets/code/data_800E8700
#assets/code/ceremony_data assets/code/startup_logo $(SRC_ASSETS_DIR)
ASM_DIRS       := $(DATA_DIR) $(DATA_DIR)/karts $(DATA_DIR)/zkarts $(DATA_DIR)/sound_data
#asm asm/os asm/unused 


# Directories containing course source and data files
COURSE_DIRS := $(shell find courses -mindepth 1 -type d)
TEXTURES_DIR = textures
TEXTURE_DIRS := textures/common

ALL_DIRS = $(BUILD_DIR) $(addprefix $(BUILD_DIR)/,$(SRC_DIRS) $(COURSE_DIRS) include $(ASM_DIRS) $(TEXTURES_DIR)/raw \
	$(TEXTURES_DIR)/standalone $(TEXTURES_DIR)/startup_logo $(TEXTURES_DIR)/crash_screen $(TEXTURES_DIR)/trophy $(TEXTURES_DIR)/courses        \
	$(TEXTURE_DIRS) $(TEXTURE_DIRS)/tlut $(BIN_DIR) assets assets/code assets/code/common_data assets/code/startup_logo assets/code/ceremony_data assets/code/data_800E45C0 assets/code/data_800E8700) assets/course_metadata

# file dependencies generated by splitter
MAKEFILE_SPLIT = Makefile.split
include $(MAKEFILE_SPLIT)

# These are files that need to be encoded into EUC-JP in order for the ROM to match
# We filter them out from the regular C_FILES since we don't need nor want the
# UTF-8 versions getting compiled
#EUC_JP_FILES := src/ending/credits.c src/cpu_vehicles_camera_path.c src/menu_items.c
#C_FILES := $(filter-out %.inc.c,$(filter-out $(EUC_JP_FILES),$(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))))
C_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
S_FILES := $(foreach dir,$(ASM_DIRS),$(wildcard $(dir)/*.s))
# Include source files in courses/course_name/files.c but exclude .inc.c files.
COURSE_FILES := $(foreach dir,$(COURSE_DIRS),$(filter-out %.inc.c,$(wildcard $(dir)/*.c)))


# Object files
O_FILES := \
  $(foreach file,$(C_FILES),$(BUILD_DIR)/$(file:.c=.o)) \
  $(foreach file,$(COURSE_FILES),$(BUILD_DIR)/$(file:.c=.o)) \
  $(foreach file,$(S_FILES),$(BUILD_DIR)/$(file:.s=.o)) \
  $(EUC_JP_FILES:%.c=$(BUILD_DIR)/%.jp.o)

# Automatic dependency files
DEP_FILES := $(O_FILES:.o=.d) $(BUILD_DIR)/$(LD_SCRIPT).d

# Files with GLOBAL_ASM blocks
GLOBAL_ASM_C_FILES != grep -rl 'GLOBAL_ASM(' $(wildcard src/*.c)
GLOBAL_ASM_OS_FILES != grep -rl 'GLOBAL_ASM(' $(wildcard src/os/*.c)
GLOBAL_ASM_AUDIO_C_FILES != grep -rl 'GLOBAL_ASM(' $(wildcard src/audio/*.c)
GLOBAL_ASM_RACING_C_FILES != grep -rl 'GLOBAL_ASM(' $(wildcard src/racing/*.c)
GLOBAL_ASM_O_FILES = $(foreach file,$(GLOBAL_ASM_C_FILES),$(BUILD_DIR)/$(file:.c=.o))
GLOBAL_ASM_OS_O_FILES = $(foreach file,$(GLOBAL_ASM_OS_FILES),$(BUILD_DIR)/$(file:.c=.o))
GLOBAL_ASM_AUDIO_O_FILES = $(foreach file,$(GLOBAL_ASM_AUDIO_C_FILES),$(BUILD_DIR)/$(file:.c=.o))
GLOBAL_ASM_RACING_O_FILES = $(foreach file,$(GLOBAL_ASM_RACING_C_FILES),$(BUILD_DIR)/$(file:.c=.o))

ifneq ($(BLENDER),)
else ifneq ($(call find-command,blender),)
  BLENDER := blender
else ifeq ($(DETECTED_OS), windows)
  BLENDER := "C:\Program Files\Blender Foundation\Blender 3.6\blender.exe"
endif

MODELS_JSON := $(call rwildcard,models,*.json)
MODELS_PROC := $(MODELS_JSON:%.json=%)

models/%: models/%.json
	$(PYTHON) tools/blender/extract_models.py $(BLENDER) $<

#==============================================================================#
# Compiler Options                                                             #
#==============================================================================#

# detect prefix for MIPS toolchain
#ifneq ($(CROSS),)
#else ifneq ($(call find-command,mips64-elf-ld),)
#  CROSS := mips64-elf-
# else ifneq ($(call find-command,mips-n64-ld),)
#   CROSS := mips-n64-
#else ifneq ($(call find-command,mips64-ld),)
#  CROSS := mips64-
#else ifneq ($(call find-command,mips-linux-gnu-ld),)
#  CROSS := mips-linux-gnu-
#else ifneq ($(call find-command,mips64-linux-gnu-ld),)
#  CROSS := mips64-linux-gnu-
#else ifneq ($(call find-command,mips-ld),)
#  CROSS := mips-
#else
#  $(error Unable to detect a suitable MIPS toolchain installed)
#endif

CROSS :=

AS      := kos-as
  
#ifeq ($(COMPILER),kos-cc)
  CC      := kos-cc
#$(CROSS)gcc
#else
#  IDO_ROOT := $(TOOLS_DIR)/ido-recomp/$(DETECTED_OS)
#  CC      := $(IDO_ROOT)/cc
#endif
LD      := sh-elf-ld
AR      := kos-ar
OBJDUMP := sh-elf-objdump
OBJCOPY := sh-elf-objcopy

OPT_FLAGS := 

ifeq ($(TARGET_N64),1)
  TARGET_CFLAGS := -nostdinc -DTARGET_N64 -D_LANGUAGE_C 
  CC_CFLAGS := -fno-builtin
endif

ifeq ($(TARGET_DC),1)
  TARGET_CFLAGS := -DGBI_FLOATS -fno-strict-aliasing -Wall -Os -DTARGET_DC -D_LANGUAGE_C -DTARGET_DC -D_LANGUAGE_C  -Iinclude -Ibuild/us -Ibuild/us/include -Isrc -Isrc/racing -Isrc/ending -I. -DVERSION_US=1 -DF3DEX_GBI=1 -DF3D_OLD=1 -D_LANGUAGE_C=1 -DNON_MATCHING=1 -DAVOID_UB=1 -DAVOID_UB=1 -DGCC=1 -g3 -Wall -Wextra -DENABLE_OPENGL -Os -Wno-incompatible-pointer-types -Wno-int-conversion
  CC_CFLAGS :=
endif

INCLUDE_DIRS := include $(BUILD_DIR) $(BUILD_DIR)/include src src/racing src/ending .
ifeq ($(TARGET_N64),1)
  INCLUDE_DIRS += include/libc
endif

C_DEFINES := $(foreach d,$(DEFINES),-D$(d))
DEF_INC_CFLAGS := $(foreach i,$(INCLUDE_DIRS),-I$(i)) $(C_DEFINES)

# Prefer clang as C preprocessor if installed on the system
ifneq (,$(call find-command,clang))
  CPP      := clang
  CPPFLAGS := -E -P -x c -Wno-trigraphs $(DEF_INC_CFLAGS)
else ifneq (,$(call find-command,cpp))
  CPP      := cpp
  CPPFLAGS := -P -Wno-trigraphs $(DEF_INC_CFLAGS)
else
  $(error Unable to find cpp or clang)
endif

# Check code syntax with host compiler
CC_CHECK ?= kos-cc
CC_CHECK_CFLAGS := -fsyntax-only -fsigned-char $(CC_CFLAGS) $(TARGET_CFLAGS) -std=gnu90 -Wall -Wempty-body -Wextra -Wno-format-security -DNON_MATCHING -DAVOID_UB $(DEF_INC_CFLAGS) -g3

# C compiler options
HIDE_WARNINGS := -woff 838,649,807
CFLAGS = $(OPT_FLAGS) $(TARGET_CFLAGS) $(MIPSISET) $(DEF_INC_CFLAGS) -g3
ifeq ($(COMPILER),kos-cc)
  CFLAGS += -Wall -Wextra -Wno-incompatible-pointer-types -Wno-int-conversion
else
#  CFLAGS += $(HIDE_WARNINGS) -non_shared -Wab,-r4300_mul -Xcpluscomm -fullwarn -signed -32
endif

ASFLAGS = -I include -I $(BUILD_DIR) $(VERSION_ASFLAGS) $(foreach d,$(DEFINES),--defsym $(d))

# Fills end of rom
OBJCOPYFLAGS =
# --pad-to=0xC00000 --gap-fill=0xFF

LDFLAGS = -Map $(BUILD_DIR)/$(TARGET).map --no-check-sections

# Ensure that gcc treats the code as 32-bit
#CC_CHECK += -m32

# Prevent a crash with -sopt
export LANG := C



#==============================================================================#
# Miscellaneous Tools                                                          #
#==============================================================================#

MIO0TOOL              := $(TOOLS_DIR)/mio0
N64CKSUM              := $(TOOLS_DIR)/n64cksum
N64GRAPHICS           := $(TOOLS_DIR)/n64graphics
DLPACKER              := $(TOOLS_DIR)/displaylist_packer
BIN2C                 := $(PYTHON) $(TOOLS_DIR)/bin2c.py
EXTRACT_DATA_FOR_MIO  := $(TOOLS_DIR)/extract_data_for_mio
ASSET_EXTRACT         := $(PYTHON) $(TOOLS_DIR)/new_extract_assets.py
LINKONLY_GENERATOR    := $(PYTHON) $(TOOLS_DIR)/linkonly_generator.py
TORCH                 := $(TOOLS_DIR)/torch/cmake-build-release/torch
EMULATOR               = mupen64plus
EMU_FLAGS              = --noosd
LOADER                 = loader64
LOADER_FLAGS           = -vwf
SHA1SUM               ?= sha1sum
FALSE                 ?= false
PRINT                 ?= printf
TOUCH                 ?= touch

ifeq ($(COLOR),1)
NO_COL  := \033[0m
RED     := \033[0;31m
GREEN   := \033[0;32m
BLUE    := \033[0;34m
YELLOW  := \033[0;33m
BLINK   := \033[33;5m
endif

# Use Objcopy instead of extract_data_for_mio
#ifeq ($(COMPILER),gcc)
  EXTRACT_DATA_FOR_MIO := $(OBJCOPY) -O binary --only-section=.data
#endif

# Common build print status function
define print
  @$(PRINT) "$(GREEN)$(1) $(YELLOW)$(2)$(GREEN) -> $(BLUE)$(3)$(NO_COL)\n"
endef

# Override commmands for GCC Safe Files
ifeq ($(GCC),1)
  $(BUILD_DIR)/src/gfx/gfx_retro_dc.o: TARGET_CFLAGS += -O3 
  $(BUILD_DIR)/src/audio/mixer.o: TARGET_CFLAGS += -O3 
  $(BUILD_DIR)/src/audio/synthesis.o: TARGET_CFLAGS += -O3 
  $(BUILD_DIR)/src/racing/math_util.o: TARGET_CFLAGS += -O3
  $(BUILD_DIR)/src/math_util_2.o: TARGET_CFLAGS += -O3
  $(BUILD_DIR)/src/racing/collision.o: TARGET_CFLAGS += -O3
  $(BUILD_DIR)/courses/%/course_textures.linkonly.o: TARGET_CFLAGS += -fno-lto
  $(BUILD_DIR)/courses/%/course_displaylists.inc.o: TARGET_CFLAGS += -fno-lto
  $(BUILD_DIR)/courses/%/course_vertices.inc.o: TARGET_CFLAGS += -fno-lto
  $(BUILD_DIR)/courses/%/course_data.o: TARGET_CFLAGS += -fno-lto
  $(BUILD_DIR)/assets/code/startup_logo/startup_logo.o: TARGET_CFLAGS += -fno-lto
  $(BUILD_DIR)/assets/code/ceremony_data/ceremony_data.o: TARGET_CFLAGS += -fno-lto
  $(BUILD_DIR)/src/data/data_segment2.o: TARGET_CFLAGS += -fno-lto
  $(BUILD_DIR)/src/data/textures.o: TARGET_CFLAGS += -fno-lto
  $(BUILD_DIR)/courses/staff_ghost_data.o: TARGET_CFLAGS += -fno-lto
  $(SAFE_C_FILES): OPT_FLAGS := 
  $(SAFE_C_FILES): CC        := kos-cc
  $(SAFE_C_FILES): CC_CHECK := kos-cc
endif



#==============================================================================#
# Main Targets                                                                 #
#==============================================================================#

all: $(ELF)
ifeq ($(COMPARE),1)
	@$(PRINT) "$(GREEN)Checking if ROM matches.. $(NO_COL)\n"
	@$(SHA1SUM) -c $(TARGET).sha1 > $(NULL_OUT) && $(PRINT) "$(TARGET): $(GREEN)OK$(NO_COL)\n" || ($(PRINT) "$(YELLOW)Building the ROM file has succeeded, but does not match the original ROM.\nThis is expected, and not an error, if you are making modifications.\nTo silence this message, use 'make COMPARE=0.' $(NO_COL)\n" && $(FALSE))
endif

assets:
	@echo "Extracting torch assets..."
	$(V)$(TORCH) code $(BASEROM)
	$(V)$(TORCH) header $(BASEROM)
	$(V)$(TORCH) modding export $(BASEROM)

doc:
	$(V)$(PYTHON) $(TOOLS_DIR)/doxygen_symbol_gen.py
	doxygen
	@$(PRINT) "$(GREEN)Documentation generated in docs/html$(NO_COL)\n"
	@$(PRINT) "$(GREEN)Results can be viewed by opening docs/html/index.html in a web browser$(NO_COL)\n"

format:
	@$(PYTHON) $(TOOLS_DIR)/format.py -j $(N_THREADS)

clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) -r build/$(ELF)
	rm -f IP.BIN
	rm -f 1ST_READ.BIN
	rm -f mariokart64.iso
	rm -f mk64.bin
	rm -f mariokart64.cdi
	rm -f mariokart64.ds.iso
	rm -f loader.bin
	rm -f loader.elf
	rm -f data_segment2.O
	rm -f test.o
	rm -f memcpy32.o
	rm -rf tmp

model_extract: $(MODELS_PROC)

fast64_blender:
	$(BLENDER) --python tools/blender/fast64_run.py

distclean: distclean_assets
	$(RM) -r $(BUILD_DIR_BASE)
	$(PYTHON) extract_assets.py --clean
	make -C $(TOOLS_DIR) clean

distclean_assets: ;

test: $(ROM)
	$(EMULATOR) $(EMU_FLAGS) $<

load: $(ROM)
	$(LOADER) $(LOADER_FLAGS) $<

# Make sure build directory exists before compiling anything
DUMMY != mkdir -p $(ALL_DIRS)

#==============================================================================#
# Texture Generation                                                           #
#==============================================================================#

# RGBA32, RGBA16, IA16, IA8, IA4, IA1, I8, I4
$(BUILD_DIR)/%: %.png
	@$(PRINT) "$(GREEN)Converting:  $(BLUE) $< -> $@$(NO_COL)\n"
	$(V)$(N64GRAPHICS) -i $@ -g $< -f $(lastword $(subst ., ,$@))

$(BUILD_DIR)/textures/%.mio0: $(BUILD_DIR)/textures/%
	$(V)$(MIO0TOOL) -c $< $@

ASSET_INCLUDES := $(shell find $(ASSET_DIR)/include -type f -name "*.mk")

$(foreach inc,$(ASSET_INCLUDES),$(eval include $(inc)))



#==============================================================================#
# Compressed Segment Generation                                                #
#==============================================================================#

$(BUILD_DIR)/%.mio0: %.bin
	@$(PRINT) "$(GREEN)Compressing binary files:  $(BLUE)$@ $(NO_COL)\n"
	$(V)$(MIO0TOOL) -c $< $@

$(BUILD_DIR)/%.mio0.o: $(BUILD_DIR)/%.mio0.s
	@$(PRINT) "$(GREEN)Compiling mio0:  $(BLUE)$@ $(NO_COL)\n"
	$(V)$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/%.mio0.s: $(BUILD_DIR)/%.mio0
	$(call print,Generating mio0 asm:,$<,$@)
	$(V)$(PRINT) ".section .data\n\n.balign 4\n\n.incbin \"$<\"\n" > $@

$(BUILD_DIR)/src/crash_screen.o: src/crash_screen.c
	@$(PRINT) "$(GREEN)Compiling Crash Screen:  $(BLUE)$@ $(NO_COL)\n"
	$(V)$(N64GRAPHICS) -i $(BUILD_DIR)/textures/crash_screen/crash_screen_font.ia1.inc.c -g textures/crash_screen/crash_screen_font.ia1.png -f ia1 -s u8
#	@$(CC_CHECK) $(CC_CHECK_CFLAGS) -MMD -MP -MT $@ -MF $(BUILD_DIR)/$*.d $<
	$(V)$(CC) -fno-lto -c $(CFLAGS) -o $@ $<
#	$(V)$(PYTHON) $(TOOLS_DIR)/set_o32abi_bit.py $@

#==============================================================================#
# Common Textures Segment Generation                                           #
#==============================================================================#

TEXTURE_FILES := $(foreach dir,$(TEXTURE_DIRS),$(subst .png, , $(wildcard $(dir)/*)))

TEXTURE_FILES_TLUT := $(foreach dir,$(TEXTURE_DIRS)/tlut,$(subst .png, , $(wildcard $(dir)/*)))


$(TEXTURE_FILES):
	@$(PRINT) "$(GREEN)Converting:  $(BLUE) $< -> $@$(NO_COL)\n"
	$(V)$(N64GRAPHICS) -i $(BUILD_DIR)/$@.inc.c -g $@.png -f $(lastword $(subst ., ,$@)) -s u8

# TLUT
$(TEXTURE_FILES_TLUT):
	@$(PRINT) "$(GREEN)Converting:  $(BLUE) $< -> $@$(NO_COL)\n"
	$(V)$(N64GRAPHICS) -i $(BUILD_DIR)/$@.inc.c -g $@.png -f $(lastword $(subst ., ,$@)) -s u8 -c $(lastword $(subst ., ,$(subst .$(lastword $(subst ., ,$(TEXTURE_FILES_TLUT))), ,$(TEXTURE_FILES_TLUT)))) -p $(BUILD_DIR)/$@.tlut.inc.c

# common textures
$(BUILD_DIR)/assets/code/common_data/common_data.o: assets/code/common_data/common_data.c $(TEXTURE_FILES) $(TEXTURE_FILES_TLUT)
	@$(PRINT) "$(GREEN)Compiling Common Textures:  $(BLUE)$@ $(NO_COL)\n"
#	@$(CC_CHECK) $(CC_CHECK_CFLAGS) -MMD -MP -MT $@ -MF $(BUILD_DIR)/$*.d $<
	$(V)$(CC) -fno-lto -c $(CFLAGS) -o $@ $<
#	$(V)$(PYTHON) $(TOOLS_DIR)/set_o32abi_bit.py $@



#==============================================================================#
# Course Packed Displaylists Generation                                        #
#==============================================================================#


%/course_textures.linkonly.c %/course_textures.linkonly.h: %/course_offsets.c
	$(V)$(LINKONLY_GENERATOR) $(lastword $(subst /, ,$*))

# Its unclear why this is necessary. Everything I undesrtand about `make` says that just
# `$(BUILD_DIR)/%/course_displaylists.inc.o: %/course_textures.linkonly.h`
# Should work identical to this. But in practice it doesn't :(
COURSE_DISPLAYLIST_OFILES := $(foreach dir,$(COURSE_DIRS),$(BUILD_DIR)/$(dir)/course_displaylists.inc.o)
$(COURSE_DISPLAYLIST_OFILES): $(BUILD_DIR)/%/course_displaylists.inc.o: %/course_textures.linkonly.h

%/course_textures.linkonly.elf: %/course_textures.linkonly.o
	$(V)$(LD) -EL -t -e 0 -Ttext=05000000 -Map $@.map -o $@ $< --no-check-sections

%/course_displaylists.inc.elf: %/course_displaylists.inc.o %/course_textures.linkonly.elf
	$(V)$(LD) -EL -t -e 0 -Ttext=07000000 -Map $@.map -R $*/course_textures.linkonly.elf -o $@ $< --no-check-sections

%/course_displaylists.inc.bin: %/course_displaylists.inc.elf
	$(V)$(EXTRACT_DATA_FOR_MIO) $< $@

#==============================================================================#
# Course Geography Generation                                                  #
#==============================================================================#

COURSE_GEOGRAPHY_TARGETS := $(foreach dir,$(COURSE_DIRS),$(BUILD_DIR)/$(dir)/course_geography.mio0.o)

%/course_vertices.inc.elf: %/course_vertices.inc.o
	$(V)$(LD) -EL -t -e 0 -Ttext=0F000000 -Map $@.map -o $@ $< --no-check-sections

%/course_vertices.inc.bin: %/course_vertices.inc.elf
	$(V)$(EXTRACT_DATA_FOR_MIO) $< $@

%/course_vertices.inc.mio0: %/course_vertices.inc.bin
	@$(PRINT) "$(GREEN)Compressing Course Geography:  $(BLUE)$@ $(NO_COL)\n"
	$(V)$(MIO0TOOL) -c $< $@

# Course vertices and displaylists are included together due to no alignment between the two files.
#%/course_geography.mio0.s: %/course_vertices.inc.mio0 %/course_displaylists_packed.inc.bin
%/course_geography.mio0.s: %/course_vertices.inc.mio0 %/course_displaylists.inc.bin
	$(V)$(PRINT) ".include \"macros.inc\"\n\n.section .data\n\n.balign 4\n\nglabel __$(lastword $(subst /, ,$*))_vertexSegmentRomStart\n\nglabel _d_course_$(lastword $(subst /, ,$*))_vertex\n\n.incbin \"$(@D)/course_vertices.inc.mio0\"\n\n.balign 4\n\nglabel _d_course_$(lastword $(subst /, ,$*))_packed\n\n.incbin \"$(@D)/course_displaylists.inc.bin\"\n\n.balign 0x10\nglabel __$(lastword $(subst /, ,$*))_vertexSegmentRomEnd\n\n" > $@



#==============================================================================#
# Course Data Generation                                                       #
#==============================================================================#

COURSE_DATA_ELFS := $(foreach dir,$(COURSE_DIRS),$(BUILD_DIR)/$(dir)/course_data.elf)
LDFLAGS += $(foreach elf,$(COURSE_DATA_ELFS),-R $(elf)) 

CFLAGS += -fno-data-sections
COURSE_DATA_TARGETS := $(foreach dir,$(COURSE_DIRS),$(BUILD_DIR)/$(dir)/course_data.mio0.o)

COURSE_DISPLAYLIST_OFILES := $(foreach dir,$(COURSE_DIRS),$(BUILD_DIR)/$(dir)/course_data.o)
$(COURSE_DISPLAYLIST_OFILES): $(BUILD_DIR)/%/course_data.o: %/course_textures.linkonly.h

%/course_data.elf: %/course_data.o %/course_displaylists.inc.elf
	$(V)$(LD) -EL -t -e 0 -Ttext=06000000 -Map $@.map -R $*/course_displaylists.inc.elf -o $@ $< --no-check-sections

%/course_data.bin: %/course_data.elf
	$(V)$(EXTRACT_DATA_FOR_MIO) $< $@

%/course_data.mio0: %/course_data.bin
	@$(PRINT) "$(GREEN)Compressing Course Data:  $(BLUE)$@ $(NO_COL)\n"
	$(V)$(MIO0TOOL) -c $< $@

%/course_data.mio0.s: %/course_data.mio0
	$(V)$(PRINT) ".include \"macros.inc\"\n\n.section .data\n\n.balign 4\n\nglabel __course_$(lastword $(subst /, ,$*))_dl_mio0SegmentRomStart\n\n.incbin \"$<\"\n\nglabel __course_$(lastword $(subst /, ,$*))_dl_mio0SegmentRomEnd\n\n" > $@


#==============================================================================#
# Source Code Generation                                                       #
#==============================================================================#
#$(BUILD_DIR)/%.jp.c: %.c
#	$(call print,Encoding:,$<,$@)
#	$(V)iconv -t EUC-JP -f UTF-8 $< > $@

$(BUILD_DIR)/%.o: %.c
	$(call print,Compiling:,$<,$@)
#	$(V)$(CC_CHECK) $(CC_CHECK_CFLAGS) -MMD -MP -MT $@ -MF $(BUILD_DIR)/$*.d $<
	$(V)$(CC) -c $(CFLAGS) -o $@ $<
#	$(V)$(PYTHON) $(TOOLS_DIR)/set_o32abi_bit.py $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c
	$(call print,Compiling:,$<,$@)
#	$(V)$(CC_CHECK) $(CC_CHECK_CFLAGS) -MMD -MP -MT $@ -MF $(BUILD_DIR)/$*.d $<
	$(V)$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: %.s $(MIO0_FILES) $(RAW_TEXTURE_FILES)
	$(V)$(AS) $(ASFLAGS) -o $@ $<

#$(EUC_JP_FILES:%.c=$(BUILD_DIR)/%.jp.o): CC := $(PYTHON) $(TOOLS_DIR)/asm_processor/build.py $(CC) -- $(AS) $(ASFLAGS) --

$(GLOBAL_ASM_O_FILES): CC := $(PYTHON) $(TOOLS_DIR)/asm_processor/build.py $(CC) -- $(AS) $(ASFLAGS) --

$(GLOBAL_ASM_OS_O_FILES): CC := $(PYTHON) $(TOOLS_DIR)/asm_processor/build.py $(CC) -- $(AS) $(ASFLAGS) --

$(GLOBAL_ASM_AUDIO_O_FILES): CC := $(PYTHON) $(TOOLS_DIR)/asm_processor/build.py $(CC) -- $(AS) $(ASFLAGS) --

$(GLOBAL_ASM_RACING_O_FILES): CC := $(PYTHON) $(TOOLS_DIR)/asm_processor/build.py $(CC) -- $(AS) $(ASFLAGS) --

#==============================================================================#
# Libultra Definitions                                                         #
#==============================================================================#

$(BUILD_DIR)/src/os/%.o:          OPT_FLAGS := 
$(BUILD_DIR)/src/os/math/%.o:     OPT_FLAGS := 
$(BUILD_DIR)/src/os/math/ll%.o:   OPT_FLAGS := 
#$(BUILD_DIR)/src/os/math/ll%.o:   MIPSISET := -mips3
$(BUILD_DIR)/src/os/ldiv.o:       OPT_FLAGS := 
$(BUILD_DIR)/src/os/string.o:     OPT_FLAGS := 
$(BUILD_DIR)/src/os/gu%.o:        OPT_FLAGS := 
$(BUILD_DIR)/src/os/al%.o:        OPT_FLAGS := 
$(BUILD_DIR)/src/os/__osLeoInterrupt.o:        OPT_FLAGS := 
$(BUILD_DIR)/src/os/_Printf.o:        OPT_FLAGS := 
$(BUILD_DIR)/src/os/_Litob.o:        OPT_FLAGS := 
$(BUILD_DIR)/src/os/_Ldtob.o:        OPT_FLAGS := 
$(BUILD_DIR)/src/os/osSyncPrintf.o:        OPT_FLAGS := 

# Alternate compiler flags needed for matching
ifeq ($(COMPILER),ido)
    $(BUILD_DIR)/src/audio/%.o:        OPT_FLAGS := -O2 -use_readwrite_const
    $(BUILD_DIR)/src/audio/port_eu.o:  OPT_FLAGS := -O2 -framepointer
    $(BUILD_DIR)/src/audio/external.o:  OPT_FLAGS := -O2 -framepointer
endif



#==============================================================================#
# Compile Trophy and Podium Models                                             #
#==============================================================================#

LDFLAGS += -R $(BUILD_DIR)/assets/code/ceremony_data/ceremony_data.elf

%/ceremony_data.elf: %/ceremony_data.o
	$(V)$(LD) -EL -t -e 0 -Ttext=0B000000 -Map $@.map -o $@ $< --no-check-sections

%/ceremony_data.bin: %/ceremony_data.elf
	$(V)$(EXTRACT_DATA_FOR_MIO) $< $@

%/ceremony_data.mio0: %/ceremony_data.bin
	@$(PRINT) "$(GREEN)Compressing Trophy Model:  $(BLUE)$@ $(NO_COL)\n"
	$(V)$(MIO0TOOL) -c $< $@

%/ceremony_data.mio0.s: %/ceremony_data.mio0
	$(V)$(PRINT) ".include \"macros.inc\"\n\n.data\n\n.balign 4\n\nglabel __ceremonyDataSegmentRomStart\n\nglabel _ceremony_data\n\n.incbin \"$<\"\n\n.balign 16\nglabel __ceremonyDataSegmentRomEnd\nglabel _ceremonyData_end\n" > $@


#==============================================================================#
# Compile Startup Logo                                                         #
#==============================================================================#

LDFLAGS += -R $(BUILD_DIR)/assets/code/startup_logo/startup_logo.elf

%/startup_logo.elf: %/startup_logo.o
	$(V)$(LD) -m shlelf -t -e 0 -Ttext=06000000 -Map $@.map -o $@ $< --no-check-sections

%/startup_logo.bin: %/startup_logo.elf
	$(V)$(EXTRACT_DATA_FOR_MIO) $< $@

%/startup_logo.mio0: %/startup_logo.bin
	@$(PRINT) "$(GREEN)Compressing Startup Logo Model:  $(BLUE)$@ $(NO_COL)\n"
	$(V)$(MIO0TOOL) -c $< $@

%/startup_logo.mio0.s: %/startup_logo.mio0
	$(V)$(PRINT) ".include \"macros.inc\"\n\n.data\n\n.balign 4\n\nglabel __startupLogoSegmentRomStart\n\nglabel _startup_logo\n\n.incbin \"$<\"\n\n.balign 16\n\nglabel __startupLogoSegmentRomEnd\n\nglabel _startupLogo_end\n" > $@

#==============================================================================#
# Compile Common Textures                                                      #
#==============================================================================#

LDFLAGS += -R $(BUILD_DIR)/assets/code/common_data/common_data.elf

%/common_data.elf: %/common_data.o
	$(V)$(LD) -EL -t -e 0 -Ttext=0D000000 -Map $@.map -o $@ $< --no-check-sections

%/common_data.bin: %/common_data.elf
	$(V)$(EXTRACT_DATA_FOR_MIO) $< $@

%/common_data.mio0: %/common_data.bin
	@$(PRINT) "$(GREEN)Compressing Common Textures:  $(BLUE)$@ $(NO_COL)\n"
	$(V)$(MIO0TOOL) -c $< $@

%/common_data.mio0.s: %/common_data.mio0
	$(V)$(PRINT) ".include \"macros.inc\"\n\n.section .data\n\nglabel __common_texturesSegmentRomStart\n\n.balign 4\n\n.incbin \"$<\"\n\nglabel __common_texturesSegmentRomEnd\n\n" > $@



#==============================================================================#
# Finalize and Link                                                            #
#==============================================================================#

memcpy32.o: memcpy32.S
	kos-as memcpy32.S -o memcpy32.o

test.o: $(O_FILES)
	sh-elf-ld -EL --relocatable build/us/data/zkarts/all_kart.o build/us/courses/staff_ghost_data.o -o test.o
	sh-elf-objcopy --add-symbol __kart_texturesSegmentRomEnd=.data:0x004fcac4 test.o

data_segment2.o: $(O_FILES)
	sh-elf-ld -EL --relocatable build/us/src/data/textures.o build/us/src/data/data_segment2.o -o data_segment2.o
	sh-elf-objcopy --add-symbol __data_segment2SegmentRomStart=.data:0x0 data_segment2.o
	sh-elf-objcopy --add-symbol __data_segment2SegmentRomEnd=.data:0x00007d90 data_segment2.o

# Link MK64 ELF file
REAL_OBJFILES := build/us/src/vmu.o build/us/src/dcaudio/driver.o build/us/src/buffers/audio_heap.o build/us/src/audio/audio_session_presets.o build/us/src/audio/effects.o build/us/src/audio/mixer.o build/us/src/audio/heap.o build/us/src/audio/playback.o build/us/src/audio/seqplayer.o build/us/src/audio/data.o build/us/src/audio/load.o build/us/src/audio/port_eu.o build/us/src/audio/synthesis.o build/us/src/audio/external.o build/us/src/buffers/all_kinds_of_buffers.o build/us/src/gfx/gfx_cc.o build/us/src/gfx/gfx_dc.o build/us/src/gfx/gfx_gldc.o build/us/src/gfx/gfx_retro_dc.o memcpy32.o build/us/src/dcprofiler.o test.o build/us/src/compression/libmio0.o build/us/src/compression/libtkmk00.o build/us/courses/mario_raceway/course_offsets.o build/us/courses/choco_mountain/course_offsets.o build/us/courses/bowsers_castle/course_offsets.o build/us/courses/banshee_boardwalk/course_offsets.o build/us/courses/yoshi_valley/course_offsets.o build/us/courses/frappe_snowland/course_offsets.o build/us/courses/koopa_troopa_beach/course_offsets.o build/us/courses/royal_raceway/course_offsets.o build/us/courses/luigi_raceway/course_offsets.o build/us/courses/moo_moo_farm/course_offsets.o build/us/courses/toads_turnpike/course_offsets.o build/us/courses/kalimari_desert/course_offsets.o build/us/courses/sherbet_land/course_offsets.o build/us/courses/rainbow_road/course_offsets.o build/us/courses/wario_stadium/course_offsets.o build/us/courses/block_fort/course_offsets.o build/us/courses/skyscraper/course_offsets.o build/us/courses/double_deck/course_offsets.o build/us/courses/dks_jungle_parkway/course_offsets.o build/us/courses/big_donut/course_offsets.o build/us/assets/code/startup_logo/startup_logo.mio0.o build/us/assets/code/ceremony_data/ceremony_data.mio0.o data_segment2.o build/us/data/textures_0a.o build/us/data/other_textures.o build/us/data/texture_tkmk00.o build/us/src/code_800029B0.o build/us/src/profiler.o build/us/src/crash_screen.o build/us/src/animation.o build/us/src/staff_ghosts.o build/us/src/cpu_vehicles_camera_path.o build/us/src/camera.o build/us/src/render_player.o build/us/src/kart_dma.o build/us/src/player_controller.o build/us/src/spawn_players.o build/us/src/code_8003DC40.o build/us/src/gbiMacro.o build/us/src/math_util_2.o build/us/src/render_objects.o build/us/src/code_80057C60.o build/us/src/code_80086E70.o build/us/src/effects.o build/us/src/code_80091440.o build/us/src/menu_items.o build/us/src/code_800AF9B0.o build/us/src/menus.o build/us/src/save.o build/us/src/ending/credits.o build/us/src/racing/race_logic.o build/us/src/racing/render_courses.o build/us/src/racing/actors.o build/us/src/racing/skybox_and_splitscreen.o build/us/src/racing/memory.o build/us/src/racing/collision.o build/us/src/racing/actors_extended.o build/us/src/racing/math_util.o build/us/courses/courseTable.o build/us/src/buffers/random.o build/us/src/buffers/buffers.o build/us/src/update_objects.o build/us/src/ending/code_80280000.o build/us/src/ending/podium_ceremony_actors.o build/us/src/ending/camera_junk.o build/us/src/ending/code_80281780.o build/us/src/ending/code_80281C40.o build/us/src/ending/ceremony_and_credits.o build/us/src/ending/dl_unk_80284EE0.o build/us/src/data/path_spawn_metadata.o build/us/src/code_80057C60_var.o build/us/src/data/some_data.o build/us/src/data/kart_attributes.o build/us/assets/code/data_800E8700/data_800E8700.o build/us/assets/code/data_800E45C0/data_800E45C0.o build/us/src/code_8006E9C0.o build/us/src/os/gu*.o build/us/src/os/ultra_reimpl.o build/us/src/buffers/trig_tables.o build/us/src/main.o
$(ELF):	$(O_FILES) memcpy32.o test.o data_segment2.o $(COURSE_DATA_TARGETS) $(BUILD_DIR)/assets/code/startup_logo/startup_logo.mio0.o $(BUILD_DIR)/assets/code/ceremony_data/ceremony_data.mio0.o $(BUILD_DIR)/assets/code/common_data/common_data.mio0.o $(COURSE_GEOGRAPHY_TARGETS) undefined_syms.txt
	@$(PRINT) "$(GREEN)Linking ELF file:  $(BLUE)$@ $(NO_COL)\n"
	kos-cc -g3 -o ${BUILD_DIR_BASE}/$@ -Xlinker -Map=build/us/mario-kart.elf.map -Wl,--just-symbols=build/us/assets/code/common_data/common_data.elf -Wl,--just-symbols=build/us/courses/mario_raceway/course_data.elf -Wl,--just-symbols=build/us/courses/choco_mountain/course_data.elf -Wl,--just-symbols=build/us/courses/bowsers_castle/course_data.elf -Wl,--just-symbols=build/us/courses/banshee_boardwalk/course_data.elf -Wl,--just-symbols=build/us/courses/yoshi_valley/course_data.elf -Wl,--just-symbols=build/us/courses/frappe_snowland/course_data.elf -Wl,--just-symbols=build/us/courses/koopa_troopa_beach/course_data.elf -Wl,--just-symbols=build/us/courses/royal_raceway/course_data.elf -Wl,--just-symbols=build/us/courses/luigi_raceway/course_data.elf -Wl,--just-symbols=build/us/courses/moo_moo_farm/course_data.elf -Wl,--just-symbols=build/us/courses/toads_turnpike/course_data.elf -Wl,--just-symbols=build/us/courses/kalimari_desert/course_data.elf -Wl,--just-symbols=build/us/courses/sherbet_land/course_data.elf -Wl,--just-symbols=build/us/courses/rainbow_road/course_data.elf -Wl,--just-symbols=build/us/courses/wario_stadium/course_data.elf -Wl,--just-symbols=build/us/courses/block_fort/course_data.elf -Wl,--just-symbols=build/us/courses/skyscraper/course_data.elf -Wl,--just-symbols=build/us/courses/double_deck/course_data.elf -Wl,--just-symbols=build/us/courses/dks_jungle_parkway/course_data.elf -Wl,--just-symbols=build/us/courses/big_donut/course_data.elf -Wl,--just-symbols=build/us/assets/code/startup_logo/startup_logo.elf -Wl,--just-symbols=build/us/assets/code/ceremony_data/ceremony_data.elf -Wl,--just-symbols=build/us/courses/yoshi_valley/course_data.elf -Wl,--just-symbols=build/us/courses/skyscraper/course_data.elf -Wl,--just-symbols=build/us/courses/choco_mountain/course_data.elf -Wl,--just-symbols=build/us/courses/block_fort/course_data.elf -Wl,--just-symbols=build/us/courses/bowsers_castle/course_data.elf -Wl,--just-symbols=build/us/courses/toads_turnpike/course_data.elf -Wl,--just-symbols=build/us/courses/kalimari_desert/course_data.elf -Wl,--just-symbols=build/us/courses/luigi_raceway/course_data.elf -Wl,--just-symbols=build/us/courses/frappe_snowland/course_data.elf -Wl,--just-symbols=build/us/courses/rainbow_road/course_data.elf -Wl,--just-symbols=build/us/courses/double_deck/course_data.elf -Wl,--just-symbols=build/us/courses/mario_raceway/course_data.elf -Wl,--just-symbols=build/us/courses/big_donut/course_data.elf -Wl,--just-symbols=build/us/courses/wario_stadium/course_data.elf -Wl,--just-symbols=build/us/courses/koopa_troopa_beach/course_data.elf -Wl,--just-symbols=build/us/courses/sherbet_land/course_data.elf -Wl,--just-symbols=build/us/courses/dks_jungle_parkway/course_data.elf -Wl,--just-symbols=build/us/courses/banshee_boardwalk/course_data.elf -Wl,--just-symbols=build/us/courses/moo_moo_farm/course_data.elf -Wl,--just-symbols=build/us/courses/royal_raceway/course_data.elf $(REAL_OBJFILES) -lGL
	./generate_dc_data.sh

ifeq ($(HAVE_CDI4DC), yes)

IP.BIN:
	rm -f IP.BIN
	$(KOS_BASE)/utils/makeip/makeip ip.txt IP.BIN

1ST_READ.BIN: loader.bin
	rm -f 1ST_READ.BIN
	$(KOS_BASE)/utils/scramble/scramble loader.bin 1ST_READ.BIN

tmp: mk64.bin 1ST_READ.BIN
	rm -rf tmp
	mkdir tmp
	cp -r dc_data tmp/dc_data
	cp mk64.bin tmp/mk64.bin
	cp ghost.ico tmp/ghost.ico
	cp kart.ico tmp/kart.ico
	cp 1ST_READ.BIN tmp/1ST_READ.BIN

mariokart64.iso: IP.BIN loader.bin tmp
	rm -f mariokart64.iso
	mkisofs -C 0,11702 -V "Mario Kart 64" -G IP.BIN -r -J -l -o mariokart64.iso tmp
	rm -rf tmp

mariokart64.ds.iso: IP.BIN loader.bin tmp
	rm -f mariokart64.ds.iso
	cp loader.bin tmp/1ST_READ.BIN
	mkisofs -V "Mario Kart 64" -G IP.BIN -r -J -l -o mariokart64.ds.iso tmp
	rm -rf tmp

mariokart64.cdi: mariokart64.iso
	cdi4dc mariokart64.iso mariokart64.cdi > cdi.log
	@echo && echo && echo "*** CDI Baked Successfully ($@) ***" && echo && echo

dsiso: mariokart64.ds.iso

else

mariokart64.cdi: mk64.bin loader.elf
	@test -s ${BUILD_DIR_BASE}/mario-kart.elf || { echo "Please run make before running make cdi . Exiting"; exit 1; }
	$(RM) mariokart64.cdi
	mkdcdisc -f ghost.ico -f kart.ico -f mk64.bin -d dc_data -e loader.elf -o mariokart64.cdi -n "Mario Kart 64" -v 3

endif

loader.elf: loader.c
	kos-cc loader.c -o loader.elf -Iinclude

loader.bin: loader.elf
	rm -f loader.bin
	kos-objcopy -R .stack -O binary loader.elf loader.bin

mk64.bin: ${ELF}
	rm -f mk64.bin
	kos-objcopy -R .stack -O binary ${BUILD_DIR_BASE}/${ELF} mk64.bin

flycast: mariokart64.cdi
	flycast mariokart64.cdi

run: loader.elf mk64.bin
	$(KOS_LOADER) loader.elf -f

cdi: mariokart64.cdi

dcload:
	sudo ./dcload-ip/host-src/tool/dc-tool-ip -x ${BUILD_DIR_BASE}/mario-kart.elf -c ./

.PHONY: all clean distclean distclean_assets default diff test load assets
# with no prerequisites, .SECONDARY causes no intermediate target to be removed
.SECONDARY:

# Remove built-in rules, to improve performance
MAKEFLAGS += --no-builtin-rules

-include $(DEP_FILES)


print-% : ; $(info $* is a $(flavor $*) variable set to [$($*)]) @true
