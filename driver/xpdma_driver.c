/**
 *
 */

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/fs.h>       /* Needed for files operations */
#include <linux/pci.h>      /* Needed for PCI */

#include "xpdma_driver.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("PCIe driver for Xilinx CDMA subsystem (XAPP1171), Linux");
MODULE_AUTHOR("Strezhik Iurii");

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



// Prototypes
ssize_t xpdma_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos);
ssize_t xpdma_read (struct file *filp, char *buf, size_t count, loff_t *f_pos);
int xpdma_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
int xpdma_open(struct inode *inode, struct file *filp);
int xpdma_release(struct inode *inode, struct file *filp);

// Aliasing write, read, ioctl, etc...
struct file_operations xpdma_intf = {
        read : xpdma_read,
        write : xpdma_write,
        unlocked_ioctl : xpdma_ioctl,
        //llseek:     xpdma_lseek,
        open : xpdma_open,
        release : xpdma_release,
};

ssize_t xpdma_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    return (SUCCESS);
}

ssize_t xpdma_read (struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    return (SUCCESS);
}

int xpdma_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
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
    printk(KERN_INFO"%s: Init: Virt HW address %X\n", DEVICE_NAME, (unsigned int) gBaseVirt);

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
    printk(KERN_CRIT"%s: Init: Read buffer successfully allocated: 0x%08X\n", DEVICE_NAME, gReadBuffer);

    gWriteBuffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (NULL == gWriteBuffer) {
        printk(KERN_CRIT"%s: Init: Unable to allocate gWriteBuffer.\n", DEVICE_NAME);
        return (CRIT_ERR);
    }
    printk(KERN_CRIT"%s: Init: Write buffer successfully allocated: 0x%08X\n", DEVICE_NAME, gReadBuffer);

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
