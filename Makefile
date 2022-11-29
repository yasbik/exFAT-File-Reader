CC = clang
CFLAGS = -g -Wall -Werror -Wextra -Wpedantic

all: main.o

main.o: main.c
	$(CC) $(CFLAGS) -o main main.c

clean:
	-rm -f main.o
