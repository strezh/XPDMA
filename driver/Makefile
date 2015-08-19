obj-m := xpdma_driver.o
path := $(shell uname -r)

all:
	make -C /lib/modules/$(path)/build M=$(shell pwd) modules
clean:
	make -C /lib/modules/$(path)/build M=$(shell pwd) clean
