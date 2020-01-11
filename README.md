# Super 2020 Swadge Firmware

## Welcome

This is the firmware repository for the Super 2020 Swadge.

The corresponding hardware repository for the Super 2020 Swadge Hardware [can be found here](https://github.com/AEFeinstein/Super-2020-Swadge-HW).

## History 

The Super 2020 Swadge is based on the [2019 Swadge](https://github.com/cnlohr/swadge2019). The 2019 Swadge has a combined firmware and hardware repository.

The 2019 Swadge is based on [ESP8266 Colorchord](https://github.com/cnlohr/colorchord/tree/master/embedded8266). You can read more about [ColorChord: Embedded here](https://github.com/AEFeinstein/Swadge-Devkit-Fw/blob/master/firmware/embeddedcommon/README.md).

ESP8266 Colorchord uses [esp8266ws2812i2s](https://github.com/cnlohr/esp8266ws2812i2s). ESP8266 Colorchord is based on [ESP8266 MP3 Decoder](https://github.com/espressif/esp8266_mp3_decoder/).

## Working on this Project

If you would like to work on this project, start by following the [Getting Started Guide](https://github.com/AEFeinstein/Swadge-Devkit-Fw/blob/master/docs/GETTING_STARTED.md). By the end of the guide you should have an enviroment where you can compile the firmware and flash it to a Swadge.

Once your environment is set up, read the [Contribution Guide](https://github.com/AEFeinstein/Swadge-Devkit-Fw/blob/master/docs/CONTRIBUTING.md). This guide outlines the best practices for getting your code into this project.

The Super 2020 Swadge uses [cnlohr's exp8266 environment](https://github.com/cnlohr/esp82xx), which in turn uses the ESP8266 Non-OS SDK. You may find the [ESP8266 Non-OS SDK API Reference](https://www.espressif.com/sites/default/files/documentation/2c-esp8266_non_os_sdk_api_reference_en.pdf) useful.

[pyFlashGui](https://github.com/AEFeinstein/Super-2020-Swadge-FW/tree/master/piflash/pyFlashGui) is used for flashing Swadges. It's a Python GUI program.
