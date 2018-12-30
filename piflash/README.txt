Download binaries to flash from a binaries.tar.gz file here: https://github.com/cnlohr/swadge2019/releases

Make sure:
 * apt-get install python-pip
 * pip install pyserial

Edit /boot/config.txt
 * enable_uart=1

To run on a PC, not a Raspberry Pi, run:
```while :; do ./flash.sh /dev/ttyUSB0 Alpha; sleep 1; done```
Remember to replace
 * ```/dev/ttyUSB0``` with your serial port
 * ```Alpha``` with a phrase to be spoken after a successful flash