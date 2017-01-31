#ifndef XPDMA_CONTROL_H
#define XPDMA_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct xpdma_t;
typedef struct xpdma_t xpdma_t;

/**
 * Open device with PCIe DMA
 */
xpdma_t *xpdma_open(int id);

/**
 * Close device with PCIe DMA
 */
void xpdma_close(xpdma_t * device);

/**
 * Send data to DDR
 */
int xpdma_send(xpdma_t *fpga, void *data, unsigned int count, unsigned int addr);

/**
 * Receive data from DDR
 */
int xpdma_recv(xpdma_t *fpga, void *data, unsigned int count, unsigned int addr);

/**
 *
 */
void xpdma_setCfgReg(xpdma_t *fpga, uint32_t regNumber, uint32_t data);

/**
 *
 */
uint32_t xpdma_getCfgReg(xpdma_t *fpga, uint32_t regNumber);


void xpdma_writeReg(xpdma_t *fpga, uint32_t addr, uint32_t value);
uint32_t xpdma_readReg(xpdma_t *fpga, uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif 
