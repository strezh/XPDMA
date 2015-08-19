# XPDMA

## Overview

Xilinx PCI Express Endpoint-DMA Initiator Subsystem based on Xilinx XAPP1171 for KC705 Development Board

Now tested in Linux Debian 7.0 (Wheezy) with Linux kernel 3.2.0 x64

## Features

v.0.0.1
- system generation script updated for Xilinx Vivado 14.3
- fix AXI addresses and block sizes
- tested on KC705 only
- make scatter gather mode in driver
- add linux driver for read/write DDR over PCIe
- speed on 100 MB data test (don't included now):
    Write: ~580 MB/s
    Read:  ~560 MB/s

## TODO
- add simple test software for driver

