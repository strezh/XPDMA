#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include "xpdma.h"
#include <sys/time.h> 

#define TEST_SIZE   1024*1024*1024 // 1GB test data
#define TEST_ADDR   0 // offset of DDR start address
#define BOARD_ID    0 // board number (for multiple boards)

int main() {
    xpdma_t * fpga;
    uint32_t buf_size = TEST_SIZE;
    uint32_t addr_in = TEST_ADDR;
    uint32_t addr_out = TEST_ADDR;
    uint32_t c = 0;
    uint32_t err_count = 0;

    char *data_in;
    char *data_out;

    unsigned int len = 0;
    struct timeval _timers[4];
    double time_ms[4];

    printf("Open FPGA: ");
    fpga = xpdma_open(BOARD_ID);
    if (NULL == fpga) {
        printf ("Failed to open XPDMA device\n");
        return 1;
    }
    printf("Successfull\n");

    data_in = (char *)malloc(buf_size);
    if (NULL == data_in) {
        printf ("Failed to allocate input buffer memory (size: %lu bytes)\n", buf_size);
        xpdma_close(fpga);
        return 1;
    }

    data_out = (char *)malloc(buf_size);
    if (NULL == data_out) {
        printf ("Failed to allocate output buffer memory (size: %lu bytes)\n", buf_size);
        xpdma_close(fpga);
        return 1;
    }

    printf("Fill input data: ");
    for (c = 0; c < buf_size; ++c)
        //data_in[c] = rand()%256;
        data_in[c] = c;
    printf("Ok\n");
    memset(data_out, 0, buf_size);

    printf("Send Data: ");
    gettimeofday(&_timers[0], NULL);
    xpdma_send(fpga, data_in, buf_size, addr_in);
    gettimeofday(&_timers[1], NULL);
    printf("Ok\n");

    printf("Receive Data: ");
    gettimeofday(&_timers[2], NULL);
    xpdma_recv(fpga, data_out, buf_size, addr_out);
    gettimeofday(&_timers[3], NULL);
    printf("Ok\n");

    printf("Close FPGA\n");
    xpdma_close(fpga);

    printf("Check Data: ");
    for (c = 0; c < buf_size; ++c)
        err_count += (data_in[c] != data_out[c]);

    if (err_count)
        printf("%lu errors\n", err_count);
    else
        printf("Ok\n");

    free(data_in);
    free(data_out);

    for (c = 0; c < 4; ++c)
        time_ms[c] =
                ((double)_timers[c].tv_sec*1000.0) +
                ((double)_timers[c].tv_usec/1000.0);

    printf("Send speed: %f MB/s (%f ms)\n", buf_size/(1024*1024)/((time_ms[1] - time_ms[0])/1000.0), (time_ms[1] - time_ms[0]));
    printf("Recv speed: %f MB/s (%f ms)\n", buf_size/1024/1024/((time_ms[3] - time_ms[2])/1000.0), (time_ms[3] - time_ms[2]));
    return 0;
}
