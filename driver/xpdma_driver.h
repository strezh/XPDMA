#ifndef XPDMA_DRIVER_H
#define XPDMA_DRIVER_H

#define DEVICE_NAME "xpdma"

#define VENDOR_ID 0x10EE // Xilinx Vendor ID
#define DEVICE_ID 0x7024

#define SUCCESS      0
#define CRIT_ERR    -1

// Struct Used for Writing CDMA Register
typedef struct {
    uint32_t reg;
    uint32_t value;
} cdmaReg_t;

// ioctl commands
enum {
    IOCTL_RESET, // Reset CDMA

    IOCTL_RDCFGREG,  // Read PCIe config registers
    IOCTL_WRCFGREG,  // Write PCIe config registers
    
    IOCTL_RDCDMAREG, // Read CDMA config registers
    IOCTL_WRCDMAREG, // Write CDMA config registers

    IOCTL_SEND,      // Send data from Host system to AXI CDMA
    IOCTL_RECV,      // Receive data from AXI CDMA to Host system
};

#endif //XPDMA_DRIVER_H
