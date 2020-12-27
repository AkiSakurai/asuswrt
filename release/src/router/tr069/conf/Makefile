all: lib.c
	gcc -c -Wall -fPIC lib.c
	gcc -shared -o libdev.so lib.o
clean:
	rm -rf *.o *.so
