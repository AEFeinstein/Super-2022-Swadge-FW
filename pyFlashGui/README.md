# Compatibility
This Python script should be compatible with Python 2 & 3. I have tested it on Windows and Linux, but not OSX. Python 3 is preferred.

# Summary
This Python script will automatically open all USB serial ports which match the VID & PID of a CP2012N. We've tested this with up to ten programmers at a time.

For all open ports, it will continuously attempt to flash a Swadge using `esptool`.

The UI will flash green for four seconds when a Swadge is successfully flashed. It will flash red if the flash failed.

# Dependencies
The following Python modules must be installed:
* ``python3`` (https://www.python.org/downloads/)
* ``tkinter`` (https://tkdocs.com/tutorial/install.html)
* ``pyserial`` (https://pyserial.readthedocs.io/en/latest/pyserial.html or ```pip install pyserial```)
* ``esptool`` (this dependency was replaced with a local version with bugfixes, no install necessary)

For the script to actually flash firmware, the following files must be in the same directory as ``pyFlashGui.py``:
* ``image.elf-0x00000.bin`` (from compilation)
* ``image.elf-0x10000.bin`` (from compilation)
* ``blank.bin`` (from ESP8266_NONOS_SDK/bin)
* ``esp_init_data_default_v08.bin`` (from ESP8266_NONOS_SDK/bin)

# Instructions

1. Download and install the dependencies
1. Download the final Swadge firmware from the Releases tab: https://github.com/AEFeinstein/Super-2020-Swadge-FW/releases/download/2.0.0/Super_2020_Swadge_FW.zip
1. Plug in your programmer with a microUSB cable
1. Run the programmer script (``python3 pyFlashGui.py`` from a terminal)
1. Switch the programmer to "ON" and "5V"
1. Hold down the "GPIO0" button and plug in your Swadge
1. Wait for the GUI to flash green, then you're done.

We also sent a video to the manufacturer about how to flash Swadges with test firmware: https://youtu.be/LsFIoCyYO6w
