all : swadgemu

CFLAGS := -DEMU -DSWADGE_VERSION=3 -O0 -I.  -g -Isysincstubs -I../user -I../user/display -I../user/hdw -I../user/modes  -I../user/modes/colorchord  -I../user/utils -I../user/utils/spi_mem -I../user/utils/wireless -DUSER_SETTINGS_ADDR=0x6C000 -DUSER_SETTINGS_SIZE=0x3000



LDFLAGS := -lX11 -lm

SWADGEC:=../user/user_main.c ../user/utils/hsv_utils.c ../user/utils/spi_mem/nvm_interface.c ../user/utils/synced_timer.c ../user/modes/colorchord/DFT32.c ../user/modes/colorchord/embeddednf.c ../user/modes/colorchord/embeddedout.c ../user/modes/colorchord/mode_colorchord.c


swadgemu : rawdraw/CNFG.c swadgemu.c oled.c $(SWADGEC)
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ swadgemu


