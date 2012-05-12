CC=gcc
CFLAGS=-ggdb -m32 -lpthread `pkg-config --cflags gtk+-2.0 gmodule-export-2.0` -export-dynamic
LIBS=`pkg-config --libs gtk+-2.0`

SRC=$(wildcard src/*.c)

all:
	@(make omegle)

omegle: $(SRC)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

clean:
	rm -vf omegle
