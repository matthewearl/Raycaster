CC=gcc
CFLAGS=-Wall -O3 -ffast-math -funroll-loops -march=i686 -malign-double -fstrict-aliasing -ggdb
#CFLAGS=-ggdb -Wall -pg
#CFLAGS=-g

all: raycaster

raycaster: raycaster.o vector.o tga.o physics.o world.o
	$(CC) $(CFLAGS) physics.o tga.o raycaster.o vector.o world.o -o raycaster -lSDL -lpthread

raycaster.o: raycaster.c raycaster.h vector.h world.h
	$(CC) $(CFLAGS) -c raycaster.c -o raycaster.o

vector.o: vector.c
	$(CC) $(CFLAGS) -c vector.c -o vector.o

tga.o: tga.c
	$(CC) $(CFLAGS) -c tga.c -o tga.o

physics.o: physics.c raycaster.h world.h vector.h
	$(CC) $(CFLAGS) -c physics.c -o physics.o

world.o: world.c raycaster.h world.h vector.h
	$(CC) $(CFLAGS) -c world.c -o world.o

clean:
	-rm *.o raycaster
