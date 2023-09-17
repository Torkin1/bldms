module_name=bldms

obj-m += $(module_name).o
bldms-objs += logic/main.o logic/device/driver.o logic/ops/vfs_supported.o logic/ops/vfs_unsupported.o logic/device/device.o logic/block_layer/block_layer.o logic/block_layer/block_manipulation.o logic/block_layer/block_serialization.o logic/block_layer/blocks_list.o logic/device/device_core.o logic/usctm/usctm.o logic/usctm/lib/vtpmo.o logic/singlefilefs/singlefilefs.o logic/singlefilefs/file.o logic/singlefilefs/dir.o test/tests.o

PWD := $(CURDIR)

debug:
	KCFLAGS="-DDEBUG -DINIT_KERNELSPACE_TESTS -I$(PWD)/logic -I$(PWD)/test" make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) modules

debug-wt:
	KCFLAGS="-DDEBUG -DINIT_KERNELSPACE_TESTS -DBLDMS_BLOCK_SYNC_IO -I$(PWD)/logic -I$(PWD)/test" make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) clean

reload: unload load

load: bldms.ko
	sudo insmod bldms.ko
	lsmod | grep $(module_name)

unload: 
	-sudo umount /home/vboxuser/bldms/test_mount
	-sudo rmmod -f bldms