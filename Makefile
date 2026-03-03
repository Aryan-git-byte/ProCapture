CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0`
SRC = src/main.c
OUT = procapture

all: build

build:
	$(CC) $(SRC) -o $(OUT) $(CFLAGS) $(LIBS)

clean:
	rm -f $(OUT)

install:
	cp $(OUT) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(OUT)
