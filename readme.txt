*************************************************************************
   ____  ____ 
  /   /\/   / 
 /___/  \  /   
 \   \   \/    © Copyright 2013 Xilinx, Inc. All rights reserved.
  \   \        This file contains confidential and proprietary 
  /   /        information of Xilinx, Inc. and is protected under U.S. 
 /___/   /\    and international copyright and other intellectual 
 \   \  /  \   property laws. 
  \___\/\___\ 
 
*************************************************************************

Vendor: Xilinx 
Current readme.txt Version: <1.0.0>
Date Last Modified:  20SEP2013
Date Created: <20AUG2013>

Associated Filename: xapp1171.zip
Associated Document: XAPP1171, PCI Express Endpoint-DMA Initiator Subsystem

Supported Device(s): Kintex-7, Zynq 7000
   
*************************************************************************

Disclaimer: 

      This disclaimer is not a license and does not grant any rights to 
      the materials distributed herewith. Except as otherwise provided in 
      a valid license issued to you by Xilinx, and to the maximum extent 
      permitted by applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE 
      "AS IS" AND WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL 
      WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, 
      INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, 
      NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and 
      (2) Xilinx shall not be liable (whether in contract or tort, 
      including negligence, or under any other theory of liability) for 
      any loss or damage of any kind or nature related to, arising under 
      or in connection with these materials, including for any direct, or 
      any indirect, special, incidental, or consequential loss or damage 
      (including loss of data, profits, goodwill, or any type of loss or 
      damage suffered as a result of any action brought by a third party) 
      even if such damage or loss was reasonably foreseeable or Xilinx 
      had been advised of the possibility of the same.

Critical Applications:

      Xilinx products are not designed or intended to be fail-safe, or 
      for use in any application requiring fail-safe performance, such as 
      life-support or safety devices or systems, Class III medical 
      devices, nuclear facilities, applications related to the deployment 
      of airbags, or any other applications that could lead to death, 
      personal injury, or severe property or environmental damage 
      (individually and collectively, "Critical Applications"). Customer 
      assumes the sole risk and liability of any use of Xilinx products 
      in Critical Applications, subject only to applicable laws and 
      regulations governing limitations on product liability.

THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS 
FILE AT ALL TIMES.

*************************************************************************

This readme file contains these sections:

1. REVISION HISTORY
2. OVERVIEW
3. SOFTWARE TOOLS AND SYSTEM REQUIREMENTS
4. DESIGN FILE HIERARCHY
5. INSTALLATION AND OPERATING INSTRUCTIONS
6. SUPPORT


1. REVISION HISTORY 

            Readme  
Date        Version      Revision Description
=========================================================================
20SEP2013   1.0          Initial Xilinx release.
=========================================================================


2. OVERVIEW

This readme describes how to use the files that come with XAPP1171

This directory contains the files necessary to generate the PCIe-CDMA 
Subsystem for the Kintex KC705 and Zynq ZC706 development boards. The 
Subsystem consists of various IPs and demonstrates the use of the AXI PCI
Express and AXI Central DMA IP cores to initiate data transfers over
PCI Express.

This application note and the associated designs demonstrate several key
features including:
  - Generating a Block Diagram subsystem in Vivado IP Integrator using 
    Vivado TCL commands and scripting
  - PCI Express Endpoint configuration
  - DMA initiated data transfers over PCI Express
  - Achieve High-Throughput into the Zynq-7000 device processing system 
    (PS) through the High-Performance AXI interface
  - Dynamic Address Translation between a 64-bit Root Complex (Host) 
    address space and a 32-bit FPGA (AXI) address space
  - A methodology to perform DMA Scatter Gather (SG) operations using 
    Dynamic Address Translation

Included IP Cores:
  - Zynq-7000 Processing System (ZC706 design only)
  - MIG AXI DDR3 Memory Controller (KC705 design only)
  - AXI Memory Mapped to PCI Express
  - AXI Central Direct Memory Access (CDMA)
  - AXI Interface Block RAM Controller
  - Block Memory Generator
  - AXI Interconnect
  - Proc System Reset


3. SOFTWARE TOOLS AND SYSTEM REQUIREMENTS

* Xilinx Vivado Design Suite 2013.3
* Xilinx Software Development Kit (SDK) 14.7


4. DESIGN FILE HIERARCHY

The directory structure below this top-level folder is described 
below:

\kintexSubsystemFiles\           KC705 generation scripts and design files
 |                               directory.
 +-- kintexGenerationScript.tcl  Subsystem generation script.
 +-- kintexDesignWrapper.v       Top-level design wrapper.
 +-- kintexConstraints.xdc       Top-level XDC constraints file.
 +-- kintexDdrConfiguration.prj  DDR3 configuration for the MIG DDR3 Memory
                                 controller.
\zynqSubsystemFiles\             ZC706 generation scripts and design files
 |                               directory.
 +-- zynqGenerationScript.tcl    Subsystem generation script.
 +-- zynqDesignWrapper.v         Top-level design wrapper.
 +-- zynqConstraints.xdc         Top-level XDC constraints file. 


5. INSTALLATION AND OPERATING INSTRUCTIONS 

To Generate the Subsystem use the following steps.
  1) Download the appropriate files for your device (ZC706 or KC705).
  2) Modify the "FILES_DIR" variable inside the <Device>GenerationScript.tcl
      file to match your environment.
  3) Create the project and block diagram by running Vivado with the -source 
      option to run the <Device>GenerationScript.tcl script.
          vivado do -source <Device>SubsystemFiles/<Device>GenerationScript.tcl
  4) View the generated block diagram and customize as necessary.
  5) Click "Generate Bitstream" in the flow navigator to run synthesis and
      implementation.


6. SUPPORT

To obtain technical support for this application note, go to 
www.xilinx.com/support to locate answers to known issues in the Xilinx
Answers Database or to create a WebCase. 

