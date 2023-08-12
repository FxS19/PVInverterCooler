#!/bin/bash
echo "Removing old spiffs image"
rm ./LITTLEFS/*.littleFs
echo ""
# https://github.com/igrr/mkspiffs
echo "Creating SPIFFS image"
mklittlefs -c data/  -p 256 -b 8192 -s 2072576 ./LITTLEFS/data.littleFs
echo ""
echo "Uploading"
# https://github.com/espressif/esptool
esptool.py --baud 921600 write_flash 0x200000 ./LITTLEFS/data.littleFs

# https://github.com/esp8266/Arduino/issues/2977
