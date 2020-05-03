# Directories to find headers in
INCDIRS  := $(shell find ../user/ -type d) \
            sysincstubs \
			rawdraw \
            .

# Preprocessor defines
DEFINES  := USER_SETTINGS_ADDR=0x6C000 \
            USER_SETTINGS_SIZE=0x3000 \
            ASSETS_ADDR=0x6F000 \
            ASSETS_SIZE=0x51000 \
            SWADGE_VERSION=3 \
            EMU \
			NO_SOUND_PARAMETERS \
			DFREQ=16000

# All the compiler flags
CFLAGS   := -g $(patsubst %, -I%, $(INCDIRS)) $(patsubst %, -D%, $(DEFINES))
CFLAGS += -Os -s -flto -ffunction-sections -fdata-sections -Wl,--gc-sections

# All the linker flags
LDFLAGS  := -lX11 -lm -lpthread -lasound

# A list of files to exclude from compilation
EXCLUDEC := oled.c \
            adc.c \
            cnlohr_i2c.c \
            gpio_user.c \
            hpatimer.c \
            QMA6981.c \
            ws2812_i2s.c \
            maxtime.c \
            PartitionMap.c \
            espNowUtils.c

# All the source files to compile
SWADGEC  := $(shell find ../user/ -iname "*.c" $(patsubst %, -not -iname %, $(EXCLUDEC)))
RAWDRAWC := rawdraw/CNFG.c
EMUC     := swadgemu.c oled.c sound/sound.c sound/sound_alsa.c

# Makefile targets that don't make what they're called
.PHONY: all clean

# Build everything
all : swadgemu

# Build swadgemu
swadgemu : $(RAWDRAWC) $(SWADGEC) $(EMUC)
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Clean everything
clean :
	rm -rf *.o *~ swadgemu
