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

// Handles .tga file loading

#include "tga.h"

byte *getPixel ( bitmap_t *bitmap, int x, int y )
{
/*	if(x > bitmap->width || y > bitmap->height || x < 0 || y < 0)
		return NULL;*/

	return (byte*)(bitmap->image+(bitmap->bitsperpixel>>3)*((y*bitmap->width)+x));
}

int drawLine ( bitmap_t *bitmap, int x1, int y1, int x2, int y2, byte r, byte g, byte b )
{
	int xdiff,ydiff,xdiffmod,ydiffmod;
	int x,y;
	byte *p;

	xdiff = x2-x1;
	ydiff = y2-y1;

	if(xdiff < 0)
		xdiffmod = -xdiff;
	else
		xdiffmod = xdiff;
	if(ydiff < 0)
		ydiffmod = -ydiff;
	else
		ydiffmod = ydiff;


	if(xdiffmod > ydiffmod)
	{
		for(x=x1;x!=x2;xdiff<0?x--:x++)
		{
			p = getPixel(bitmap, x, y1+(int)((float)ydiff*(float)(x-x1)/(float)xdiff));
			if(p)
			{
				p[0] = b;
				p[1] = g;
				p[2] = r;
			}
		}
	} else
	{
		for(y=y1;y!=y2;ydiff<0?y--:y++)
		{
			p = getPixel(bitmap, x1+(int)((float)xdiff*(float)(y-y1)/(float)ydiff), y);
			if(p)
			{
				p[0] = b;
				p[1] = g;
				p[2] = r;
			}
		}
	}
	return 1;
}

int getPixelState ( bitmap_t* bitmap, int x, int y )
{
	// if it is beyond the border we assume it is white
	if(x > bitmap->width || y > bitmap->height)
		return 1;

	return *(byte*)(bitmap->image+(y*bitmap->width)+x)<128?0:1;
}

float compareAreas ( bitmap_t *area1, bitmap_t *area2, int xorigin1, int yorigin1, int xorigin2, int yorigin2, int areaWidth, int areaHeight )
{
	int x, y;
	int totalSimilarPixels=0;

	for(y=0;y<areaHeight;y++)
	{
		for(x=0;x<areaWidth;x++)
		{
			if(getPixelState ( area1, xorigin1+x, yorigin1+y ) == getPixelState ( area2, xorigin2+x, yorigin2+y ))
				totalSimilarPixels++;
		}
	}

	return (float)totalSimilarPixels/(float)(areaWidth*areaHeight);
}


int getword(FILE *f_in)
{
	return getc(f_in) + getc(f_in)*256;
}

// Exactly the same is done
#define putshort(a,b) putword(a,b)

void putword(WORD num, FILE *f_out)
{
	putc(((char*)&num)[0], f_out);
	putc(((char*)&num)[1], f_out);
}

short getshort(FILE *f_in)
{
	return (short)(getc(f_in)+getc(f_in)*256);
}



int allocTGA ( bitmap_t *bitmap )
{
	bitmap->image = (byte*)malloc(bitmap->width*bitmap->height*(bitmap->bitsperpixel>>3));
	return 1;
}

int freeTGA ( bitmap_t* bitmap )
{
	free( bitmap->image );
	return 1;
}

int writeTGA ( char* filename, bitmap_t *bitmap )
{
	FILE *f_out = fopen(filename, "wb");
	int y;
	byte *tempbuf;

	if(!f_out)
	{
		printf("Failed to open output file: %s\n", filename);
		return 0;
	}

	putc(0, f_out);	// id length
	putc(0, f_out); // colour map type
	putc(2, f_out); // we will only do uncompressed types for now

	fseek(f_out, 8, SEEK_SET);
	putshort(bitmap->xorigin, f_out);	// xorigin
	putshort(bitmap->yorigin, f_out);	// yorigin
	putword(bitmap->width, f_out);		// width
	putword(bitmap->height, f_out);		// height
	putc(bitmap->bitsperpixel, f_out);	// bpp
	putc( 0%00100100, f_out ); 		// image descriptor byte

	// jump to the start of the actual data
	fseek(f_out, 18, SEEK_SET);

	// write the tga upside down...
	for(y=bitmap->height-1;y>=0;y--)
	{
		tempbuf = bitmap->image + y*bitmap->width*(bitmap->bitsperpixel>>3);
		fwrite ((char*)tempbuf, bitmap->bitsperpixel>>3, bitmap->width, f_out);
	}
	fclose(f_out);

	return 1;
}

int CreateBlankBitmap ( bitmap_t *bitmap, int width, int height, int bpp )
{
	bitmap->xorigin = 0;
	bitmap->yorigin = 0;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->bitsperpixel = bpp;
	allocTGA(bitmap);
	memset(bitmap->image, 0, bitmap->width*bitmap->height*bitmap->bitsperpixel>>3);
	return 1;
}

int loadTGA ( char *filename, bitmap_t *bitmap )
{
	FILE *f_in = fopen(filename, "rb");
	int idLength;
	int colourMapType;
	int x, y;
	int i;
	byte *tempbuf;
	int type;
	byte header;
	byte count;
	byte temppixel[4];
	byte imagedescriptor;
	int done;

	if(!f_in)
	{
		printf("Could not open input file: %s\n", filename);
		return 0;
	}

	idLength = getc(f_in);
	colourMapType = getc(f_in);

	if(colourMapType)
	{
		printf("Program does not handle colour maps\n");
		return 0;
	}

	// We only handle type 2 and type 10 TGA's atm
	type = getc(f_in);
	if(type != 2 && type != 10)
	{
		printf("input file is not of type 2\n");
		return 0;
	}

	fseek(f_in, 8, SEEK_SET);
	bitmap->xorigin = getshort(f_in);
	bitmap->yorigin = getshort(f_in);
	bitmap->width = getword(f_in);
	bitmap->height = getword(f_in);
	bitmap->bitsperpixel = getc(f_in);
	imagedescriptor = getc(f_in);

	if(bitmap->bitsperpixel == 24)
		bitmap->translucent = 0;
	else
		bitmap->translucent = 1;
	
	// Seek to the start of the pixel data
	fseek(f_in, 18+idLength, SEEK_SET);
	
	bitmap->image = (char*)malloc(bitmap->height*bitmap->width*(bitmap->bitsperpixel>>3));
	tempbuf = bitmap->image;
	if(type == 2)
	{
		for(y=0;y<bitmap->height;y++)
		{
			if(!(32&imagedescriptor))
				tempbuf = bitmap->image+bitmap->width*(bitmap->bitsperpixel>>3)*(bitmap->height-1-y);
			for(x=0;x<bitmap->width;x++)
			{
				for(i=0;i<bitmap->bitsperpixel>>3;i++)
					*(tempbuf++) = getc(f_in);
				if(bitmap->bitsperpixel==32&&*(tempbuf-1)<255)
					bitmap->translucent=1;
			}
		}
	} else
	{
		done = x = y = 0;
		while ( !done )
		{
			// Read in the header packet
			header = getc(f_in);
			count = header & 127;
			count++;

			if(header & 128)
			{
				// We have a run-length
				for(i=0;i<bitmap->bitsperpixel>>3;i++)
					temppixel[i] = getc(f_in);
				
				if(bitmap->bitsperpixel==32&&temppixel[3]<255)
					bitmap->translucent=1;

				// temppixel is the pixel we want to load
				for(;y<bitmap->height;y++)
				{
					if(!(32&imagedescriptor))
						tempbuf = bitmap->image+(bitmap->width*(bitmap->height-1-y)+x)*(bitmap->bitsperpixel>>3);
					for(;x<bitmap->width;)
					{
						for(i=0;i<bitmap->bitsperpixel>>3;i++)
							*(tempbuf++) = temppixel[i];
						x++;
						count--;
						if(!count)
							goto out;
					}
					x=0;
				}
				done=1;
			} else
			{
				// We have a normal set of pixels
				for(;y<bitmap->height;y++)
				{
					if(!(32&imagedescriptor))
						tempbuf = bitmap->image+(bitmap->width*(bitmap->height-1-y)+x)*(bitmap->bitsperpixel>>3);
					for(;x<bitmap->width;)
					{
						for(i=0;i<bitmap->bitsperpixel>>3;i++)
							*(tempbuf++) = getc(f_in);
						if(bitmap->bitsperpixel==32&&*(tempbuf-1)<255)
							bitmap->translucent=1;
						x++;
						count--;
						if(!count)
							goto out;
					}
					x=0;
				}
				done=1;
			}
out:;
		}
	}
	fclose(f_in);
	return 1;
}

