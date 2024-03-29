# Makefile by Adam, 2019

################################################################################
# This tells the makefile to use the xtensa binaries to build
################################################################################

GCC_FOLDER = $(ESP_ROOT)/xtensa-lx106-elf
ESPTOOL_PY = python ../pyFlashGui/esptool/esptool.py
SDK        = ./ESP8266_NONOS_SDK

XTGCCLIB     = $(GCC_FOLDER)/lib/gcc/xtensa-lx106-elf/4.8.5/libgcc.a
PREFIX       = $(GCC_FOLDER)/bin/xtensa-lx106-elf-
CC           = $(PREFIX)gcc
OBJCOPY      = $(PREFIX)objcopy
LD           = $(PREFIX)ld
AR           = $(PREFIX)ar
CP           = cp

################################################################################
# Options for burning
################################################################################

FLASH_WRITE_FLAGS = --flash_size 2MB # --verify 

# These values all come from the Getting Started Guide, tables 4-1 & 4-2, for 2M flash
# https://www.espressif.com/sites/default/files/documentation/2a-esp8266-sdk_getting_started_guide_en.pdf
MAX_SPI_FLASH_SIZE = 0xC0000

FW_FILE1_ADDR = 0x00000
FW_FILE1_SIZE = 0x10000

FW_FILE2_ADDR = 0x10000 # FW_FILE1_ADDR + FW_FILE1_SIZE
# FW_FILE2_SIZE _MUST_ be in sync with irom0_0_seg len in eagle.app.v6.ld
# There's 0xC0000 total space for code, persistent storage, and assets
# This partition is just for code
FW_FILE2_SIZE = 0x6D000

# This partition is for persistent storage
USER_SETTINGS_ADDR = 0x7D000 # $(FW_FILE2_ADDR) + $(FW_FILE2_SIZE)
USER_SETTINGS_SIZE =  0x3000

# This partition is for assets
ASSETS_ADDR = 0x80000 # $(USER_SETTINGS_ADDR) + $(USER_SETTINGS_SIZE)
ASSETS_SIZE = 0x40000 # $(MAX_SPI_FLASH_SIZE) - $(ASSETS_ADDR)

# Three ESP-specific partitions we must flash and register
RF_CAL_ADDR    = 0x1FB000
RF_CAL_SIZE    =   0x1000
PHY_DATA_ADDR  = 0x1FC000
PHY_DATA_SIZE  =   0x1000
SYS_PARAM_ADDR = 0x1FD000
SYS_PARAM_SIZE =   0x3000

# blank.bin is only 0x1000 long, so make sure to clear out all of SYS_PARAM
SYS_PARAM_ADDR2 = 0x1FE000
SYS_PARAM_ADDR3 = 0x1FF000

################################################################################
# Compilation options
################################################################################

# This is a list of directories to scan for c files recursively
SRC_DIRS_RECURSIVE = user
# Add the gdbstub directory if applicable
ifneq ($(ESP_GDB),)
  SRC_DIRS_RECURSIVE += gdbstub
endif

# This is a list of directories to scan for c files not recursively
SRC_DIRS_FLAT =

# This is a list of files to compile directly. There's no scanning here
SRC_FILES = $(SDK)/driver_lib/driver/uart.c

# This is a list of all source directories
SRC_DIRS = $(shell find $(SRC_DIRS_RECURSIVE) -type d) $(SRC_DIRS_FLAT)

# This is a list of all source c files
SOURCES   = $(shell find $(SRC_DIRS) -maxdepth 1 -iname "*.[c]") $(SRC_FILES)
S_SOURCES = $(shell find $(SRC_DIRS) -maxdepth 1 -iname "*.[s]")

# These are flags for the compiler, all files
CFLAGS = \
	-c \
	-mlongcalls \
	-flto \
	-Wl,--relax \
	-Wl,--gc-sections \
	-nostdlib \
	-Wl,-Map,output.map \
	-B$(SDK)/lib \
	-std=gnu99 \
	-MD \
	-O3
ifneq ($(ESP_GDB),)
  CFLAGS += -ggdb
endif


# These are warning flags for the compiler, all files
CFLAGS_WARNINGS = \
	-Wextra \
	-Wundef \
	-Wformat=2 \
	-Winvalid-pch \
	-Wlogical-op \
	-Wmissing-format-attribute \
	-Wmissing-include-dirs \
	-Wpointer-arith \
	-Wunused-but-set-variable \
	-Wunused-local-typedefs \
	-Wuninitialized \
	-Wshadow \
	-Wredundant-decls \
	-Wjump-misses-init \
	-Wswitch-default \
	-Wcast-align \
	-Wformat-nonliteral \
	-Wswitch-enum

# These are warning flags for the compiler, just for files outside $(SDK)
CFLAGS_WARNINGS_EXTRA = \
	-Wall \
	-Wunused \
	-Wunused-macros \
	-Wmissing-declarations \
	-Wmissing-prototypes
	# -Wstrict-prototypes
	# -Wcast-qual \
	# -Wpedantic \
	# -Wconversion \
	# -Wsign-conversion \
	# -Wdouble-promotion \

# See these values in user_config.h
# #define SWADGE_DEV_KIT  0
# #define SWADGE_BBKIWI   1
# #define SWADGE_BARREL   2
# #define SWADGE_2019     3
# #define BARREL_1_0_0    4
# #define SWADGE_CHAINSAW 5
ifeq ($(SET_SWADGE_VERSION),0)
  swadgeversion = 0
else ifeq ($(SET_SWADGE_VERSION),1)
  swadgeversion = 1
else ifeq ($(SET_SWADGE_VERSION),2)
  swadgeversion = 2
else ifeq ($(SET_SWADGE_VERSION),3)
  swadgeversion = 3
else ifeq ($(SET_SWADGE_VERSION),4)
  swadgeversion = 4
else ifeq ($(SET_SWADGE_VERSION),5)
  swadgeversion = 5
else
  swadgeversion = 5 # default to SWADGE_CHAINSAW
endif

# Used by the ESP SDK
DEFINES_LIST = \
	USE_OPTIMIZE_PRINTF=1 \
	ICACHE_FLASH=1 \

# Used by this project
DEFINES_LIST += \
	SOFTAP_CHANNEL=11 \
	WS2812_FOUR_SAMPLE=1 \
	SWADGE_VERSION=$(swadgeversion)

# Create a variable with the git hash and branch name
GIT_HASH  = $(shell git rev-parse --short HEAD)
GIT_HASH := $(GIT_HASH)-
GIT_HASH := $(GIT_HASH)$(shell git rev-parse --abbrev-ref HEAD)
GIT_HASH := \"$(GIT_HASH)\"

# Export the flash addresses and sizes to the source code
DEFINES_LIST += \
	FW_FILE1_ADDR=$(FW_FILE1_ADDR) \
	FW_FILE1_SIZE=$(FW_FILE1_SIZE) \
	FW_FILE2_ADDR=$(FW_FILE2_ADDR) \
	FW_FILE2_SIZE=$(FW_FILE2_SIZE) \
	USER_SETTINGS_ADDR=$(USER_SETTINGS_ADDR) \
	USER_SETTINGS_SIZE=$(USER_SETTINGS_SIZE) \
	ASSETS_ADDR=$(ASSETS_ADDR) \
	ASSETS_SIZE=$(ASSETS_SIZE) \
	RF_CAL_ADDR=$(RF_CAL_ADDR) \
	RF_CAL_SIZE=$(RF_CAL_SIZE) \
	PHY_DATA_ADDR=$(PHY_DATA_ADDR) \
	PHY_DATA_SIZE=$(PHY_DATA_SIZE) \
	SYS_PARAM_ADDR=$(SYS_PARAM_ADDR) \
	SYS_PARAM_SIZE=$(SYS_PARAM_SIZE) \
	GIT_HASH=$(GIT_HASH)

ifneq ($(ESP_GDB),)
  DEFINES_LIST += USE_ESP_GDB
endif

DEFINES = $(patsubst %, -D%, $(DEFINES_LIST))

# Treat every source directory as one to search for headers in, also add a few more
INC = $(patsubst %, -I%, \
	$(SRC_DIRS) \
	$(SDK)/driver_lib/include/ \
	$(SDK)/include)

# This is the directory in which object files will be stored
OBJ_DIR = obj

# This is a list of objects to build
OBJECTS =$(patsubst %.c, $(OBJ_DIR)/%.o, $(SOURCES))
OBJECTS+=$(patsubst %.S, $(OBJ_DIR)/%.o, $(S_SOURCES))

################################################################################
# Linker options
################################################################################

# This is a list of libraries to include. Order doesn't matter
LIBS = \
	main \
	lwip \
	wpa \
	net80211 \
	phy \
	espnow \
	crypto \
	pp \
	m \
	gcc_stripped \
	c

# These are directories to look for library files in
LIB_DIRS = \
	. \
	$(SDK)/lib \
	$(ESP_ROOT)/xtensa-lx106-elf/xtensa-lx106-elf/sysroot/lib/

# This combines the flags for the linker to find and use libraries
LIBRARY_FLAGS = $(patsubst %, -L%, $(LIB_DIRS)) --start-group $(patsubst %, -l%, $(LIBS)) --end-group

# These are flags for the linker
EXTRA_LDFLAGS = -O3 -flto -nostdlib -u call_user_start

# These are the linker scripts to build with
LINKER_SCRIPTS = \
	eagle.app.v6.ld \
	eagle.rom.addr.v6.ld

# This adds the flags and directories to the linker scripts
LINKER_SCRIPT_FLAGS = $(patsubst %, -T ./ld/%, $(LINKER_SCRIPTS))

################################################################################
# Build Filenames
################################################################################

# These are the files to build
FW_FILE0 = image.elf
FW_FILE1 = image.elf-0x00000.bin
FW_FILE2 = image.elf-0x10000.bin
LIBGCC_STRIPPED = libgcc_stripped.a
DBG_MAP=image.map
DBG_LST=image.lst
ASSETS_FILE=assets.bin
ASSETS_DIR=assets

################################################################################
# Targets for Building
################################################################################

# This list of targets do not build files which match their name
.PHONY: all clean debug bump_submodule erase dumprom wipechip burnitall burn burn_cutecom docs cppcheck print-%

# Build everything!
all: $(FW_FILE1) $(FW_FILE2) $(ASSETS_FILE)

# To build the two firmware files for flashing, you have to build the main file
$(FW_FILE1) $(FW_FILE2) : $(FW_FILE0)
	PATH="$(PATH)":$(GCC_FOLDER)/bin $(ESPTOOL_PY) elf2image $(FW_FILE0)

# To build the main file, you have to compile the objects and create the stripped libgcc
$(FW_FILE0): $(OBJECTS) $(LIBGCC_STRIPPED)
	$(LD) $(OBJECTS) $(EXTRA_LDFLAGS) $(LIBRARY_FLAGS) $(LINKER_SCRIPT_FLAGS) -o $@

# This assmbles each .S file into an o file
./$(OBJ_DIR)/%.o: ./%.S
	@mkdir -p $(@D) # This creates a directory before building an object in it.
	$(CC) $(CFLAGS) $(CFLAGS_WARNINGS) $(CFLAGS_WARNINGS_EXTRA) $(DEFINES) $(INC) $< -o $@

# This compiles each c file into an o file
./$(OBJ_DIR)/%.o: ./%.c
	@mkdir -p $(@D) # This creates a directory before building an object in it.
	$(CC) $(CFLAGS) $(CFLAGS_WARNINGS) $(CFLAGS_WARNINGS_EXTRA) $(DEFINES) $(INC) $< -o $@

# This compiles each c file in the SDK folder into an o file. Fewer warnings than other files
./$(OBJ_DIR)/$(SDK)/%.o: ./$(SDK)/%.c
	@mkdir -p $(@D) # This creates a directory before building an object in it.
	$(CC) $(CFLAGS) $(CFLAGS_WARNINGS) $(DEFINES) $(INC) $< -o $@

# This copies and stripps the libgcc.a file from the toolchain
$(LIBGCC_STRIPPED) :
	$(CP) $(XTGCCLIB) libgcc_stripped.a
	$(AR) dv libgcc_stripped.a __gcc_bcmp.o
	$(AR) dv libgcc_stripped.a _absvdi2.o
	$(AR) dv libgcc_stripped.a _absvsi2.o
	$(AR) dv libgcc_stripped.a _addsubsf3.o
	$(AR) dv libgcc_stripped.a _addvdi3.o
	$(AR) dv libgcc_stripped.a _addvsi3.o
	$(AR) dv libgcc_stripped.a _ashldi3.o
	$(AR) dv libgcc_stripped.a _bswapdi2.o
	$(AR) dv libgcc_stripped.a _bswapsi2.o
	$(AR) dv libgcc_stripped.a _clear_cache.o
	$(AR) dv libgcc_stripped.a _clrsbdi2.o
	$(AR) dv libgcc_stripped.a _clrsbsi2.o
	$(AR) dv libgcc_stripped.a _clzdi2.o
	$(AR) dv libgcc_stripped.a _clzsi2.o
	$(AR) dv libgcc_stripped.a _cmpdi2.o
	$(AR) dv libgcc_stripped.a _ctzdi2.o
	$(AR) dv libgcc_stripped.a _ctzsi2.o
	$(AR) dv libgcc_stripped.a _divdc3.o
	$(AR) dv libgcc_stripped.a _divdf3.o
	$(AR) dv libgcc_stripped.a _divdi3.o
	$(AR) dv libgcc_stripped.a _divsc3.o
	$(AR) dv libgcc_stripped.a _divsi3.o
	$(AR) dv libgcc_stripped.a _extendsfdf2.o
	$(AR) dv libgcc_stripped.a _ffsdi2.o
	$(AR) dv libgcc_stripped.a _ffssi2.o
	$(AR) dv libgcc_stripped.a _fixdfdi.o
	$(AR) dv libgcc_stripped.a _fixdfsi.o
	$(AR) dv libgcc_stripped.a _fixsfdi.o
	$(AR) dv libgcc_stripped.a _fixunsdfdi.o
	$(AR) dv libgcc_stripped.a _fixunsdfsi.o
	$(AR) dv libgcc_stripped.a _fixunssfdi.o
	$(AR) dv libgcc_stripped.a _fixunssfsi.o
	$(AR) dv libgcc_stripped.a _floatdidf.o
	$(AR) dv libgcc_stripped.a _floatdisf.o
	$(AR) dv libgcc_stripped.a _floatsidf.o
	$(AR) dv libgcc_stripped.a _floatsisf.o
	$(AR) dv libgcc_stripped.a _floatundidf.o
	$(AR) dv libgcc_stripped.a _floatundisf.o
	$(AR) dv libgcc_stripped.a _floatunsidf.o
	$(AR) dv libgcc_stripped.a _floatunsisf.o
	$(AR) dv libgcc_stripped.a _lshrdi3.o
	$(AR) dv libgcc_stripped.a _moddi3.o
	$(AR) dv libgcc_stripped.a _muldc3.o
	$(AR) dv libgcc_stripped.a _muldf3.o
	$(AR) dv libgcc_stripped.a _muldi3.o
	$(AR) dv libgcc_stripped.a _mulsc3.o
	$(AR) dv libgcc_stripped.a _mulsf3.o
	$(AR) dv libgcc_stripped.a _mulsi3.o
	$(AR) dv libgcc_stripped.a _mulvdi3.o
	$(AR) dv libgcc_stripped.a _mulvsi3.o
	$(AR) dv libgcc_stripped.a _negdf2.o
	$(AR) dv libgcc_stripped.a _negdi2.o
	$(AR) dv libgcc_stripped.a _negsf2.o
	$(AR) dv libgcc_stripped.a _negvdi2.o
	$(AR) dv libgcc_stripped.a _negvsi2.o
	$(AR) dv libgcc_stripped.a _paritydi2.o
	$(AR) dv libgcc_stripped.a _paritysi2.o
	$(AR) dv libgcc_stripped.a _popcountdi2.o
	$(AR) dv libgcc_stripped.a _popcountsi2.o
	$(AR) dv libgcc_stripped.a _powidf2.o
	$(AR) dv libgcc_stripped.a _powisf2.o
	$(AR) dv libgcc_stripped.a _subvdi3.o
	$(AR) dv libgcc_stripped.a _subvsi3.o
	$(AR) dv libgcc_stripped.a _truncdfsf2.o
	$(AR) dv libgcc_stripped.a _ucmpdi2.o
	$(AR) dv libgcc_stripped.a _udiv_w_sdiv.o
	$(AR) dv libgcc_stripped.a _udivdi3.o
	$(AR) dv libgcc_stripped.a _udivmoddi4.o
	$(AR) dv libgcc_stripped.a _udivsi3.o
	$(AR) dv libgcc_stripped.a _umoddi3.o
	$(AR) dv libgcc_stripped.a _umodsi3.o
	$(AR) dv libgcc_stripped.a _umulsidi3.o

# To build the assets, run a python script
$(ASSETS_FILE):
	python3 ../ESP-Asset-Packer/espAssetPacker.py -d $(ASSETS_DIR) -o $(ASSETS_FILE)

# This clean everything
clean:
	-@find ./ -type f -name '$(FW_FILE0)' -delete
	-@find ./ -type f -name '$(FW_FILE1)' -delete
	-@find ./ -type f -name '$(FW_FILE2)' -delete
	-@find ./ -type f -name '$(DBG_MAP)' -delete
	-@find ./ -type f -name '$(DBG_LST)' -delete
	-@find ./ -type f -name '$(notdir $(LIBGCC_STRIPPED))' -delete
	-@find ./$(OBJ_DIR)/ -type f -name '*.o' -delete
	-@find ./$(OBJ_DIR)/ -type f -name '*.d' -delete
	-@rm -rf docs
	-@find ./ -type f -name '$(ASSETS_FILE)' -delete

################################################################################
# Targets for Flashing
################################################################################

# Build and burn the two firmware files
burn : all
	$(ESPTOOL_PY) --baud $(ESP_FLASH_BITRATE) --port $(ESP_PORT) write_flash -fm dio $(FLASH_WRITE_FLAGS) \
		$(FW_FILE1_ADDR) $(FW_FILE1) \
		$(FW_FILE2_ADDR) $(FW_FILE2) \
		$(ASSETS_ADDR)   $(ASSETS_FILE)

# Build everything and burn the two firmware files, and init data
burnitall : all
	$(ESPTOOL_PY) --baud $(ESP_FLASH_BITRATE) --port $(ESP_PORT) write_flash -fm dio $(FLASH_WRITE_FLAGS) \
		$(FW_FILE1_ADDR)   $(FW_FILE1) \
		$(FW_FILE2_ADDR)   $(FW_FILE2) \
		$(ASSETS_ADDR)     $(ASSETS_FILE) \
		$(RF_CAL_ADDR)     $(SDK)/bin/blank.bin \
		$(PHY_DATA_ADDR)   $(SDK)/bin/esp_init_data_default_v08.bin \
		$(SYS_PARAM_ADDR)  $(SDK)/bin/blank.bin \
		$(SYS_PARAM_ADDR2) $(SDK)/bin/blank.bin \
		$(SYS_PARAM_ADDR3) $(SDK)/bin/blank.bin

# Erase the ESP8266
erase :
	$(ESPTOOL_PY) --baud $(ESP_FLASH_BITRATE) --port $(ESP_PORT) erase_flash

# Write zeros to the ESP8266, more aggressive than erase
wipechip :
	dd if=/dev/zero of=/tmp/zeroes bs=1M count=1
	$(ESPTOOL_PY) --baud $(ESP_FLASH_BITRATE)  --port $(ESP_PORT) write_flash 0x0 /tmp/zeroes

################################################################################
# Targets for Debugging
################################################################################

# Generate map and list files from the build elf. Also print some information
debug : $(FW_FILE0)
	$(PREFIX)nm -S -n $(FW_FILE0) > $(DBG_MAP)
	$(PREFIX)objdump -S $(FW_FILE0) > $(DBG_LST)
	$(PREFIX)size -A $(FW_FILE0) | grep -v debug

# Dump the ROM from address 0x100000 into dump.bin
dumprom :
	$(ESPTOOL_PY) --baud $(ESP_FLASH_BITRATE) --port $(ESP_PORT) read_flash 0 1048576 dump.bin

# Kill all cutecom serial terminals, burn the firmware, and open cutecom again
# Note, this target won't technically finish until cutecom is closed
burn_cutecom :
	-killall cutecom
	make burn
	-cutecom &

docs :
	doxygen swadge2019.doxyfile

cppcheck:
	cppcheck --std=c99 --platform=unix32 --suppress=missingIncludeSystem --enable=all $(DEFINES) $(INC) user/ > /dev/null

################################################################################
# General Utility Targets
################################################################################

# Update the submodules
bump_submodule :
	cd ESP8266_NONOS_SDK; git pull origin master; git submodule update --init --recursive; cd ..

# Print any value from this makefile
print-%  : ; @echo $* = $($*)

#################################################################################
# Needed to automatically detect changes in header files
# https://nathandumont.com/blog/automatically-detect-changes-in-header-files-in-a
#################################################################################
-include $(OBJECTS:.o=.d)
