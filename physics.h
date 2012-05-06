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

#ifndef _PHYSICS_H_
#define _PHYSICS_H_

#include "raycaster.h"
#include "world.h"

#define KEY_FORWARD	1
#define KEY_RIGHT	2
#define KEY_LEFT	4
#define KEY_BACK	8
#define KEY_TRIGHT	16
#define KEY_TLEFT	32

#define RUN_SPEED		20000.0f	/* units per second */
#define TURN_SPEED		-3.0f	/* radians per second */
#define GRAVITY			2048.0f	/* units per second^2 */
#define MOUSE_SENS		0.01f	/* radians per pixel */

#define MONSTER_RUNSPEED	100.0f	/* units per second */

void dophysics ( raycaster_t *r, world_t *w, int dt );
void physics_player ( raycaster_t *r, world_t *w, entity_t *e, int dt );
void physics_monster ( raycaster_t *r, world_t *w, entity_t *e, int dt );
#endif

