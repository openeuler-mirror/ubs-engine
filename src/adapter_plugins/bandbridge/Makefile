# SPDX-License-Identifier: GPL-2.0
KDIR ?= /lib/modules/$(shell uname -r)/build
MODULE_NAME := bandbridge
SRCS := \
    bandbridge_main.c \
    bandbridge_device.c \
    bandbridge_ctrlq.c \
    bandbridge_log.c

OBJS := $(SRCS:.c=.o)

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := $(OBJS)

ccflags-y := $(INCLUDE_DIRS) -DDEBUG -Wall


all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.mod.c *.symvers *.order
install:
	sudo insmod $(MODULE_NAME).ko
uninstall:
	sudo rmmod $(MODULE_NAME)
debug: ccflags-y += -DDEBUG -g
debug: all

.PHONY: all clean install uninstall debug