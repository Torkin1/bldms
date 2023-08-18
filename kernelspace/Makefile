module_name=bldms

obj-m += $(module_name).o
bldms-objs += main.o driver.o device.o vfs_supported.o

PWD := $(CURDIR)

debug:
	KCFLAGS=-DDEBUG  make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) clean

reload: unload load

load: bldms.ko
	sudo insmod bldms.ko

unload: 
	sudo rmmod -f bldms