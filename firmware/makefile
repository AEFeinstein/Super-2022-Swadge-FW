# Makefile by Adam, 2019

# This tells the makefile to use the xtensa binaries to build
CC=$(ESP_ROOT)/xtensa-lx106-elf/bin/xtensa-lx106-elf-gcc
LD=$(ESP_ROOT)/xtensa-lx106-elf/bin/xtensa-lx106-elf-ld

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
SOURCES := $(shell find $(SRC_DIRS) -maxdepth 1 -name "*.c")

# These are flags for the compiler
EXTRA_CFLAGS = \
	-c \
	-mlongcalls \
	-Os \
	-Wall \
	-Wextra \
	-flto \
	-Wl,--relax \
	-Wl,--gc-sections \
	-nostdlib \
	-Wl,-Map,output.map \
	-Besp82xx/toolchain/esp_nonos_sdk/lib

# These are a list of #define symbols in the makefile
DEFINES = \
	ICACHE_FLASH \
	DISABLE_CHARRX \
	QUIET_REFLASH \
	WS2812_FOUR_SAMPLE \
	USE_OPTIMIZE_PRINTF \
	MFS_PAGE_OFFSET=532480 \
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
	. \
	esp82xx/include \
	esp82xx/toolchain/esp_nonos_sdk/include)

# This is the directory in which object filese will be stored
OBJ_DIR = obj

# This is a list of objects to build
OBJECTS=$(patsubst %.c, $(OBJ_DIR)/%.o, $(SOURCES))

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
	esp82xx/toolchain/esp_nonos_sdk/lib \
	$(ESP_ROOT)/xtensa-lx106-elf/xtensa-lx106-elf/sysroot/lib/

# This combines the flags for the linker to find and use libraries
LIBRARY_FLAGS = $(patsubst %, -L%, $(LIB_DIRS)) --start-group $(patsubst %, -l%, $(LIBS)) --end-group

# These are flags for the linker
EXTRA_LDFLAGS = -Os -flto -nostdlib -u call_user_start

# These are the linker scripts to build with
LINKER_SCRIPTS = \
	eagle.app.v6.ld \
	eagle.rom.addr.v6.ld

# This adds the flags and directories to the linker scripts
LINKER_SCRIPT_FLAGS = $(patsubst %, -T esp82xx/toolchain/esp_nonos_sdk/ld/%, $(LINKER_SCRIPTS))

# This is the binary to build
EXECUTABLE = image.elf

# This creates a directory before building an object in it.
DIR_GUARD=@mkdir -p $(@D)

.PHONY: all clean

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(LD) $(OBJECTS) $(EXTRA_LDFLAGS) $(LIBRARY_FLAGS) $(LINKER_SCRIPT_FLAGS) -o $@ 
	$(ESP_ROOT)/esptool/esptool.py elf2image $@

./$(OBJ_DIR)/%.o: ./%.c
	$(DIR_GUARD)
	$(CC) $(EXTRA_CFLAGS) $(INC) $< -o $@

clean:
	-@find ./ -type f -name '$(EXECUTABLE)*' -delete
	-@find ./$(OBJ_DIR)/ -type f -name '*.o' -delete

print-%  : ; @echo $* = $($*)
