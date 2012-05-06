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

#ifndef _WORLD_H_
#define _WORLD_H_

#include "raycaster.h"

#define HUNK_ENTITIES	16

#define DEGREESTORADS(a)	((a)*0.01745f)
#define RADSTODEGREES(a)	((a)*57.295f)

typedef enum
{
	ENTITYTYPE_PLAYER,
	ENTITYTYPE_SPAWN,
	ENTITYTYPE_PLASMA,
	ENTITYTYPE_STATIC,
	ENTITYTYPE_MONSTER
} entitytype_t;

struct entity_s;
struct world_s;

typedef struct world_s
{
	int allocatedentities;
	int numentities;
	struct 	entity_s *entities;
	struct entity_s *playerentity;
	int time;	/* ms */
	
	raycaster_t *raycaster;
} world_t;

typedef struct entity_s
{
	vector2d_t pos,vel,acc;
	float vpos,vvel;
	vector2d_t angle;
	entitytype_t type;

	texture_t *texture;
	int follow;
	
	texture_t **frames;

	int onground,keys;
	void (*physics)(raycaster_t *r,world_t *w,struct entity_s *e,int dt);
	void (*think)(world_t *w,struct entity_s *e);
	int nextthink;
	
	platform_t *currentplatform;

	/* monster */
	int shoottime;
	vector2d_t wishdir;
} entity_t;

typedef struct entitystring_s
{
	char name[64];
	entitytype_t type;
	int (*spawn)(world_t *world, entity_t *ent,char *strings);
	void (*free)(world_t *world, entity_t *ent);
} entitystring_t;

extern entitystring_t entitylookup[];

void setupworld ( world_t *world );
void initworld ( world_t *world, raycaster_t *r );
void freeworld ( world_t *world );
int addentity ( world_t *world, char *string );

#endif

