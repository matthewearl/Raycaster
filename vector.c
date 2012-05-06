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

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "vector.h"

void
vectorcopy( vector2d_t *a, vector2d_t *b )
{
	memcpy(a,b,sizeof(vector2d_t));
}

void
vectorzero( vector2d_t *a )
{
	a->x=a->y=0.0f;
}

void
vectorscale( vector2d_t *a, float f, vector2d_t *b )
{
	b->x=a->x*f;
	b->y=a->y*f;
}

void
vectoradd( vector2d_t *a, vector2d_t *b, vector2d_t *o )
{
	o->x=a->x+b->x;
	o->y=a->y+b->y;
}

void
vectorsubtract( vector2d_t *a, vector2d_t *b, vector2d_t *o )
{
	o->x=a->x-b->x;
	o->y=a->y-b->y;
}

float sqrtf(float f);

float
vectorlength( vector2d_t *a )
{
	return sqrtf(a->x*a->x+a->y*a->y);
}

void
vectornormalise( vector2d_t *a, vector2d_t *b )
{
	vectorscale(a,1.0f/vectorlength(a),b);
}

float
dotproduct( vector2d_t *a, vector2d_t *b )
{
	return a->x*b->x+a->y*b->y;
}

/* rotate the vector 90 degrees */
void
vectorrot90( vector2d_t *a, vector2d_t *b )
{
	vector2d_t r;
	r.x = -a->y;
	r.y = a->x;

	vectorcopy(b,&r);
}

void
vectorrotangle ( vector2d_t *a, float angle, vector2d_t *out )
{
	float c,s;
	vector2d_t b;
	c = cos(angle);
	s = sin(angle);
	b.x = a->x*c + a->y*s;
	b.y = a->x*(-s) + a->y*c;
	vectorcopy(out,&b);
}

/* dir should be normalised */
void
rotatebyvector ( vector2d_t *a, vector2d_t *dir, vector2d_t *out )
{
	vector2d_t xvec,temp,temp2;
	
	vectorscale(dir,a->y,&temp);
	vectorrot90(dir,&xvec);
	vectorscale(&xvec,a->x,&temp2);
	vectoradd(&temp2,&temp,out);
}

void
angletovector ( float angle, vector2d_t *out )
{
	out->x = cosf(angle);
	out->y = sinf(angle);
}

void
vectormidpoint ( vector2d_t *a, vector2d_t *b, float f, vector2d_t *out )
{
	out->x = a->x + f*(b->x-a->x);
	out->y = a->y + f*(b->y-a->y);
}



