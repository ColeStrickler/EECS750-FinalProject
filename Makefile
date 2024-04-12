



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



clean: 
	rm -rf main demo attacker test timer