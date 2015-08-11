/**
 *
 */

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/fs.h>       /* Needed for files operations */
#include <linux/pci.h>      /* Needed for PCI */
#include <asm/uaccess.h>    /* Needed for copy_to_user & copy_from_user */

#include "xpdma_driver.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("PCIe driver for Xilinx CDMA subsystem (XAPP1171), Linux");
MODULE_AUTHOR("Strezhik Iurii");

// Max CDMA buffer size
#define MAX_BTT             0x007FFFFF  // 8 MBytes maximum for DMA Transfer */
//#define BUF_SIZE            (4<<20)     // 4 MBytes
#define BUF_SIZE            16*PAGE_SIZE   // 64 kBytes

#define TRANS_BRAM_ADDR     0x00000000  // Translation BRAM offset
#define AXI_PCIE_CTL_ADDR   0x00008000  // AXI PCIe control offset
#define AXI_CDMA_ADDR       0x0000c000  // AXI CDMA LITE control offset

#define AXI_PCIE_DM_ADDR    0x80000000  // AXI:BAR1 Address
#define AXI_PCIE_SG_ADDR    0x80800000  // AXI:BAR0 Address
#define AXI_BRAM_ADDR       0x81000000  // AXI Translation BRAM Address
#define AXI_DDR3_ADDR       0x00000000  // AXI DDR3 Address

#define SG_OFFSET           0x40        // Scatter Gather next descriptor offset
#define BRAM_STEP           0x8         // Translation Vector Length
#define ADDR_BTT            0x00000008  // 64 bit address translation descriptor control length

#define CDMA_CR_SG_EN       0x00000008  // Scatter gather mode

#define AXIBAR2PCIEBAR_1U   0x208

/* Scatter Gather Transfer descriptor */
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

// Struct Used for send/receive data
typedef struct {
    char *data;
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
sg_desc_t *gDescChain = NULL;        // Address Translation Descriptors chain



// Prototypes
ssize_t xpdma_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos);
ssize_t xpdma_read (struct file *filp, char *buf, size_t count, loff_t *f_pos);
long xpdma_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
int xpdma_open(struct inode *inode, struct file *filp);
int xpdma_release(struct inode *inode, struct file *filp);
static inline u32 xpdma_readReg (u32 reg);
static inline void xpdma_writeReg (u32 reg, u32 val);
ssize_t xpdma_send (const char *data, size_t count, u32 addr);
ssize_t xpdma_recv (char *data, size_t count, u32 addr);

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

    memcpy((char *)gReadBuffer, gWriteBuffer, BUF_SIZE); // debug cycle

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
        default:
            break;
    }

    return (SUCCESS);
}

sg_desc_t *create_desc_chain(int direction, void *data, u32 size, u32 addr)
{
    // length of desctriptors chain
    int chainLength = ((size + MAX_BTT - 1) / MAX_BTT);
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

    gDescChain = kmalloc(2 * sizeof(sg_desc_t) * chainLength, GFP_KERNEL);
    if (gDescChain == NULL) {
        printk(KERN_INFO"%s: Descriptors Chain create error: memory allocation failed\n", DEVICE_NAME);
        return NULL;
    }

    // fill descriptor chain
    for (count = 0; count < chainLength; ++count) {
        sg_desc_t *addrDesc = gDescChain + 2 * count; // address translation descriptor
        sg_desc_t *dataDesc = addrDesc + 1;           // target data transfer descriptor
        btt = (unmappedSize > MAX_BTT) ? MAX_BTT : unmappedSize;

        // fill address translation descriptor
        addrDesc->nextDesc = sgAddr + SG_OFFSET;
        addrDesc->srcAddr  = bramAddr;
        addrDesc->destAddr = AXIBAR2PCIEBAR_1U;
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

    gDescChain[2*chainLength - 1].nextDesc = AXI_PCIE_SG_ADDR; // tail descriptor pointed to head of chain

    return (SUCCESS);
}

int xpdma_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO"%s: Open: module opened\n", DEVICE_NAME);
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
    return readl(gBaseVirt + reg);
}

static inline void xpdma_writeReg (u32 reg, u32 val)
{
    writel(val, (gBaseVirt + reg));
}

ssize_t xpdma_send (const char *data, size_t count, u32 addr)
{
    return (SUCCESS);
}

ssize_t xpdma_recv (char *data, size_t count, u32 addr)
{
    return (SUCCESS);
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
    }
    pci_set_consistent_dma_mask(gDev, 0x7fffffff);

    gReadBuffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (NULL == gReadBuffer) {
        printk(KERN_CRIT"%s: Init: Unable to allocate gReadBuffer.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    printk(KERN_CRIT"%s: Init: Read buffer successfully allocated: 0x%08lX\n", DEVICE_NAME, (size_t) gReadBuffer);

    gWriteBuffer = kmalloc(BUF_SIZE, GFP_KERNEL);
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
    printk("%s driver is loaded\n", DEVICE_NAME);

    return (SUCCESS);
}

static void xpdma_exit (void)
{
    // Check if we have a memory region and free it
    if (gStatFlags & HAVE_MEM_REGION) {
        (void) release_mem_region(gBaseHdwr, gBaseLen);
    }

    // Free Write and Read buffers allocated to use
    if (NULL != gReadBuffer)
        (void) kfree(gReadBuffer);
    if (NULL != gWriteBuffer)
        (void) kfree(gWriteBuffer);

    gReadBuffer = NULL;
    gWriteBuffer = NULL;

    //  Free up memory pointed to by virtual address
    if (gBaseVirt != NULL)
        iounmap(gBaseVirt);

    gBaseVirt = NULL;

    // Unregister Device Driver
    if (gStatFlags & HAVE_KERNEL_REG)
        unregister_chrdev(gDrvrMajor, DEVICE_NAME);

    gStatFlags = 0;
    printk(KERN_ALERT"%s driver is unloaded\n", DEVICE_NAME);
}

module_init(xpdma_init);
module_exit(xpdma_exit);
