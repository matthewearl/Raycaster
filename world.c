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

/* The "world" is the section of the program which determimes
 * where all the entities are. At the end it sets up the 
 * raycaster so that it renders the scene properly
 */
#include <stdlib.h>
#include <string.h>
#include "raycaster.h"
#include "world.h"

entity_t *allocentity ( world_t *world )
{
	entity_t *newent;
	
	if(world->numentities == world->allocatedentities)
	{
		world->allocatedentities += HUNK_ENTITIES;
		world->entities = (entity_t*)realloc(world->entities,
				sizeof(entity_t)*world->allocatedentities);
	}
	newent = &world->entities[world->numentities++];
	memset(newent,0,sizeof(entity_t));

	return newent;
}

void
setfollowangle ( world_t *w, entity_t *e, vector2d_t *normal )
{
/*	vectorsubtract( &w->playerentity->pos, &e->pos, normal );
	vectornormalise(normal,normal);*/
	vectorcopy( normal, &w->playerentity->angle );
}

int
spawnplayer ( world_t *world )
{
	int i;
	entity_t *playerent,*e=NULL;

	/* find a spawn point */
	for(i=0;i<world->numentities;i++)
	{
		e = &world->entities[i];
		if(e->type==ENTITYTYPE_SPAWN)
			break;
	}
	if(i==world->numentities)
	{
		fprintf(stderr,"Could not find spawn point\n");
		return 0;
	}

	world->playerentity = playerent = allocentity ( world );
	vectorcopy(&playerent->pos, &e->pos);
	vectorcopy(&playerent->angle, &e->angle);

	playerent->type = ENTITYTYPE_PLAYER;
	playerent->vvel = 0.0f;
	playerent->onground = 1;
	playerent->keys = 0;
	playerent->physics = physics_player;
	playerent->currentplatform = pickplatform ( world->raycaster, 
						&playerent->pos);
	if(playerent->currentplatform == &world->raycaster->level.infplatform)
		printf("Warning: Spawn point on infplat\n");
	playerent->vpos = playerent->currentplatform->floorheight;
	return 1;
}	

void
setupworld ( world_t *world )
{
	int i;
	entity_t *e;
	vector2d_t angle,dir,verts[2];
	
	world->time = SDL_GetTicks();
	
	if(!world->playerentity)
	{
		if(!spawnplayer( world ))
			return;
	}
	for(i=0;i<world->numentities;i++)
	{
		e = &world->entities[i];
		
		if(e->think && world->time > e->nextthink)
		{
			void (*think)(world_t *w, struct entity_s *e);
			think = e->think;
			e->think = NULL;
			think(world,e);
		}
		
		/* don't need to add transparent stuff to the world */
		if(!e->texture)
			continue;
		if(e->follow)
			setfollowangle(world,e,&angle);
		else
			vectorcopy(&angle,&e->angle);

		vectorrot90( &angle, &dir );
		vectorscale( &dir, e->texture->width/2, &dir );
		vectorsubtract( &e->pos, &dir, &verts[0]);
		vectoradd( &e->pos, &dir, &verts[1]);
		
		addsprite(world->raycaster,verts,e->texture->height,e->vpos,
				SURFACE_NONE,e->texture);
	}

	/* copy over player view pos to raycaster */
	world->raycaster->currentplatform = world->playerentity->currentplatform;
	vectorcopy(&world->raycaster->viewpos,&world->playerentity->pos);
	vectorcopy(&world->raycaster->viewdir,&world->playerentity->angle);
	world->raycaster->eyelevel = world->playerentity->vpos+VIEW_HEIGHT;
}

int
findvalueforkey ( char *strings, char *key, char *value, int maxlen )
{
	char *pos,*next,*end,*eq;
	int keylen;

	keylen = strlen(key);
	for(pos=strings;;)
	{
		end = next = strchr(pos,'\\');
		if(!end)
			end = strings+strlen(strings);
		eq = strchr(pos,'=');
		if(eq && eq < end && keylen == eq-pos && 
			maxlen > end-eq &&
			!strncasecmp(pos,key,keylen))
		{
			memcpy(value,eq+1,end-eq-1);
			value[end-eq-1] = '\0';
			return 1;
		}	
		if(!next)
			break;
		pos = next+1;
	}
	return 0;
}

enum
{
	MONSTERFRAME_STAND,
	MONSTERFRAME_WALK1,
	MONSTERFRAME_WALK2,
	MONSTERFRAME_AIM,
	MONSTERFRAME_FIRE,
	MONSTERFRAME_MAX
};

char *monsterframelookup[] = 
		{
			"monster_stand.tga",
			"monster_walk1.tga",
			"monster_walk2.tga",
			"monster_aim.tga",
			"monster_fire.tga"
		};

/* how quickly the monster is able to think
 */
#define MONSTER_WIT	100	/* ms */
#define MONSTER_AIMTIME		200	/* ms */
#define MONSTER_RELOADTIME	1000	/* ms */

#define MONSTER_SHOOTRANGE	300.0f	/* units */
#define MONSTER_MUZZLEHEIGHT	64.0f	/* units */
#define MONSTER_FIRETIME	50 /* ms */

void monster_think ( world_t *world, entity_t *ent );

void
monster_aimpose ( world_t *world, entity_t *ent )
{
	ent->texture = ent->frames[MONSTERFRAME_AIM];
	ent->think = monster_think;
	ent->nextthink = world->time + MONSTER_WIT;
}

/* root of the monster's thinking - make a decision about
 * what to do
 */
void
monster_think ( world_t *world, entity_t *ent )
{
	vector2d_t dir;
	float dist;

	ent->wishdir.x = ent->wishdir.y = 0.0f;
	
	ent->think = monster_think;
	ent->nextthink = world->time + MONSTER_WIT;
	
	if(!world->playerentity)
	{
		ent->texture = ent->frames[MONSTERFRAME_STAND];
		ent->shoottime = -1;
		return;
	}
	if(!pointcanseepoint(world->raycaster,
			&world->playerentity->pos,world->playerentity->vpos+VIEW_HEIGHT,
			&ent->pos,ent->vpos+MONSTER_MUZZLEHEIGHT))
	{
		ent->texture = ent->frames[MONSTERFRAME_STAND];
		ent->nextthink = world->time + MONSTER_WIT*5;	/* monster is not very alert in the absence of
					   the player */
		ent->shoottime = -1;
		return;
	}
	vectorsubtract(&world->playerentity->pos,&ent->pos,&dir);
	dist = vectorlength(&dir);
	vectorscale(&dir,1.0f/dist,&dir);

	if(dist > MONSTER_SHOOTRANGE)
	{
		if((world->time/276)%2)
		{
			ent->texture = ent->frames[MONSTERFRAME_WALK1];
		} else
		{
			ent->texture = ent->frames[MONSTERFRAME_WALK2];
		}
		vectorcopy(&ent->wishdir,&dir);
		ent->shoottime = -1;		/* if we back off and re-approach
						 * the monster still has to take aim
						 */
		return;
	} else if(ent->shoottime > 0 && world->time > ent->shoottime )
	{
		ent->texture = ent->frames[MONSTERFRAME_FIRE];
		ent->shoottime = world->time + MONSTER_RELOADTIME;
		ent->think = monster_aimpose;				/* return to aim pose */
		ent->nextthink = world->time + MONSTER_FIRETIME;
		return;
	} else
	{
		if(ent->shoottime < 0)
		{
			ent->texture = ent->frames[MONSTERFRAME_AIM];
			ent->shoottime = world->time + MONSTER_AIMTIME;
		}
	}
}

int
spawn_monster ( world_t *world, entity_t *ent, char *strings )
{
	int i;
	
	ent->frames = (texture_t**)malloc(sizeof(texture_t*)*MONSTERFRAME_MAX);
	ent->follow = 1;
	
	for(i=0;i<MONSTERFRAME_MAX;i++)
	{
		ent->frames[i] = texturefrompath(world->raycaster,
				monsterframelookup[i]);
	}
	ent->texture = ent->frames[MONSTERFRAME_STAND];

	ent->think = monster_think;
	ent->nextthink = world->time;
	ent->physics = physics_monster;
	
	ent->currentplatform = pickplatform ( world->raycaster, 
						&ent->pos);
	ent->vpos = ent->currentplatform->floorheight;
	
	return 1;
}

int
spawn_static ( world_t *world, entity_t *ent, char *strings )
{
	char buffer[64];

	if(!findvalueforkey(strings,"follow",buffer,sizeof(buffer)))
		return 0;
	ent->follow = atoi(buffer);
	if(!findvalueforkey(strings,"texture",buffer,sizeof(buffer)))
		return 0;
	ent->texture = texturefrompath(world->raycaster,buffer);
	if(!ent->texture)
		return 0;

	ent->currentplatform = pickplatform ( world->raycaster, 
						&ent->pos);
	ent->vpos = ent->currentplatform->floorheight;
	
	return 1;
}

/* Only entities which can be added from the map editor are in this lookup
 */
entitystring_t entitylookup[] = 
	{
		{ "spawn", ENTITYTYPE_SPAWN, NULL, NULL },
		{ "static", ENTITYTYPE_STATIC, spawn_static, NULL  },
		{ "monster", ENTITYTYPE_MONSTER, spawn_monster, NULL  }
	};

/* Adds an entity to the world based on the strings
 * defined in the level editor.
 */
int
addentity ( world_t *world, char *strings )
{
	int i,lim;
	entitystring_t *es;
	char type[64],coords[64],anglestr[64];
	entity_t *newent;
	float angle;

	newent = allocentity(world);
	
	if(!findvalueforkey(strings, "type",type,sizeof(type)))
	{
		fprintf(stderr,"No classname in entity strings\n");
		return 0;
	}
	if(!findvalueforkey(strings, "coords",coords,sizeof(coords)))
	{
		fprintf(stderr,"No coords in entity strings\n");
		return 0;
	}
	if(!findvalueforkey(strings, "angle",anglestr,sizeof(anglestr)))
	{
		fprintf(stderr,"No angle in entity strings\n");
		return 0;
	}
	
	if(sscanf(coords,"%f %f",&newent->pos.x,&newent->pos.y) != 2)
	{
		fprintf(stderr,"Malformed coords in entity strings\n");
		return 0;
	}
	if(sscanf(anglestr,"%f",&angle) != 1)
	{
		fprintf(stderr,"Malformed angle in entity strings\n");
		return 0;
	}
	
	angletovector(DEGREESTORADS(angle),&newent->angle);
	
	lim = sizeof(entitylookup)/sizeof(entitystring_t);
	for(i=0;i<lim;i++)
	{
		es = &entitylookup[i];
		if(strcasecmp(es->name,type))
		{
			continue;
		}
		newent->type = es->type;
		if(es->spawn)
			return es->spawn(world,newent,strings);
		return 1;
	}
	fprintf(stderr,"Entity type \"%s\" not found\n",type);
	return 0;
}

void
initworld ( world_t *world, raycaster_t *r )
{
	world->time = SDL_GetTicks();
	world->raycaster = r;
	world->allocatedentities = 0;
	world->numentities = 0;
	world->entities = NULL;
	world->playerentity = NULL;
	return;
	
}

void
freeworld ( world_t *world )
{
	int i,j,lim;
	entity_t *e;
	entitystring_t *es;
	
	for(i=0;i<world->numentities;i++)
	{
		e = &world->entities[i];
		lim = sizeof(entitylookup)/sizeof(entitystring_t);
		for(j=0;j<lim;j++)
		{	
			es = &entitylookup[i];
			
			if(es->type != e->type)
				continue;
			if(es->free)
				es->free(world,e);
			break;
		}
	}
	free(world->entities);
}

