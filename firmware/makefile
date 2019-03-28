# Makefile by Adam, 2019

################################################################################
# This tells the makefile to use the xtensa binaries to build
################################################################################

GCC_FOLDER = $(ESP_ROOT)/xtensa-lx106-elf
ESPTOOL_PY = $(ESP_ROOT)/esptool/esptool.py
SDK        = ./esp82xx/toolchain/esp_nonos_sdk

XTLIB        = $(SDK)/lib
ESP_GCC_VERS = 4.8.5
XTGCCLIB     = $(GCC_FOLDER)/lib/gcc/xtensa-lx106-elf/$(ESP_GCC_VERS)/libgcc.a
FOLDERPREFIX = $(GCC_FOLDER)/bin
PREFIX       = $(FOLDERPREFIX)/xtensa-lx106-elf-
CC           = $(PREFIX)gcc
OBJCOPY      = $(PREFIX)objcopy
LD           = $(PREFIX)ld
AR           = $(PREFIX)ar
CP           = cp

################################################################################
# Options for burning
################################################################################

FWBURNFLAGS  = -b 2000000
PORT  = /dev/ttyUSB0
FLASH_WRITE_FLAGS = # --verify
MFS_PAGE_OFFSET=532480

################################################################################
# Compilation options
################################################################################

# This is a list of directories to scan for c files recursively
SRC_DIRS_RECURSIVE = \
	user

# This is a list of directories to scan for c files not recursively
SRC_DIRS_FLAT = \
	esp82xx/fwsrc \
	embeddedcommon

# This is a list of all source directories
SRC_DIRS = $(shell find $(SRC_DIRS_RECURSIVE) -type d) $(SRC_DIRS_FLAT)

# This is a list of all source c files 
SOURCES  = $(shell find $(SRC_DIRS) -maxdepth 1 -name "*.c")

# These are flags for the compiler
EXTRA_CFLAGS = \
	-c \
	-mlongcalls \
	-Os \
	-flto \
	-Wl,--relax \
	-Wl,--gc-sections \
	-nostdlib \
	-Wl,-Map,output.map \
	-B$(SDK)/lib \
	-g \
	-std=gnu99 \
	-Wall \
	-Wextra \
	-Wundef \
	-Wunused \
	-Wformat=2 \
	-Winvalid-pch \
	-Wlogical-op \
	-Wmissing-declarations \
	-Wmissing-format-attribute \
	-Wmissing-include-dirs \
	-Wpointer-arith

# And these are warnings, lots of warnings
#EXTRA_CFLAGS := $(EXTRA_CFLAGS) \
#	-Wcast-align \
#	-Wcast-qual \
#	-Wconversion \
#	-Wdouble-promotion \
#	-Wformat-nonliteral \
#	-Wjump-misses-init \
#	-Wmissing-prototypes \
#	-Wpedantic \
#	-Wredundant-decls \
#	-Wshadow \
#	-Wsign-conversion \
#	-Wstrict-prototypes \
#	-Wswitch-default \
#	-Wswitch-enum

# These are a list of #define symbols in the makefile
DEFINES = \
	ICACHE_FLASH \
	DISABLE_CHARRX \
	QUIET_REFLASH \
	WS2812_FOUR_SAMPLE \
	USE_OPTIMIZE_PRINTF \
	MFS_PAGE_OFFSET=$(MFS_PAGE_OFFSET) \
	DISABLE_MDNS \
	DISABLE_AUTO_SWITCH_TO_AP \
	DISABLE_NET_REFLASH \
	SOFTAP_CHANNEL=11 \
	WEB_PORT=80 \
	COM_PORT=7777 \
	BACKEND_PORT=7878 \
	SLOWTICK_MS=50 \
	VERSSTR='"tmpstr"' # TODO get the actual version string

# This appends the defines to the EXTRA_CFLAGS
EXTRA_CFLAGS := $(EXTRA_CFLAGS) $(patsubst %, -D%, $(DEFINES))

# Treat every source directory as one to search for headers in, also add a few more
INC = $(patsubst %, -I%, \
	$(SRC_DIRS) \
	esp82xx/include \
	$(SDK)/include)

# This is the directory in which object files will be stored
OBJ_DIR = obj

# This is a list of objects to build
OBJECTS=$(patsubst %.c, $(OBJ_DIR)/%.o, $(SOURCES))

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
	esp82xx \
	$(SDK)/lib \
	$(ESP_ROOT)/xtensa-lx106-elf/xtensa-lx106-elf/sysroot/lib/

# This combines the flags for the linker to find and use libraries
LIBRARY_FLAGS = $(patsubst %, -L%, $(LIB_DIRS)) --start-group $(patsubst %, -l%, $(LIBS)) --end-group

# These are flags for the linker
EXTRA_LDFLAGS = -Os -flto -nostdlib -u call_user_start -g

# These are the linker scripts to build with
LINKER_SCRIPTS = \
	eagle.app.v6.ld \
	eagle.rom.addr.v6.ld

# This adds the flags and directories to the linker scripts
LINKER_SCRIPT_FLAGS = $(patsubst %, -T $(SDK)/ld/%, $(LINKER_SCRIPTS))

################################################################################
# Build Filenames
################################################################################

# These are the files to build
FW_FILE0 = image.elf
FW_FILE1 = image.elf-0x00000.bin
FW_FILE2 = image.elf-0x10000.bin
LIBGCC_STRIPPED = esp82xx/libgcc_stripped.a
WEBPAGE_MFS = web/page.mpfs
DBG_MAP=image.map
DBG_LST=image.lst

################################################################################
# Targets for Building
################################################################################

# This list of targets do not build files which match their name
.PHONY: all clean debug bump_submodule erase dumprom wipechip burnitall burn burn_cutecom docs print-%

# Build everything!
all: $(FW_FILE1) $(FW_FILE2) $(WEBPAGE_MFS)

# To build the two firmware files for flashing, you have to build the main file
$(FW_FILE1) $(FW_FILE2) : $(FW_FILE0)
	PATH=$(PATH):$(ESP_ROOT)/xtensa-lx106-elf/bin $(ESP_ROOT)/esptool/esptool.py elf2image $(FW_FILE0)

# To build the main file, you have to compile the objects and create the stripped libgcc
$(FW_FILE0): $(OBJECTS) $(LIBGCC_STRIPPED)
	$(LD) $(OBJECTS) $(EXTRA_LDFLAGS) $(LIBRARY_FLAGS) $(LINKER_SCRIPT_FLAGS) -o $@ 

# This compiles each c file into an o file
./$(OBJ_DIR)/%.o: ./%.c
	@mkdir -p $(@D) # This creates a directory before building an object in it.
	$(CC) $(EXTRA_CFLAGS) $(INC) $< -o $@

# This copies and stripps the libgcc.a file from the toolchain
$(LIBGCC_STRIPPED) :
	cp $(GCC_FOLDER)/lib/gcc/xtensa-lx106-elf/$(ESP_GCC_VERS)/libgcc.a esp82xx/libgcc_stripped.a
	$(AR) dv esp82xx/libgcc_stripped.a __gcc_bcmp.o
	$(AR) dv esp82xx/libgcc_stripped.a _absvdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _absvsi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _addsubsf3.o
	$(AR) dv esp82xx/libgcc_stripped.a _addvdi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _addvsi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _ashldi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _bswapdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _bswapsi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _clear_cache.o
	$(AR) dv esp82xx/libgcc_stripped.a _clrsbdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _clrsbsi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _clzdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _clzsi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _cmpdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _ctzdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _ctzsi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _divdc3.o
	$(AR) dv esp82xx/libgcc_stripped.a _divdf3.o
	$(AR) dv esp82xx/libgcc_stripped.a _divdi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _divsc3.o
	$(AR) dv esp82xx/libgcc_stripped.a _divsi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _extendsfdf2.o
	$(AR) dv esp82xx/libgcc_stripped.a _ffsdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _ffssi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _fixdfdi.o
	$(AR) dv esp82xx/libgcc_stripped.a _fixdfsi.o
	$(AR) dv esp82xx/libgcc_stripped.a _fixsfdi.o
	$(AR) dv esp82xx/libgcc_stripped.a _fixunsdfdi.o
	$(AR) dv esp82xx/libgcc_stripped.a _fixunsdfsi.o
	$(AR) dv esp82xx/libgcc_stripped.a _fixunssfdi.o
	$(AR) dv esp82xx/libgcc_stripped.a _fixunssfsi.o
	$(AR) dv esp82xx/libgcc_stripped.a _floatdidf.o
	$(AR) dv esp82xx/libgcc_stripped.a _floatdisf.o
	$(AR) dv esp82xx/libgcc_stripped.a _floatsidf.o
	$(AR) dv esp82xx/libgcc_stripped.a _floatsisf.o
	$(AR) dv esp82xx/libgcc_stripped.a _floatundidf.o
	$(AR) dv esp82xx/libgcc_stripped.a _floatundisf.o
	$(AR) dv esp82xx/libgcc_stripped.a _floatunsidf.o
	$(AR) dv esp82xx/libgcc_stripped.a _floatunsisf.o
	$(AR) dv esp82xx/libgcc_stripped.a _lshrdi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _moddi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _muldc3.o
	$(AR) dv esp82xx/libgcc_stripped.a _muldf3.o
	$(AR) dv esp82xx/libgcc_stripped.a _muldi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _mulsc3.o
	$(AR) dv esp82xx/libgcc_stripped.a _mulsf3.o
	$(AR) dv esp82xx/libgcc_stripped.a _mulsi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _mulvdi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _mulvsi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _negdf2.o
	$(AR) dv esp82xx/libgcc_stripped.a _negdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _negsf2.o
	$(AR) dv esp82xx/libgcc_stripped.a _negvdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _negvsi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _paritydi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _paritysi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _popcountdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _popcountsi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _powidf2.o
	$(AR) dv esp82xx/libgcc_stripped.a _powisf2.o
	$(AR) dv esp82xx/libgcc_stripped.a _subvdi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _subvsi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _truncdfsf2.o
	$(AR) dv esp82xx/libgcc_stripped.a _ucmpdi2.o
	$(AR) dv esp82xx/libgcc_stripped.a _udiv_w_sdiv.o
	$(AR) dv esp82xx/libgcc_stripped.a _udivdi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _udivmoddi4.o
	$(AR) dv esp82xx/libgcc_stripped.a _udivsi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _umoddi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _umodsi3.o
	$(AR) dv esp82xx/libgcc_stripped.a _umulsidi3.o

# To build the webpage file, call its makefile
$(WEBPAGE_MFS) :
	make -C ./web

# This clean everything
clean:
	-@find ./ -type f -name '$(FW_FILE0)' -delete
	-@find ./ -type f -name '$(FW_FILE1)' -delete
	-@find ./ -type f -name '$(FW_FILE2)' -delete
	-@find ./ -type f -name '$(DBG_MAP)' -delete
	-@find ./ -type f -name '$(DBG_LST)' -delete
	-@find ./ -type f -name '$(notdir $(LIBGCC_STRIPPED))' -delete
	-@find ./$(OBJ_DIR)/ -type f -name '*.o' -delete
	-@rm -rf docs
	-@make -C ./web clean

################################################################################
# Targets for Flashing
################################################################################

# Build and burn the two firmware files
burn : $(FW_FILE1) $(FW_FILE2)
	$(ESPTOOL_PY) $(FWBURNFLAGS) --port $(PORT) write_flash -fm dio $(FLASH_WRITE_FLAGS) 0x00000 $(FW_FILE1) 0x10000 $(FW_FILE2)

# Build everything and burn the two firmware files, init data, and the webpage
burnitall : all
	$(ESPTOOL_PY) $(FWBURNFLAGS) --port $(PORT) write_flash -fm dio $(FLASH_WRITE_FLAGS) 0x00000 $(FW_FILE1) 0x10000 $(FW_FILE2) 0x3FC000 $(SDK)/bin/esp_init_data_default_v08.bin $(MFS_PAGE_OFFSET) $(WEBPAGE_MFS)

# Erase the ESP8266
erase :
	$(ESPTOOL_PY) $(FWBURNFLAGS) --port $(PORT) erase_flash
	
# Write zeros to the ESP8266, more aggressive than erase
wipechip :
	dd if=/dev/zero of=/tmp/zeroes bs=1M count=1
	$(ESPTOOL_PY) $(FWBURNFLAGS)  --port $(PORT) write_flash 0x0 /tmp/zeroes

################################################################################
# Targets for Debugging
################################################################################

# Generate map and list files from the build elf. Also print some information
debug : $(FW_FILE0)
	nm -S -n $(FW_FILE0) > $(DBG_MAP)
	$(PREFIX)objdump -S $(FW_FILE0) > $(DBG_LST)
	$(PREFIX)size -A $(FW_FILE0) | grep -v debug

# Dump the ROM from address 0x100000 into dump.bin
dumprom :
	$(ESPTOOL_PY) $(FWBURNFLAGS) --port $(PORT) read_flash 0 1048576 dump.bin

# Kill all cutecom serial terminals, burn the firmware, and open cutecom again
# Note, this target won't technically finish until cutecom is closed
burn_cutecom :
	-killall cutecom
	make burn
	-cutecom &
	
docs :
	doxygen swadge2019.doxyfile

################################################################################
# General Utility Targets
################################################################################

# Update the submodules
bump_submodule :
	cd esp82xx; git pull origin master; git submodule update --init --recursive; cd ..
	cd esp82xx/toolchain/esp_nonos_sdk; git pull origin master; git submodule update --init --recursive; cd ../../..

# Print any value from this makefile
print-%  : ; @echo $* = $($*)
