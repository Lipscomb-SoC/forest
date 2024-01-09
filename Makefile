CFLAGS=-Wall -g
CPPFLAGS=-Wall -g -std=gnu++20

all: forest explore

main.c: input.h

input.c: rooms.h items.h inter.h

rooms.c: rooms.h rooms-desc.h items.h

items.c: items.h items-desc.h rooms.h

inter.c: inter.h inter-even.h items.h rooms.h

forest: main.c input.c rooms.c items.c inter.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f forest explore
