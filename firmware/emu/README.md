# The Swadge Emulator

This guide will get you set up with the Swadge Emulator using Visual Studio Code and Windows Subsystem for Linux or native Linux.
If you're already running native Linux, you can skip the "For Windows" Section.

TODO: Build instructions for native Windows

TODO: Build instructions for Android

## For Windows and WSL
1. Download and install [VcXsrv Windows X Server](https://sourceforge.net/projects/vcxsrv/files/latest/download). This will allow WSL to forward graphics to Windows
1. Download and install [PulseAudio on Windows](http://bosmans.ch/pulseaudio/pulseaudio-1.1.zip). This will allow WSL to forward audio to Windows. PulseAudio on Windows doesn't have a true installer, so you can extract the zip wherever is convenient for you. I recommend adding the extracted bin folder [to your system path](https://docs.telerik.com/teststudio/features/test-runners/add-path-environment-variables) for convenience.
1. In PulseAudio's `etc/default.pa` file, replace this line:
    ```
    #load-module module-native-protocol-tcp
    ```
    with this line:
    ```
    load-module module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1
    ```
	And replace this line:
	```
    load-module module-waveout sink_name=output source_name=input
	```
    with this line:
    ```
    load-module module-waveout sink_name=output source_name=input record=0
    ```
1. In PulseAudio's `etc/daemon.conf` file, replace this line:
    ```
    ; exit-idle-time = 20
    ```
    with this line:
    ```
    exit-idle-time = -1
    ```
1. Start VcXsrv by running XLaunch. All the default options are fine. When it's running, you should see an X icon in the system tray.
1. Start pulseaudio.exe. You can double click on it or run it in a terminal like PowerShell. 
1. Now open a WSL terminal, either standalone or in Visual Studio Code. Install pulseaudio and alsa-utils:
	```
	# sudo apt-get install pulseaudio alsa-utils
	```
1. Run this command to tell WSL where to forward audio and graphics:
	```
	# printf "\nexport DISPLAY=:0.0\nexport PULSE_SERVER=tcp:localhost\n" >> ~/.bashrc
	```
1. Restart WSL by closing all open windows.
1. Check that graphics are working by running the program `xeyes`. You should see eyes following your pointer. You can close the eyes. Unless you like being watched, of course.
1. Check that audio is working by running `speaker-test -t sine -f 220 -s1`. Warning, this might be loud, so turn your speakers down first. You should hear a sine wave. Exit this program with `ctrl+c` otherwise it will run forever.

If you see eyes and hear a tone, you're good to go. Remember that both VcXsrv and pulseaudio must be running in Windows before starting WSL.

## For Linux

1. Even if you've checked out and built this project previously, make sure to initialize the new submodules for the emulator
	```
	# git submodule update --init --recursive
	```
1. Install the dependencies
	```
	# sudo apt-get install libasound2-dev mesa-common-dev
	```
1. To build the emulator, run `make` in the `emu` folder.
    
	If you are running Visual Studio Code, you can also also build with `ctrl+shift+b` and select the `make all (emulator)` build task.
1. The emulator also needs a copy of `assets.bin` in the `firmware/emu/` folder. You can generate this by running `make clean all` from the `firmware/` folder, then copying `assets.bin` from `firmware/` to `/firmware/emu/`.
1. To run the emulator run `./swadgemu` from the `emu` folder.
    
	If you are running Visual Studio Code, you can also run with `F5`. This will also automatically attach GDB, so you can set breakpoints, watch variables, and otherwise debug as you do.
