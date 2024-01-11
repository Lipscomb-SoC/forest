CFLAGS=-Wall -g
CPPFLAGS=-Wall -g -std=gnu++20

all: forest explore

main.cpp: input.h

input.cpp: rooms.h items.h inter.h

rooms.cpp: rooms.h rooms-desc.h items.h

items.cpp: items.h items-desc.h rooms.h

inter.cpp: inter.h inter-even.h items.h rooms.h

forest: main.cpp input.cpp rooms.cpp items.cpp inter.cpp
	$(CXX) $(CPPFLAGS) -o $@ $^

clean:
	rm -f forest explore
