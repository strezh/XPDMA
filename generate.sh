#!/bin/bash

VIVADO_PATH='/opt/Xilinx/Vivado/2015.4'

if [ -z "$PATH" ]; then
  PATH=${VIVADO_PATH}/bin
else
  PATH=${VIVADO_PATH}/bin:$PATH
fi
export PATH

vivado -source ./kintexSubsystemFiles/kintexGenerationScript.tcl &
