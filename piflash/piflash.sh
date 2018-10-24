#!/bin/sh -e

true || echo 2 > /sys/class/gpio/export
true || echo 3 > /sys/class/gpio/export
true || echo out > /sys/class/gpio/gpio2/direction
true || echo out > /sys/class/gpio/gpio3/direction

echo "Continuing"

echo 1 > /sys/class/gpio/gpio2/value # Power
echo 0 > /sys/class/gpio/gpio3/value # GPIO0

sleep .2
# Boot up in bootloader mode.
echo 0 > /sys/class/gpio/gpio2/value

