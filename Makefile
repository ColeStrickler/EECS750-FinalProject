

GHOSTRACE_OBJ = src/build/ipi.c.o\
 				src/build/timer.c.o 

all:
	gcc ./src/main.c -o main
	

demo: clean
	gcc ./src/flushreloaddemo/demo_flush_reload.c -o demo

run_demo:
	chmod +x demo
	./demo


window: 
	gcc ./src/attacker.c -o attacker -pthread
	chmod +x attacker
	sudo setcap cap_sys_admin+ep attacker

run: clean window
	./attacker 100


src/build/%.c.o: src/ghostrace/utils/%.c
	mkdir -p "$(@D)"
	gcc $< -c -o $@

ghostrace: clean 
	gcc  -I./src/build  src/ghostrace/utils/ipi.c src/ghostrace/utils/timer.c ./src/ghostrace/ghostrace.c -o ghostrace -pthread 
	chmod +x ghostrace
	sudo setcap cap_sys_admin+ep ghostrace
	./test.sh

clean: 
	rm -rf main demo attacker test timer ghostrace ./src/build/*