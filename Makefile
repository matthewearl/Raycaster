# Copyright (C) Matthew Earl
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

CC=gcc
CFLAGS=-Wall -O3 -ffast-math -funroll-loops -malign-double -fstrict-aliasing -ggdb
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
