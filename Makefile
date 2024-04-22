demo: clean
	gcc ./src/flushreloaddemo/demo_flush_reload.c -o demo

run_demo:
	chmod +x demo
	./demo

window: 
	gcc ./src/attacker.c -o attacker -pthread
	chmod +x attacker
	sudo setcap cap_sys_admin+ep attacker
	./attacker 2

run_user: ghostrace
	./ghostrace 20000 10000000 200000


ghostrace: clean
	gcc -O3 -fno-stack-protector -I./src/build  src/ghostrace/utils/ipi.c src/ghostrace/utils/timer.c ./src/ghostrace/ghostrace.c -o ghostrace -pthread 
	chmod +x ghostrace
	sudo setcap cap_sys_admin+ep ghostrace
	

MODULE_NAME := vuln_driver
KERNEL_SRC := /lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)
obj-m += vuln_driver.o


driver: $(MODULE_NAME).ko

$(MODULE_NAME).ko : $(MODULE_NAME).o driver_com
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules
	
driver_com:
	gcc src/driver/utils/ipi.c src/driver/utils/timer.c src/driver/io_driver_com.c -o io_driver_com 
	chmod +x io_driver_com

cp_driver: 
	cp src/driver/$(MODULE_NAME).c .



$(MODULE_NAME).o : $(MODULE_NAME).c 
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

$(MODULE_NAME).c : cp_driver


install:
	sudo insmod ./vuln_driver.ko

remove:
	sudo rmmod ./vuln_driver.ko

view:
	sudo dmesg -t | tail -50

clean:
	rm -rf main demo attacker test timer ghostrace ./src/build/* ./vuln_driver.c ./io_driver_com
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean
	make remove


# clean is not a file
.PHONY: clean