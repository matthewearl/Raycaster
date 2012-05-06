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
#include "raycaster.h"
#include "physics.h"
#include "vector.h"
#include "world.h"

#define TRIG_ANGLES		512
#define TRIG_ANGLES_OVER_4	128

float
sinlookup[TRIG_ANGLES];

void
inittriglookup( void )
{
	int i;
	for(i=0;i<TRIG_ANGLES;i++)
		sinlookup[i] = sinf((float)i*2.0f*M_PI/(float)TRIG_ANGLES);
}

float
quicksin ( float f )
{
	return sinlookup[(int)f & (TRIG_ANGLES-1)];
}

float
quickcos ( float f )
{
	return sinlookup[(((int)f)-TRIG_ANGLES_OVER_4)
				& (TRIG_ANGLES-1)];
}

#define STEP_HEIGHT	64.0f
#define PUSH_EXTRA	0.5f
void
moveplayer ( raycaster_t *r, entity_t *p, vector2d_t *origin,
		vector2d_t *dir, float dist )
{
	intersection_t *in;
	vector2d_t temp,origviewpos;
	platform_t *prevplat=p->currentplatform,*prevprevplat=NULL;;
	int headclip;
	float pushextra;
	
	r->numintersections = 0;
	in=edgeintersect(r,p->currentplatform,dir,origin,0.0f,NULL,NULL);
	while(0<1)
	{
		if(!in)
			return;
		if(dist <= in->distance)
			break;
		prevprevplat=prevplat;
		prevplat=in->platform;
		if(in->platform == &r->level.infplatform)
			break;
		
		if(p->vpos + VIEW_HEIGHT >= prevplat->ceilheight)
			break;
		if(prevplat->ceilheight - prevplat->floorheight < VIEW_HEIGHT)
			break;
		
		in=edgeintersect(r,in->platform,dir,&in->pos,in->distance,NULL,NULL);
	}
	vectorcopy(&origviewpos,&p->pos);
	vectorscale(dir,dist,&temp);
	vectoradd(&temp,&p->pos,&p->pos);
	if(!prevprevplat)
		return;
	headclip = (p->vpos + VIEW_HEIGHT >= prevplat->ceilheight);
	if(!headclip && prevplat->floorheight < p->vpos)
	{
		p->onground = 0;
		p->vvel = 0.0f;
		p->currentplatform = prevplat;
		return;
	}
	headclip = (prevplat->floorheight + VIEW_HEIGHT >= prevplat->ceilheight);
	if(!headclip && prevplat->floorheight <= p->vpos + STEP_HEIGHT)
	{
		p->currentplatform = prevplat;
		p->vpos = prevplat->floorheight;
		return;
	}
	
	if(in->edge->rightplat == p->currentplatform)
	{
		pushextra = -PUSH_EXTRA;
	} else
	{
		pushextra = PUSH_EXTRA;
	}
	
	vectorscale(&in->edge->normal,
			pushextra+dotproduct(dir,&in->edge->normal),&temp);
	vectorsubtract(dir,&temp,dir);
	dist = vectorlength ( dir );
	vectorscale( dir, 1.0f/dist, dir );
	moveplayer ( r, p, origin, dir, dist );
	
/*	vectorcopy(&p->pos,&origviewpos);*/
}

void
dogravity ( entity_t *p, int dt )
{
	float dtf = 0.001f*(float)dt;
	if(p->onground)
		return;
	p->vvel -= GRAVITY*dtf;
	p->vpos += p->vvel*dtf;
	if(p->vpos <= p->currentplatform->floorheight)
	{
		p->vpos = p->currentplatform->floorheight;
		p->onground = 1;
		p->vvel = 0.0f;
	}
}

void
physics_monster ( raycaster_t *r, world_t *w, entity_t *e, int dt )
{
	if(e->wishdir.x == 0.0f && e->wishdir.y == 0.0f)
		return;
	
	moveplayer ( r, e, &e->pos, &e->wishdir, ((float)dt)*0.001f*MONSTER_RUNSPEED);
	dogravity(e,dt);
}

void
physics_player ( raycaster_t *r, world_t *w, entity_t *p, int dt )
{
	vector2d_t forward,right,temp;
	float forwardf=0.0f,rightf=0.0f,dist,dtf,turn=0.0f;
	
	if(p->keys == 0 && p->onground && r->mousespeed.x == 0.0f)
		return;
	
	dtf = (float)dt*0.001f;
	
	if(p->keys & KEY_TRIGHT)
		turn += TURN_SPEED;

	if(p->keys & KEY_TLEFT)
		turn -= TURN_SPEED;
	
	turn -= r->mousespeed.x * MOUSE_SENS;
	
	vectorrotangle(&p->angle, turn*dtf, &p->angle);
	
	vectorcopy(&forward,&p->angle);
	vectorrot90(&forward,&right);
	
	if(p->keys & KEY_FORWARD)
		forwardf += 1.0f;

	if(p->keys & KEY_BACK)
		forwardf -= 1.0f;
	
	if(p->keys & KEY_RIGHT)
		rightf += 1.0f;

	if(p->keys & KEY_LEFT)
		rightf -= 1.0f;
	
	if(forwardf != 0.0f || rightf != 0.0f)
	{
		dist = RUN_SPEED*dtf;
		rightf *= dtf;
		forwardf *= dtf;
		
		if(forwardf != 0.0f && rightf != 0.0f)
		{
			forwardf*=0.707107f;	/* 1/sqrt(2) */
			rightf*=0.707107f;
		}
		
		vectorscale(&forward,forwardf,&temp);
		vectorscale(&right,rightf,&right);
		vectoradd(&temp,&right,&temp);
		
		moveplayer(r,p,&p->pos,&temp,dist);
	}
	dogravity(p,dt);
}

void
dophysics ( raycaster_t *r, world_t *w, int dt )
{
	int i;
	entity_t *e;

	for(i=0;i<w->numentities;i++)
	{
		e = &w->entities[i];
		if(!e->physics)
			continue;
		e->physics(r,w,e,dt);
	}
}

