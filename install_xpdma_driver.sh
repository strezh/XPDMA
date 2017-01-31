#!/bin/bash

cd driver
make

DEVICE="xpdma"
DEV_PATH="/dev/${DEVICE}"

# Remove module (if loaded)
sudo make unload > /dev/null

# Insert module 
sudo make load

cd ../software
make
