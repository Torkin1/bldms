obj-m += bldms.o

PWD := $(CURDIR)

debug:
	
	KCFLAGS=-DDEBUG  make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) modules

all:
	make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) clean

reload: bldms.ko
	sudo rmmod -f bldms
	sudo insmod bldms.ko

load: bldms.ko
	sudo insmod bldms.ko