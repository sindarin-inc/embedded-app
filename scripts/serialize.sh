#!/usr/bin/env sh

set -e

pbpaste > serial_number.bin
SN=$(cat serial_number.bin)
if [[ $(wc -c < serial_number.bin) -ne 14 ]]; then
    echo "File length is not 14 characters";
    exit 1
fi
read -p "Flashing ${SN} Are you sure? [y/n] " answer
if [[ ${answer} != "y" ]]; then
    echo "Aborting"
    exit 0
fi
# Recommended music for burning fuses:
# https://www.youtube.com/watch?v=hVck6DkOi38
# 🎶 burn burn yes you're gonna burn 🎶
echo "Burning ${SN} to fuses..."
espefuse.py --debug --port /dev/cu.usbmodem* --chip esp32s3 burn_block_data --offset 8 BLOCK_USR_DATA serial_number.bin
rm serial_number.bin
