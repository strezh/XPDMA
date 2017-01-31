/**
 *
 */

#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* Needed for files operations */
#include <linux/pci.h>          /* Needed for PCI */
#include <asm/uaccess.h>        /* Needed for copy_to_user & copy_from_user */
#include <linux/delay.h>        /* udelay, mdelay */
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include "xpdma_driver.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("PCIe driver for Xilinx CDMA subsystem (XAPP1171), Linux");
MODULE_AUTHOR("Strezhik Iurii");

// Max CDMA buffer size
#define MAX_BTT             0x007FFFFF   // 8 MBytes maximum for DMA Transfer */
#define BUF_SIZE            (4<<20)      // 4 MBytes read/write buffer size
#define TRANSFER_SIZE       (4<<20)      // 4 MBytes transfer size for scatter gather
#define DESCRIPTOR_SIZE     64           // 64-byte aligned Transfer Descriptor

#define BRAM_OFFSET         0x00000000   // Translation BRAM offset
#define PCIE_CTL_OFFSET     0x00008000   // AXI PCIe control offset
#define CDMA_OFFSET         0x0000c000   // AXI CDMA LITE control offset

// AXI CDMA Register Offsets
#define CDMA_CONTROL_OFFSET 0x00         // Control Register
#define CDMA_STATUS_OFFSET  0x04         // Status Register
#define CDMA_CDESC_OFFSET   0x08         // Current descriptor Register
#define CDMA_TDESC_OFFSET   0x10         // Tail descriptor Register
#define CDMA_SRCADDR_OFFSET 0x18         // Source Address Register
#define CDMA_DSTADDR_OFFSET 0x20         // Dest Address Register
#define CDMA_BTT_OFFSET     0x28         // Bytes to transfer Register

#define AXI_PCIE_DM_ADDR    0x80000000   // AXI:BAR1 Address
#define AXI_PCIE_SG_ADDR    0x80800000   // AXI:BAR0 Address
#define AXI_BRAM_ADDR       0x81000000   // AXI Translation BRAM Address
#define AXI_DDR3_ADDR       0x00000000   // AXI DDR3 Address

#define SG_COMPLETE_MASK    0xF0000000   // Scatter Gather Operation Complete status flag mask
#define SG_DEC_ERR_MASK     0x40000000   // Scatter Gather Operation Decode Error flag mask
#define SG_SLAVE_ERR_MASK   0x20000000   // Scatter Gather Operation Slave Error flag mask
#define SG_INT_ERR_MASK     0x10000000   // Scatter Gather Operation Internal Error flag mask

#define BRAM_STEP           0x8          // Translation Vector Length
#define ADDR_BTT            0x00000008   // 64 bit address translation descriptor control length

#define CDMA_CR_SG_EN       0x00000008   // Scatter gather mode enable
#define CDMA_CR_IDLE_MASK   0x00000002   // CDMA Idle mask
#define CDMA_CR_RESET_MASK  0x00000004   // CDMA Reset mask
#define AXIBAR2PCIEBAR_0U   0x208        // AXI:BAR0 Upper Address Translation (bits [63:32])
#define AXIBAR2PCIEBAR_0L   0x20C        // AXI:BAR0 Lower Address Translation (bits [31:0])
#define AXIBAR2PCIEBAR_1U   0x210        // AXI:BAR1 Upper Address Translation (bits [63:32])
#define AXIBAR2PCIEBAR_1L   0x214        // AXI:BAR1 Lower Address Translation (bits [31:0])

#define CDMA_RESET_LOOP	    1000000      // Reset timeout counter limit
#define SG_TRANSFER_LOOP    1000000      // Scatter Gather Transfer timeout counter limit

// Scatter Gather Transfer descriptor
typedef struct {
    u32 nextDesc;   /* 0x00 */
    u32 na1;	    /* 0x04 */
    u32 srcAddr;    /* 0x08 */
    u32 na2;        /* 0x0C */
    u32 destAddr;   /* 0x10 */
    u32 na3;        /* 0x14 */
    u32 control;    /* 0x18 */
    u32 status;     /* 0x1C */
} __aligned(DESCRIPTOR_SIZE) sg_desc_t;

#define HAVE_KERNEL_REG     0x01    // Kernel registration
#define HAVE_MEM_REGION     0x02    // I/O Memory region

int gDrvrMajor = 241;               // Major number not dynamic
int gKernelRegFlag = 0;


static dev_t first;         // Global variable for the first device number
static struct cdev c_dev;     // Global variable for the character device structure
static struct class *cl;     // Global variable for the device class

//semaphores
static struct semaphore gSemDma;

struct xpdma_state {
    struct pci_dev *dev;
    bool used;
    unsigned int statFlags /*= 0x00*/; // Status flags used for cleanup
    unsigned long baseHdwr;        // Base register address (Hardware address) 
    unsigned long baseLen;         // Base register address Length
    void *baseVirt /*= NULL*/;         // Base register address (Virtual address, for I/O)
    char *readBuffer /*= NULL*/;       // Pointer to dword aligned DMA Read buffer
    char *writeBuffer /*= NULL*/;      // Pointer to dword aligned DMA Write buffer
    sg_desc_t *descChain;          // Translation Descriptors chain
    size_t descChainLength;
    dma_addr_t readHWAddr;
    dma_addr_t writeHWAddr;
    dma_addr_t descChainHWAddr;
};

static struct xpdma_state xpdmas[XPDMA_NUM_MAX];


// struct pci_dev *xpdmas[id].dev = NULL;        // PCI device structure
// unsigned int xpdmas[id].statFlags = 0x00;     // Status flags used for cleanup
// unsigned long xpdmas[id].baseHdwr;            // Base register address (Hardware address)
// unsigned long xpdmas[id].baseLen;             // Base register address Length
// void *xpdmas[id].baseVirt = NULL;             // Base register address (Virtual address, for I/O)
// char *xpdmas[id].readBuffer = NULL;           // Pointer to dword aligned DMA Read buffer
// char *xpdmas[id].writeBuffer = NULL;          // Pointer to dword aligned DMA Write buffer

// sg_desc_t *xpdmas[id].descChain;              // Translation Descriptors chain
// size_t xpdmas[id].descChainLength;

// dma_addr_t xpdmas[id].readHWAddr;
// dma_addr_t xpdmas[id].writeHWAddr;
// dma_addr_t xpdmas[id].descChainHWAddr;

// Prototypes
static int xpdma_reset(int id);
// ssize_t xpdma_write (int id, struct file *filp, const char *buf, size_t count, loff_t *f_pos);
// ssize_t xpdma_read (int id, struct file *filp, char *buf, size_t count, loff_t *f_pos);
long xpdma_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
int xpdma_open(struct inode *inode, struct file *filp);
int xpdma_release(struct inode *inode, struct file *filp);
static inline u32 xpdma_readReg (int id, u32 reg);
static inline void xpdma_writeReg (int id, u32 reg, u32 val);
ssize_t xpdma_send (int id, void *data, size_t count, u32 addr);
ssize_t xpdma_recv (int id, void *data, size_t count, u32 addr);
void xpdma_showInfo (int id);

// Aliasing write, read, ioctl, etc...
struct file_operations xpdma_intf = {
        //read           : xpdma_read,
        //write          : xpdma_write,
        unlocked_ioctl : xpdma_ioctl,
        //llseek         : xpdma_lseek,
        open           : xpdma_open,
        release        : xpdma_release,
};

// ssize_t xpdma_write (int id, struct file *filp, const char *buf, size_t count, loff_t *f_pos)
// {
// //    dma_addr_t dma_addr;
// 
//     /*if ( (count % 4) != 0 )  {
//         printk("%s: xpdma_writeMem: Buffer length not dword aligned.\n",DEVICE_NAME);
//         return (CRIT_ERR);
//     }*/
// 
//     // Now it is safe to copy the data from user space.
//     if ( copy_from_user(xpdmas[id].writeBuffer, buf, count) )  {
//         printk("%s: xpdma_writeMem: Failed copy from user.\n", DEVICE_NAME);
//         return (CRIT_ERR);
//     }
// 
//     //TODO: set DMA semaphore
// 
//     printk("%s: xpdma_writeMem: WriteBuf Virt Addr = %lX Phy Addr = %lX.\n",
//            DEVICE_NAME, (size_t)xpdmas[id].writeBuffer, (size_t)xpdmas[id].writeHWAddr);
// 
//     //TODO: release DMA semaphore
// 
//     printk(KERN_INFO"%s: XPCIe_Write: %lu bytes have been written...\n", DEVICE_NAME, count);
// 
//     return (SUCCESS);
// }
// 
// ssize_t xpdma_read (int id, struct file *filp, char *buf, size_t count, loff_t *f_pos)
// {
//     //TODO: set DMA semaphore
// 
//     printk("%s: xpdma_readMem: ReadBuf Virt Addr = %lX Phy Addr = %lX.\n",
//            DEVICE_NAME, (size_t)xpdmas[id].readBuffer, (size_t)xpdmas[id].readHWAddr);
// 
//     //TODO: release DMA semaphore
// 
//     // copy the data to user space.
//     if ( copy_to_user(buf, xpdmas[id].readBuffer, count) )  {
//         printk("%s: xpdma_readMem: Failed copy to user.\n", DEVICE_NAME);
//         return (CRIT_ERR);
//     }
// 
//     printk(KERN_INFO"%s: XPCIe_Read: %lu bytes have been read...\n", DEVICE_NAME, count);
//     return (SUCCESS);
// }
// 
long xpdma_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
    u32 regx = 0;
    int result = CRIT_ERR;
    
    down(&gSemDma);
  
//    printk(KERN_INFO"%s: Ioctl command: %d \n", DEVICE_NAME, cmd);
    switch (cmd) {
        case IOCTL_RESET:
            result = xpdma_reset((*(int *)arg));
            break;
        case IOCTL_RDCDMAREG: // Read CDMA config registers
//             printk(KERN_INFO"%s: FPGA %d\n", DEVICE_NAME, (*(cdmaReg_t *)arg).id);
//             printk(KERN_INFO"%s: Read Register 0x%X\n", DEVICE_NAME, (*(cdmaReg_t *)arg).reg);
            regx = xpdma_readReg((*(cdmaReg_t *)arg).id, (*(cdmaReg_t *)arg).reg);
            (*(cdmaReg_t *)arg).value = regx;
//             printk(KERN_INFO"%s: Readed value 0x%X\n", DEVICE_NAME, regx);
            result = SUCCESS;
            break;
        case IOCTL_WRCDMAREG: // Write CDMA config registers
//             printk(KERN_INFO"%s: FPGA %d\n", DEVICE_NAME, (*(cdmaReg_t *)arg).id);
//             printk(KERN_INFO"%s: Write Register 0x%X\n", DEVICE_NAME, (*(cdmaReg_t *)arg).reg);
//             printk(KERN_INFO"%s: Write Value 0x%X\n", DEVICE_NAME, (*(cdmaReg_t *)arg).value);
            xpdma_writeReg((*(cdmaReg_t *)arg).id, (*(cdmaReg_t *)arg).reg, (*(cdmaReg_t *)arg).value);
            result = SUCCESS;
            break;
        case IOCTL_RDCFGREG:
            // TODO: Read PCIe config registers
            result = SUCCESS;
            break;
        case IOCTL_WRCFGREG:
            // TODO: Write PCIe config registers
            result = SUCCESS;
            break;
        case IOCTL_SEND:
            // Send data from Host system to AXI CDMA
//             printk(KERN_INFO"%s: FPGA %d\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).id);
//             printk(KERN_INFO"%s: Send Data size 0x%X\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).count);
//             printk(KERN_INFO"%s: Send Data address 0x%X\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).addr);
            result = xpdma_send ((*(cdmaBuffer_t *)arg).id, (*(cdmaBuffer_t *)arg).data, (*(cdmaBuffer_t *)arg).count, (*(cdmaBuffer_t *)arg).addr);
//             printk(KERN_INFO"%s: Sended\n", DEVICE_NAME);
            break;
        case IOCTL_RECV:
            // Receive data from AXI CDMA to Host system
//             printk(KERN_INFO"%s: FPGA %d\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).id);
//             printk(KERN_INFO"%s: Receive Data size 0x%X\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).count);
//             printk(KERN_INFO"%s: Receive Data address 0x%X\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).addr);
            result = xpdma_recv ((*(cdmaBuffer_t *)arg).id, (*(cdmaBuffer_t *)arg).data, (*(cdmaBuffer_t *)arg).count, (*(cdmaBuffer_t *)arg).addr);
//             printk(KERN_INFO"%s: Received\n", DEVICE_NAME);
            break;
        case IOCTL_INFO:
            xpdma_showInfo ((*(int *)arg));
            result = SUCCESS;
            break;
        default:
            break;
    }
    
    up(&gSemDma);

    return result;
}

void xpdma_showInfo (int id)
{
    uint32_t c = 0;
    
    if (!xpdmas[id].used) {
        printk(KERN_WARNING"%s: FPGA %d don't initialized!\n", DEVICE_NAME, id);
        return;
    }

    printk(KERN_INFO"%s: INFORMATION\n", DEVICE_NAME);
    printk(KERN_INFO"%s: HOST REGIONS:\n", DEVICE_NAME);
    printk(KERN_INFO"%s: xpdmas[id].baseVirt: 0x%lX\n", DEVICE_NAME, (size_t) xpdmas[id].baseVirt);
    printk(KERN_INFO"%s: xpdmas[id].readBuffer address: 0x%lX\n", DEVICE_NAME, (size_t) xpdmas[id].readBuffer);
    printk(KERN_INFO"%s: xpdmas[id].readBuffer: %s\n", DEVICE_NAME, xpdmas[id].readBuffer);
    printk(KERN_INFO"%s: xpdmas[id].writeBuffer address: 0x%lX\n", DEVICE_NAME, (size_t) xpdmas[id].writeBuffer);
    printk(KERN_INFO"%s: xpdmas[id].writeBuffer: %s\n", DEVICE_NAME, xpdmas[id].writeBuffer);
    printk(KERN_INFO"%s: xpdmas[id].descChain:          0x%lX\n", DEVICE_NAME, (size_t) xpdmas[id].descChain);
    printk(KERN_INFO"%s: xpdmas[id].descChainLength:   0x%lX\n", DEVICE_NAME, (size_t) xpdmas[id].descChainLength);

    printk(KERN_INFO"%s: REGISTERS:\n", DEVICE_NAME);

    printk(KERN_INFO"%s: BRAM:\n", DEVICE_NAME);
    for (c = 0; c <= 8*4; c += 4)
        printk(KERN_INFO"%s: 0x%08X: 0x%08X\n", DEVICE_NAME, BRAM_OFFSET + c, xpdma_readReg(id, BRAM_OFFSET + c));

    printk(KERN_INFO"%s: PCIe CTL:\n", DEVICE_NAME);
    printk(KERN_INFO"%s: 0x%08X: 0x%08X\n", DEVICE_NAME, PCIE_CTL_OFFSET, xpdma_readReg(id, PCIE_CTL_OFFSET));
    for (c = 0x208; c <= 0x234 ; c += 4)
        printk(KERN_INFO"%s: 0x%08X: 0x%08X\n", DEVICE_NAME, PCIE_CTL_OFFSET + c, xpdma_readReg(id, PCIE_CTL_OFFSET + c));

    printk(KERN_INFO"%s: CDMA CTL:\n", DEVICE_NAME);
    for (c = 0; c <= 0x28; c += 4)
        printk(KERN_INFO"%s: 0x%08X: 0x%08X\n", DEVICE_NAME, CDMA_OFFSET + c, xpdma_readReg(id, CDMA_OFFSET + c));
}

ssize_t create_desc_chain(int id, int direction, u32 size, u32 addr)
{
    // length of desctriptors chain
    u32 count = 0;
    u32 sgAddr = AXI_PCIE_SG_ADDR; // current descriptor address in chain
    u32 bramAddr = AXI_BRAM_ADDR ; // Translation BRAM Address
    u32 btt = 0;                   // current descriptor BTT
    u32 unmappedSize = size;       // unmapped data size
    u32 srcAddr = 0;               // source address (SG_DM of DDR3)
    u32 destAddr = 0;              // destination address (SG_DM of DDR3)

    xpdmas[id].descChainLength = (size + (u32)(TRANSFER_SIZE) - 1) / (u32)(TRANSFER_SIZE);
//    printk(KERN_INFO"%s: xpdmas[id].descChainLength = %lu\n", DEVICE_NAME, xpdmas[id].descChainLength);

    // TODO: future: add PCI_DMA_NONE as indicator of MEM 2 MEM transitions
    if (direction == PCI_DMA_FROMDEVICE) {
        srcAddr  = AXI_DDR3_ADDR + addr;
        destAddr = AXI_PCIE_DM_ADDR;
    } else if (direction == PCI_DMA_TODEVICE) {
        srcAddr  = AXI_PCIE_DM_ADDR;
        destAddr = AXI_DDR3_ADDR + addr;
    } else {
        printk(KERN_INFO"%s: Descriptors Chain create error: unknown direction\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    // fill descriptor chain
//    printk(KERN_INFO"%s: fill descriptor chain\n", DEVICE_NAME);
    for (count = 0; count < xpdmas[id].descChainLength; ++count) {
        sg_desc_t *addrDesc = xpdmas[id].descChain + 2 * count; // address translation descriptor
        sg_desc_t *dataDesc = addrDesc + 1;                // target data transfer descriptor
        btt = (unmappedSize > TRANSFER_SIZE) ? TRANSFER_SIZE : unmappedSize;

        // fill address translation descriptor
//        printk(KERN_INFO"%s: fill address translation descriptor\n", DEVICE_NAME);
        addrDesc->nextDesc  = sgAddr + DESCRIPTOR_SIZE;
        addrDesc->srcAddr   = bramAddr;
        addrDesc->destAddr  = AXI_BRAM_ADDR + PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_1U;
        addrDesc->control   = ADDR_BTT;
        addrDesc->status    = 0x00000000;
        sgAddr += DESCRIPTOR_SIZE;

        // fill target data transfer descriptor
//        printk(KERN_INFO"%s: fill address data transfer descriptor\n", DEVICE_NAME);
        dataDesc->nextDesc  = sgAddr + DESCRIPTOR_SIZE;
        dataDesc->srcAddr   = srcAddr;
        dataDesc->destAddr  = destAddr;
        dataDesc->control   = btt;
        dataDesc->status    = 0x00000000;
        sgAddr += DESCRIPTOR_SIZE;

//        printk(KERN_INFO"%s: update counters\n", DEVICE_NAME);
        bramAddr += BRAM_STEP;
        unmappedSize -= btt;
        srcAddr += btt;
        destAddr += btt;
    }

    xpdmas[id].descChain[2 * xpdmas[id].descChainLength - 1].nextDesc = AXI_PCIE_SG_ADDR; // tail descriptor pointed to chain head

    return (SUCCESS);
}

void show_descriptors(int id)
{
    int c = 0;
    sg_desc_t *descriptor = xpdmas[id].descChain;

    if (!xpdmas[id].used) {
        printk(KERN_WARNING"%s: FPGA %d don't initialized!\n", DEVICE_NAME, id);
        return;
    }

    printk(KERN_INFO
    "%s: Translation vectors:\n", DEVICE_NAME);
    printk(KERN_INFO
    "%s: Operation_1 Upper: %08X\n", DEVICE_NAME, xpdma_readReg(id, 0));
    printk(KERN_INFO
    "%s: Operation_1 Lower: %08X\n", DEVICE_NAME, xpdma_readReg(id, 4));
    printk(KERN_INFO
    "%s: Operation_2 Upper: %08X\n", DEVICE_NAME, xpdma_readReg(id, 8));
    printk(KERN_INFO
    "%s: Operation_2 Lower: %08X\n", DEVICE_NAME, xpdma_readReg(id, 12));

    for (c = 0; c < 4; ++c) {
        printk(KERN_INFO
        "%s: Descriptor %d\n", DEVICE_NAME, c);
        printk(KERN_INFO
        "%s: nextDesc 0x%08X\n", DEVICE_NAME, descriptor->nextDesc);
        printk(KERN_INFO
        "%s: srcAddr 0x%08X\n", DEVICE_NAME, descriptor->srcAddr);
        printk(KERN_INFO
        "%s: destAddr 0x%08X\n", DEVICE_NAME, descriptor->destAddr);
        printk(KERN_INFO
        "%s: control 0x%08X\n", DEVICE_NAME, descriptor->control);
        printk(KERN_INFO
        "%s: status 0x%08X\n", DEVICE_NAME, descriptor->status);
        descriptor++;        // target data transfer descriptor
    }
}

int xpdma_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO"%s: Open: module opened\n", DEVICE_NAME);
    return (SUCCESS);
}

static int xpdma_reset(int id)
{
    int loop = CDMA_RESET_LOOP;
    u32 tmp;

    if (!xpdmas[id].used) {
        printk(KERN_WARNING"%s: FPGA %d don't initialized!\n", DEVICE_NAME, id);
        return (CRIT_ERR);
    }

    printk(KERN_INFO"%s: RESET CDMA\n", DEVICE_NAME);
    //down(&gSemDma);
    xpdma_writeReg(id, (CDMA_OFFSET + CDMA_CONTROL_OFFSET),
                   xpdma_readReg(id, CDMA_OFFSET + CDMA_CONTROL_OFFSET) | CDMA_CR_RESET_MASK);

    tmp = xpdma_readReg(id, CDMA_OFFSET + CDMA_CONTROL_OFFSET) & CDMA_CR_RESET_MASK;

    /* Wait for the hardware to finish reset */
    while (loop && tmp) {
        tmp = xpdma_readReg(id, CDMA_OFFSET + CDMA_CONTROL_OFFSET) & CDMA_CR_RESET_MASK;
        loop--;
    }

    if (!loop) {
        printk(KERN_INFO"%s: reset timeout, CONTROL_REG: 0x%08X, STATUS_REG 0x%08X\n",
                DEVICE_NAME,
                xpdma_readReg(id, CDMA_OFFSET + CDMA_CONTROL_OFFSET),
                xpdma_readReg(id, CDMA_OFFSET + CDMA_STATUS_OFFSET));
        return (CRIT_ERR);
    }

    // For Axi CDMA, always do sg transfers if sg mode is built in
    xpdma_writeReg(id, CDMA_OFFSET + CDMA_CONTROL_OFFSET, tmp | CDMA_CR_SG_EN);

    //up(&gSemDma);

    printk(KERN_INFO"%s: SUCCESSFULLY RESET CDMA!\n", DEVICE_NAME);

    return (SUCCESS);
}

static int xpdma_isIdle(int id)
{
    if (!xpdmas[id].used) {
        printk(KERN_WARNING"%s: FPGA %d don't initialized!\n", DEVICE_NAME, id);
        return 0;
    }
    return xpdma_readReg(id, CDMA_OFFSET + CDMA_STATUS_OFFSET) &
           CDMA_CR_IDLE_MASK;
}

static int sg_operation(int id, int direction, size_t count, u32 addr)
{
    u32 status = 0;
    size_t pntr = 0;
    size_t delayTime = 0;
    u32 countBuf = count;
    size_t bramOffset = 0;

    if (!xpdma_isIdle(id)){
        printk(KERN_INFO"%s: CDMA is not idle\n", DEVICE_NAME);
        xpdma_showInfo(id);
        return (CRIT_ERR);
    }

    // 1. Set DMA to Scatter Gather Mode
//    printk(KERN_INFO"%s: 1. Set DMA to Scatter Gather Mode\n", DEVICE_NAME);
    xpdma_writeReg (id, CDMA_OFFSET + CDMA_CONTROL_OFFSET, CDMA_CR_SG_EN);

    // 2. Create Descriptors chain
//    printk(KERN_INFO"%s: 2. Create Descriptors chain\n", DEVICE_NAME);
    create_desc_chain(id, direction, count, addr);

    // 3. Update PCIe Translation vector
    pntr =  (size_t) (xpdmas[id].descChainHWAddr);
//    printk(KERN_INFO"%s: 3. Update PCIe Translation vector\n", DEVICE_NAME);
//    printk(KERN_INFO"%s: xpdmas[id].descChain 0x%016lX\n", DEVICE_NAME, pntr);
    xpdma_writeReg (id, (PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_0L), (pntr >> 0)  & 0xFFFFFFFF); // Lower 32 bit
    xpdma_writeReg (id, (PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_0U), (pntr >> 32) & 0xFFFFFFFF); // Upper 32 bit

    // 4. Write appropriate Translation Vectors
//    printk(KERN_INFO"%s: 4. Write Translation Vectors to BRAM\n", DEVICE_NAME);
    if (PCI_DMA_FROMDEVICE == direction) {
        pntr = (size_t)(xpdmas[id].readHWAddr);
    } else if (PCI_DMA_TODEVICE == direction) {
        pntr = (size_t)(xpdmas[id].writeHWAddr);
    } else {
        printk(KERN_INFO"%s: Write Translation Vectors to BRAM error: unknown direction\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    countBuf = xpdmas[id].descChainLength;
    while (countBuf) {
//        printk(KERN_INFO"%s: pntr 0x%016lX\n", DEVICE_NAME, pntr);
//        printk(KERN_INFO"%s: bramOffset 0x%016lX\n", DEVICE_NAME, bramOffset);
//        printk(KERN_INFO"%s: countBuf 0x%08X\n", DEVICE_NAME, countBuf);
        xpdma_writeReg (id, (BRAM_OFFSET + bramOffset + 4), (pntr >> 0 ) & 0xFFFFFFFF); // Lower 32 bit
        xpdma_writeReg (id, (BRAM_OFFSET + bramOffset + 0), (pntr >> 32) & 0xFFFFFFFF); // Upper 32 bit

        pntr += TRANSFER_SIZE;
        bramOffset += BRAM_STEP;
        countBuf--;
    }

    // 5. Write a valid pointer to DMA CURDESC_PNTR
//    printk(KERN_INFO"%s: 5. Write a valid pointer to DMA CURDESC_PNTR\n", DEVICE_NAME);
    xpdma_writeReg (id, (CDMA_OFFSET + CDMA_CDESC_OFFSET), (AXI_PCIE_SG_ADDR));

    // 6. Write a valid pointer to DMA TAILDESC_PNTR
//    printk(KERN_INFO"%s: 6. Write a valid pointer to DMA TAILDESC_PNTR\n", DEVICE_NAME);
    xpdma_writeReg (id, (CDMA_OFFSET + CDMA_TDESC_OFFSET), (AXI_PCIE_SG_ADDR) + ((2 * xpdmas[id].descChainLength - 1) * (DESCRIPTOR_SIZE)));

    // wait for Scatter Gather operation...
//    printk(KERN_INFO"%s: Scatter Gather must be started!\n", DEVICE_NAME);

    delayTime = SG_TRANSFER_LOOP;
    while (delayTime) {
        delayTime--;
        udelay(10);// TODO: can it be less?

        status = (xpdmas[id].descChain + 2 * xpdmas[id].descChainLength - 1)->status;

//        printk(KERN_INFO
//        "%s: Scatter Gather Operation: loop counter %08X\n", DEVICE_NAME, SG_TRANSFER_LOOP - delayTime);

//        printk(KERN_INFO
//        "%s: Scatter Gather Operation: status 0x%08X\n", DEVICE_NAME, status);

        if (status & SG_DEC_ERR_MASK) {
            printk(KERN_INFO
            "%s: Scatter Gather Operation: Decode Error\n", DEVICE_NAME);
            show_descriptors(id);
            return (CRIT_ERR);
        }

        if (status & SG_SLAVE_ERR_MASK) {
            printk(KERN_INFO
            "%s: Scatter Gather Operation: Slave Error\n", DEVICE_NAME);
            show_descriptors(id);
            return (CRIT_ERR);
        }

        if (status & SG_INT_ERR_MASK) {
            printk(KERN_INFO
            "%s: Scatter Gather Operation: Internal Error\n", DEVICE_NAME);
            show_descriptors(id);
            return (CRIT_ERR);
        }

        if (status & SG_COMPLETE_MASK) {
//            printk(KERN_INFO
//            "%s: Scatter Gather Operation: Completed successfully\n", DEVICE_NAME);
            return (SUCCESS);
        }
    }
//    printk(KERN_INFO
//    "%s: xpdmas[id].readBuffer: %s\n", DEVICE_NAME, xpdmas[id].readBuffer);
//    printk(KERN_INFO
//    "%s: xpdmas[id].writeBuffer: %s\n", DEVICE_NAME, xpdmas[id].writeBuffer);

    printk(KERN_INFO"%s: Scatter Gather Operation error: Timeout Error\n", DEVICE_NAME);
    show_descriptors(id);
    return (CRIT_ERR);
}

static int sg_block(int id, int direction, void *data, size_t count, u32 addr)
{
    size_t unsended = count;
    char *curData = data;
    u32 curAddr = addr;
    u32 btt = BUF_SIZE;

    if ( (addr % 4) != 0 )  {
        printk(KERN_WARNING"%s: Scatter Gather: Address %08X not dword aligned.\n", DEVICE_NAME, addr);
        return (CRIT_ERR);
    }

    // divide block
    while (unsended) {
        btt = (unsended < BUF_SIZE) ? unsended : BUF_SIZE;
//        printk(KERN_INFO"%s: SG Block: BTT=%u\tunsended=%lu \n", DEVICE_NAME, btt, unsended);

        // TODO: remove this multiple checks
        if (PCI_DMA_TODEVICE == direction)
            if ( copy_from_user(xpdmas[id].writeBuffer, curData, btt) )  {
                printk(KERN_WARNING"%s: sg_block: Failed copy from user.\n", DEVICE_NAME);
                return (CRIT_ERR);
            }

        sg_operation(id, direction, btt, curAddr);

        // TODO: remove this multiple checks
        if (PCI_DMA_FROMDEVICE == direction)
            if ( copy_to_user(curData, xpdmas[id].readBuffer, btt) )  {
                printk("%s: sg_block: Failed copy to user.\n", DEVICE_NAME);
                return (CRIT_ERR);
            }

        curData += BUF_SIZE;
        curAddr += BUF_SIZE;
        unsended -= btt;
    }

    return (SUCCESS);
}

ssize_t xpdma_send (int id, void *data, size_t count, u32 addr)
{
    if (!xpdmas[id].used) {
        printk(KERN_WARNING"%s: FPGA %d don't initialized!\n", DEVICE_NAME, id);
        return (CRIT_ERR);
    }

    //down(&gSemDma);
    sg_block(id, PCI_DMA_TODEVICE, (void *)data, count, addr);
    //up(&gSemDma);

    return (SUCCESS);
}

ssize_t xpdma_recv (int id, void *data, size_t count, u32 addr)
{
    if (!xpdmas[id].used) {
        printk(KERN_WARNING"%s: FPGA %d don't initialized!\n", DEVICE_NAME, id);
        return (CRIT_ERR);
    }

    //down(&gSemDma);
    sg_block(id, PCI_DMA_FROMDEVICE, (void *)data, count, addr);
    //up(&gSemDma);

    return (SUCCESS);
}

int xpdma_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO"%s: Release: module released\n", DEVICE_NAME);
    return (SUCCESS);
}

// IO access (with byte addressing)
static inline u32 xpdma_readReg (int id, u32 reg)
{
    if (!xpdmas[id].used) {
        printk(KERN_WARNING"%s: FPGA %d don't initialized!\n", DEVICE_NAME, id);
        return 0;
    }
//    printk(KERN_INFO"%s: xpdma_readReg: address:0x%08X\t\n", DEVICE_NAME, reg);
    return readl(xpdmas[id].baseVirt + reg);
}

static inline void xpdma_writeReg (int id, u32 reg, u32 val)
{
    if (!xpdmas[id].used) {
        printk(KERN_WARNING"%s: FPGA %d don't initialized!\n", DEVICE_NAME, id);
        return;
    }
//    u32 prev = xpdma_readReg(reg);
//    printk(KERN_INFO"%s: xpdma_writeReg: address:0x%08X\t data:0x%08X -> 0x%08X\n", DEVICE_NAME, reg, prev, val);
    writel(val, (xpdmas[id].baseVirt + reg));
}

static int xpdma_getResource(int id) 
{
    //dev = pci_get_device(VENDOR_ID, DEVICE_ID, dev);
    if (NULL == xpdmas[id].dev) {
        printk(KERN_WARNING"%s: getResource: Hardware not found.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    // Set Bus Master Enable (BME) bit
    pci_set_master(xpdmas[id].dev);

    // Get Base Address of BAR0 registers
    xpdmas[id].baseHdwr = pci_resource_start(xpdmas[id].dev, 0);
    if (0 > xpdmas[id].baseHdwr) {
        printk(KERN_WARNING"%s: getResource: Base Address not set.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
//     printk(KERN_INFO"%s: getResource: Base hw val %X\n", DEVICE_NAME, (unsigned int) xpdmas[id].baseHdwr);

    // Get the Base Address Length
    xpdmas[id].baseLen = pci_resource_len(xpdmas[id].dev, 0);
//     printk(KERN_INFO"%s: getResource: Base hw len %d\n", DEVICE_NAME, (unsigned int) xpdmas[id].baseLen);

    // Get Virtual HW address
    xpdmas[id].baseVirt = ioremap(xpdmas[id].baseHdwr, xpdmas[id].baseLen);
    if (!xpdmas[id].baseVirt) {
        printk(KERN_WARNING"%s: getResource: Could not remap memory.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
//    printk(KERN_INFO"%s: Init: Virt HW address %lX\n", DEVICE_NAME, (size_t) xpdmas[id].baseVirt);

    // Check the memory region to see if it is in use
    if (0 > check_mem_region(xpdmas[id].baseHdwr, xpdmas[id].baseLen)) {
        printk(KERN_WARNING"%s: getResource: Memory in use.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    // Try to gain exclusive control of memory for hardware.
    request_mem_region(xpdmas[id].baseHdwr, xpdmas[id].baseLen, "Xilinx_PCIe_CDMA_Driver");
    xpdmas[id].statFlags |= HAVE_MEM_REGION;
//     printk(KERN_INFO"%s: getResource: Initialize Hardware Done..\n", DEVICE_NAME);

    // Bus Master Enable
    if (0 > pci_enable_device(xpdmas[id].dev)) {
        printk(KERN_CRIT"%s: getResource: Device not enabled.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    // Set DMA Mask
    if (0 > pci_set_dma_mask(xpdmas[id].dev, 0x7FFFFFFFFFFFFFFF)) {
        printk(KERN_CRIT"%s: getResource: DMA not supported\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    pci_set_consistent_dma_mask(xpdmas[id].dev, 0x7FFFFFFFFFFFFFFF);

    xpdmas[id].readBuffer = dma_alloc_coherent( &xpdmas[id].dev->dev, BUF_SIZE, &xpdmas[id].readHWAddr, GFP_KERNEL );
    if (NULL == xpdmas[id].readBuffer) {
        printk(KERN_CRIT"%s: getResource: Unable to allocate xpdmas[id].readBuffer\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
//     printk(KERN_INFO"%s: getResource: Read buffer allocated: 0x%016lX, Phy:0x%016lX\n",
//            DEVICE_NAME, (size_t) xpdmas[id].readBuffer, (size_t) xpdmas[id].readHWAddr);

    xpdmas[id].writeBuffer = dma_alloc_coherent( &xpdmas[id].dev->dev, BUF_SIZE, &xpdmas[id].writeHWAddr, GFP_KERNEL );
    if (NULL == xpdmas[id].writeBuffer) {
        printk(KERN_CRIT"%s: getResource: Unable to allocate xpdmas[id].writeBuffer\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
//     printk(KERN_INFO"%s: getResource: Write buffer allocated: 0x%016lX, Phy:0x%016lX\n",
//             DEVICE_NAME, (size_t) xpdmas[id].writeBuffer, (size_t) xpdmas[id].writeHWAddr);

    xpdmas[id].descChain = dma_alloc_coherent( &xpdmas[id].dev->dev, BUF_SIZE, &xpdmas[id].descChainHWAddr, GFP_KERNEL );
    if (NULL == xpdmas[id].descChain) {
        printk(KERN_CRIT"%s: getResource: Unable to allocate xpdmas[id].descChain\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
//     printk(KERN_INFO"%s: getResource: Descriptor chain buffer allocated: 0x%016lX, Phy:0x%016lX\n",
//             DEVICE_NAME, (size_t) (xpdmas[id].descChain), (size_t) xpdmas[id].descChainHWAddr);

    return (SUCCESS);
}

static int xpdma_init (void)
{
    int c = 0;
    sema_init(&gSemDma, 1);

//     printk(KERN_INFO"%s: Init: set default values\n", DEVICE_NAME);
    for (c = 0; c < XPDMA_NUM_MAX; ++c) {
        xpdmas[c].used = 0;
        xpdmas[c].statFlags = 0x00;
        xpdmas[c].baseVirt = NULL;
        xpdmas[c].readBuffer = NULL;
        xpdmas[c].writeBuffer = NULL;
    }

    printk(KERN_INFO"%s: Init: try to found boards\n", DEVICE_NAME);

    for (c = 0; c < XPDMA_NUM_MAX; ++c) {
        xpdmas[c].dev = pci_get_device(VENDOR_ID, DEVICE_ID, (c > 0) ? xpdmas[c-1].dev: NULL);
        if (xpdmas[c].dev) {
            printk(KERN_INFO"%s: Init: found board %d\n", DEVICE_NAME, c);
            if (xpdma_getResource(c) == SUCCESS)
                xpdmas[c].used = 1;
            else
                printk(KERN_WARNING"%s: Init: board %d don't get resources!\n", DEVICE_NAME, c);
        } else {
            printk(KERN_INFO"%s: Init: not found board %d\n", DEVICE_NAME, c);
            break;
        }
    }

    printk(KERN_INFO"%s: Init: finish found boards\n", DEVICE_NAME);

    // Register driver as a character device.
    //if (0 > register_chrdev(gDrvrMajor, DEVICE_NAME, &xpdma_intf)) {
    //    printk(KERN_WARNING"%s: Init: module not register\n", DEVICE_NAME);
    //    return (CRIT_ERR);
    //}

    gDrvrMajor = alloc_chrdev_region( &first, 0, 1, DEVICE_NAME );

    if(0 > gDrvrMajor)
    {
        printk(KERN_ALERT"%s: Device Registration failed\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    //else
    //{
    //    printk( KERN_INFO"%s: Major number is: %d\n",DEVICE_NAME, gDrvrMajor );
    //    return 0;
    //}

    if ( NULL == (cl = class_create( THIS_MODULE, "chardev" ) ))
    {
        printk(KERN_ALERT"%s: Class creation failed\n", DEVICE_NAME);
        unregister_chrdev_region( first, 1 );
        return -1;
    }
    printk(KERN_INFO"%s: Init: module registered\n", DEVICE_NAME);

    if( NULL == device_create( cl, NULL, first, NULL, DEVICE_NAME ))
    {
        printk(KERN_ALERT"%s: Device creation failed\n", DEVICE_NAME);
        class_destroy(cl);
        unregister_chrdev_region( first, 1 );
        return (CRIT_ERR);
    }

    cdev_init( &c_dev, &xpdma_intf );

    if( cdev_add( &c_dev, first, 1 ) == -1)
    {
        printk(KERN_ALERT"%s: Device addition failed\n", DEVICE_NAME);
        device_destroy( cl, first );
        class_destroy( cl );
        unregister_chrdev_region( first, 1 );
        return (CRIT_ERR);
    }

    gKernelRegFlag |= HAVE_KERNEL_REG;
    printk(KERN_INFO"%s: Init: driver is loaded\n", DEVICE_NAME);

    for (c = 0; c < XPDMA_NUM_MAX; ++c) {
        if (xpdmas[c].used) {
            // try to reset CDMA
            if (xpdma_reset(c)) {
                printk(KERN_WARNING"%s: Init: RESET timeout\n", DEVICE_NAME);
                return (CRIT_ERR);
            }
        }
    }

    printk(KERN_INFO"%s: Init: done\n", DEVICE_NAME);
    return (SUCCESS);
}

static void xpdma_exit (void)
{
    int id = 0;

//     printk(KERN_INFO"%s: Exit: unload module resources\n", DEVICE_NAME);
    for (id = 0; id < XPDMA_NUM_MAX; ++id) {
        if (xpdmas[id].used) {
            // Check if we have a memory region and free it
            if (xpdmas[id].statFlags & HAVE_MEM_REGION) {
                release_mem_region(xpdmas[id].baseHdwr, xpdmas[id].baseLen);
            }

//             printk(KERN_INFO"%s: xpdma_exit: erase xpdmas[id].readBuffer\n", DEVICE_NAME);
            // Free Write, Read and Descriptor buffers allocated to use
            if (NULL != xpdmas[id].readBuffer)
                dma_free_coherent( &xpdmas[id].dev->dev, BUF_SIZE, xpdmas[id].readBuffer, xpdmas[id].readHWAddr);

//             printk(KERN_INFO"%s: xpdma_exit: erase xpdmas[id].writeBuffer\n", DEVICE_NAME);
            if (NULL != xpdmas[id].writeBuffer)
                dma_free_coherent( &xpdmas[id].dev->dev, BUF_SIZE, xpdmas[id].writeBuffer, xpdmas[id].writeHWAddr);

//             printk(KERN_INFO"%s: xpdma_exit: erase xpdmas[id].descChain\n", DEVICE_NAME);
            if (NULL != xpdmas[id].writeBuffer)
                dma_free_coherent( &xpdmas[id].dev->dev, BUF_SIZE, xpdmas[id].descChain, xpdmas[id].descChainHWAddr);

            xpdmas[id].readBuffer = NULL;
            xpdmas[id].writeBuffer = NULL;
            xpdmas[id].descChain = NULL;

            // Unmap virtual device address
//             printk(KERN_INFO"%s: xpdma_exit: unmap xpdmas[id].baseVirt\n", DEVICE_NAME);
            if (xpdmas[id].baseVirt != NULL)
                iounmap(xpdmas[id].baseVirt);

            xpdmas[id].baseVirt = NULL;

            xpdmas[id].statFlags = 0;
            xpdmas[id].used = 0;
        }
    }
    // Unregister Device Driver

    if (gKernelRegFlag & HAVE_KERNEL_REG) {
//        unregister_chrdev(gDrvrMajor, DEVICE_NAME);

        cdev_del(&c_dev);
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        printk(KERN_ALERT"%s: Device unregistered\n", DEVICE_NAME);
    }


    printk(KERN_ALERT"%s: driver is unloaded\n", DEVICE_NAME);
}

module_init(xpdma_init);
module_exit(xpdma_exit);
