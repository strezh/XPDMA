#!/bin/bash

cd driver
make

DEVICE="xpdma"
DRIVER="xpdma_driver"
DEV_PATH="/dev/${DEVICE}"

# Remove module (if loaded)
sudo rm -rf ${DEV_PATH}
sudo rmmod ${DRIVER}

# Insert module 
sudo mknod  ${DEV_PATH} c 241 1
sudo chown user ${DEV_PATH}
sudo chmod 0644 ${DEV_PATH}
ls -al ${DEV_PATH}

sudo insmod ${DRIVER}.ko

