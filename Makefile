TARGET=forest
CC?=gcc
CFLAGS=-O3 -Wall

CFILES=main.c input.c rooms.c items.c inter.c

default:
	$(CC) $(CFLAGS) -o $(TARGET) $(CFILES)

clean:
	rm -f $(TARGET)

run: default
	./$(TARGET)

all: default
