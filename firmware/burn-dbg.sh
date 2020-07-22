#!/bin/bash

taskkill.exe /IM "putty.exe" /F
unset ESP_GDB && export SET_SWADGE_VERSION=5 && make -j$(nproc) all
make burn
cmd.exe /C "START /B putty.exe -serial $ESP_PORT_WIN -sercfg 74880"