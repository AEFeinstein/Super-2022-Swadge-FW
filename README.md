# Swadge Devkit Firmware

Repo for the Swadge 2020 Devkit

Hardware repo is over here: https://github.com/AEFeinstein/Swadge-Devkit

## Getting Started Guide

### Getting a Linux Environment

The Swadge is compiled from a Linux environment. If you have one of those already, great, skip to part two. If you don't, here's how to set up Window Subsystem for Linux.

1. Install Windows Subsystem for Linux by folloing this guide: https://docs.microsoft.com/en-us/windows/wsl/install-win10
1. Install Ubuntu 18.04 LTS from the Microsoft Store: https://www.microsoft.com/store/apps/9N9TNGVNDL3Q
1. Initialize Ubuntu 18.04 LTS by folling the guide here: https://docs.microsoft.com/en-us/windows/wsl/initialize-distro

### Compiling and Flashing a Swadge

1. Update your packages and install all dependencies
    ```
    adam@DESKTOP-CAVEKDJ:~$ sudo apt-get update
    adam@DESKTOP-CAVEKDJ:~$ sudo apt-get dist-upgrade
    adam@DESKTOP-CAVEKDJ:~$ sudo apt-get install build-essential make unrar-free autoconf automake libtool gcc g++ gperf flex bison texinfo gawk ncurses-dev libexpat-dev python-dev python python-serial sed git unzip bash help2man wget bzip2 libtool-bin libusb-1.0-0-dev
    ```
1. Check out the [pfalcon's esp-open-sdk](https://github.com/pfalcon/esp-open-sdk), move to the ```esp-open-sdk``` folder and build it. More detailed instructions are on this project's page. Warning, building this takes a while (like 30 minutes)!
    ```
    adam@DESKTOP-CAVEKDJ:~$ git clone --recursive https://github.com/pfalcon/esp-open-sdk.git
    adam@DESKTOP-CAVEKDJ:~$ cd esp-open-sdk/
    adam@DESKTOP-CAVEKDJ:~/esp-open-sdk$ make
    ```
1. Set up environment variables by appending the following to your ```.bashrc``` file. You'll want to modify them with your own home folder name.
    ```
    adam@DESKTOP-CAVEKDJ:~$ nano ~/.bashrc
    
    Modify and append this:
    export PATH=/home/adam/esp-open-sdk/xtensa-lx106-elf/bin:$PATH
    export ESP_ROOT=/home/adam/esp-open-sdk
    ```
1. Restart your Linux environment so the environment variables are actually set.
1. Check out this repository, move to the ```firmware``` folder, and build it
    ```
    adam@DESKTOP-CAVEKDJ:~/esp-open-sdk$ cd ~/
    adam@DESKTOP-CAVEKDJ:~$ git clone --recursive https://github.com/AEFeinstein/Swadge-Devkit-Fw.git
    adam@DESKTOP-CAVEKDJ:~$ cd Swadge-Devkit-Fw/firmware/
    adam@DESKTOP-CAVEKDJ:~/Swadge-Devkit-Fw/firmware$ make
    ```
1. Flash the firmware to an ESP8266. You make have to modify ```PORT``` in ```makefile``` to point to your specific serial port. If you're using Windows Subsystem for Linux, [the Windows port ```COM{N}``` maps to the Linux port ```/dev/ttyS{N}```](https://blogs.msdn.microsoft.com/wsl/2017/04/14/serial-support-on-the-windows-subsystem-for-linux/). You may have to set read and write permissions on the serial port before burning.
    ```
    adam@DESKTOP-CAVEKDJ:~/Swadge-Devkit-Fw/firmware$ sudo chmod 666 /dev/ttyS4
    adam@DESKTOP-CAVEKDJ:~/Swadge-Devkit-Fw/firmware$ make burn
    ```
 ### Setting up an Eclipse IDE
 
 This is optional, and coming soon
