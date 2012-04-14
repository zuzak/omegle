CC=gcc
CFLAGS=-ggdb -m32
SRC=$(wildcard src/*.c)

all:
	@(make omegle)

omegle: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -vf omegle
