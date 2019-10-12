# Compatibility
This Python script should be compatible with Python 2 & 3. I have tested it on Windows, but no other platform.

# Summary
This Python script will automatically open all USB serial ports which match the VID & PID of a CP2012N.

For all open ports, it will continuously attempt to flash a Swadge using esptool.

The UI will flash green for four seconds when a Swadge is successfully flashed.

# Dependencies
The following Python modules must be installed:
* ``tkinter`` (this should be installed with Python, use Google if you don't seem to have it)
* ``pyserial``
* ``esptool``

This can be done with the command:
```pip install pyserial esptool```

For the script to actually flash firmware, the following files must be in the same directory as ``pyFlashGui.py``:
* ``image.elf-0x00000.bin`` (from compilation)
* ``image.elf-0x10000.bin`` (from compilation)
* ``blank.bin`` (from ESP8266_NONOS_SDK/bin)
* ``esp_init_data_default_v08.bin`` (from ESP8266_NONOS_SDK/bin)
