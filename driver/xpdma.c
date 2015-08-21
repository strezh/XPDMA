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

struct xpdma_t {
    int fd;
};

xpdma_t *xpdma_open() 
{
    xpdma_t * device;
    device = (xpdma_t *)malloc(sizeof(xpdma_t));
    if (device == NULL)
        return NULL;

    device->fd = open("/dev/" DEVICE_NAME, O_RDWR | O_SYNC);

    if (device->fd < 0) {
        free(device);
        return NULL;
    }
    return device;
}

void xpdma_close(xpdma_t * device) {
    close(device->fd);
    free(device);
}

int xpdma_send(xpdma_t *fpga, void *data, unsigned int count, unsigned int addr)
{
    cdmaBuffer_t buffer = {data, count, addr};
    ioctl(fpga->fd, IOCTL_SEND, &buffer);
    return 0;
}

int xpdma_recv(xpdma_t *fpga, void *data, unsigned int count, unsigned int addr)
{
    cdmaBuffer_t buffer = {data, count, addr};
    ioctl(fpga->fd, IOCTL_RECV, &buffer);
    return 0;
}

void xpdma_writeReg(xpdma_t *fpga, uint32_t addr, uint32_t value)
{
    cdmaReg_t data;
    data.reg = addr;
    data.value = value;
    ioctl(fpga->fd, IOCTL_WRCDMAREG, &data);
}

uint32_t xpdma_readReg(xpdma_t *fpga, uint32_t addr)
{
    uint32_t ret = addr;
    ioctl(fpga->fd, IOCTL_RDCDMAREG, &ret);
    return ret;
}

void xpdma_read(xpdma_t *fpga, void *data, unsigned int count)
{
    read(fpga->fd, data, count);
}

void xpdma_write(xpdma_t *fpga, void *data, unsigned int count)
{
    write(fpga->fd, data, count);
}

void xpdma_test_sg(xpdma_t *fpga, void *data, unsigned int count)
{
    cdmaBuffer_t buffer;
    buffer.data = data;
    buffer.count = count;
    buffer.addr = 0x1;

    ioctl(fpga->fd, IOCTL_SEND, &buffer);
    ioctl(fpga->fd, IOCTL_RECV, &buffer);
}

void xpdma_info(xpdma_t *fpga)
{
    ioctl(fpga->fd, IOCTL_INFO);
}
