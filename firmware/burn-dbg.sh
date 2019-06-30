#!/bin/bash

taskkill.exe /IM "putty.exe" /F
make burn
putty.exe -serial COM3 -sercfg 74880
