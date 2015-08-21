#!/bin/bash

cd driver
make

DEVICE="xpdma"
DEV_PATH="/dev/${DEVICE}"

# Remove module (if loaded)
sudo make unload
sudo rm -rf ${DEV_PATH}

# Insert module 
sudo mknod  ${DEV_PATH} c 241 1
sudo chown user ${DEV_PATH}
sudo chmod 0644 ${DEV_PATH}
ls -al ${DEV_PATH}

sudo make load

cd ../software
make
