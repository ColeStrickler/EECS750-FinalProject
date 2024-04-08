



all:
	gcc ./src/main.c -o main
	

demo: clean
	gcc ./src/flushreloaddemo/demo_flush_reload.c -o demo

run:
	chmod +x demo
	./demo

clean: 
	rm -rf main demo