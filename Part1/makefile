main: main.c input.c func.c hash.c
	gcc  -o main main.c input.c func.c hash.c -I.  -g -lm

all: run

run: main
	time ./main

.PHONY: all run