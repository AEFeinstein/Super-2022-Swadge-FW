
APPNAME=swadgemu
RAWDRAWANDROID=rawdrawandroid
SWADGEMU:=..

#you probably want to type:
#  make run

SOUNDDRIVER:=$(SWADGEMU)/sound/sound_android.c

EXTRA_ASSETS_TRIGGER=assetsrecipe

assetsrecipe :
	mkdir -p makecapk/assets
	cp ../assets.bin makecapk/assets

### TODO ### How should we copy assets in?

include ../Makefile

CFLAGS+=-I. -DANDROID  -ffunction-sections -Os
LDFLAGS:=-lOpenSLES -s

SRC:=androidsupport.c $(EMUC) $(SWADGEC)

include rawdrawandroid/Makefile

all : makecapk.apk

