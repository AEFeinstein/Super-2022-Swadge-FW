
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

CFLAGS+=-I. -DANDROID
LDFLAGS:=-lOpenSLES
EXTRASRC:=androidsupport.c $(EMUC) $(SWADGEC)

include rawdrawandroid/Makefile

all : makecapk.apk

