#!/bin/bash

taskkill.exe /IM "putty.exe" /F
make burn
cmd.exe /C "START /B putty.exe -serial $ESP_PORT_WIN -sercfg 74880"