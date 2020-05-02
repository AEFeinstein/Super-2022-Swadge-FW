all : swadgemu

CFLAGS := -DEMU -DSWADGE_VERSION=3 -O0 -I.  -g -Isysincstubs -I../user -I../user/display -I../user/hdw -I../user/modes  -I../user/modes/colorchord  -I../user/utils -I../user/utils/spi_mem -I../user/utils/wireless -DUSER_SETTINGS_ADDR=0x6C000 -DUSER_SETTINGS_SIZE=0x3000 -DASSETS_ADDR=0x6F000 -DASSETS_SIZE=0x51000

LDFLAGS := -lX11 -lm

SWADGEC:=../user/user_main.c ../user/utils/hsv_utils.c ../user/utils/spi_mem/nvm_interface.c ../user/utils/synced_timer.c ../user/modes/colorchord/DFT32.c ../user/modes/colorchord/embeddednf.c ../user/modes/colorchord/embeddedout.c ../user/modes/colorchord/mode_colorchord.c ../user/hdw/buttons.c ../user/modes/mode_ring.c ../user/modes/mode_swadgepass.c ../user/modes/mode_test.c ../user/modes/mode_magpet.c ../user/modes/colorchord/mode_colorchord.c ../user/utils/assets.c ../user/utils/fastlz.c ../user/display/bresenham.c ../user/display/font.c ../user/display/sprite.c ../user/display/fonts/IbmVga8.c ../user/display/fonts/Radiostars.c ../user/display/fonts/TomThumb.c ../user/utils/wireless/p2pConnection.c ../user/utils/linked_list.c


swadgemu : rawdraw/CNFG.c swadgemu.c oled.c $(SWADGEC)
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ swadgemu


