module_name=bldms

obj-m += $(module_name).o
bldms-objs += main.o driver/driver.o device/device.o driver/ops/vfs_supported.o driver/ops/request.o

PWD := $(CURDIR)

debug:
	KCFLAGS="-DDEBUG -I$(PWD)" make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) clean

reload: unload load

load: bldms.ko
	sudo insmod bldms.ko
	lsmod | grep $(module_name)

unload: 
	sudo rmmod -f bldms