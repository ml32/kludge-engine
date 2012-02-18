CC=gcc
CFLAGS=-std=c99 -g -pg -pedantic -Wall -I/usr/local/include -Iinclude
LDFLAGS=-L/usr/local/lib -L/usr/lib/nvidia-current -lSDL -lGL -lpng -lm
OBJS=main.o input-sdl.o vid-sdl.o platform-sdl.o frame.o terrain.o model.o model-iqm2.o array.o model-obj.o camera.o bvhtree.o renderer.o renderer-gl3.o sphere.o matrix-sw.o quat-sw.o material.o material-mtl.o texture.o texture-png.o resource.o
BINARYNAME=test

all: main

clean:
	rm -f $(BINARYNAME) $(OBJS)

main: $(OBJS)
	$(CC) -o $(BINARYNAME) $(OBJS) $(LDFLAGS) 

main.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c main.c

.o:
	$(CC) $(CFLAGS) $(INCLUDES) -lGL -c $*.c
