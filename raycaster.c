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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "raycaster.h"
#include "world.h"
#include "vector.h"
#include "physics.h"
#include "tga.h"

#define SCREEN_WIDTH	1024
#define SCREEN_HEIGHT	768

#define DEFAULT_LEVEL	"levels/out.lvl"

#define PACKED	__attribute__((packed))

/* Edge structure, as stored in the file. */
typedef struct fedge_s
{
	int32_t vertrefs[2];
	int32_t leftplatref,rightplatref;
} PACKED fedge_t;

/* Vector structure, as stored in the file. */
typedef struct fvector2d_s
{
	float x,y;
} PACKED fvector2d_t;

/* Vertex structure, as stored in the file. */
typedef struct fvert_s
{
	vector2d_t pos;
	int32_t numedges;
	uint32_t dummy;
} PACKED fvert_t;

/* Platform structure, as stored in the file. */
typedef struct fplatform_s
{
	float ceilheight,floorheight;
	int32_t numedges;
	uint32_t dummy;
} PACKED fplatform_t;

/* Level structure, as stored in the file. */
typedef struct levelfile_s
{
	int32_t numedges;
	uint32_t dummy1;
	
	int32_t numplatforms;
	uint32_t dummy2;
	fplatform_t infplatform;
	
	int32_t numverts;
	uint32_t dummy3;
	vector2d_t size;
} PACKED levelfile_t;

/* Intermediate vertex structure, used when loading the level. */
typedef struct ivert_s
{
	fvert_t f;
	int32_t *edgerefs;
} ivert_t;

/* Intermediate platform structure, used when loading the level. */
typedef struct iplatform_s
{
	fplatform_t f;
	int32_t *edgerefs;
} iplatform_t;


#define FINAL_FLOOR		1
#define FINAL_CEILING		2

#define PRECISION_BITS			8
#define DOUBLE_PRECISION_BITS		(2*PRECISION_BITS)
#define PRECISION_PRODUCT		((float)(1<<PRECISION_BITS))		/* 2^PRECISION_BITS */
#define PRECISION_PRODUCT_SQUARE	(PRECISION_PRODUCT*PRECISION_PRODUCT) 	/* 2^DOUBLE_PRECISION_BITS */

/**************************************************************/

int
intlog2 ( int num, int *log )
{
	int i,n;

	for(n=1,i=0;i<8*sizeof(int)-2;i++,n=n<<1)
	{
		if(num == n)
		{
			*log = i;
			return 1;
		}
	}
	return 0;
}

int
setlogdimensions ( texture_t *t )
{
	if(!intlog2(t->width,&t->log2width))
		return 0;
	if(!intlog2(t->height,&t->log2height))
		return 0;
	return 1;
}

int
loadtexture ( texture_t *t, raycaster_t *r, char *filename )
{
	byte *bpixel;
	int x,y;
	bitmap_t b;

	snprintf(t->path, sizeof(t->path), "textures/%s", filename);
	t->pixels = NULL;
	
	if(!loadTGA(t->path,&b))
		return 0;
	t->width = b.width;
	t->height = b.height;

	t->widthmask = t->width-1;
	t->heightmask = t->height-1;

	t->widthmaskshift = (t->width<<DOUBLE_PRECISION_BITS)-1;
	t->heightmaskshift = (t->height<<DOUBLE_PRECISION_BITS)-1;

	t->widthmasksmallshift = (t->width<<PRECISION_BITS)-1;
	t->heightmasksmallshift = (t->height<<PRECISION_BITS)-1;
	
	if(!setlogdimensions(t))
	{
		printf("Bad texture size in %s, %ix%i\n", t->path,t->width,t->height);
		freeTGA(&b);
		return 0;
	}
	
	t->pixels = (unsigned short*)malloc(sizeof(unsigned short)*b.width*b.height);
	for(x=0;x<b.width;x++)
	{
		for(y=0;y<b.height;y++)
		{
			bpixel = (byte*)getPixel ( &b,x,y );
			t->pixels[y+x*b.height] = 
				SDL_MapRGB(r->screen->format,bpixel[2],bpixel[1],bpixel[0]);
		}
	}
	freeTGA(&b);
	return 1;
}

texture_t *
allocatetexture ( level_t *l )
{
	texture_t *new;

	new = (texture_t*)malloc(sizeof(texture_t));
	
	new->next = NULL;
	if(!l->lasttexture)
	{
		l->texturelist = l->lasttexture = new;
		new->prev = NULL;
		return new;
	}
	l->lasttexture->next = new;
	new->prev = l->lasttexture;
	l->lasttexture = new;
	
	return new;
}

texture_t *
texturefrompath ( raycaster_t *r, char *path )
{
	texture_t *t;
	
	for(t=r->level.texturelist;t;t=t->next)
	{
		if(!strcmp(path,t->path))
			return t;
	}
	printf("loading texture %s...\n", path);
	t = allocatetexture ( &r->level );
	if(!loadtexture( t, r, path ))
		return NULL;
	
	return t;
}

unsigned short
gettexturepixel ( texture_t *t, int x, int y )
{
	return t->pixels[y+(x<<t->log2height)];
}

void
freetexture ( texture_t *t )
{
	if(t->pixels)
	{
		free(t->pixels);
		t->pixels = NULL;
	}
	
}

/**************************************************************/

int
startsdl( raycaster_t *r )
{
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1)
	{
		printf("Failed to start SDL\n");
		return 0;
	}
	
	r->screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 16, SDL_HWSURFACE|SDL_HWPALETTE);
	if(!r->screen)
	{
		fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
		return 0;
	}
	return 1;
}

void
convertedge ( raycaster_t *r, level_t *l, fedge_t *fe, edge_t *e )
{
	int i;
	
	e->texture = texturefrompath(r,"wall.tga");
	for(i=0;i<2;i++)
		e->verts[i] = &l->verts[fe->vertrefs[i]];
	if(fe->leftplatref == -1)
		e->leftplat = &l->infplatform;
	else
		e->leftplat = &l->platforms[fe->leftplatref];
		
	if(fe->rightplatref == -1)
		e->rightplat = &l->infplatform;
	else
		e->rightplat = &l->platforms[fe->rightplatref];

	vectorsubtract(&e->verts[1]->pos,&e->verts[0]->pos,&e->line);
	vectornormalise(&e->line,&e->line);
	vectorrot90(&e->line,&e->normal);
	e->planedist = dotproduct(&e->normal,&e->verts[0]->pos);
}

void
convertvert ( raycaster_t *r, level_t *l, ivert_t *iv, vert_t *v )
{
	int i;
	v->pos.x = iv->f.pos.x;
	v->pos.y = iv->f.pos.y;
	v->numedges = iv->f.numedges;
	
	v->edges = (edge_t**)malloc(sizeof(edge_t*)*iv->f.numedges);
	for(i=0;i<v->numedges;i++)
		v->edges[i] = &l->edges[iv->edgerefs[i]];
}

void
convertplatform ( raycaster_t *r, level_t *l, iplatform_t *ip, platform_t *p )
{
	int i;
	
	p->ceilheight = ip->f.ceilheight;
	p->floorheight = ip->f.floorheight;

	p->numedges = ip->f.numedges;
	p->edges = (edge_t**)malloc(sizeof(edge_t*)*ip->f.numedges);
	for(i=0;i<ip->f.numedges;i++)
	{
		p->edges[i] = &l->edges[ip->edgerefs[i]];
	}
	p->texture = texturefrompath(r,"floor.tga");
	p->allocatedsprites = 4;
	p->sprites = (sprite_t**)malloc(p->allocatedsprites*sizeof(sprite_t*));
	p->numsprites = 0;

}

void
optimiseplatform ( raycaster_t *r, level_t *l, platform_t *p)
{
	int i,changeceil=1,changefloor=1,firstfloor=1,firstceil=1;
	platform_t *n; /* Neighbouring platform. */
	float highest=0.0f,lowest=0.0f;

	/* If the platform's floor is higher than all its neighbours' ceilings then
	 * set the platform's floor height to the maximum of its neighbours
	 * ceilings. This is to minimise overdraw.
	 *
	 * Do similar if the platform's ceiling is lower than all of its
	 * neighbours' floors.
	 */
	for(i=0;i<p->numedges;i++)
	{
		if(p->edges[i]->leftplat != p)
			n = p->edges[i]->leftplat;
		else
			n = p->edges[i]->rightplat;
		if(p->floorheight > n->ceilheight)
		{
			if(firstfloor || n->ceilheight > highest)
				highest = n->ceilheight;
			firstfloor = 0;
		} else 
		{
			changefloor = 0;
		}
		
		if(p->ceilheight < n->floorheight)
		{
			if(firstceil || n->floorheight < lowest)
				lowest = n->floorheight;
			firstceil = 0;
		} else
		{
			changeceil = 0;
		}
	}
	if(changefloor)
		p->floorheight = highest+1.0f;	
	if(changeceil)
		p->ceilheight = lowest-1.0f;
}

int
loadlevel ( raycaster_t *r, char *filename )
{
	FILE *f;
	levelfile_t lf;
	level_t *l=&r->level;
	int i;
	fedge_t *fedges;
	iplatform_t *iplatforms;
	iplatform_t iinfplatform;
	ivert_t *iverts;
	
	f=fopen(filename,"rb");
	if(!f)
	{
		printf("Could not open file %s\n", filename);
		return 0;
	}
	
	fread(&lf,sizeof(levelfile_t),1,f);

	fedges = (fedge_t*)malloc(sizeof(fedge_t)*lf.numedges);
	for(i=0;i<lf.numedges;i++)
	{	
		fread(&fedges[i],sizeof(fedge_t),1,f);
	}
	
	iplatforms = (iplatform_t*)malloc(sizeof(iplatform_t)*lf.numplatforms);
	for(i=0;i<lf.numplatforms;i++)
	{	
		fread(&iplatforms[i].f,sizeof(fplatform_t),1,f);
		iplatforms[i].edgerefs = 
			(int32_t*)malloc(sizeof(int32_t)*iplatforms[i].f.numedges);
		fread(iplatforms[i].edgerefs,sizeof(int32_t),iplatforms[i].f.numedges,f);
	}

	iinfplatform.f = lf.infplatform;
	iinfplatform.edgerefs = 
		(int32_t*)malloc(sizeof(int32_t)*iinfplatform.f.numedges);
	fread(iinfplatform.edgerefs,sizeof(int32_t),iinfplatform.f.numedges,f);
	
	iverts = (ivert_t*)malloc(sizeof(ivert_t)*lf.numverts);
	for(i=0;i<lf.numverts;i++)
	{	
		fread(&iverts[i].f,sizeof(fvert_t),1,f);
		iverts[i].edgerefs = 
			(int32_t*)malloc(sizeof(int32_t)*iverts[i].f.numedges);
		fread(iverts[i].edgerefs,sizeof(int32_t),iverts[i].f.numedges,f);
	}
	
	fclose(f);
	
	l->numedges = lf.numedges;
	l->numplatforms = lf.numplatforms;
	l->numverts = lf.numverts;
	l->size.x = lf.size.x;
	l->size.y = lf.size.y;
	
	l->edges = (edge_t*)malloc(sizeof(edge_t)*l->numedges);
	l->platforms = (platform_t*)malloc(sizeof(platform_t)*l->numplatforms);
	l->verts = (vert_t*)malloc(sizeof(vert_t)*l->numverts);

	for(i=0;i<lf.numverts;i++)
		convertvert(r,l,&iverts[i],&l->verts[i]);	
	for(i=0;i<lf.numedges;i++)
		convertedge(r,l,&fedges[i],&l->edges[i]);
	for(i=0;i<lf.numplatforms;i++)
		convertplatform(r,l,&iplatforms[i],&l->platforms[i]);	
	convertplatform(r,l,&iinfplatform,&l->infplatform);

	for(i=0;i<lf.numplatforms;i++)
		optimiseplatform(r,l,&l->platforms[i]);	
	optimiseplatform(r,l,&l->infplatform);
	
	/*
	 * Cleanup.
     */
	for(i=0;i<lf.numplatforms;i++)
		free(iplatforms[i].edgerefs);
	free(iinfplatform.edgerefs);
	free(iplatforms);
	for(i=0;i<lf.numverts;i++)
		free(iverts[i].edgerefs);
	free(iverts);
	free(fedges);

	return 1;
}

void
handlekeypress( raycaster_t *r, world_t *w, int event, int *done, int down )
{
	int movekey;
	switch(event)
	{
		case SDLK_ESCAPE:
			*done=1;
			return;
		case SDLK_w:
			movekey=KEY_FORWARD;
			break;
		case SDLK_a:
			movekey=KEY_LEFT;
			break;
		case SDLK_s:
			movekey=KEY_BACK;
			break;
		case SDLK_d:
			movekey=KEY_RIGHT;
			break;
		case SDLK_n:
			movekey=KEY_TLEFT;
			break;
		case SDLK_m:
			movekey=KEY_TRIGHT;
			break;
		default:
			return;
	}
	if(down)
		w->playerentity->keys |= movekey;
	else
		w->playerentity->keys &= ~movekey;
}
	
void
handleevents( raycaster_t *r, world_t *w, int *done )
{
	SDL_Event event;
	
	while(SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_KEYDOWN:
				handlekeypress(r,w,event.key.keysym.sym,done,1);
				break;
			case SDL_KEYUP:
				handlekeypress(r,w,event.key.keysym.sym,done,0);
				break;
			case SDL_QUIT:
				*done = 1;
				break;
			case SDL_MOUSEMOTION:
				r->cursorx=event.motion.x;
				r->cursory=event.motion.y;

				break;
			default:
				break;
		}
	}
}

#define HUNK_SPRITES	4

void
addspritetolist ( raycaster_t *r, sprite_t **i, float mingrad, float maxgrad )
{
	if(!(*i)->renderflag)
	{
		if(r->numsprites == r->allocatedsprites)
		{
			r->allocatedsprites+=HUNK_SPRITES;
			r->spritelist = (sprite_t**)realloc(r->spritelist,
					sizeof(sprite_t*)*r->allocatedsprites);
		}
		memcpy(&r->spritelist[r->numsprites++],i,sizeof(sprite_t*));
	
		(*i)->renderflag = 1;
	}
	
	(*i)->mingrad = mingrad;
	(*i)->maxgrad = maxgrad;
	
	return;
}

#define DIST_THRESHOLD	0.1f

int
linelineintersect ( vector2d_t *origin, vector2d_t *dir, vector2d_t *vert1, 
		vector2d_t *vert2, vector2d_t *normal, vector2d_t *normalrot, 
		float *dist, float *texoffset, vector2d_t *poi )
{
	float d,dp1,dp2;
	
	d=dotproduct(dir,normal);
	if(d==0.0f)
		return 0;
	
	*dist = (dotproduct(vert1,normal)-dotproduct(origin,normal));

	if((*dist <= 0.0f && d > 0.0f) ||
		(*dist > 0.0f && d <= 0.0f))
		return 0;

	*dist/=d;

	vectorscale(dir,*dist,poi);
	vectoradd(poi,origin,poi);

	d = dotproduct(poi,normalrot);
	dp1 = dotproduct(vert1,normalrot);
	dp2 = dotproduct(vert2,normalrot);
	
	if(texoffset)
		*texoffset = d-dp1;

	if( (d>=dp1 && d<=dp2) || (d<=dp1 && d>=dp2) )
		return 1;
	return 0;
}

/* check for intersections of the passed ray with the specified platform */
intersection_t *
edgeintersect ( raycaster_t *r, platform_t *p, vector2d_t *dir, 
		vector2d_t *passedorigin, float prevdist, 
		intersection_t *intersection, edge_t *ignoreedge)
{
	vector2d_t poi,origin;
	int i,first,done;
	float dist,mindist=0.0f,texoffset;
	platform_t *nextplatform;
	intersection_t in;
	edge_t *e;
	
	vectorcopy(&origin,passedorigin);
	
	first = 1;
	done = 0;
	for(i=0;i<p->numedges;i++)
	{
		e=p->edges[i];
		
		if(!linelineintersect(&origin,dir,&e->verts[0]->pos,
					&e->verts[1]->pos,&e->normal,&e->line,
					&dist,&texoffset,&poi))
		{
			continue;
		}

		// Determine what the next platform is, and check that it's in the forward
		// direction.
		if(e->leftplat != p)
		{
			if (dotproduct(&e->normal, dir) > 0.0f)
				continue;
			nextplatform = e->leftplat;
		}
		else if(e->rightplat != p)
		{
			if (dotproduct(&e->normal, dir) < 0.0f)
				continue;
			nextplatform = e->rightplat;
		}
		else
		{
			printf("edgeintersect() problem\n");
			nextplatform = NULL;
		}

		if(first || dist < mindist)
		{
			mindist = dist;
			first = 0;
		} else
		{
			continue;
		}
		
		vectorcopy(&in.pos,&poi);
		in.distance = dist + prevdist;
		in.edge = e;
		in.final = 0;
		in.platform = nextplatform;
		
		in.texoffset = texoffset;
		done = 1;
	}
	if(!done)
	{
		printf("No intersection!\n");
		return NULL;
	}
	memcpy(intersection,&in,sizeof(intersection_t));
	return intersection;
}

/* sets intersection to the nearest point on the line where the cylinder either
 * leaves a platform or enters a platform
 */
void
cylinderintersect ( raycaster_t *r, platform_t **plats, int nplats, 
			vector2d_t *dir, vector2d_t *passedorigin, float radius
			,float prevdist, solidintersection_t *intersection )
{
	int i,j;
	platform_t *plat;
	edge_t *e;
	float mindist = -1.0f,dist;
	vector2d_t verts[2],push;
	vector2d_t poi;
	
	for(i=0;i<nplats;i++)
	{
		plat = plats[i];
		for(j=0;j<plat->numedges;j++)
		{
			e = plat->edges[j];
			vectorscale(&e->normal,radius,&push);

			vectoradd(&e->verts[0]->pos,&push,&verts[0]);
			vectoradd(&e->verts[1]->pos,&push,&verts[1]);
			
			if(linelineintersect(passedorigin,dir,&verts[0],&verts[1],
					&e->normal,&e->line,&dist,NULL,&poi) &&
					(dist < mindist || mindist < 0.0f))
			{
				intersection->distance = dist;
				intersection->edge = e;
				if(e->rightplat != plat)
				{
					intersection->type = INTERSECTION_LEAVING;
				} else
				{
					intersection->type = INTERSECTION_ENTRY;
				}
				intersection->platform = e->leftplat;
				mindist = dist;
			}
			
			vectorsubtract(&e->verts[0]->pos,&push,&verts[0]);
			vectorsubtract(&e->verts[1]->pos,&push,&verts[1]);
			
			if(linelineintersect(passedorigin,dir,&verts[0],&verts[1],
					&e->normal,&e->line,&dist,NULL,&poi) &&
					(dist < mindist || mindist < 0.0f))
			{
				intersection->distance = dist;
				intersection->edge = e;
				if(e->rightplat == plat)
				{
					intersection->type = INTERSECTION_LEAVING;
				} else
				{
					intersection->type = INTERSECTION_ENTRY;
				}
				intersection->platform = e->rightplat;
				mindist = dist;
			}
			
		}
	}
}

/* checkceiling
 *
 * Determines if a platform's ceiling appears lower on the screen than
 * the previous lowest ceiling
 */
int
checkceiling ( raycaster_t *r, platform_t *p, float g, float *best, 
		intersection_t **in, intersection_t *current , int first )
{
	if(first || g < *best)
	{
		*best = g;
		*in = current;
		return 1;
	}
	return 0;
}

/* checkfloor
 *
 * Determines if a platform's floor appears higher on the screen than
 * the previous highest floor
 */
int
checkfloor ( raycaster_t *r, platform_t *p, float g, float *best, 
		intersection_t **in, intersection_t *current , int first )
{
	if(first || g > *best)
	{
		*best = g;
		*in = current;
		return 1;
	}
	return 0;
}

/* checkdone
 *
 * Determine if the highest floor is above the lowest ceiling
 * ie. our view has been obscured and we can not see beyond these
 * platforms
 */
int
checkdone( int first, float high, float low, intersection_t *hi,
		intersection_t *li )
{
	if(first)
		return 0;
	if(high > low)
	{
		hi->final |= FINAL_FLOOR;
		li->final |= FINAL_CEILING;
		return 1;
	}
	return 0;
}

#define SCREEN_DISTANCE	1.0f
#define TAN_FOV		1.0f	/* tan(45) */
#define HALF_SCREEN_HEIGHT	(SCREEN_HEIGHT>>1)

float pixeltogradcoefficent;
float
pixeltogradslow ( int pixel )
{
	return (float)(pixel-HALF_SCREEN_HEIGHT)*pixeltogradcoefficent;
}

float pixeltograd[SCREEN_HEIGHT];
float invpixeltograd[SCREEN_HEIGHT];
int invpixeltogradint[SCREEN_HEIGHT];
void
pixeltogradinit ( void )
{
	int i=0;
	for(i=0;i<SCREEN_HEIGHT;i++)
	{
		pixeltograd[i] = pixeltogradslow(i);
		invpixeltograd[i] = 1.0f/pixeltograd[i];
		invpixeltogradint[i] = (int)(PRECISION_PRODUCT*invpixeltograd[i]);
	}
}

float gradtopixelcoefficent;
int
gradtopixel ( float grad )
{
	return (int)(grad*gradtopixelcoefficent)+HALF_SCREEN_HEIGHT;
}

void
drawvertline ( raycaster_t *r, float g1, float g2, int x, int re, int g, int b )
{
	unsigned short *pixel;
	int p1,p2,t,y;
	p1 = gradtopixel(g1);
	p2 = gradtopixel(g2);
	if(p2 < p1)
	{
		t=p1;
		p1=p2;
		p2=t;
	}
	if(p1 < 0)
		p1 = 0;
	else if(p1 > SCREEN_HEIGHT-1)
		p1 = SCREEN_HEIGHT-1;
	if(p2 < 0)
		p2 = 0;
	else if(p2 > SCREEN_HEIGHT-1)
		p2 = SCREEN_HEIGHT-1;

	pixel = ((unsigned short*)r->screen->pixels)+(p1)*SCREEN_WIDTH+(x);
	for(y=p1;y<p2;y++)
	{
		*pixel = SDL_MapRGB(r->screen->format, re,g,b);
		pixel += SCREEN_WIDTH;
	}
}

int
propermodulo( int a, int b )
{
	if(a<0)	
	{
		while(a<0)
			a+=b;
		return a;
	}
	return a%b;
}

/* drawwall
 *
 * Draw a piece of a wall, between gradients g1 and g2
 */
inline void
drawwall ( raycaster_t *r, intersection_t *in, float g1, float g2, int x )
{
	unsigned short *pixel,*tpixel;
	int p1,p2,y,tx,ty,i,h1,h2;
	texture_t *t=in->edge->texture;
	
	p1 = gradtopixel(g1);
	p2 = gradtopixel(g2);
	if(p2 == p1)
		return;

	/* Clamp p1,p2 and the corresponding values of h1,h2 to the screen.
	 */
	if(p1 < 0)
		p1 = 0;

	if(p2 > SCREEN_HEIGHT-1)
		p2 = SCREEN_HEIGHT-1;

	h1 = (int)(PRECISION_PRODUCT * (r->eyelevel + pixeltograd[p1] * in->distance));
	h2 = (int)(PRECISION_PRODUCT * (r->eyelevel + pixeltograd[p2] * in->distance));

	tx = ((int)in->texoffset)&(t->widthmask);
	i = ((h2-h1)/(p2-p1)) & t->heightmasksmallshift;
	ty = h1 & t->heightmasksmallshift;
	pixel = ((unsigned short*)r->screen->pixels)+(p1)*SCREEN_WIDTH+(x);
	tpixel = &t->pixels[tx<<t->log2height];
	for(y=p1;y<p2;y++)
	{
		*pixel = tpixel[ty>>PRECISION_BITS];

		/* Recalculate the vertical texture pixel `ty`. Most of the time do this by
		 * adding on a constant but periodically do an expensive recalculation. This is
		 * to mitigate acculated rounding errors in the increment value `i`.
		 */
		if ((y & 0xF) == 0)
			ty = ((h2 * (y - p1) + h1 * (p2 - y)) / (p2 - p1)) & t->heightmasksmallshift;
		else
			ty = (ty+i)&(t->heightmasksmallshift);
		pixel += SCREEN_WIDTH;
	}
}


/* drawfloor
 *
 * h is the distance the floor is above or below current eye level
 *   it is required for correct texture mapping
 */
inline void
drawfloor ( raycaster_t *r, platform_t *p, float h,
		vector2d_t *dir, float g1, float g2, int x )
{
	unsigned short *pixel;
	int p1,p2,y,ty,tx;;
	int hdirx,hdiry,ox,oy;
	texture_t *t;
	
	p1 = gradtopixel(g1);
	p2 = gradtopixel(g2);
	
	if(p1 < 0)
		p1 = 0;
	
	if(p2 > SCREEN_HEIGHT-1)
		p2 = SCREEN_HEIGHT-1;
	
	pixel = ((unsigned short*)r->screen->pixels)+(p1)*SCREEN_WIDTH+(x);
	hdirx = (int)(dir->x*h*PRECISION_PRODUCT);
	hdiry = (int)(dir->y*h*PRECISION_PRODUCT);
	ox = ((int)r->viewpos.x)<<DOUBLE_PRECISION_BITS;
	oy = ((int)r->viewpos.y)<<DOUBLE_PRECISION_BITS;
	
	t=p->texture;
	for(y=p1;y<p2;y++)
	{
		tx = (hdirx*invpixeltogradint[y]+ox)&(p->texture->widthmaskshift);
		ty = (hdiry*invpixeltogradint[y]+oy)&(p->texture->heightmaskshift);
		
		*pixel = t->pixels[(ty>>DOUBLE_PRECISION_BITS)+((tx>>DOUBLE_PRECISION_BITS)<<t->log2height)];
		
		pixel += SCREEN_WIDTH;
	}
}

static float clamp(float v, float low, float high)
{
	return v < low ? low : (v > high ? high : v);
}

void
drawsprite ( raycaster_t *r, sprite_t *s, vector2d_t *dir, int x )
{
	float sprmingrad,sprmaxgrad,mingr,maxgr;
	int i,y;
	int p1,p2,p1b,p2b,ty,tx,h1=s->heights[0],h2=s->heights[1];
	unsigned short *pixel,*tpixel;
	texture_t *t=s->texture;
	
	sprmingrad = (s->heights[0]-r->eyelevel)/s->dist;
	sprmaxgrad = (s->heights[1]-r->eyelevel)/s->dist;

	p1b = gradtopixel(sprmaxgrad);
	p2b = gradtopixel(sprmingrad);
	
	if(sprmingrad > s->mingrad)
	{
		mingr = sprmingrad;
		p2 = p2b;
	} else
	{
		mingr = s->mingrad;
		p2 = gradtopixel(mingr);
	}
	
	if(sprmaxgrad < s->maxgrad)
	{
		maxgr = sprmaxgrad;
		p1 = p1b;
	} else
	{
		maxgr = s->maxgrad;
		p1 = gradtopixel(maxgr);
	}

	if(p1 < 0)
		p1 = 0;
	if(p2 > SCREEN_HEIGHT-1)
		p2 = SCREEN_HEIGHT-1;
	
	tx = ((int)s->texoffset)%(t->widthmask);
	pixel = ((unsigned short*)r->screen->pixels)+(p1)*SCREEN_WIDTH+(x);
	tpixel = &t->pixels[tx<<t->log2height];
	ty = ((((p1-p1b)*(h2-h1))<<PRECISION_BITS)/
			(p2b-p1b))&(t->heightmasksmallshift);
	i = ((h2-h1)<<PRECISION_BITS)/(p2b-p1b);

	for(y=p1;y<p2;y++)
	{
		if(tpixel[ty>>PRECISION_BITS] != r->transpixel)
			*pixel = tpixel[ty>>PRECISION_BITS];
		ty = (ty+i)&(t->heightmasksmallshift);
		pixel += SCREEN_WIDTH;
	}	
}

void
drawsprites( raycaster_t *r, vector2d_t *dir, int x )
{
	int i;
	sprite_t *s;
	
	for(i=0;i<r->numsprites;i++)
	{
		s = r->spritelist[i];
		drawsprite(r,s,dir,x);
		s->renderflag = 0;
	}
}

/* drawcolumn
 *
 * This function draws a vertical line of pixels representing
 * the world.
 *
 * x is the column we are drawing.
 */
void
drawcolumn ( raycaster_t *r, vector2d_t *dir, int x )
{
	int i;
	intersection_t in;
	float floorgrad, ceilgrad;
	float prevfloorgrad, prevceilgrad;
	float maxfloorgrad, minceilgrad;
	float g1,g2;
	float prevdist;
	platform_t *prevplat;
	sprite_t *s;	
	vector2d_t poi;
	
	/* Step through the intersections rendering the ceiling and the floor
     * from near to far. Keep track of the highest floor gradient and lowest
     * ceiling gradient rendered so far and use this to prevent drawing over
     * the nearer surfaces.
	 *
     * When stepping through also add sprites to a list of sprites to be
     * drawn. The sprites are rendered last.
     */
	maxfloorgrad = pixeltograd[SCREEN_HEIGHT-1];    /* Highest floor gradient so far. */
	minceilgrad = pixeltograd[0];                   /* Lowest ceiling gradient so far. */

	prevfloorgrad = -INFINITY; /* Straight down. */
	prevceilgrad = +INFINITY;  /* Straight up. */

	prevplat = r->currentplatform;
	prevdist = 0.0f;

	r->numsprites = 0;

	if (edgeintersect(r, prevplat, dir, &r->viewpos, 0.0f, &in, NULL) == NULL)
		return;
	while(maxfloorgrad < minceilgrad)
	{
		/* Add sprites to list to be rendered. */
		for(i=0;i<prevplat->numsprites;i++)
		{
			s=prevplat->sprites[i];
			if(linelineintersect(&r->viewpos,dir,&s->verts[0],&s->verts[1],
				&s->normal,&s->line,&s->dist,&s->texoffset,&poi) &&
				s->dist > prevdist && s->dist < in.distance)
			{
				addspritetolist(r,&prevplat->sprites[i],maxfloorgrad,minceilgrad);
			}
		}

		/* Draw floor from prev platform to current platform. */
		floorgrad = (prevplat->floorheight - r->eyelevel) / in.distance;
		g2 = clamp(prevfloorgrad, maxfloorgrad, minceilgrad);
		g1 = clamp(floorgrad, maxfloorgrad, minceilgrad);
		if (g2 < g1)
		{
			drawfloor(r, prevplat, prevplat->floorheight - r->eyelevel, dir, g1, g2, x);
			maxfloorgrad = g1;
		}
		prevfloorgrad = floorgrad;

		/* Draw ceiling from prev platform to current platform. */
		ceilgrad = (prevplat->ceilheight - r->eyelevel) / in.distance;
		g1 = clamp(prevceilgrad, maxfloorgrad, minceilgrad);
		g2 = clamp(ceilgrad, maxfloorgrad, minceilgrad);
		if (g2 < g1)
		{
			drawfloor(r, prevplat, prevplat->ceilheight - r->eyelevel, dir, g1, g2, x);
			minceilgrad = g2;
		}
		prevceilgrad = ceilgrad;

		/* Draw wall from prev floor to current floor. */
		floorgrad = (in.platform->floorheight - r->eyelevel) / in.distance;
		g2 = clamp(prevfloorgrad, maxfloorgrad, minceilgrad);
		g1 = clamp(floorgrad, maxfloorgrad, minceilgrad);
		if (g2 < g1)
		{
			drawwall(r, &in, g1, g2, x);
			maxfloorgrad = g1;
		}
		prevfloorgrad = floorgrad;

		/* Draw wall from prev ceiling to current ceiling. */
		ceilgrad = (in.platform->ceilheight - r->eyelevel) / in.distance;
		g1 = clamp(prevceilgrad, maxfloorgrad, minceilgrad);
		g2 = clamp(ceilgrad, maxfloorgrad, minceilgrad);
		if (g2 < g1)
		{
			drawwall(r, &in, g1, g2, x);
			minceilgrad = g2;
		}
		prevceilgrad = ceilgrad;

		prevplat = in.platform;
		prevdist = in.distance;
		if (maxfloorgrad < minceilgrad)
			if (edgeintersect(r, prevplat, dir, &in.pos, prevdist, &in, NULL) == NULL)
				return;
	}

	drawsprites(r,dir,x);
}

void
drawscene ( raycaster_t *r )
{
	int x;
	vector2d_t v,temp;
	
	for(x=0;x<SCREEN_WIDTH;x++)
	{
		vectorrot90(&r->viewdir,&temp);
		vectorscale(&r->viewdir,SCREEN_DISTANCE,&v);
		vectorscale(&temp,TAN_FOV*SCREEN_DISTANCE*
				((2.0f*(float)x/(float)SCREEN_WIDTH)-1.0f),&temp);
		vectoradd(&v,&temp,&v);
		
/*		vectornormalise(&v,&v);*/
		
		drawcolumn(r,&v,x);
	}
}

#define MIN_PHYSICS_FRAME_TIME	10	/* ms */
#define MIN_MOUSE_POLL_TIME	0	/* ms */
#define MIN_FPS_POLL_TIME 1000 /* ms */

void
perframe ( raycaster_t *r, world_t *w )
{
	static int last=0,current;
	int dt;
	
	current = SDL_GetTicks();
	dt = current-last;
	if(dt > MIN_PHYSICS_FRAME_TIME)
	{
		dophysics(r,w,MIN_PHYSICS_FRAME_TIME);
		last = current;
	}
	
	if(current > r->lastmousepolltime + MIN_MOUSE_POLL_TIME)
	{
		if(r->lastcursorx > -1)
		{
			r->mousespeed.x = ((float)(r->cursorx-r->lastcursorx))/(float)(current-r->lastmousepolltime);
			r->mousespeed.y = (float)(r->cursory-r->lastcursory)/(float)(current-r->lastmousepolltime);
			vectorscale(&r->mousespeed,1000.0f,&r->mousespeed);
		}

		r->lastmousepolltime = current;
		r->lastcursorx = r->cursorx;
		r->lastcursory = r->cursory;
	}

	if(current > r->lastfpsreporttime + MIN_FPS_POLL_TIME)
	{
		printf("FPS: %f (%f ms per frame)\n",
			(float)(1000.0f * r->framessincelastreport)/(float)(current - r->lastfpsreporttime),
			(float)(current - r->lastfpsreporttime)/(float)r->framessincelastreport);

		r->framessincelastreport = 0;
		r->lastfpsreporttime = current;
	} else
	{
		r->framessincelastreport++;
	}
}

void
drawscreen( raycaster_t *r )
{
	SDL_Rect screenrect;
	
	screenrect.x=0;
	screenrect.y=0;
	screenrect.w=SCREEN_WIDTH;
	screenrect.h=SCREEN_HEIGHT;
	
	drawscene(r);
	SDL_UpdateRects(r->screen, 1, &screenrect);
}

void
twopiconvention ( float *a )
{
	if(*a < 0)
	{
		do
		{
			*a += 2*M_PI;
		} while(*a<0);
		return;
	}
	if(*a > 2.0f*M_PI)
	{
		do
		{
			*a -= 2*M_PI;
		} while(*a>2.0f*M_PI);
		return;
	}
}

void
piconvention ( float *a )
{
	if(*a < -M_PI)
	{
		do
		{
			*a += 2*M_PI;
		} while(*a<-M_PI);
		return;
	}
	if(*a > M_PI)
	{
		do
		{
			*a -= 2*M_PI;
		} while(*a>M_PI);
		return;
	}
}

enum
{
	CONV_PI,
	CONV_TWO_PI
};

/* returns the angle ACB using the specified convention */
float
findangle ( vector2d_t *a, vector2d_t *b, vector2d_t *c, int convention )
{
	float f;
	vector2d_t t1,t2;
	vectorsubtract(a,c,&t1);
	vectorsubtract(b,c,&t2);

	f=atan2f(t2.x,t2.y)-atan2f(t1.x,t1.y);
	if(convention == CONV_PI)
		piconvention(&f);
	else
		twopiconvention(&f);
		
	return f;
}

#if 1
int
isinplatform( raycaster_t *r, platform_t *p, vector2d_t *v )
{
	int i;
	float angle,totalangle;
	edge_t *d;
	
	totalangle=0.0f;
	for(i=0;i<p->numedges;i++)
	{
		d=p->edges[i];
		angle=findangle(&d->verts[0]->pos,
				&d->verts[1]->pos,v,CONV_PI);
		if(d->leftplat == p)
		{
			totalangle+=angle;
		} else if(d->rightplat == p)
		{
			totalangle-=angle;
		} else
		{
			printf("Edge not associated with parent platform\n");
		}
	}
	if(totalangle > M_PI)
		return 1;
	return 0;
}
#else
int
isinplatform2( raycaster_t *r, platform_t *p, vector2d_t *v )
{
	int i,count=0;
	edge_t *d;
	vector2d_t verts[2];
	
	for(i=0;i<p->numedges;i++)
	{
		d=p->edges[i];
		vectorsubtract ( &d->verts[0]->pos, v, &verts[0] );
		vectorsubtract ( &d->verts[1]->pos, v, &verts[1] );
	
		if((verts[0].y > 0.0f) == (verts[1].y > 0.0f))
			continue;
		if(verts[0].x <= 0.0f && verts[1].x <= 0.0f)
			continue;
		if( verts[0].x > 0.0f && verts[1].x > 0.0f )
		{
			count++;
			continue;
		}
		if ( verts[0].x*(verts[1].y-verts[1].y) > verts[0].y*(verts[1].x-verts[1].x) )
		{
			count++;
			continue;
		}
	}
	return count&1;
}

#endif
#if 0
int
cylinderisinplatform ( raycaster_t *r, platform_t *p, vector2d_t *v, float radius )
{
	edge_t *e;
	vert_t *vert;
	vector2d_t diff;
	int i;
	float dist,dp1,dp2,dpv,radsqr;
	
	radsqr = radius*radius;
	
	if(isinplatform(r,p,v))
		return 1;
	/* otherwise check if we're radius distance from any edge
	 */
	for(i=0;i<p->numedges;i++)
	{
		e = p->edges[i];
		
		dist = dotproduct(&e->normal, v)-e->planedist;
		if(dist > radius)
			continue;
		if(dist < -radius)
			continue;
		
		dp1 = dotproduct(&e->line, &e->verts[0]->pos);
		dp2 = dotproduct(&e->line, &e->verts[1]->pos);
		dpv = dotproduct(&e->line, v);
		if( (dpv > dp1) == (dpv > dp2) )
			continue;

		return 1;
	}

	/* check if we're within radius distance of any point
	 */
	for(i=0;i<p->numverts;i++)
	{
		vert = p->verts[i];
		vectorsubtract(v,&vert->pos,&diff);
		if(diff.x*diff.x + diff.y*diff.y > radsqr)
			continue;
		return 1;
	}
	return 0;
}
int
cylinderpickplatforms ( raycaster_t *r, vector2d_t *v, float radius, 
		platform_t **list )
{
	int i,num=0;
	
	level_t *l=&r->level;
	for(i=0;i<l->numplatforms;i++)
	{
		if(cylinderisinplatform(r,&l->platforms[i],v,radius))
		{
			if(num == MAX_CYLINDER_PICKS)
				break;
			list[num++] = &l->platforms[i];
		}
	}
	return num;
}

#endif
platform_t *
pickplatform ( raycaster_t *r, vector2d_t *v )
{
	int i;
	
	level_t *l=&r->level;
	for(i=0;i<l->numplatforms;i++)
	{
		if(isinplatform(r,&l->platforms[i],v))
			return &l->platforms[i];
	}

	printf("Pick platform returned infplat!\n");
	return &l->infplatform;
}

int
pointcanseepoint ( raycaster_t *r, vector2d_t *v1, float v1height, vector2d_t *v2, float v2height )
{
	intersection_t in;
	float tracedist,tracegrad,dist=0.0f,h;
	platform_t *plat;
	vector2d_t dir,pos,offs;

	vectorsubtract ( v2, v1, &dir );
	
	tracedist = vectorlength ( &dir );
	vectorscale(&dir,1.0f/tracedist,&dir);

	tracegrad = (v2height-v1height)/tracedist;

	vectorcopy(&pos,v1);
	plat = pickplatform(r,v1);
	if(!plat)
		return 0;

	vectorscale(&dir,0.001f,&offs);
	while ( 0 < 1)
	{
		edgeintersect ( r, plat, &dir, &pos, dist, &in, NULL );
		if(in.distance > tracedist)
			return 1;
		
		/* Check if trace intersects with this edge's ceilings
		 */
		if( in.edge->rightplat->ceilheight < in.edge->leftplat->ceilheight )
		{
			h = in.edge->rightplat->ceilheight;
		} else
		{
			h = in.edge->leftplat->ceilheight;
		}
		if(((h-v1height)/in.distance) < tracegrad)
			return 0;
		
		/* Check if trace intersects with this edge's floors
		 */
		if( in.edge->rightplat->floorheight > in.edge->leftplat->floorheight )
		{
			h = in.edge->rightplat->floorheight;
		} else
		{
			h = in.edge->leftplat->floorheight;
		}
		if(((h-v1height)/in.distance) > tracegrad)
			return 0;
		
		dist = in.distance;
/*		vectorcopy(&pos,&in.pos);*/
		vectoradd(&offs,&in.pos,&pos);
		plat = in.platform;
	}
	
	return 1;
}

void
initvariables ( raycaster_t *r )
{
	r->viewpos.y = 384.0f;
	r->viewpos.x = 32.0f;

	r->currentplatform = pickplatform(r,&r->viewpos);
	
	r->numsprites = 0;
	r->allocatedsprites = HUNK_SPRITES;
	r->spritelist = (sprite_t**)malloc(
			sizeof(sprite_t)*r->allocatedsprites);

	r->transpixel = SDL_MapRGB(r->screen->format,255,0,255);

	r->mousespeed.x = r->mousespeed.y = 0.0f;
	r->lastcursorx = r->lastcursory = -1;
	r->lastmousepolltime = 0;
	r->lastfpsreporttime = 0;
	r->framessincelastreport = 0;

	gradtopixelcoefficent = -(float)(SCREEN_WIDTH*HALF_SCREEN_HEIGHT)/((float)SCREEN_HEIGHT*TAN_FOV);
	pixeltogradcoefficent = -(TAN_FOV*(float)SCREEN_HEIGHT/(float)SCREEN_WIDTH)/(float)HALF_SCREEN_HEIGHT;
	pixeltogradinit();
	return;
}
#define HUNK_PLATFORM_SPRITES	4
void
addspritetoplatform( sprite_t *sprite, platform_t *platform )
{
	if(platform->numsprites >= platform->allocatedsprites)
	{
		platform->allocatedsprites += HUNK_PLATFORM_SPRITES;
		platform->sprites = (sprite_t**)realloc(platform->sprites, 
				platform->allocatedsprites*sizeof(sprite_t*));
	}
	platform->sprites[platform->numsprites++] = sprite;
}

/* addsprite
 * 
 * designates a sprite to be rendered by assigning platforms to it,
 * and setting its parameters.
 *
 * surface defines whether the sprite is "attached" to the floor or ceiling
 * 	   it is used for determining the vertical position of the sprite only
 * vdist is the distance the sprite will be from the floor or ceiling
 */
sprite_t *
addsprite ( raycaster_t *r, vector2d_t *verts, float height, float vdist, 
		int surface, texture_t *texture )
{
	platform_t *currentplat;
	vector2d_t direction,pos;
	sprite_t *sprite;
	intersection_t currentint;
	float width,dist,highestfloor=0.0f,lowestceil=0.0f;
	int first=1,i;
	
	currentplat = pickplatform(r,&verts[0]);

	sprite = (sprite_t*)malloc(sizeof(sprite_t));

	vectorsubtract(&verts[1],&verts[0],&direction);
	width = vectorlength(&direction);
	vectorscale(&direction,1.0f/width,&direction);
	
	vectorcopy(&sprite->line,&direction);
	vectorrot90(&sprite->line,&sprite->normal);
	vectorcopy(&pos,&verts[0]);

	dist = 0.0f;
	while(0<1)
	{
		edgeintersect(r,currentplat,&direction,&pos,dist,&currentint,NULL);
		for(i=0;i<currentplat->numsprites;i++)
		{
			if(currentplat->sprites[i] == sprite)
				break;
		}	
		
		if(i==currentplat->numsprites)
		{
			addspritetoplatform(sprite,currentplat);
		}
		if(currentplat == &r->level.infplatform)
		{
			fprintf(stderr,"Sprite is on inf platform\n");
			break;
		}
		if(currentint.distance > width)
		{
			/* The sprite ends in this platform */
			break;
		}
		if(surface == SURFACE_FLOOR && 
			(first || currentplat->floorheight > highestfloor) )
		{
			highestfloor = currentplat->floorheight;
			first = 0;
		} else if(surface == SURFACE_CEILING && 
			(first || currentplat->ceilheight < lowestceil) )
		{
			lowestceil = currentplat->ceilheight;
			first = 0;
		}
		dist = currentint.distance;
		currentplat = currentint.platform;
		vectorcopy(&pos,&currentint.pos);
	}
	if(surface == SURFACE_FLOOR)
	{
		sprite->heights[0] = highestfloor + vdist;
		sprite->heights[1] = highestfloor + vdist + height;
	} else if(surface == SURFACE_CEILING)
	{
		sprite->heights[1] = lowestceil - vdist;
		sprite->heights[0] = lowestceil - vdist - height;
	} else /* surface == SURFACE NONE */
	{
		/* just take the height value directly */
		sprite->heights[0] = vdist;
		sprite->heights[1] = vdist + height;
	}
	memcpy(sprite->verts,verts,2*sizeof(vector2d_t));
	sprite->texture = texture;
	sprite->renderflag = 0;
	return sprite;
}

void
initraycaster( raycaster_t *r, char *level )
{
	printf("loading level...\n");
	
	memset(r,0,sizeof(r));
	memset(&r->level,0,sizeof(level_t));
	
	if(!startsdl(r))
		return;
	if(!loadlevel(r,level))
		return;

	initvariables(r);
}

void
cleanup ( raycaster_t *r )
{
	texture_t *t,*next;
	level_t *l=&r->level;
	int i;
	for(i=0;i<l->numplatforms;i++)
	{
		free(l->platforms[i].edges);
		free(l->platforms[i].sprites);
	}
	free(l->platforms);
	free(l->infplatform.edges);
	free(l->infplatform.sprites);
	for(i=0;i<l->numverts;i++)
		free(l->verts[i].edges);
	free(l->verts);
	free(l->edges);

	for(next=r->level.texturelist;(t=next);)
	{
		next = t->next;
		freetexture(t);
		free(t);
	}
	
	SDL_Quit();
}

/* remove all sprites from all platforms -
 * they are re-added each frame
 */
void
clearsprites ( raycaster_t *r )
{
	int i;
	platform_t *p;

	for(i=0;i<r->level.numplatforms;i++)
	{
		p = &r->level.platforms[i];
		p->numsprites = 0;
	}
}
	      

void
renderloop( raycaster_t *r, world_t *w )
{
	int done;
	
	done = 0;
	while(!done)
	{
		handleevents(r,w,&done);
		setupworld (w);
		drawscreen(r);
		clearsprites(r);
		perframe(r,w);
	}
}

int
main ( int argc, char **argv )
{
	char *level;
	raycaster_t r;
	world_t w;

	printf("Copyright Notice: This program is licensed under the GNU General Public License\nSee COPYING for details\n\n");

	if(argc >= 2)
		level = argv[1];
	else
		level = DEFAULT_LEVEL;
	initraycaster(&r,level);
	initworld(&w,&r);
	if(!addentity(&w, "type=spawn\\coords=384 384\\angle=0"))
		return 0;
	if(!addentity(&w, "type=monster\\coords=300 300\\angle=90"))
		return 0;
	
	renderloop(&r,&w);
	
	cleanup(&r);
	freeworld(&w);
	return 0;
}

