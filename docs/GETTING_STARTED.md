# 1. Getting a Linux Environment

The Swadge is compiled from a Linux environment. If you have one of those already, great, skip to part two. If you don't, here's how to set up Window Subsystem for Linux.

1. Install Windows Subsystem for Linux (WSL) by following this guide: https://docs.microsoft.com/en-us/windows/wsl/install-win10
1. Install Ubuntu 18.04 LTS from the Microsoft Store: https://www.microsoft.com/store/apps/9N9TNGVNDL3Q
1. Initialize Ubuntu 18.04 LTS by following the guide here: https://docs.microsoft.com/en-us/windows/wsl/initialize-distro

You can also set up a Linux virtual machine, like [Xubuntu](https://xubuntu.org/download/) running in [VirtualBox](https://www.virtualbox.org/wiki/Downloads). Google for a guide if you need one.

It is also possible to set up a Mac environment using a fork of pfalcon's esp-open-sdk (see below). See the following which outlines the procedure. https://github.com/pfalcon/esp-open-sdk/issues/342
especially the comments made on March 15 2019 by phibo23. 


# 2. Compiling and Flashing a Swadge

1. Update your packages and install all dependencies
    ```
    $ sudo apt-get update
    $ sudo apt-get dist-upgrade
    $ sudo apt-get install build-essential make unrar-free autoconf automake libtool gcc g++ gperf flex bison texinfo gawk ncurses-dev libexpat-dev python-dev python python-serial sed git unzip bash help2man wget bzip2 libtool-bin libusb-1.0-0-dev
    ```
1. Check out the [pfalcon's esp-open-sdk](https://github.com/pfalcon/esp-open-sdk), move to the ```esp-open-sdk``` folder and build it. More detailed instructions are on that project's page. Warning, building this takes a while (like 30 minutes)!
    ```
    $ git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
    $ cd esp-open-sdk/
    /esp-open-sdk$ make
    ```
1. Set up environment variables by appending the following to your ```.bashrc``` file. You'll want to modify them with your own home folder name.
    ```
    $ nano ~/.bashrc
    
    Append this, after changing the username in the path:
    export PATH=/home/adam/esp-open-sdk/xtensa-lx106-elf/bin:$PATH
    export ESP_ROOT=/home/adam/esp-open-sdk
    ```
1. Restart your Linux environment so the environment variables are actually set.
1. Check out this repository, move to the ```firmware``` folder, and build it. 
    ```
    $ git clone --recursive https://github.com/AEFeinstein/Swadge-Devkit-Fw.git
    $ cd Swadge-Devkit-Fw/firmware/
    /Swadge-Devkit-Fw/firmware$ make
    ```
1. Flash the firmware to an ESP8266. You will need to add two environment variables to your ```.bashrc``` file so ```makefile``` knows where to find the Swadge, and an optional third one to automatically start ```putty.exe```. ```ESP_PORT``` is the ESP8266's serial port, and will be specific to your machine. ```ESP_FLASH_BITRATE``` is how fast the firmware is flashed. 2000000 is a common value, though if it doesn't work, try something slower, like 1500000. ```ESP_PORT_WIN``` is an optional Windows COM port to be used when starting ```putty.exe``` from WSL.
    ```
    $ nano ~/.bashrc
    
    Append this, after changing the port:
    export ESP_PORT=/dev/ttyS3
    export ESP_FLASH_BITRATE=2000000
    export ESP_PORT_WIN=COM3
    ```
   If you're using WSL, [the Windows port ```COM{N}``` maps to the Linux port ```/dev/ttyS{N}```](https://blogs.msdn.microsoft.com/wsl/2017/04/14/serial-support-on-the-windows-subsystem-for-linux/). If you're using a virtual machine, you'll need to forward your COM port to a Linux serial port.
1. Restart your Linux environment so the environment variables are actually set.
1. Flash the firmware to your Swadge. You may have to set read and write permissions on the serial port before flashing.
    ```
    $ sudo chmod 666 /dev/ttyS3
    $ make burn
    ```

# 3. Setting up an IDE
 
Programming in a text editor is nice. I think working in an IDE is nicer. Installing VSCode or Eclipse in Linux is pretty easy, and there are plenty of guides out there.

Last year I used [Eclipse IDE for C/C++ Developers](https://www.eclipse.org/downloads/packages/).
Here's a guide to using Eclipse from within WSL: https://www.cs.odu.edu/~zeil/FAQs/Public/win10Bash/
Eclipse & WSL is kind of janky. I find Eclipse runs better in either native Linux or a virtual machine.

This year I'm trying [Visual Studio Code](https://code.visualstudio.com/)
Here's a guide to using Visual Studio Code with WSL: https://code.visualstudio.com/docs/remote/wsl

This repo has project files for both Eclipse and Visual Studio Code. The project is based on a makefile, so you can do whatever you're comfortable with.

# 4. Hardware Connection

The I2S Out (WS2812 in) pin is the same as RX1 (pin 25), which means if you are programming via the UART, the LEDs will blink wildly.

The UART is used for both programming and printing debug statements, so any serial terminal must be closed before programming. For WSL, the ```burn``` task calls ```burn-dbg.sh```, which kills all instances of [```putty.exe```](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html), flashes the new firmware, and restarts ```putty.exe```. Similar scripts may be written for other environments and serial terminals. You cannot start a serial terminal from a makefile, since the makefile requires all programs it started to finish in order to finish.

# 5. Programming with the Programmer

The Swadge programmer is a breakout for all the ESP8266 pins, a USB-UART chip, and some useful buttons and switches. It can be used in conjunction with [pyFlashGui](https://github.com/AEFeinstein/Swadge-Devkit-Fw/tree/master/piflash/pyFlashGui) to program lots of Swadges quickly. To program a Swadge with the programmer:
1. Connect the programmer to your computer and note what serial port is created.
1. Set the Swadge to USB power
2. Set the programmer to "OFF" and "5V"
3. Plug the Swadge into the programmer
4. Either hold the `GPIO0` button or put a jumper between `GND` and `SCL` breakout pins
5. Set the programmer to "ON"
6. Program your Swadge with the serial port noted in step 1.