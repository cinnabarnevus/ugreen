#!/bin/bash
echo "upgrade mcu fw"
i2cset -y -f 12 0x15 0x60 0x01
sleep 1
./mcu_upgrade_tool  /dev/i2c-12 ./fw/mini54-application.bin
