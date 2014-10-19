
# Assume the source tree is where the running kernel was built
# You should set KERNELDIR in the environment if it's elsewhere
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
# The current directory is passed to sub-makes as argument
	PWD := $(shell pwd)
	EXTRA_CFLAGS := -I/usr/realtime/include
	KBUILD_EXTRA_SYMBOLS := /usr/realtime/modules/Module.symvers

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers

#obj-m := driverPCIe.o
#obj-m := pcie_interrupt_driver.o
obj-m := rtai_pcie_interrupt_driver.o
