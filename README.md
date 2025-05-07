installing on Ubuntu/debian-based systems
run these:
sudo apt update //make sure system is up to date
sudo apt install build-essential linux-headers-$(uname -r) 

then make a directory:
mkdir ~/sys_health_monitor

move to directory:
cd ~/sys_health_monitor

Save the Kernel Module code:
nano sys_health_monitor.c
(copy and paste code into nano "program.c")

create the makefile
nano Makefile
copy and paste this code:
obj-m += sys_health_monitor.o  # Name of the module

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

Compile the module
type "make" into terminal

Load the kernel module:
sudo insmod sys_health_monitor.ko

check if module is loaded
lsmod | grep sys_health

View kernel logs with:
sudo dmesg | tail -20

Check module in /proc:
cat /proc/sys_health

Unload the module:
sudo rmmod sys_health_monitor
