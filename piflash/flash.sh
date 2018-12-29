#!/bin/sh -e

# flash.sh /dev/portname name-of-this-jig

setterm -term linux -back default -fore default -clear all
#clear
#echo "Ready to flash"

if esptool/esptool.py -p $1 -b 2000000 --after no_reset --before no_reset --chip esp8266 write_flash 0x00000 image.elf-0x00000.bin 0x10000 image.elf-0x10000.bin 532480 page.mpfs
then
  setterm -term linux -back green -fore white -clear all
  say "$2 done"
  # aplay /usr/share/sounds/alsa/Front_Left.wav
  # echo "Success"
else
  setterm -term linux -back red -fore white -clear all
  # say "$2 error"
  # aplay /usr/share/sounds/alsa/Front_Center.wav
  # echo "Failure, exit status $?"
fi