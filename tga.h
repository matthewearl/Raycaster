/*
 * Copyright (C) Matthew Earl
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

// Header file for tga.c
#ifndef _TGA_H_
#define _TGA_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned short WORD;
typedef unsigned char byte;

typedef struct _bitmap_t
{
	byte *image;
	WORD width, height;
	short xorigin, yorigin;
	int bitsperpixel;
	int translucent;		// 0 if bpp is 24 or alpha channel is solid
} bitmap_t;

int writeTGA ( char* filename, bitmap_t *bitmap );
int loadTGA ( char *filename, bitmap_t *bitmap );
float compareAreas ( bitmap_t *area1, bitmap_t *area2, int xorigin1, int yorigin1, int xorigin2, int yorigin2, int areaWidth, int areaHeight );
int allocTGA ( bitmap_t *bitmap );
int freeTGA ( bitmap_t* bitmap );
int drawLine ( bitmap_t *bitmap, int x1, int y1, int x2, int y2, byte r, byte g, byte b );
int CreateBlankBitmap ( bitmap_t *bitmap, int width, int height, int bpp );
byte *getPixel ( bitmap_t *bitmap, int x, int y );

#endif

