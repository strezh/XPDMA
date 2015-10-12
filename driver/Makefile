# Filename: Makefile
# Version: 0.1
# Author: Strezhik Iurii
# Description: Makefile for Linux x64 PCIe DMA Subsystem XPDMA driver

NAME := xpdma

# Build variables
KERNEL_VER := $(shell uname -r)
KERNEL_DIR := /lib/modules/$(KERNEL_VER)/build

LIB_SRCS := xpdma.c
LIB_OBJS := $(patsubst %.c,%.o,$(LIB_SRCS))

obj-m += $(NAME).o
$(NAME)-y := xpdma_driver.o

all: $(NAME).ko $(NAME).a

clean:
	make -C $(KERNEL_DIR) M=$(shell pwd) clean
	rm -Rf *.ko *.cmd *.o *.a *.a.* .*.cmd Module.symvers Module.markers modules.order *.mod.c .tmp_versions

$(NAME).ko: *.c *.h
	#make -C $(KDIR) SUBDIRS=`pwd` modules
	make -C $(KERNEL_DIR) M=$(shell pwd) modules
	rm -rf $(LIB_OBJS)

$(NAME).a: $(LIB_OBJS)
	$(CC) -shared $(LIB_OBJS) -o lib$@

$(LIB_OBJS): $(LIB_SRCS)
	$(CC) -g -Wall -static -fPIC -c $^

load: $(NAME).ko
	insmod $(NAME).ko

unload:
	rmmod $(NAME)





