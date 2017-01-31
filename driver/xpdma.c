//
// Created by user on 8/3/15.
//

#include <stddef.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#include "xpdma.h"
#include <stdio.h>
#include "../driver/xpdma_driver.h"

#define CTR_REG_OFFSET 0x00004000 // TODO: temporary. For tests only
#define CTR_REG_SIZE   (4<<10)    // 4 kB configuration memory

struct xpdma_t {
    int fd;
    int id;
};

static int gfd = -1; // global device file escriptor
static int gOpenCount = 0; // counter for opened devices

//#include <semaphore.h>
//#define SEM_NAME "/xpdma_sem"
//static sem_t *sem = NULL;

#include <stdint.h>
inline void logger(const char* data, const uint32_t addr)
{
    FILE* flog = fopen("/home/user/xpdma_log", "a+b");
    if (flog!=NULL) {
        fprintf(flog, "%s: 0x%04X\n", data, addr);
        //fputs (data, flog);
        fclose (flog);
    }
}

xpdma_t *xpdma_open(int id) 
{

    //logger("xpdma_open ", 0);
    //if ((sem = sem_open(SEM_NAME, O_CREAT, 0666, 1)) == SEM_FAILED) {
    //    perror("sem_open");
    //    return NULL;
    //}

    //sem_wait (sem); 
    xpdma_t * device;
    if (id >= XPDMA_NUM_MAX)
        return NULL;

    device = (xpdma_t *)malloc(sizeof(xpdma_t));
    if (device == NULL)
        return NULL;

    if (gfd < 0) 
        gfd = open("/dev/" DEVICE_NAME, O_RDWR | O_SYNC);
    
    if (gfd < 0) {
        free(device);
        ////logger("xpdma_open: failed\n");
        return NULL;
    }

    device->id = id;
    device->fd = gfd;
    gOpenCount++;
    //sem_post (sem);
    
    ////logger("xpdma_open: finish\n");

    return device;
}

void xpdma_close(xpdma_t * device) {
    //logger("xpdma_close ", 0);
    //sem_wait (sem); 
    //printf ("free DEVICE\n");
    if (device != NULL) {
        free(device);
        device = NULL;
        ////logger("xpdma_close: free(device) \n");
    }

    if (gOpenCount == 0) {
        close(gfd);
        gfd = -1;
    } else {
        gOpenCount--;
    }
    //sem_post (sem);

    //sem_close(sem);
    ////sem_unlink(//sem_NAME);
    ////logger("xpdma_close: finish\n");
    //printf ("end free DEVICE\n");
}

int xpdma_send(xpdma_t *fpga, void *data, unsigned int count, unsigned int addr)
{
    ////logger("xpdma_send ", addr);
    if (fpga == NULL)
        return -1;

    if ( addr % 4 )
        return -1;
    
    cdmaBuffer_t buffer = {fpga->id, data, count, addr};
    
    ////logger("xpdma_send: lock\n");
    //sem_wait (sem); 
    ////logger("xpdma_send: ioctl", addr);
    ioctl(fpga->fd, IOCTL_SEND, &buffer);
    ////logger("xpdma_send: unlock", addr);
    //sem_post (sem);
    ////logger("xpdma_send: finish\n");
    return 0;
}

int xpdma_recv(xpdma_t *fpga, void *data, unsigned int count, unsigned int addr)
{
    //logger("xpdma_recv ", addr);
    if (fpga == NULL)
        return -1;

    if ( addr % 4 )
        return -1;

    cdmaBuffer_t buffer = {fpga->id, data, count, addr};
    
    ////logger("xpdma_recv: lock\n");
    //sem_wait (sem); 
    ////logger("xpdma_recv: ioctl\n");
    ioctl(fpga->fd, IOCTL_RECV, &buffer);
    ////logger("xpdma_recv: unlock\n");
    //sem_post (sem);
    ////logger("xpdma_recv: finish\n");
    return 0;
}

void xpdma_writeReg(xpdma_t *fpga, uint32_t addr, uint32_t value)
{
    ////logger("xpdma_writeReg ", addr);
    if (fpga == NULL)
        return;
    
    cdmaReg_t data;
    data.id = fpga->id;
    data.reg = addr;
    data.value = value;
    
    ////logger("xpdma_writeReg: lock\n");
    //sem_wait (sem); 
    //logger("xpdma_writeReg: ioctl", addr);
    ioctl(fpga->fd, IOCTL_WRCDMAREG, &data);
    //logger("xpdma_writeReg: unlock", addr);
    //sem_post (sem);
    ////logger("xpdma_writeReg: finish\n");
}

uint32_t xpdma_readReg(xpdma_t *fpga, uint32_t addr)
{
    ////logger("xpdma_readReg ",addr);
    if (fpga == NULL)
        return 0;

    cdmaReg_t data;
    data.id = fpga->id;
    data.reg = addr;
    data.value = 0;
    
    ////logger("xpdma_readReg: lock\n");
    //sem_wait (sem); 
    //logger("xpdma_readReg: ioctl", addr);
    ioctl(fpga->fd, IOCTL_RDCDMAREG, &data);
    //logger("xpdma_readReg: unlock", addr);
    //sem_post (sem);
    ////logger("xpdma_readReg: finish\n");
    return data.value;
}

/*void xpdma_read(xpdma_t *fpga, void *data, unsigned int count)
{
    read(fpga->fd, data, count);
}

void xpdma_write(xpdma_t *fpga, void *data, unsigned int count)
{
    write(fpga->fd, data, count);
}*/

void xpdma_test_sg(xpdma_t *fpga, void *data, unsigned int count)
{
    //logger("xpdma_test_sg: ", 0);
    if (fpga == NULL)
        return;

    cdmaBuffer_t buffer;
    buffer.id = fpga->id;
    buffer.data = data;
    buffer.count = count;
    buffer.addr = 0x1;

    //sem_wait (sem); 
    ioctl(fpga->fd, IOCTL_SEND, &buffer);
    ioctl(fpga->fd, IOCTL_RECV, &buffer);
    //sem_post (sem);
    ////logger("xpdma_test_sg: finish\n");
}

void xpdma_info(xpdma_t *fpga)
{
    //logger("xpdma_info ",0);
    if (fpga == NULL)
        return;

    //sem_wait (sem); 
    ioctl(fpga->fd, IOCTL_INFO, fpga->id);
    //sem_post (sem);
    ////logger("xpdma_info: finish\n");
}


void xpdma_setCfgReg(xpdma_t *fpga, uint32_t regNumber, uint32_t data)
{
    //logger("xpdma_setCfgReg ", regNumber);
    if (fpga == NULL)
        return;

    if (regNumber > CTR_REG_SIZE) {
        printf("setCfgReg: Wrong reg number :%08X!\n", regNumber);
        return;
    }
    xpdma_writeReg(fpga, regNumber*4 + CTR_REG_OFFSET, data);
    ////logger("xpdma_setCfgReg: finish\n");
}

uint32_t xpdma_getCfgReg(xpdma_t *fpga, uint32_t regNumber)
{
    //logger("xpdma_getCfgReg ",regNumber);
    if (fpga == NULL)
        return 0;

    if (regNumber > CTR_REG_SIZE) {
        printf("getCfgReg: Wrong reg number :%08X!\n", regNumber);
        return 0;
    }
    ////logger("xpdma_getCfgReg: finish\n");
    return xpdma_readReg(fpga, regNumber*4 + CTR_REG_OFFSET);
}
