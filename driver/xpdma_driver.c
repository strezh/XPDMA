/**
 *
 */

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/fs.h>       /* Needed for files operations */
#include <linux/pci.h>      /* Needed for PCI */
#include <asm/uaccess.h>    /* Needed for copy_to_user & copy_from_user */
#include <linux/delay.h>    /* udelay, mdelay */

#include "xpdma_driver.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("PCIe driver for Xilinx CDMA subsystem (XAPP1171), Linux");
MODULE_AUTHOR("Strezhik Iurii");

// Max CDMA buffer size
#define MAX_BTT             0x007FFFFF  // 8 MBytes maximum for DMA Transfer */
//#define BUF_SIZE            (4<<20)     // 4 MBytes
#define BUF_SIZE            16*PAGE_SIZE   // 64 kBytes

#define BRAM_OFFSET         0x00000000  // Translation BRAM offset
#define PCIE_CTL_OFFSET     0x00008000  // AXI PCIe control offset
#define CDMA_OFFSET         0x0000c000  // AXI CDMA LITE control offset

// AXI CDMA Register Offsets
#define CDMA_CONTROL_OFFSET	0x00 // Control Register
#define CDMA_STATUS_OFFSET	0x04 // Status Register
#define CDMA_CDESC_OFFSET	0x08 // Current descriptor Register
#define CDMA_TDESC_OFFSET	0x10 // Tail descriptor Register
#define CDMA_SRCADDR_OFFSET	0x18 // Source Address Register
#define CDMA_DSTADDR_OFFSET	0x20 // Dest Address Register
#define CDMA_BTT_OFFSET		0x28 // Bytes to transfer Register

#define AXI_PCIE_DM_ADDR    0x80000000  // AXI:BAR1 Address
#define AXI_PCIE_SG_ADDR    0x80800000  // AXI:BAR0 Address
#define AXI_BRAM_ADDR       0x81000000  // AXI Translation BRAM Address
#define AXI_DDR3_ADDR       0x00000000  // AXI DDR3 Address

#define SG_COMPLETE_MASK    0xF0000000  // Scatter Gather Operation Complete status flag mask
#define SG_DEC_ERR_MASK     0x40000000  // Scatter Gather Operation Decode Error flag mask
#define SG_SLAVE_ERR_MASK   0x20000000  // Scatter Gather Operation Slave Error flag mask
#define SG_INT_ERR_MASK     0x10000000  // Scatter Gather Operation Internal Error flag mask

#define SG_OFFSET           0x40        // Scatter Gather next descriptor offset
#define BRAM_STEP           0x8         // Translation Vector Length
#define ADDR_BTT            0x00000008  // 64 bit address translation descriptor control length

#define CDMA_CR_SG_EN       0x00000008  // Scatter gather mode enable

#define AXIBAR2PCIEBAR_0U   0x208       // AXI:BAR0 Upper Address Translation (bits [63:32])
#define AXIBAR2PCIEBAR_0L   0x20C       // AXI:BAR0 Lower Address Translation (bits [31:0])
#define AXIBAR2PCIEBAR_1U   0x210       // AXI:BAR1 Upper Address Translation (bits [63:32])
#define AXIBAR2PCIEBAR_1L   0x214       // AXI:BAR1 Lower Address Translation (bits [31:0])

// Scatter Gather Transfer descriptor
typedef struct {
    u32 nextDesc;	/* 0x00 */
    u32 na1;	/* 0x04 */
    u32 srcAddr;	/* 0x08 */
    u32 na2;	/* 0x0C */
    u32 destAddr;	/* 0x10 */
    u32 na3;	/* 0x14 */
    u32 control;	/* 0x18 */
    u32 status;	/* 0x1C */
} __aligned(64) sg_desc_t;

// Scatter Gather descriptor chain
typedef struct {
    sg_desc_t *desc;
    sg_desc_t *headDesc;
    sg_desc_t *tailDesc;
} sg_chain_t;

// Struct Used for send/receive data
typedef struct {
    void *data;
    u32 count;
    u32 addr;
} cdmaBuffer_t;

#define HAVE_KERNEL_REG     0x01    // Kernel registration
#define HAVE_MEM_REGION     0x02    // I/O Memory region


int gDrvrMajor = 241;               // Major number not dynamic.
struct pci_dev *gDev = NULL;        // PCI device structure.
unsigned int gStatFlags = 0x00;     // Status flags used for cleanup
unsigned long gBaseHdwr;            // Base register address (Hardware address)
unsigned long gBaseLen;             // Base register address Length
void *gBaseVirt = NULL;             // Base register address (Virtual address, for I/O).
char *gReadBuffer = NULL;           // Pointer to dword aligned DMA Read buffer.
char *gWriteBuffer = NULL;          // Pointer to dword aligned DMA Write buffer.
sg_chain_t gDescChain;              // Address Translation Descriptors chain

dma_addr_t gReadHWAddr;
dma_addr_t gWriteHWAddr;



// Prototypes
ssize_t xpdma_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos);
ssize_t xpdma_read (struct file *filp, char *buf, size_t count, loff_t *f_pos);
long xpdma_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
int xpdma_open(struct inode *inode, struct file *filp);
int xpdma_release(struct inode *inode, struct file *filp);
static inline u32 xpdma_readReg (u32 reg);
static inline void xpdma_writeReg (u32 reg, u32 val);
ssize_t xpdma_send (void *data, size_t count, u32 addr);
ssize_t xpdma_recv (void *data, size_t count, u32 addr);
void xpdma_showInfo (void);

// Aliasing write, read, ioctl, etc...
struct file_operations xpdma_intf = {
        read           : xpdma_read,
        write          : xpdma_write,
        unlocked_ioctl : xpdma_ioctl,
        //llseek         : xpdma_lseek,
        open           : xpdma_open,
        release        : xpdma_release,
};

ssize_t xpdma_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    dma_addr_t dma_addr;

    /*if ( (count % 4) != 0 )  {
        printk("%s: xpdma_writeMem: Buffer length not dword aligned.\n",DEVICE_NAME);
        return (CRIT_ERR);
    }*/

    // Now it is safe to copy the data from user space.
    if ( copy_from_user(gWriteBuffer, buf, count) )  {
        printk("%s: xpdma_writeMem: Failed copy from user.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    //TODO: set DMA semaphore
    dma_addr = pci_map_single(gDev, gWriteBuffer, BUF_SIZE, PCI_DMA_TODEVICE);
    if ( 0 == dma_addr )  {
        printk("%s: xpdma_writeMem: Map error.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    printk("%s: xpdma_writeMem: WriteBuf Virt Addr = %lX Phy Addr = %lX.\n",
           DEVICE_NAME, (size_t)gWriteBuffer, (size_t)dma_addr);

    pci_unmap_single(gDev, dma_addr, BUF_SIZE, PCI_DMA_TODEVICE);

    //TODO: release DMA semaphore

    printk(KERN_INFO"%s: XPCIe_Write: %lu bytes have been written...\n", DEVICE_NAME, count);

    return (SUCCESS);
}

ssize_t xpdma_read (struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    dma_addr_t dma_addr;

    //TODO: set DMA semaphore
    dma_addr = pci_map_single(gDev, gReadBuffer, BUF_SIZE, PCI_DMA_FROMDEVICE);

    if ( 0 == dma_addr )  {
        printk("%s: xpdma_readMem: Map error.\n",DEVICE_NAME);
        return (CRIT_ERR);
    }

    printk("%s: xpdma_readMem: ReadBuf Virt Addr = %lX Phy Addr = %lX.\n",
           DEVICE_NAME, (size_t)gReadBuffer, (size_t)dma_addr);

    // Unmap the DMA buffer so it is safe for normal access again.
    pci_unmap_single(gDev, dma_addr, BUF_SIZE, PCI_DMA_FROMDEVICE);

    //TODO: release DMA semaphore

    // TODO: debug cycle
//    memcpy((char *)gReadBuffer, gWriteBuffer, BUF_SIZE);

    // Now it is safe to copy the data to user space.
    if ( copy_to_user(buf, gReadBuffer, count) )  {
        printk("%s: xpdma_readMem: Failed copy to user.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    printk(KERN_INFO"%s: XPCIe_Read: %lu bytes have been read...\n", DEVICE_NAME, count);
    return (SUCCESS);
}

long xpdma_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
    u32 regx = 0;

    printk(KERN_INFO"%s: Ioctl command: %d \n", DEVICE_NAME, cmd);
    switch (cmd) {
        case IOCTL_RESET:
            // TODO: Reset CDMA
            break;
        case IOCTL_RDCDMAREG: // Read CDMA config registers
            printk(KERN_INFO"%s: Read Register 0x%X\n", DEVICE_NAME, (*(u32 *)arg));
            regx = xpdma_readReg(*((u32 *)arg));
            *((u32 *)arg) = regx;
            printk(KERN_INFO"%s: Readed value 0x%X\n", DEVICE_NAME, regx);
            break;
        case IOCTL_WRCDMAREG: // Write CDMA config registers
            printk(KERN_INFO"%s: Write Register 0x%X\n", DEVICE_NAME, (*(cdmaReg_t *)arg).reg);
            printk(KERN_INFO"%s: Write Value 0x%X\n", DEVICE_NAME, (*(cdmaReg_t *)arg).value);
            xpdma_writeReg((*(cdmaReg_t *)arg).reg, (*(cdmaReg_t *)arg).value);
            break;
        case IOCTL_RDCFGREG:
            // TODO: Read PCIe config registers
            break;
        case IOCTL_WRCFGREG:
            // TODO: Write PCIe config registers
            break;
        case IOCTL_SEND:
            // Send data from Host system to AXI CDMA
            printk(KERN_INFO"%s: Send Data size 0x%X\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).count);
            printk(KERN_INFO"%s: Send Data address 0x%X\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).addr);
            xpdma_send ((*(cdmaBuffer_t *)arg).data, (*(cdmaBuffer_t *)arg).count, (*(cdmaBuffer_t *)arg).addr);
            printk(KERN_INFO"%s: Sended\n", DEVICE_NAME);
            break;
        case IOCTL_RECV:
            // Receive data from AXI CDMA to Host system
            printk(KERN_INFO"%s: Receive Data size 0x%X\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).count);
            printk(KERN_INFO"%s: Receive Data address 0x%X\n", DEVICE_NAME, (*(cdmaBuffer_t *)arg).addr);
            xpdma_recv ((*(cdmaBuffer_t *)arg).data, (*(cdmaBuffer_t *)arg).count, (*(cdmaBuffer_t *)arg).addr);
            printk(KERN_INFO"%s: Received\n", DEVICE_NAME);
            break;
        case IOCTL_INFO:
            xpdma_showInfo ();
        default:
            break;
    }

    return (SUCCESS);
}

void xpdma_showInfo (void)
{
    uint32_t c = 0;

    printk(KERN_INFO"%s: INFORMATION\n", DEVICE_NAME);
    printk(KERN_INFO"%s: HOST REGIONS:\n", DEVICE_NAME);
    printk(KERN_INFO"%s: gBaseVirt: 0x%lX\n", DEVICE_NAME, (size_t) gBaseVirt);
    printk(KERN_INFO"%s: gReadBuffer: 0x%lX\n", DEVICE_NAME, (size_t) gReadBuffer);
    printk(KERN_INFO"%s: gWriteBuffer: 0x%lX\n", DEVICE_NAME, (size_t) gWriteBuffer);
    printk(KERN_INFO"%s: gDescChain:          0x%lX\n", DEVICE_NAME, (size_t) &gDescChain);
    printk(KERN_INFO"%s: gDescChain.desc:     0x%lX\n", DEVICE_NAME, (size_t) (gDescChain.desc));
    printk(KERN_INFO"%s: gDescChain.headDesc: 0x%lX\n", DEVICE_NAME, (size_t) (gDescChain.headDesc));
    printk(KERN_INFO"%s: gDescChain.tailDesc: 0x%lX\n", DEVICE_NAME, (size_t) (gDescChain.tailDesc));

    printk(KERN_INFO"%s: REGISTERS:\n", DEVICE_NAME);

    printk(KERN_INFO"%s: BRAM:\n", DEVICE_NAME);
    for (c = 0; c < 100; c += 4)
        printk(KERN_INFO"%s: 0x%08X: 0x%08X\n", DEVICE_NAME, BRAM_OFFSET + c, xpdma_readReg(BRAM_OFFSET + c));

    printk(KERN_INFO"%s: PCIe CTL:\n", DEVICE_NAME);
    printk(KERN_INFO"%s: 0x%08X: 0x%08X\n", DEVICE_NAME, PCIE_CTL_OFFSET, xpdma_readReg(PCIE_CTL_OFFSET));
    for (c = 0; c < 100; c += 4)
        printk(KERN_INFO"%s: 0x%08X: 0x%08X\n", DEVICE_NAME, PCIE_CTL_OFFSET + 0x200 + c, xpdma_readReg(PCIE_CTL_OFFSET + 0x200 + c));

    printk(KERN_INFO"%s: CDMA CTL:\n", DEVICE_NAME);
    for (c = 0; c < 100; c += 4)
        printk(KERN_INFO"%s: 0x%08X: 0x%08X\n", DEVICE_NAME, CDMA_OFFSET + c, xpdma_readReg(CDMA_OFFSET + c));
}

int *create_desc_chain(int direction, void *data, u32 size, u32 addr)
{
    // length of desctriptors chain
    /*int chainLength = ((size + BUF_SIZE - 1) / BUF_SIZE);
    int count = 0;

    u32 sgAddr = AXI_PCIE_SG_ADDR; // current descriptor address in chain
    u32 bramAddr = AXI_BRAM_ADDR ; // Translation BRAM Address
    u32 btt = 0;                   // current descriptor BTT
    u32 unmappedSize = size;       // unmapped data size
    u32 srcAddr = 0;               // source address (SG_DM of DDR3)
    u32 destAddr = 0;              // destination address (SG_DM of DDR3)

    if (direction == PCI_DMA_FROMDEVICE) {
        srcAddr  = AXI_DDR3_ADDR + addr;
        destAddr = AXI_PCIE_DM_ADDR;
    } else if (direction == PCI_DMA_TODEVICE) {
        srcAddr  = AXI_PCIE_DM_ADDR;
        destAddr = AXI_DDR3_ADDR + addr;
    } else {
        printk(KERN_INFO"%s: Descriptors Chain create error: unknown direction\n", DEVICE_NAME);
        return NULL;
    }

    gDescChain.desc = kmalloc(2 * sizeof(sg_desc_t) * chainLength, GFP_KERNEL);
    if (gDescChain.desc == NULL) {
        printk(KERN_INFO"%s: Descriptors Chain create error: memory allocation failed\n", DEVICE_NAME);
        return NULL;
    }

    // fill descriptor chain
    for (count = 0; count < chainLength; ++count) {
        sg_desc_t *addrDesc = gDescChain.desc + 2 * count; // address translation descriptor
        sg_desc_t *dataDesc = addrDesc + 1;           // target data transfer descriptor
        btt = (unmappedSize > BUF_SIZE) ? BUF_SIZE : unmappedSize;

        // fill address translation descriptor
        addrDesc->nextDesc = sgAddr + SG_OFFSET;
        addrDesc->srcAddr  = bramAddr;
        addrDesc->destAddr = AXI_BRAM_ADDR + PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_1U;
        addrDesc->control   = ADDR_BTT;
        addrDesc->status    = 0x0;
        sgAddr += SG_OFFSET;

        // fill target data transfer descriptor
        dataDesc->nextDesc = sgAddr + SG_OFFSET;
        dataDesc->srcAddr  = srcAddr;
        dataDesc->destAddr = destAddr;
        dataDesc->control   = btt;
        dataDesc->status    = 0x0;
        sgAddr += SG_OFFSET;

        bramAddr += BRAM_STEP;
        unmappedSize -= btt;
    }

    gDescChain.desc[2*chainLength - 1].nextDesc = AXI_PCIE_SG_ADDR; // tail descriptor pointed to head of chain
    gDescChain.headDesc = gDescChain.desc;
    gDescChain.tailDesc = gDescChain.desc + (2 * chainLength - 1);*/

    int chainLength = 2; // 1 write and 1 read
    sg_desc_t *descriptor = NULL;

    u32 sgAddr = AXI_PCIE_SG_ADDR; // current descriptor address in chain
    u32 btt = 0;                   // current descriptor BTT
    u32 unmappedSize = size;       // unmapped data size
    //gDescChain.desc = kmalloc(2 * sizeof(sg_desc_t) * chainLength, GFP_KERNEL);
    gDescChain.desc = kmalloc(2 * sizeof(sg_desc_t) * chainLength, GFP_KERNEL);
    if (gDescChain.desc == NULL) {
        printk(KERN_INFO"%s: Descriptors Chain create error: memory allocation failed\n", DEVICE_NAME);
        return NULL;
    }

    descriptor = gDescChain.desc; // address translation descriptor
    btt = (unmappedSize > MAX_BTT) ? MAX_BTT : unmappedSize;

    // Descriptor 0
    descriptor->nextDesc = sgAddr + SG_OFFSET;
    descriptor->srcAddr  = AXI_BRAM_ADDR + BRAM_OFFSET;
    descriptor->destAddr = AXI_BRAM_ADDR + PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_1U;
    descriptor->control  = ADDR_BTT;
    descriptor->status   = 0x00000000;
    sgAddr += SG_OFFSET;
    descriptor++;        // target data transfer descriptor

    // Descriptor 1
    descriptor->nextDesc = sgAddr + SG_OFFSET;
    descriptor->srcAddr  = AXI_PCIE_DM_ADDR;
    descriptor->destAddr = AXI_DDR3_ADDR + 0x100000;
    descriptor->control  = 0x10000;
    descriptor->status   = 0x00000000;
    sgAddr += SG_OFFSET;
    descriptor++;        // target data transfer descriptor

    // Descriptor 2
    descriptor->nextDesc = sgAddr + SG_OFFSET;
    descriptor->srcAddr  = AXI_BRAM_ADDR + BRAM_STEP;
    descriptor->destAddr = AXI_BRAM_ADDR + PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_1U;
    descriptor->control  = ADDR_BTT;
    descriptor->status   = 0x00000000;
    sgAddr += SG_OFFSET;
    descriptor++;        // target data transfer descriptor

    // Descriptor 3
    descriptor->nextDesc = sgAddr + SG_OFFSET;
    descriptor->srcAddr  = AXI_DDR3_ADDR + 0x110000;
    descriptor->destAddr = AXI_PCIE_DM_ADDR;
    descriptor->control  = 0x10000;
    descriptor->status   = 0x00000000;
    sgAddr += SG_OFFSET;

    gDescChain.desc[2*chainLength - 1].nextDesc = AXI_PCIE_SG_ADDR; // tail descriptor pointed to head of chain
    gDescChain.headDesc = gDescChain.desc;
    gDescChain.tailDesc = gDescChain.desc + (2 * chainLength - 1);

    gDescChain.desc[2*chainLength - 1].nextDesc = AXI_PCIE_SG_ADDR; // tail descriptor pointed to head of chain
    gDescChain.headDesc = gDescChain.desc;
    gDescChain.tailDesc = gDescChain.desc + (2 * chainLength - 1);
    return (SUCCESS);
}

void show_descriptors(void)
{
    int c = 0;
    sg_desc_t *descriptor = gDescChain.desc;
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

static int sg_operation(int direction, void *data, size_t count, u32 addr)
{
    dma_addr_t dma_addr;
    u32 status = 0;
    size_t pntr = 0;
    u32 lowPntr = 0;
    u32 hignPntr = 0;
    size_t delayTime = 0; // TODO: temporary variable
//    dma_addr_t wpage; // TODO: TEST
//    dma_addr_t rpage; // TODO: TEST
//    wpage = pci_map_single(gDev, gWriteBuffer, BUF_SIZE, PCI_DMA_TODEVICE);
//    if ( 0 == wpage )  {
//        printk("%s: sg_operation: wpage map error.\n",DEVICE_NAME);
//        return (CRIT_ERR);
//    }
//    printk("%s: sg_operation: wpage: 0x%08lX \n",DEVICE_NAME, (size_t) wpage);
//
//    rpage = pci_map_single(gDev, gReadBuffer, BUF_SIZE, PCI_DMA_FROMDEVICE);
//    if ( 0 == rpage )  {
//        printk("%s: sg_operation: rpage map error.\n",DEVICE_NAME);
//        return (CRIT_ERR);
//    }
//    printk("%s: sg_operation: rpage: 0x%08lX \n",DEVICE_NAME, (size_t) rpage);

    xpdma_showInfo();

    // IRQ ON
    /*xpdma_writeReg(CDMA_CONTROL_OFFSET,
                   xpdma_readReg(CDMA_CONTROL_OFFSET) |
                       0x00007000);*/


    // 1. Set DMA to Scatter Gather Mode
    printk(KERN_INFO"%s: 1. Set DMA to Scatter Gather Mode\n", DEVICE_NAME);
    /*xpdma_writeReg (CDMA_OFFSET + CDMA_CONTROL_OFFSET,
                    xpdma_readReg (CDMA_OFFSET + CDMA_CONTROL_OFFSET) & ~CDMA_CR_SG_EN);
    xpdma_writeReg (CDMA_OFFSET + CDMA_CONTROL_OFFSET,
                    xpdma_readReg (CDMA_OFFSET + CDMA_CONTROL_OFFSET) | CDMA_CR_SG_EN);*/
    xpdma_writeReg (CDMA_OFFSET + CDMA_CONTROL_OFFSET, CDMA_CR_SG_EN);

    // 2. Create Descriptors chain
    printk(KERN_INFO"%s: 2. Create Descriptors chain\n", DEVICE_NAME);
    create_desc_chain(direction, data, count, addr);
    show_descriptors();

    dma_addr = pci_map_single(gDev, gDescChain.desc, BUF_SIZE, PCI_DMA_TODEVICE);

    // 3. Update PCIe Translation vector
    //pntr =  (size_t) (gDescChain.desc);
    pntr =  (size_t) (dma_addr);
    printk(KERN_INFO"%s: 3. Update PCIe Translation vector\n", DEVICE_NAME);
    printk(KERN_INFO"%s: gDescChain.desc 0x%016lX\n", DEVICE_NAME, pntr);
    lowPntr =  (pntr >> 0) & 0xFFFFFFFF;
    hignPntr = (pntr >> 32) & 0xFFFFFFFF;
    xpdma_writeReg ((PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_0L), lowPntr);
    xpdma_writeReg ((PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_0U), hignPntr);
    xpdma_writeReg ((PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_1L), 0x00000000);
    xpdma_writeReg ((PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_1U), 0x00000000);

    // 4. Write appropriate Translation Vectors
    // TODO: only test! Change to real address values in pntr

    pntr =  (size_t) (gWriteHWAddr);
//    pntr =  (size_t) (gWriteBuffer);
//    pntr =  (size_t) (wpage);
    printk(KERN_INFO"%s: 4. Write appropriate Translation Vectors\n", DEVICE_NAME);
    printk(KERN_INFO"%s: gWriteBuffer 0x%016lX\n", DEVICE_NAME, pntr);
    lowPntr =  (pntr >> 0) & 0xFFFFFFFF;
    hignPntr = (pntr >> 32) & 0xFFFFFFFF;
    xpdma_writeReg ((BRAM_OFFSET + 0 + 4), lowPntr);
    xpdma_writeReg ((BRAM_OFFSET + 0 + 0), hignPntr);

    pntr =  (size_t) (gReadHWAddr);
//    pntr =  (size_t) (gReadBuffer);
//    pntr =  (size_t) (rpage);
    printk(KERN_INFO"%s: gReadBuffer 0x%016lX\n", DEVICE_NAME, pntr);
    lowPntr =  (pntr >> 0) & 0xFFFFFFFF;
    hignPntr = (pntr >> 32) & 0xFFFFFFFF;
    xpdma_writeReg ((BRAM_OFFSET + 8 + 4), lowPntr);
    xpdma_writeReg ((BRAM_OFFSET + 8 + 0), hignPntr);

    // 5. Write a valid pointer to DMA CURDESC_PNTR
    printk(KERN_INFO"%s: 5. Write a valid pointer to DMA CURDESC_PNTR\n", DEVICE_NAME);
    //xpdma_writeReg ((CDMA_OFFSET + CDMA_CDESC_OFFSET), (u32)(gDescChain.headDesc));
    xpdma_writeReg ((CDMA_OFFSET + CDMA_CDESC_OFFSET), (u32)(AXI_PCIE_SG_ADDR + 0x00));

    // 6. Write a valid pointer to DMA TAILDESC_PNTR
    printk(KERN_INFO"%s: 6. Write a valid pointer to DMA TAILDESC_PNTR\n", DEVICE_NAME);
    //xpdma_writeReg ((CDMA_OFFSET + CDMA_TDESC_OFFSET), (u32)(gDescChain.tailDesc));
    xpdma_writeReg ((CDMA_OFFSET + CDMA_TDESC_OFFSET), (u32)(AXI_PCIE_SG_ADDR + 0xC0));

    // wait for Scatter Gather operation...
    printk(KERN_INFO"%s: Scatter Gather must be started!\n", DEVICE_NAME);
    #define TIMEOUT 2
    while (delayTime < TIMEOUT) {
        delayTime++;
        mdelay(500);
        // TODO: must be less

        status = gDescChain.tailDesc->status;

//        if (status) {
//            printk(KERN_INFO
//            "%s: Unmap buffers\n", DEVICE_NAME);
//            pci_unmap_single(gDev, wpage, BUF_SIZE, PCI_DMA_TODEVICE);
//            pci_unmap_single(gDev, rpage, BUF_SIZE, PCI_DMA_FROMDEVICE);
//        }

        printk(KERN_INFO
        "%s: Scatter Gather Operation: Maybe completed :)\n", DEVICE_NAME);
        printk(KERN_INFO
        "%s: Status: 0x%08X\n", DEVICE_NAME, status);
        show_descriptors();

        // Release Descriptors chain
        if (status) {
            if (NULL != gDescChain.desc)
                kfree(gDescChain.desc);
        }

        if (status & SG_DEC_ERR_MASK) {
            printk(KERN_INFO
            "%s: Scatter Gather Operation: Decode Error\n", DEVICE_NAME);
            return (CRIT_ERR);
        }

        if (status & SG_SLAVE_ERR_MASK) {
            printk(KERN_INFO
            "%s: Scatter Gather Operation: Slave Error\n", DEVICE_NAME);
            return (CRIT_ERR);
        }

        if (status & SG_INT_ERR_MASK) {
            printk(KERN_INFO
            "%s: Scatter Gather Operation: Internal Error\n", DEVICE_NAME);
            return (CRIT_ERR);
        }

        if (status & SG_COMPLETE_MASK) {
            printk(KERN_INFO
            "%s: Scatter Gather Operation: Completed successfully\n", DEVICE_NAME);
            printk(KERN_INFO
            "%s: gReadBuffer: %s\n", DEVICE_NAME, gReadBuffer);
            printk(KERN_INFO
            "%s: gWriteBuffer: %s\n", DEVICE_NAME, gWriteBuffer);
            return (SUCCESS);
        }
    }
    printk(KERN_INFO
    "%s: gReadBuffer: %s\n", DEVICE_NAME, gReadBuffer);
    printk(KERN_INFO
    "%s: gWriteBuffer: %s\n", DEVICE_NAME, gWriteBuffer);

    printk(KERN_INFO"%s: Descriptors Chain create error: Timeout Error\n", DEVICE_NAME);

    xpdma_showInfo();

    if (NULL != gDescChain.desc)
        kfree(gDescChain.desc);

    pci_unmap_single(gDev, dma_addr, BUF_SIZE, PCI_DMA_FROMDEVICE);

    return (CRIT_ERR);
}

static int sg_block(int direction, void *data, size_t count, u32 addr)
{
    size_t unsended = count;
    char *curData = data;
    u32 curAddr = addr;
    u32 btt = BUF_SIZE;

    while (unsended > 0) {
        btt = (unsended < BUF_SIZE) ? unsended : BUF_SIZE;
        printk(KERN_INFO"%s: SG Block: BTT=%u\tunsended=%lu \n", DEVICE_NAME, btt, unsended);
        sg_operation(direction, curData, btt, curAddr);
        curData += BUF_SIZE;
        curAddr += BUF_SIZE;
        unsended -= btt;
    }

    return (SUCCESS);
}

ssize_t xpdma_send (void *data, size_t count, u32 addr)
{
    sg_block(PCI_DMA_TODEVICE, (void *)data, count, addr);
    return (SUCCESS);
}

ssize_t xpdma_recv (void *data, size_t count, u32 addr)
{
    sg_block(PCI_DMA_FROMDEVICE, (void *)data, count, addr);
    return (SUCCESS);
}

int xpdma_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO"%s: Release: module released\n", DEVICE_NAME);
    return (SUCCESS);
}

/* IO access */
static inline u32 xpdma_readReg (u32 reg)
{
//    printk(KERN_INFO"%s: xpdma_readReg: address:0x%08X\t\n", DEVICE_NAME, reg);
    return readl(gBaseVirt + reg);
}

static inline void xpdma_writeReg (u32 reg, u32 val)
{
    u32 prev = xpdma_readReg(reg);
    printk(KERN_INFO"%s: xpdma_writeReg: address:0x%08X\t data:0x%08X -> 0x%08X\n", DEVICE_NAME, reg, prev, val);
    writel(val, (gBaseVirt + reg));
}

static int xpdma_init (void)
{
    gDev = pci_get_device(VENDOR_ID, DEVICE_ID, gDev);
    if (NULL == gDev) {
        printk(KERN_WARNING"%s: Init: Hardware not found.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    // Get Base Address of BAR0 registers
    gBaseHdwr = pci_resource_start(gDev, 0);
    if (0 > gBaseHdwr) {
        printk(KERN_WARNING"%s: Init: Base Address not set.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    printk(KERN_INFO"%s: Init: Base hw val %X\n", DEVICE_NAME, (unsigned int) gBaseHdwr);

    // Get the Base Address Length
    gBaseLen = pci_resource_len(gDev, 0);
    printk(KERN_INFO"%s: Init: Base hw len %d\n", DEVICE_NAME, (unsigned int) gBaseLen);

    // Get Virtual HW address
    gBaseVirt = ioremap(gBaseHdwr, gBaseLen);
    if (!gBaseVirt) {
        printk(KERN_WARNING"%s: Init: Could not remap memory.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    printk(KERN_INFO"%s: Init: Virt HW address %lX\n", DEVICE_NAME, (size_t) gBaseVirt);

    // Check the memory region to see if it is in use
    if (0 > check_mem_region(gBaseHdwr, gBaseLen)) {
        printk(KERN_WARNING"%s: Init: Memory in use.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    // Try to gain exclusive control of memory for demo hardware.
    request_mem_region(gBaseHdwr, gBaseLen, "Xilinx_PCIe_CDMA_Driver");
    gStatFlags = gStatFlags | HAVE_MEM_REGION;
    printk(KERN_INFO"%s: Init: Initialize Hardware Done..\n", DEVICE_NAME);

    // Bus Master Enable
    if (0 > pci_enable_device(gDev)) {
        printk(KERN_WARNING"%s: Init: Device not enabled.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    // Set DMA Mask
    if (0 > pci_set_dma_mask(gDev, 0x7fffffff)) {
        printk("%s: Init: DMA not supported\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    pci_set_consistent_dma_mask(gDev, 0x7fffffff);

    //gReadBuffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    gReadBuffer = pci_alloc_consistent(gDev, BUF_SIZE, &gReadHWAddr);
    if (NULL == gReadBuffer) {
        printk(KERN_CRIT"%s: Init: Unable to allocate gReadBuffer.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    printk(KERN_CRIT"%s: Init: Read buffer successfully allocated: 0x%08lX\n", DEVICE_NAME, (size_t) gReadBuffer);

    //gWriteBuffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    gWriteBuffer = pci_alloc_consistent(gDev, BUF_SIZE, &gWriteHWAddr);
    if (NULL == gWriteBuffer) {
        printk(KERN_CRIT"%s: Init: Unable to allocate gWriteBuffer.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    printk(KERN_CRIT"%s: Init: Write buffer successfully allocated: 0x%08lX\n", DEVICE_NAME, (size_t) gWriteBuffer);

    // Register driver as a character device.
    if (0 > register_chrdev(gDrvrMajor, DEVICE_NAME, &xpdma_intf)) {
        printk(KERN_WARNING"%s: Init: will not register\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    printk(KERN_INFO"%s: Init: module registered\n", DEVICE_NAME);

    gStatFlags = gStatFlags | HAVE_KERNEL_REG;
    printk(KERN_INFO"%s: driver is loaded\n", DEVICE_NAME);

    return (SUCCESS);
}

static void xpdma_exit (void)
{

    // Check if we have a memory region and free it
    if (gStatFlags & HAVE_MEM_REGION) {
        (void) release_mem_region(gBaseHdwr, gBaseLen);
    }

    printk(KERN_INFO"%s: erase gReadBuffer\n", DEVICE_NAME);
    // Free Write and Read buffers allocated to use
    if (NULL != gReadBuffer) {
        printk(KERN_INFO"%s: pci free\n", DEVICE_NAME);
        pci_free_consistent(gDev, BUF_SIZE, gReadBuffer, gReadHWAddr);
//        printk(KERN_INFO"%s: kfree\n", DEVICE_NAME);
//        (void) kfree(gReadBuffer);
    }

    printk(KERN_INFO"%s: erase gWriteBuffer\n", DEVICE_NAME);
    if (NULL != gWriteBuffer) {
        printk(KERN_INFO"%s: pci free\n", DEVICE_NAME);
        pci_free_consistent(gDev, BUF_SIZE, gWriteBuffer, gWriteHWAddr);
//        printk(KERN_INFO"%s: kfree\n", DEVICE_NAME);
//        (void) kfree(gWriteBuffer);
    }

    gReadBuffer = NULL;
    gWriteBuffer = NULL;

    printk(KERN_INFO"%s: unmap gBaseVirt\n", DEVICE_NAME);
    //  Free up memory pointed to by virtual address
    if (gBaseVirt != NULL)
        iounmap(gBaseVirt);

    gBaseVirt = NULL;

    // Unregister Device Driver
    if (gStatFlags & HAVE_KERNEL_REG)
        unregister_chrdev(gDrvrMajor, DEVICE_NAME);

    gStatFlags = 0;
    printk(KERN_ALERT"%s: driver is unloaded\n", DEVICE_NAME);
}

module_init(xpdma_init);
module_exit(xpdma_exit);
