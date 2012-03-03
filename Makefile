CC=gcc
CFLAGS=-std=c99 -g -pg -pedantic -Wall -I/usr/local/include -Iinclude
LDFLAGS=-L/usr/local/lib -L/usr/lib/nvidia-current -lglfw -lglew32 -lopengl32 -lmingw32 -lpng -lz -lm
OBJS=main.o input-glfw.o vid-glfw.o platform-glfw.o frame.o terrain.o model.o model-iqm2.o array.o model-obj.o camera.o bvhtree.o renderer.o renderer-gl3.o sphere.o matrix-sw.o quat-sw.o material.o material-mtl.o texture.o texture-png.o resource.o strsep.o
BINARYNAME=test

all: main

clean:
	rm -f $(BINARYNAME) $(OBJS)

main: $(OBJS)
	$(CC) $(CFLAGS) -o $(BINARYNAME) $(OBJS) $(LDFLAGS) 

main.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c main.c

.o:
	$(CC) $(CFLAGS) $(INCLUDES) -lGL -c $*.c
