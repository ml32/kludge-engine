CC=gcc
CFLAGS=-std=c99 -g -pg -pedantic -Wall -I/usr/local/include -Iinclude
LDFLAGS=-L/usr/local/lib -lSDL -lGL -lpng
OBJS=main.o input-sdl.o vid-sdl.o platform-sdl.o model.o model-iqm2.o array.o model-obj.o camera.o bvhtree.o renderer.o renderer-gl3.o sphere.o matrix-sw.o quat-sw.o texture.o texture-png.o resource.o
BINARYNAME=test

all: main

clean:
	rm -f $(BINARYNAME) $(OBJS)

main: $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $(BINARYNAME) $(OBJS)

main.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c main.c

.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $*.c
