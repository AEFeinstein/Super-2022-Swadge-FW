# Getting a Linux Environment

The Swadge is compiled from a Linux environment. If you have one of those already, great, skip to part two. If you don't, here's how to set up Window Subsystem for Linux.

1. Install Windows Subsystem for Linux (WSL) by following this guide: https://docs.microsoft.com/en-us/windows/wsl/install-win10
1. Install Ubuntu 20.04 LTS from the Microsoft Store: https://www.microsoft.com/en-us/p/ubuntu-2004-lts/9n6svws3rx71
1. Initialize Ubuntu 20.04 LTS by following the guide here, and **DO NOT UPDATE TO WSL2**: https://docs.microsoft.com/en-us/windows/wsl/initialize-distro

You can also set up a Linux virtual machine, like [Xubuntu](https://xubuntu.org/download/) running in [VirtualBox](https://www.virtualbox.org/wiki/Downloads). Google for a guide if you need one.

It is also possible to set up a Mac environment using a fork of pfalcon's esp-open-sdk (see below). See the following which outlines the procedure. https://github.com/pfalcon/esp-open-sdk/issues/342
especially the comments made on March 15 2019 by phibo23. 

# Compiling The Firmware

If you want to take the easy way out, run this script: [`swadge-setup.sh`](/docs/swadge-setup.sh). It will install dependencies, download the project, and compile it.
The script was tested on Ubuntu 21.04 as of May 7th, 2021. The script was derived from the [CircleCI build job](/.circleci/config.yml).
The steps below do pretty much the same thing, but you have more control and understanding of the process.

1. Update your packages and install all dependencies
    ```
    $ sudo apt-get update
    $ sudo apt-get dist-upgrade
    $ sudo apt-get install build-essential make curl unrar-free autoconf automake libtool gcc g++ gperf flex bison texinfo gawk ncurses-dev libexpat-dev sed git unzip bash help2man wget bzip2 libtool-bin libusb-1.0-0-dev libx11-dev libpulse-dev libasound-dev python-dev python python-serial
    ```
    * If you're using Ubuntu 20.04+, apt-get won't be able to find `python-serial`. Run the following commands
    ```
    $ sudo apt-get update
    $ sudo apt-get dist-upgrade
    $ sudo apt-get install build-essential make curl unrar-free autoconf automake libtool gcc g++ gperf flex bison texinfo gawk ncurses-dev libexpat-dev sed git unzip bash help2man wget bzip2 libtool-bin libusb-1.0-0-dev libx11-dev libpulse-dev libasound-dev python-dev python
    $ curl https://bootstrap.pypa.io/get-pip.py --output get-pip.py
    $ sudo python2 get-pip.py
    $ pip2 install pyserial rtttl
    $ pip3 install pyserial rtttl
    ```
1. Download and unzip the ESP8266 toolchain. It's easiest to do this in your home directory.
    ```
    $ cd ~
    $ wget https://github.com/cnlohr/esp82xx_bin_toolchain/raw/master/esp-open-sdk-x86_64-20200810.tar.xz
    $ tar xJvf esp-open-sdk-x86_64-20200810.tar.xz
    $ rm esp-open-sdk-x86_64-20200810.tar.xz
    ```
1. Set up environment variables by appending the following to your `.bashrc` file. If you downloaded `esp-open-sdk` elsewhere, you'll want to modify the paths to match.
    ```
    $ nano ~/.bashrc
    
    Append this, after verifying the location of your esp-open-sdk:
    
    export ESP_ROOT=$HOME/esp-open-sdk
    ```
1. Restart your Linux environment so the environment variables are actually set.
1. Clone out this repository, move to the `firmware` folder, and build it. 
    ```
    $ git clone https://github.com/AEFeinstein/Super-2022-Swadge-FW.git --recurse-submodules
    $ cd Super-2022-Swadge-FW/firmware/
    $ unset ESP_GDB && export SET_SWADGE_VERSION=5 && make -j$(nproc)
    ```

### The Old Way

For step 2 above, before cnlohr created the precompiled 8266 toolchain, you would have to compile `esp-open-sdk`. For legacy purposes, the instructions on how to do so are as follows.

2. Check out the [pfalcon's esp-open-sdk](https://github.com/pfalcon/esp-open-sdk), move to the `esp-open-sdk` folder and build it. More detailed instructions are on that project's page. Warning, building this takes a while (like 30 minutes)!
    ```
    $ git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
    $ cd esp-open-sdk/
    $ make -j$(nproc)
    ```
    * Developers using WSL are encouraged to **avoid cloning into windows directories** (e.g., `/mnt/c/...`) due to conflicts between WSL and Windows file permissions.
    * If there are issues with `make` recognizing bash, you may need to go into `esp-open-sdk/crosstool-NG/configure.ac` and change line 193 from `|$EGREP '^GNU bash, version (3.[1-9]|4)')` to `|$EGREP '^GNU bash, version (3.[1-9]|4|5)')`.

# Setting up an IDE
 
Programming in a text editor is nice. I think working in an IDE is nicer. Installing VSCode or Eclipse in Linux is pretty easy, and there are plenty of guides out there.

~~Last year I used [Eclipse IDE for C/C++ Developers](https://www.eclipse.org/downloads/packages/).~~
~~Here's a guide to using Eclipse from within WSL: https://www.cs.odu.edu/~zeil/FAQs/Public/win10Bash/~~
~~Eclipse & WSL is kind of janky. I find Eclipse runs better in either native Linux or a virtual machine.~~

This year I'm trying [Visual Studio Code](https://code.visualstudio.com/)
Here's a guide to using Visual Studio Code with WSL: https://code.visualstudio.com/docs/remote/wsl

This repo has project files for both Eclipse and Visual Studio Code. The project is based on a makefile, so you can do whatever you're comfortable with. Simply open the `firmware` folder with your IDE of choice.

I'm a VSCode convert. I do not recommend using Eclipse.

# Flashing the Firmware

1. You will need to add two environment variables to your `.bashrc` file so `makefile` knows where to find the Swadge, and an optional third one to automatically start `putty.exe`.
    * `ESP_PORT` is the ESP8266's serial port, and will be specific to your machine. One way to find this is by running `dmesg | grep tty`, plugging your ESP8266 or programmer in, waiting a few seconds, and rerunning the command. There should be a new line of output the second time which includes the name of your serial port.
    * `ESP_FLASH_BITRATE` is how fast the firmware is flashed. 2000000 is a common value, though if it doesn't work, try something slower, like 1500000.
    * `ESP_PORT_WIN` is an optional Windows COM port to be used when starting `putty.exe` from WSL.
    ```
    $ nano ~/.bashrc
    
    Append this, after changing the port:
    
    export ESP_PORT=/dev/ttyS3
    export ESP_FLASH_BITRATE=2000000
    export ESP_PORT_WIN=COM3
    ```
   If you're using WSL, [the Windows port `COM{N}` maps to the Linux port `/dev/ttyS{N}`](https://blogs.msdn.microsoft.com/wsl/2017/04/14/serial-support-on-the-windows-subsystem-for-linux/). If you're using a virtual machine, you'll need to forward your COM port to a Linux serial port, or configure the VM software to pass the VM the USB interface device, if you're using one.
1. Restart your Linux environment so the environment variables are actually set.
1. Flash the firmware to your Swadge. You may have to set read and write permissions on the serial port before flashing. Also, if you're using the Swadge programmer, make sure to read [Programming with the Programmer](#5-programming-with-the-programmer) below.
    ```
    $ sudo chmod 666 /dev/ttyS3
    $ make burn
    ```

## Hardware Connection

The I2S Out (WS2812 in) pin is the same as RX1 (pin 25), which means if you are programming via the UART, the LEDs will blink wildly.

The UART is used for both programming and printing debug statements, so any serial terminal must be closed before programming. For WSL, the `burn` task calls `burn-dbg.sh`, which kills all instances of [`putty.exe`](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html), flashes the new firmware, and restarts `putty.exe`. Similar scripts may be written for other environments and serial terminals. You cannot start a serial terminal from a makefile, since the makefile requires all programs it started to finish in order to finish.

# Flashing with the Programmer

The Swadge programmer is a breakout for all the ESP8266 pins, a USB-UART chip, and some useful buttons and switches. It can be used in conjunction with [`pyFlashGui`](/pyFlashGui) to program lots of Swadges quickly. To program a Swadge with the programmer:
1. Connect the programmer to your computer and note what serial port is created.
1. Set the Swadge to USB power
1. Set the programmer to "OFF" and "5V"
1. Plug the Swadge into the programmer
1. Either hold the `GPIO0` button if you have a 2.0.x programmer, hold down the `GPIO15` button if you have a 2.1.x programmer, or put a jumper between `GND` and `SCL` breakout pins. See the note below for more information.
1. Set the programmer to "ON"
1. Program your Swadge with the serial port noted in step 6 of [Compiling and Flashing a Swadge](#2-compiling-and-flashing-a-swadge).

## Flashing with a Black Swadge 2022 Programmer 2.1.0

![swadge_programmer_2 1 0](https://user-images.githubusercontent.com/231180/149211700-4c0211d7-9ae9-42f1-9bfb-a7765a6dbdca.jpg)

The 2022 Programmer does not come with a jumper preinstalled on the J5 pins, which means that the power switch *must* be in the `3.3V` position. In order to use `5V` power, you must place a jumper on J5 between the two pins closest to the Swadge (PCIe) connector.

The 2022 Programmer incorrectly labeled the GPIO buttons. What is labeled as `GPIO15` is actually `GPIO0` and what is labeled as `GPIO0` is actually `GPIO15`. This means that to program a Swadge, the Swadge must be powered on while the `GPIO15` button (far right) is held down.

## Flashing with the Programmer on a Mac

So you have a Mac and want to flash some swadges. You download the [`pyFlashGui`](/pyFlashGui) folder and follow all of the instructions, but alas, no dice. Follow these handy troubleshooting tips, attempting to flash a swadge after each step:

1. Double check the instructions on the [`pyFlashGui`](/pyFlashGui) page. Make sure you have Python 3 (mac comes with Python 2 as default). Make sure you properly install `tkinter` and `pyserial`.
1. If the programmer is plugged in, but you see this, it means that either the CP2102n driver isn't installed, or you don't have permissions to access the programmer.

![No programmers detected](https://user-images.githubusercontent.com/11276131/94190388-06192d80-fe7a-11ea-9663-ed11ad73ac27.png)

3. As a sanity check, open a terminal and run `ioreg -p IOUSB -l -w 0`. If you see the text `CP2102N USB to UART Bridge Controller`, you know the programmer is plugged in. If you don't see that text, try plugging in the programmer again.
1. Install the driver from SiLabs: https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers. You may have to do some shenaniganry with your system privacy settings (as per download instructions) in order to install the driver. This is safe.
1. Add read and write permissions to the serial device with the terminal command `sudo chmod 666 /dev/ttys000`. Note that `/dev/ttyS000` may not be your serial device. You can run `ls -la /dev/tty*` to get a list of all serial devices. Pick the one with your username next to it.
1. If you run [`pyFlashGui`](/pyFlashGui), and it connects to the programmer, but you get an `esptool failed becausemodule ‘esptool’ has no attribute ‘main’` error when flashing, then you need to check what version of `esptool` came with your [`pyFlashGui`](/pyFlashGui) folder. `cd` into your [`pyFlashGui`](/pyFlashGui) folder and run `python esptool/esptool.py -h`. `esptool` SHOULD be version 3.0-dev. If you have any other version, you need to remove it and start over. Run `find / -iname "esptool.py" 2>/dev/null` to find all `esptool.py` files on your hard drive, and delete all but the one you're currently using. You may need to uninstall it with `pip` if you had installed it with `pip`, like so: `python2 -m pip uninstall esptool` and `python3 -m pip uninstall esptool`
