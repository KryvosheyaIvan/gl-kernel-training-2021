ifneq ($(KERNELRELEASE),)

# Kbuild part of makefile
obj-m := mem_module.o 

else

# kernel sources

KDIR ?= $(HOME)/repos/linux-stable

default:
	$(MAKE) -C $(KDIR) M="$$PWD"

clean:
	$(MAKE) -C $(KDIR) M="$$PWD" $@

%.i %.s : %.c
	$(MAKE) -C $(KDIR) M=$$PWD $@

ch_style:
	$(KDIR)/scripts/checkpatch.pl --no-tree -f mem_module.c

endif
