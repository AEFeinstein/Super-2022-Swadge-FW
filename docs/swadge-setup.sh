#!/bin/bash

# This script is based on the build-firmware job found in
# https://github.com/AEFeinstein/Super-2022-Swadge-FW/blob/master/.circleci/config.yml
# It was tested on Ubuntu 21.04 on March 7th, 2021
# If the build step cannot find the xtensa compiler, try rebooting and running the script again

# Move to the home folder
cd ~/

# Install Linux package dependencies
sudo apt-get update
sudo apt-get -y install build-essential make curl unrar-free autoconf automake libtool gcc g++ gperf flex bison texinfo gawk ncurses-dev libexpat-dev sed git unzip bash help2man wget bzip2 libtool-bin python3-dev python3 python3-serial python3-pip python-is-python3 zip libx11-dev libpulse-dev libasound2-dev

# Install Python dependencies
pip3 install pillow rtttl

# Install ESP8266 toolchain
wget https://github.com/cnlohr/esp82xx_bin_toolchain/raw/master/esp-open-sdk-x86_64-20200810.tar.xz
tar xJvf esp-open-sdk-x86_64-20200810.tar.xz
rm esp-open-sdk-x86_64-20200810.tar.xz

# Set up environment variables
echo 'export ESP_ROOT=$HOME/esp-open-sdk' >> ~/.bashrc
echo 'export PATH=$HOME/esp-open-sdk/xtensa-lx106-elf/bin:$PATH' >> ~/.bashrc
# And add them temporarily for this script too
export ESP_ROOT=$HOME/esp-open-sdk
export PATH=$HOME/esp-open-sdk/xtensa-lx106-elf/bin:$PATH

# Clone the git repo
git clone https://github.com/AEFeinstein/Super-2022-Swadge-FW.git --recurse-submodules

# Build the project
cd ./Super-2022-Swadge-FW/firmware
# Firmware
unset ESP_GDB && export SET_SWADGE_VERSION=5 && make -j$(nproc) all debug
# Emulator
unset ESP_GDB && export SET_SWADGE_VERSION=5 && make -C emu -j$(nproc) all
