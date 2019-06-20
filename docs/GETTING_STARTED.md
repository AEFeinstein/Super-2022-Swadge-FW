# 1. Getting a Linux Environment

The Swadge is compiled from a Linux environment. If you have one of those already, great, skip to part two. If you don't, here's how to set up Window Subsystem for Linux.

1. Install Windows Subsystem for Linux (WSL) by following this guide: https://docs.microsoft.com/en-us/windows/wsl/install-win10
1. Install Ubuntu 18.04 LTS from the Microsoft Store: https://www.microsoft.com/store/apps/9N9TNGVNDL3Q
1. Initialize Ubuntu 18.04 LTS by following the guide here: https://docs.microsoft.com/en-us/windows/wsl/initialize-distro

You can also set up a Linux virtual machine, like [Xubuntu](https://xubuntu.org/download/) running in [VirtualBox](https://www.virtualbox.org/wiki/Downloads). Google for a guide if you need one.

# 2. Compiling and Flashing a Swadge

1. Update your packages and install all dependencies
    ```
    $ sudo apt-get update
    $ sudo apt-get dist-upgrade
    $ sudo apt-get install build-essential make unrar-free autoconf automake libtool gcc g++ gperf flex bison texinfo gawk ncurses-dev libexpat-dev python-dev python python-serial sed git unzip bash help2man wget bzip2 libtool-bin libusb-1.0-0-dev
    ```
1. Check out the [pfalcon's esp-open-sdk](https://github.com/pfalcon/esp-open-sdk), move to the ```esp-open-sdk``` folder and build it. More detailed instructions are on this project's page. Warning, building this takes a while (like 30 minutes)!
    ```
    $ git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
    $ cd esp-open-sdk/
    /esp-open-sdk$ make
    ```
1. Set up environment variables by appending the following to your ```.bashrc``` file. You'll want to modify them with your own home folder name. If you're using WSL, you might have to [follow these instructions to remove Windows paths from your ```PATH``` variable](https://stackoverflow.com/a/51345880).
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
1. Flash the firmware to an ESP8266. You make have to modify ```PORT``` in ```makefile``` to point to your specific serial port. If you're using WSL, [the Windows port ```COM{N}``` maps to the Linux port ```/dev/ttyS{N}```](https://blogs.msdn.microsoft.com/wsl/2017/04/14/serial-support-on-the-windows-subsystem-for-linux/). If you're using a virtual machine, you'll need to forward your COM port to a Linux serial device. You may have to set read and write permissions on the serial port before burning.
    ```
    /Swadge-Devkit-Fw/firmware$ sudo chmod 666 /dev/ttyS4
    /Swadge-Devkit-Fw/firmware$ make burn
    ```
    If you have problems with burning the firmware or transfering page data over network (`make netburn` or `make netweb`), you should removing ```VERIFY_FLASH_WRITE``` from the makefile's list of ```DEFINES```. This way the ESP checks if the flash is written correctly.

# 3. Setting up an IDE
 
Programming in a text editor is nice. I think working in an IDE is nicer. Installing VSCode or Eclipse in Linux is pretty easy, and there are plenty of guides out there.

Last year I used [Eclipse IDE for C/C++ Developers](https://www.eclipse.org/downloads/packages/).
Here's a guide to using Eclipse from within WSL: https://www.cs.odu.edu/~zeil/FAQs/Public/win10Bash/
Eclipse & WSL is kind of janky. I find Eclipse runs better in either native Linux or a virtual machine.

This year I'm trying [Visual Studio Code](https://code.visualstudio.com/)
Here's a guide to using Visual Studio Code with WSL: https://code.visualstudio.com/docs/remote/wsl

This repo has project files for both Eclipse and Visual Studio Code. The project is based on a makefile, so you can do whatever you're comfortable with.
