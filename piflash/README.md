## Flashing from Anywhere

Download binaries to flash from a binaries.tar.gz file here: https://github.com/cnlohr/swadge2019/releases

Make sure to install pyserial:
 * apt-get install python-pip
 * pip install pyserial

## Flashing from a Raspberry Pi
To run on a Raspberry Pi, edit /boot/config.txt and add ```enable_uart=1```

## Flashing from a PC
To run on a PC, run:

```while :; do ./flash.sh /dev/ttyUSB0 Alpha; sleep 1; done```

Remember to replace
 * ```/dev/ttyUSB0``` with your serial port
 * ```Alpha``` with a phrase to be spoken after a successful flash
