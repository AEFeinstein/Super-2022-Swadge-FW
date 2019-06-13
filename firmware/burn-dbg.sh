#!/bin/bash

/mnt/c/Windows/system32/taskkill.exe /IM "putty.exe" /F
make burn
/mnt/c/Windows/system32/cmd.exe /C "START /B putty.exe -serial COM4 -sercfg 74880"
