CFLAGS=-Wall -g
CPPFLAGS=-Wall -g -std=gnu++20

all: forest explore

forest: main.c input.c rooms.c items.c inter.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f forest explore
