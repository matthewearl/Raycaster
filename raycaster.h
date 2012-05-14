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

#ifndef _RAYCASTER_H_
#define _RAYCASTER_H_

#include <SDL/SDL.h>
#include "vector.h"

#define VIEW_HEIGHT	64.0f
#define MAX_CYLINDER_PICKS	24

enum
{
	SURFACE_CEILING,
	SURFACE_FLOOR,
	SURFACE_NONE
};

typedef struct texture_s
{
	char path[64];
	unsigned short *pixels; /* pixels are stored up-down first */
	int width,height;
	int widthmask,heightmask;
	int widthmaskshift,heightmaskshift;
	int widthmasksmallshift,heightmasksmallshift;
	int log2width,log2height;

	struct texture_s *prev,*next;
} texture_t;

typedef struct edge_s
{
	struct vert_s *verts[2];
	struct platform_s *leftplat,*rightplat;
	struct texture_s *texture;
	vector2d_t line,normal;
	float planedist;
} edge_t;

typedef struct vert_s
{
	vector2d_t pos;
	int numedges;
	edge_t **edges;
} vert_t;

typedef struct sprite_s
{
	vector2d_t verts[2],line,normal;
	float heights[2]; /* "height" of the sprite */
	texture_t *texture;

	float mingrad,maxgrad;	/* used in runtime for visibility clipping */
	int nobounds;
	int renderflag;		/* flagged for rendering, prevents duplicates */
	float texoffset;
	float dist;
} sprite_t;

typedef struct platform_s
{
	float ceilheight,floorheight;
	int numedges;
	edge_t **edges;
	struct texture_s *texture;

	int allocatedsprites;
	int numsprites;
	sprite_t **sprites;
} platform_t;

typedef struct level_s
{
	int numedges;
	edge_t *edges;
	
	int numplatforms;
	platform_t *platforms;
	platform_t infplatform;
	
	texture_t *texturelist;
	texture_t *lasttexture;
	
	int numverts;
	vert_t *verts;
	vector2d_t size;	
} level_t;

#define HUNK_INTERSECTIONS	8

typedef struct intersection_s
{
	vector2d_t pos;
	edge_t *edge;
	platform_t *platform;
	float distance;
	float texoffset;
	int final;
} intersection_t;

typedef enum
{
	INTERSECTION_ENTRY,
	INTERSECTION_LEAVING
} intersectiontype_t;

typedef struct solidintersection_s
{
	vector2d_t pos;
	edge_t *edge;
	platform_t *platform;
	vert_t *vert;
	float distance;
	intersectiontype_t type;
	
} solidintersection_t;

typedef struct raycaster_s
{
	SDL_Surface *screen;
	level_t level;

	vector2d_t viewdir;
	vector2d_t viewpos;
	float eyelevel;
	platform_t *currentplatform;
	int cursorx,cursory;
	int lastcursorx,lastcursory;

	int numintersections;
	int allocatedintersections;
	intersection_t *intersections;

	int numsprites;
	int allocatedsprites;
	sprite_t **spritelist;
	unsigned short transpixel;

	vector2d_t mousespeed;
	int lastmousepolltime;

	int lastfpsreporttime;
	int framessincelastreport;
} raycaster_t;

#include "physics.h"
intersection_t *
edgeintersect ( raycaster_t *r, platform_t *p, vector2d_t *dir, 
		vector2d_t *passedorigin, float prevdist, 
		intersection_t *intersection, edge_t *ignoreedge);

sprite_t * addsprite ( raycaster_t *r, vector2d_t *verts, float height, float vdist, 
		int surface, texture_t *texture );
texture_t *texturefrompath ( raycaster_t *r, char *path );
platform_t * pickplatform ( raycaster_t *r, vector2d_t *v );
int pointcanseepoint ( raycaster_t *r, vector2d_t *v1, float v1height, vector2d_t *v2, float v2height );

#endif

