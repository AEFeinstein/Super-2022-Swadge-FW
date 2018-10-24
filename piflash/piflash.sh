#!/bin/sh -e

echo GENERAL NOTE: have you made sure to use uart_tool to switch mode to gpio?

echo 2 > /sys/class/gpio/export || true
echo 3 > /sys/class/gpio/export || true
echo out > /sys/class/gpio/gpio2/direction || true
echo out > /sys/class/gpio/gpio3/direction || true

echo "Continuing"

echo 0 > /sys/class/gpio/gpio2/value # Power
echo 0 > /sys/class/gpio/gpio3/value # GPIO0

sleep .5
# Boot up in bootloader mode.
echo 1 > /sys/class/gpio/gpio2/value
sleep .5

#killall hciattach || true
#stty -F /dev/ttyAMA0 115200 -echo raw

esptool/esptool.py  -b 1000000 --port /dev/serial0 write_flash  0x00000 image.elf-0x00000.bin 0x10000 image.elf-0x10000.bin  532480 page.mpfs


echo 0 > /sys/class/gpio/gpio2/value # Power
echo 1 > /sys/class/gpio/gpio3/value # GPIO0
sleep .3
echo 1 > /sys/class/gpio/gpio2/value

# -fm dio 
# 532480  web/page.mpfs


