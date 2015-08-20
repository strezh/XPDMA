#-------------------------------------------------------
#
# Description: This file contains the procedures 
#              necissary to generate the PCIe-CDMA 
#              subsystem for the Kintex KC705 
#              development board
#
#-------------------------------------------------------


# Set up the required file variables
# Update the FILES_DIR variable with your path to the downloaded directory
set FILES_DIR "[pwd]/kintexSubsystemFiles"
set CONSTRAINTS_FILE "${FILES_DIR}/kintexConstraints.xdc"
set DESIGN_WRAPPER_FILE "${FILES_DIR}/kintexDesignWrapper.v"
set MIG_FILE "${FILES_DIR}/kintexDdrConfiguration.prj"


#-------------------------------------------------------
# Procedure:   create_hier_cell_axi_interconnect_block
# Description: Procedure to create the AXI Interconnect 
#              block for the PCIe-CDMA Subsystem
# Instanced IP: AXI Interconnect
#-------------------------------------------------------
proc create_hier_cell_axi_interconnect_block {} {

  # Create cell and set as current instance
  create_bd_cell -type hier axi_interconnect_block
  current_bd_instance axi_interconnect_block

  # Create interface pins
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 cdma_s_axi
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 cdma_s_axi_sg
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 pcie_s_axi
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 cdma_m_axi_lite
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 translation_bram_m_axi
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 pcie_m_axi
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 pcie_m_axi_ctl
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 user_m_axi

  # Create pins
  create_bd_pin -dir I -type clk aclk
  create_bd_pin -dir I -type rst aresetn
  create_bd_pin -dir I -type clk user_aclk_in
  create_bd_pin -dir I -type rst user_aresetn_in
  create_bd_pin -dir I -type clk pcie_ctl_aclk

  # Create instance: axi_interconnect_1, and set properties
  set axi_interconnect_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_1 ]
  set_property -dict [list CONFIG.NUM_SI {3} \
                           CONFIG.NUM_MI {5} \
                           CONFIG.STRATEGY {2}] $axi_interconnect_1

  # Create interface connections
  connect_bd_intf_net -intf_net cdma_data_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/cdma_s_axi] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S00_AXI]
  connect_bd_intf_net -intf_net cdma_sg_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/cdma_s_axi_sg] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S01_AXI]
  connect_bd_intf_net -intf_net pcie_s_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/pcie_s_axi] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S02_AXI]
  connect_bd_intf_net -intf_net cdma_lite_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/cdma_m_axi_lite] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M00_AXI]
  connect_bd_intf_net -intf_net translation_bram_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/translation_bram_m_axi] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M01_AXI]
  connect_bd_intf_net -intf_net pcie_m_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/pcie_m_axi] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M03_AXI]
  connect_bd_intf_net -intf_net pcie_ctl_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/pcie_m_axi_ctl] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M04_AXI]
  connect_bd_intf_net -intf_net user_m_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/user_m_axi] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M02_AXI]

  # Create port connections
  connect_bd_net -net aclk [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/aclk] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S00_ACLK] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M00_ACLK] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/ACLK] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S01_ACLK] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S02_ACLK] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M03_ACLK] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M01_ACLK]
  connect_bd_net -net aresetn [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/aresetn] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/ARESETN] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S00_ARESETN] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M00_ARESETN] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M01_ARESETN] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S01_ARESETN] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/S02_ARESETN] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M03_ARESETN] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M04_ARESETN]
  connect_bd_net -net user_aclk [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/user_aclk_in] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M02_ACLK]
  connect_bd_net -net user_aresetn [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/user_aresetn_in] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M02_ARESETN]
  connect_bd_net -net pcie_ctl_aclk [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/pcie_ctl_aclk] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/axi_interconnect_1/M04_ACLK]

  # Set parent as current instance
  current_bd_instance ..
}


#-------------------------------------------------------
# Procedure:   create_hier_cell_pcie_cdma_subsystem
# Description: Procedure to create PCIe-CDMA subsystem
#              block
# Instanced IP: AXI Interconnect
#               AXI Memory Controller
#               Block RAM
#               AXI Central DMA
#               AXI Memory Mapped to PCI Express
# Required Custom Procs:
#              create_hier_cell_axi_interconnect_block
#-------------------------------------------------------
proc create_hier_cell_pcie_cdma_subsystem {} {

  # Create cell and set as current instance
  create_bd_cell -type hier pcie_cdma_subsystem
  current_bd_instance pcie_cdma_subsystem

  # Create interface pins
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 user_m_axi
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:pcie_7x_mgt_rtl:1.0 ext_pcie

  # Create pins
  create_bd_pin -dir O -type clk user_aclk_out
  create_bd_pin -dir O mmcm_lock
  create_bd_pin -dir I -type clk user_aclk_in
  create_bd_pin -dir I -type rst user_aresetn_in
  create_bd_pin -dir I -type clk pcie_ref_clk_100MHz
  create_bd_pin -dir I -type rst interconnect_aresetn
  create_bd_pin -dir I -type rst peripheral_aresetn

  # Create instance: translation_bram_mem, and set properties
  set translation_bram_mem [ create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.2 translation_bram_mem ]
  set_property -dict [list CONFIG.Memory_Type {True_Dual_Port_RAM}] $translation_bram_mem

  # Create instance: axi_cdma_1, and set properties
  set axi_cdma_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_cdma:4.1 axi_cdma_1 ]
  set_property -dict [list CONFIG.C_M_AXI_DATA_WIDTH {128} CONFIG.C_M_AXI_MAX_BURST_LEN {128}] $axi_cdma_1

  # Create instance: axi_pcie_1, and set properties
  set axi_pcie_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_pcie:2.5 axi_pcie_1 ]
  set_property -dict [list CONFIG.XLNX_REF_BOARD {KC705_REVC}      \
                           CONFIG.PCIE_CAP_SLOT_IMPLEMENTED {true} \
                           CONFIG.NO_OF_LANES {X8}                 \
                           CONFIG.DEVICE_ID {0x7024}               \
                           CONFIG.BAR_64BIT {true}                 \
                           CONFIG.BAR0_ENABLED {true}              \
                           CONFIG.BAR0_SCALE {Kilobytes}           \
                           CONFIG.BAR0_SIZE {64}                   \
                           CONFIG.PCIEBAR2AXIBAR_0 {0x81000000}    \
                           CONFIG.COMP_TIMEOUT {50ms}              \
                           CONFIG.AXIBAR_NUM {2}                   \
                           CONFIG.AXIBAR_AS_0 {true}               \
                           CONFIG.AXIBAR2PCIEBAR_0 {0xa0000000}    \
                           CONFIG.AXIBAR_AS_1 {true}               \
                           CONFIG.AXIBAR2PCIEBAR_1 {0xc0000000}    \
                           CONFIG.S_AXI_SUPPORTS_NARROW_BURST {true}] $axi_pcie_1

  # Create instance: translation_bram, and set properties
  set translation_bram [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.0 translation_bram ]
  set_property -dict [list CONFIG.DATA_WIDTH {128}] $translation_bram

  # Create instance: Constant block for the PCIe Core
  set intx_msi_constant [create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 intx_msi_constant]
  set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {0}] $intx_msi_constant

  # Create instance: Constant block for the PCIe Core
  set msi_vector_constant [create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 msi_vector_constant]
  set_property -dict [list CONFIG.CONST_WIDTH {5} CONFIG.CONST_VAL {0}] $msi_vector_constant

  # Create instance: axi_interconnect_block
  create_hier_cell_axi_interconnect_block

  # Create interface connections
  connect_bd_intf_net -intf_net ext_pcie_bus [get_bd_intf_pins /pcie_cdma_subsystem/ext_pcie] [get_bd_intf_pins /pcie_cdma_subsystem/axi_pcie_1/pcie_7x_mgt]
  connect_bd_intf_net -intf_net cdma_axi_sg_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_cdma_1/M_AXI_SG] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/cdma_s_axi_sg]
  connect_bd_intf_net -intf_net cdma_axi_dm_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_cdma_1/m_axi] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/cdma_s_axi]
  connect_bd_intf_net -intf_net pcie_m_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_pcie_1/M_AXI] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/pcie_s_axi]
  connect_bd_intf_net -intf_net pcie_axi_ctl_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_pcie_1/S_AXI_CTL] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/pcie_m_axi_ctl]
  connect_bd_intf_net -intf_net cdma_axi_lite_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/cdma_m_axi_lite] [get_bd_intf_pins /pcie_cdma_subsystem/axi_cdma_1/s_axi_lite]
  connect_bd_intf_net -intf_net pcie_s_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/pcie_m_axi] [get_bd_intf_pins /pcie_cdma_subsystem/axi_pcie_1/S_AXI]
  connect_bd_intf_net -intf_net translation_bram_bram_porta [get_bd_intf_pins /pcie_cdma_subsystem/translation_bram_mem/BRAM_PORTA] [get_bd_intf_pins /pcie_cdma_subsystem/translation_bram/BRAM_PORTA]
  connect_bd_intf_net -intf_net translation_bram_bram_portb [get_bd_intf_pins /pcie_cdma_subsystem/translation_bram_mem/BRAM_PORTB] [get_bd_intf_pins /pcie_cdma_subsystem/translation_bram/BRAM_PORTB]
  connect_bd_intf_net -intf_net translation_bram_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/translation_bram/S_AXI] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/translation_bram_m_axi]
  connect_bd_intf_net -intf_net user_m_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/user_m_axi] [get_bd_intf_pins /pcie_cdma_subsystem/axi_interconnect_block/user_m_axi]

  # Create port connections
  connect_bd_net -net msi_vector_constant_net [get_bd_pins /pcie_cdma_subsystem/msi_vector_constant/dout] [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/MSI_Vector_Num]
  connect_bd_net -net intx_msi_constant_net [get_bd_pins /pcie_cdma_subsystem/intx_msi_constant/dout] [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/INTX_MSI_Request]
  connect_bd_net -net pcie_axi_aclk [get_bd_pins /pcie_cdma_subsystem/user_aclk_out] [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/axi_aclk_out] [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/axi_aclk] [get_bd_pins /pcie_cdma_subsystem/translation_bram/S_AXI_ACLK] [get_bd_pins /pcie_cdma_subsystem/axi_cdma_1/s_axi_lite_aclk] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/aclk] [get_bd_pins /pcie_cdma_subsystem/axi_cdma_1/m_axi_aclk]
  connect_bd_net -net pcie_mmcm_lock [get_bd_pins /pcie_cdma_subsystem/mmcm_lock] [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/mmcm_lock]
  connect_bd_net -net axi_peripheral_aresetn [get_bd_pins /pcie_cdma_subsystem/peripheral_aresetn] [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/axi_aresetn] [get_bd_pins /pcie_cdma_subsystem/translation_bram/S_AXI_ARESETN] [get_bd_pins /pcie_cdma_subsystem/axi_cdma_1/s_axi_lite_aresetn]
  connect_bd_net -net axi_interconnect_aresetn [get_bd_pins /pcie_cdma_subsystem/interconnect_aresetn] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/aresetn]
  connect_bd_net -net sys_clk_1 [get_bd_pins /pcie_cdma_subsystem/pcie_ref_clk_100MHz] [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/REFCLK]
  connect_bd_net -net user_aclk [get_bd_pins /pcie_cdma_subsystem/user_aclk_in] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/user_aclk_in]
  connect_bd_net -net user_aresetn [get_bd_pins /pcie_cdma_subsystem/user_aresetn_in] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/user_aresetn_in]
  connect_bd_net -net pcie_ctl_aclk [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/axi_ctl_aclk] [get_bd_pins /pcie_cdma_subsystem/axi_pcie_1/axi_ctl_aclk_out] [get_bd_pins /pcie_cdma_subsystem/axi_interconnect_block/pcie_ctl_aclk]

  # Set parent as current instance
  current_bd_instance ..
}


#-------------------------------------------------------
# Procedure:   generateSubsystem
# Description: Procedure to generate the block diagram 
#              for the PCIe-CDMA Subsystem
# Instanced IP: System Reset
#               MIG DDR3 Controller
#               Util Vector Logic
# Required Custom Procs:
#              create_hier_cell_pcie_cdma_subsystem
#-------------------------------------------------------
proc generateSubsystem {migFile projName designName} {
  # Top level instance
  current_bd_instance

  # Create interface ports
  create_bd_intf_port -mode Master -vlnv xilinx.com:interface:ddrx_rtl:1.0 EXT_DDR3
  create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 EXT_SYS_CLK
  create_bd_intf_port -mode Master -vlnv xilinx.com:interface:pcie_7x_mgt_rtl:1.0 EXT_PCIE

  # Create ports
  create_bd_port -dir O -type clk pcie_clk_125MHz
  create_bd_port -dir O -type clk ddr_clk_100MHz
  create_bd_port -dir O -type rst ddr_rst
  create_bd_port -dir I -type rst EXT_SYS_RST
  set_property CONFIG.POLARITY ACTIVE_HIGH [get_bd_ports /EXT_SYS_RST]
  create_bd_port -dir I -type clk EXT_PCIE_REFCLK_100MHz
  create_bd_port -dir I -type rst reset_logic_mmcm_locked_in
  create_bd_port -dir O pcie_mmcm_locked
  create_bd_port -dir O ddr_rdy

  # Create instance: proc_sys_reset_1, and set properties
  set proc_sys_reset_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_1 ]

  # Create an inverter for the Reset signal as required by the DDR
  set ddr_reset_inv [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_vector_logic:1.0 ddr_reset_inv ]
  set_property -dict [list CONFIG.C_SIZE {1} CONFIG.C_OPERATION {not}] $ddr_reset_inv

  # Create Instance ddr3_mem and set properties
  set ddr_mem [ create_bd_cell -type ip -vlnv xilinx.com:ip:mig_7series:2.2 ddr3_mem ]
  file copy ${migFile} ./${projName}/${projName}.srcs/sources_1/bd/${designName}/ip/${designName}_ddr3_mem_0/mig_a.prj
  set_property CONFIG.XML_INPUT_FILE {mig_a.prj} $ddr_mem

  # Create instance: pcie_cdma_subsystem
  create_hier_cell_pcie_cdma_subsystem

  # Create interface connections
  connect_bd_intf_net -intf_net ext_ddr3_bus [get_bd_intf_ports /EXT_DDR3] [get_bd_intf_pins /ddr3_mem/DDR3]
  connect_bd_intf_net -intf_net ddr3_axi_bus [get_bd_intf_pins /pcie_cdma_subsystem/user_m_axi] [get_bd_intf_pins /ddr3_mem/S_AXI]
  connect_bd_intf_net -intf_net sys_clk [get_bd_intf_ports /EXT_SYS_CLK] [get_bd_intf_pins /ddr3_mem/SYS_CLK]
  connect_bd_intf_net -intf_net ext_pcie_bus [get_bd_intf_pins /pcie_cdma_subsystem/ext_pcie] [get_bd_intf_ports /ext_pcie]
  
  # Create port connections
  connect_bd_net -net axi_peripheral_aresetn [get_bd_pins /ddr3_mem/aresetn] [get_bd_pins /proc_sys_reset_1/Peripheral_aresetn] [get_bd_pins /pcie_cdma_subsystem/peripheral_aresetn]
  connect_bd_net -net axi_interconnect_aresetn [get_bd_pins /proc_sys_reset_1/Interconnect_aresetn] [get_bd_pins /pcie_cdma_subsystem/interconnect_aresetn]
  connect_bd_net -net ext_pcie_refclk_100mhz_1 [get_bd_ports /EXT_PCIE_REFCLK_100MHz] [get_bd_pins /pcie_cdma_subsystem/pcie_ref_clk_100MHz]
  connect_bd_net -net reset_logic_dcm_locked_in_1 [get_bd_ports /reset_logic_mmcm_locked_in] [get_bd_pins /proc_sys_reset_1/Dcm_locked]
  connect_bd_net -net pcie_cdma_subsystem_mmcm_lock [get_bd_ports /pcie_mmcm_locked] [get_bd_pins /pcie_cdma_subsystem/mmcm_lock]
  connect_bd_net -net ddr_rdy [get_bd_ports /ddr_rdy] [get_bd_pins /ddr3_mem/init_calib_complete]
  connect_bd_net -net ddr3_aclk [get_bd_ports /ddr_clk_100MHz] [get_bd_pins /proc_sys_reset_1/Slowest_sync_clk] [get_bd_pins /ddr3_mem/ui_clk] [get_bd_pins /pcie_cdma_subsystem/user_aclk_in]
  connect_bd_net -net pcie_aclk [get_bd_ports /pcie_clk_125MHz] [get_bd_pins /pcie_cdma_subsystem/user_aclk_out]
  connect_bd_net -net sys_reset [get_bd_ports /EXT_SYS_RST] [get_bd_pins /ddr3_mem/sys_rst] [get_bd_pins /proc_sys_reset_1/ext_reset_in]
  connect_bd_net -net ddr_reset_out [get_bd_pins /ddr3_mem/ui_clk_sync_rst] [get_bd_pins /ddr_reset_inv/Op1] [get_bd_ports /ddr_rst]
  connect_bd_net -net ddr3_aresetn [get_bd_pins /ddr_reset_inv/Res] [get_bd_pins /pcie_cdma_subsystem/user_aresetn_in]

  # Create address segments
  # DMA Data Port
  create_bd_addr_seg -range 32K -offset 0x81000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data] [get_bd_addr_segs /pcie_cdma_subsystem/translation_bram/S_AXI/Mem0] DMA_2_TransBram
  create_bd_addr_seg -range 16K -offset 0x81008000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI_CTL/CTL0] DMA_2_PcieCtl
  create_bd_addr_seg -range 64K -offset 0x80800000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI/BAR0] DMA_2_PcieSG
  create_bd_addr_seg -range 64K -offset 0x80000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI/BAR1] DMA_2_PcieDM
  create_bd_addr_seg -range 1G  -offset 0x00000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data] [get_bd_addr_segs /ddr3_mem/memmap/memaddr] DMA_2_Ddr3
  create_bd_addr_seg -range 16K -offset 0x8100C000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data] [get_bd_addr_segs /pcie_cdma_subsystem/axi_cdma_1/S_AXI_LITE/Reg] DMA_2_PCIe

  # DMA SG Port
  create_bd_addr_seg -range 64K -offset 0x80800000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data_SG] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI/BAR0] DMAsg_2_PcieSG
  create_bd_addr_seg -range 64K -offset 0x80000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data_SG] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI/BAR1] DMAsg_2_PcieDM
  create_bd_addr_seg -range 16K -offset 0x8100C000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data_SG] [get_bd_addr_segs /pcie_cdma_subsystem/axi_cdma_1/S_AXI_LITE/Reg] DMAsg_2_PCIe
  create_bd_addr_seg -range 32K -offset 0x81000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data_SG] [get_bd_addr_segs /pcie_cdma_subsystem/translation_bram/S_AXI/Mem0] DMAsg_2_TransBram
  create_bd_addr_seg -range 1G  -offset 0x00000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data_SG] [get_bd_addr_segs /ddr3_mem/memmap/memaddr] DMAsg_2_Ddr3
  create_bd_addr_seg -range 16K -offset 0x81008000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_cdma_1/Data_SG] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI_CTL/CTL0] DMAsg_2_PcieCtl

  # PCIe Master Port
  create_bd_addr_seg -range 32K -offset 0x81000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_pcie_1/M_AXI] [get_bd_addr_segs /pcie_cdma_subsystem/translation_bram/S_AXI/Mem0] PCIe_2_TransBram
  create_bd_addr_seg -range 16K -offset 0x81008000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_pcie_1/M_AXI] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI_CTL/CTL0] PCIe_2_PcieCtl
  create_bd_addr_seg -range 16K -offset 0x8100c000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_pcie_1/M_AXI] [get_bd_addr_segs /pcie_cdma_subsystem/axi_cdma_1/S_AXI_LITE/Reg] PCIe_2_DmaCtl
  create_bd_addr_seg -range 64K -offset 0x80800000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_pcie_1/M_AXI] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI/BAR0] PCIeSG_2_DMA
  create_bd_addr_seg -range 64K -offset 0x80000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_pcie_1/M_AXI] [get_bd_addr_segs /pcie_cdma_subsystem/axi_pcie_1/S_AXI/BAR1] PCIeDM_2_DMA
  create_bd_addr_seg -range 1G  -offset 0x00000000 [get_bd_addr_spaces /pcie_cdma_subsystem/axi_pcie_1/M_AXI] [get_bd_addr_segs /ddr3_mem/memmap/memaddr] PCIe_2_Ddr3
}


#-------------------------------------------------------
# Procedure:   generateProject
# Description: Generates the project, creates the block  
#              diagram, populates the block diagram,
#              generates the required IPs, and adds the
#              top level verilog and constraints files.
# Required Custom Procs:
#              generateSubsystem
#-------------------------------------------------------
proc generateProject {constraintsFile designWrapperFile migFile} {

  # Project Setup Variables
  set PROJ_NAME "project_1"
  set SUBSYSTEM_NAME "design_1"
  set PART "xc7k325tffg900-2"
  set BOARD "xilinx.com:kintex7:kc705:1.0"

  # Create the project - Builds project in the current directory.
  create_project $PROJ_NAME ./$PROJ_NAME -part $PART
  set_property board $BOARD [current_project]

  # Create the block Diagram
  create_bd_design $SUBSYSTEM_NAME

  # Generate the Subsystem
  generateSubsystem $migFile $PROJ_NAME $SUBSYSTEM_NAME

  # Generate the block Diagram
  save_bd_design
  generate_target  {synthesis simulation implementation}  [get_files */${SUBSYSTEM_NAME}.bd]

  # Generate the top Level HDL Wrapper
  import_files -norecurse $designWrapperFile

  # Generate Constraints file
  add_files -fileset constrs_1 -norecurse $constraintsFile
  import_files -fileset constrs_1 $constraintsFile

  # Update the compile order after addding the new files
  update_compile_order -fileset sources_1
  update_compile_order -fileset sim_1

}


#-------------------------------------------------------
# Procedure:   runSynthAndImpl
# Description: Runs Synthesis and Implementation for the
#              design that was generated
#-------------------------------------------------------
proc runSynthAndImpl {constraintsFile designWrapperFile} {

  # Uncomment the following lines to run synthesis as part of the generation
  launch_runs synth_1
  wait_on_run synth_1

  # Uncomment the following lines to run implementation as part of the generation
  launch_runs impl_1 -to_step write_bitstream
  wait_on_run impl_1

}


# Run the procedure to generate the project and block diagram
generateProject $CONSTRAINTS_FILE $DESIGN_WRAPPER_FILE $MIG_FILE

# Uncomment the following line to run Synthesis and Implementation on 
# the design during generation
#runSynthAndImpl

# Print out completion message
puts "Generation of the subsystem has completed."
