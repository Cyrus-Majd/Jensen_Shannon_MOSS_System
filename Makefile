all: main.c
	gcc -g -fsanitize=address main.c -lpthread -lm -o main