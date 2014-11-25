/*
MONSTERS.CPP

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

Tuesday, November 10, 1992 1:10:20 PM

Friday, May 27, 1994 11:21:07 AM
	split into MONSTERS.C, PROJECTILES.C and EFFECTS.C; unified active_monster and monster array.
Friday, September 30, 1994 5:48:25 PM (Jason)
	started adding comments again.  damage_monsters_in_radius() is less forgiving in z now.
Monday, December 5, 1994 9:07:37 PM  (Jason)
	rebellion environment function (all _clients hate all _pfhor).
Wednesday, February 1, 1995 2:29:01 AM  (Jason')
	kill_sounds; invisible monsters don't move
Wednesday, June 14, 1995 10:14:24 AM  (Jason)
	rewrite for marathon2 (halfway done).
Monday, July 10, 1995 11:49:06 AM  (Jason)
	rewrite for marathon2 done.  my bobs won't listen to your fucking whining.

Jan 30, 2000 (Loren Petrich):
	Added some typecasts
	Removed some "static" declarations that conflict with "extern"

Feb 3, 2000 (Loren Petrich):
	Treating Jjaro goo like sewage

Feb. 4, 2000 (Loren Petrich):
	Changed halt() to assert(false) for better debugging

Feb 6, 2000 (Loren Petrich):
	Added access to size of monster-definition structure

Feb 12, 2000 (Loren Petrich):
	Suppressed an exposed "dprintf" as an unnecessary interrupt.

Feb 16, 2000 (Loren Petrich):
	Added a check on the polygon index after a line-transparency check;
	this is in case there is no polygon on the other side.

Feb 17, 2000 (Loren Petrich):
	Fixed stuff near GUESS_HYPOTENUSE() to be long-distance-friendly

Feb 19, 2000 (Loren Petrich):
	Added growable lists of indices of objects to be checked for collisions

Feb 24, 2000 (Loren Petrich):
	Suppressed some asserts about monster speeds

Apr 27, 2000 (Loren Petrich):
	Added some behavior in the case of a monster both floating and flying
	to handle the map "Aqualung" correctly.

May 29, 2000 (Loren Petirch):
	Fixed side effect of fixing keyframe-never-zero bug:
	if the keyframe is zero, then a sequence never triggers shrapnel damage.
	Thus, Hunters die a soft death more harmlessly.

Jun 11, 2000 (Loren Petrich):
	Pegging health and oxygen to maximum values when damaged;
	takes into account negative damage from healing projectiles.

Jul 1, 2000 (Loren Petrich):
	Inlined the accessors

Aug 30, 2000 (Loren Petrich):
	Added stuff for unpacking and packing
	
Oct 13, 2000 (Loren Petrich)
	Converted the intersected-objects list into a Standard Template Library vector

Oct 26, 2000 (Mark Levin)
	Revealed a few functions needed by Pfhortran

Jan 12, 2003 (Loren Petrich)
	Added controllable damage kicks
*/

#include <string.h>
#include <limits.h>

#include "cseries.h"
#include "map.h"
#include "render.h"
#include "interface.h"
#include "FilmProfile.h"
#include "flood_map.h"
#include "effects.h"
#include "monsters.h"
#include "projectiles.h"
#include "player.h"
#include "platforms.h"
#include "scenery.h"
#include "SoundManager.h"
#include "fades.h"
#include "items.h"
#include "media.h"
#include "Packing.h"
#include "lua_script.h"
#include "Logging.h"

//need set_platform_state for _ds_set_platform_state
#include "platforms.h"

/*
//explosive deaths should cause damage during their key frame
*/

/* ---------- sounds */

/* ---------- constants */

#define OBSTRUCTION_DEACTIVATION_MASK 0x7

#define EVASIVE_MANOUVER_DISTANCE WORLD_ONE_HALF

#define MONSTER_EXTERNAL_DECELERATION (WORLD_ONE/200)
#define MONSTER_MINIMUM_EXTERNAL_VELOCITY (10*MONSTER_EXTERNAL_DECELERATION)
#define MONSTER_MAXIMUM_EXTERNAL_VELOCITY (TICKS_PER_SECOND*MONSTER_EXTERNAL_DECELERATION)

/* the height below which we don't bother to float up a ledge (we just run right over it) */
#define MINIMUM_FLOATING_HEIGHT WORLD_ONE_FOURTH

#define MINIMUM_ACTIVATION_SEPARATION TICKS_PER_SECOND

/* when looking for things under or at this light intensity the monster must use his dark visual range */
#define LOW_LIGHT_INTENSITY 0

/* maximum area we will search out to find a new target */
#define MAXIMUM_TARGET_SEARCH_AREA (7*WORLD_ONE*WORLD_ONE)

#define MONSTER_PLATFORM_BUFFER_DISTANCE (WORLD_ONE/8)

#define GLUE_TRIGGER_ACTIVATION_RANGE (8*WORLD_ONE)
#define MONSTER_ALERT_ACTIVATION_RANGE (5*WORLD_ONE)

#define MONSTER_PATHFINDING_OBSTRUCTION_COST (2*WORLD_ONE*WORLD_ONE)
#define MONSTER_PATHFINDING_PLATFORM_COST (4*WORLD_ONE*WORLD_ONE)
#define MINIMUM_MONSTER_PATHFINDING_POLYGON_AREA (WORLD_ONE)

#define TERMINAL_VERTICAL_MONSTER_VELOCITY (WORLD_ONE/5)

#define MINIMUM_DYING_EXTERNAL_VELOCITY (WORLD_ONE/8)

#define CIVILIANS_KILLED_BY_PLAYER_THRESHHOLD 3
#define CIVILIANS_KILLED_DECREMENT_MASK 0x1ff

#define	foreach_monster(index_variable, monster_data_variable)	\
	for(index_variable = 0, monster_data_variable = monsters; \
	index_variable < MAXIMUM_MONSTERS_PER_MAP; ++index_variable,++monster_data_variable)

enum /* monster attitudes, extracted from enemies and friends bitfields by get_monster_attitude() */
{
	_neutral,
	_friendly,
	_hostile
};

enum /* returned by find_obstructing_terrain_feature() */
{
	_standing_on_sniper_ledge,
	_entering_platform_polygon,
	_leaving_platform_polygon,
	_flying_or_floating_transition
};

#define MINIMUM_SNIPER_ELEVATION WORLD_ONE_HALF

/* ---------- structures */

struct monster_pathfinding_data
{
	struct monsterDefinition *definition;
	Monster *monster;
	
	bool cross_zone_boundaries;
};

// How much external velocity is imparted by some damage?
struct damage_kick_definition
{
	int16 base_value;
	float delta_vitality_multiplier;
	bool is_also_vertical;

	// if non-zero, will enable vertical_component if
	// delta_vitality is greater than threshold
	int16 vertical_threshold;

	// whether monsters die a hard death, or in flames
	int16 death_action;
};

/* ---------- definitions */

// LP: implements commented-out damage-kick code
struct damage_kick_definition damage_kick_definitions[NUMBER_OF_DAMAGE_TYPES] = 
{
	{0, 1, true, 0, _monster_is_dying_hard}, // _damage_explosion,
	{0, 3, true, 0, _monster_is_dying_soft}, // _damage_electrical_staff,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_projectile,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_absorbed,
	{0, 1, false, 0, _monster_is_dying_flaming}, // _damage_flame,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_hound_claws,
	{0, 1, false, 0, _monster_is_dying_flaming}, // _damage_alien_projectile,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_hulk_slap,
	{0, 3, true, 0, _monster_is_dying_soft}, // _damage_compiler_bolt,
	{0, 0, false, 100, _monster_is_dying_soft}, // _damage_fusion_bolt,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_hunter_bolt,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_fist,
	{250, 0, false, 0, _monster_is_dying_soft}, // _damage_teleporter,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_defender,
	{0, 3, true, 0, _monster_is_dying_soft}, // _damage_yeti_claws,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_yeti_projectile,
	{0, 1, false, 0, _monster_is_dying_hard}, // _damage_crushing,
	{0, 1, false, 0, _monster_is_dying_flaming}, // _damage_lava,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_suffocation,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_goo,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_energy_drain,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_oxygen_drain,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_hummer_bolt,
	{0, 0, true, 0, _monster_is_dying_soft} // _damage_shotgun_projectile,
};

/* ---------- globals */

/* import monster definition constants, structures and globals */
#include "monster_definitions.h"
/*	for monster instance definitions	*/
#include "monsters_ext.h"

// LP addition: growable list of intersected objects
static vector<int16> IntersectedObjects;

/* ---------- private prototypes */

static void monster_needs_path(int16 monster_index, bool immediately);
static void generate_new_path_for_monster(int16 monster_index);
void advance_monster_path(int16 monster_index);

static int16 get_monster_attitude(int16 monster_index, int16 target_index);
void change_monster_target(int16 monster_index, int16 target_index);
static bool switch_target_check(int16 monster_index, int16 attacker_index, int16 delta_vitality);
static bool clear_line_of_sight(int16 viewer_index, int16 target_index, bool full_circle);

static void handle_moving_or_stationary_monster(int16 monster_index);
static void execute_monster_attack(int16 monster_index);
static void kill_monster(int16 monster_index);
static bool translate_monster(int16 monster_index, world_distance distance);
static bool try_monster_attack(int16 monster_index);
			
int32 monster_pathfinding_cost_function(int16 source_polygon_index, int16 line_index,
	int16 destination_polygon_index, void *data);
int32 monster_m1_trigger_flood_proc(int16 source_polygon_index, int16 line_index,
                                    int16 destination_polygon_index, void *data);

void set_monster_action(int16 monster_index, int16 action);
void set_monster_mode(int16 monster_index, int16 new_mode, int16 target_index);

static int16 find_obstructing_terrain_feature(int16 monster_index, int16 *feature_index, int16 *relevant_polygon_index);

static int16 position_monster_projectile(int16 aggressor_index, int16 target_index, struct attack_definition *attack,
	world_point3d *origin, world_point3d *destination, world_point3d *_vector, angle theta);

static void update_monster_vertical_physics_model(int16 monster_index);
static void update_monster_physics_model(int16 monster_index);

static int32 monster_activation_flood_proc(int16 source_polygon_index, int16 line_index,
	int16 destination_polygon_index, void *data);

static bool attempt_evasive_manouvers(int16 monster_index);

static int16 nearest_goal_polygon_index(int16 polygon_index);
static int32 nearest_goal_cost_function(int16 source_polygon_index, int16 line_index,
	int16 destination_polygon_index, void *unused);

static void cause_shrapnel_damage(int16 monster_index);

static inline int16 get_monster_drop_mask();
static inline int16 get_major_demotion_mask();
static inline int16 get_minor_promotion_mask();

// For external use
monsterDefinition *get_monster_definition_external(const int16 type);

/* ---------- code */

static bool mTYPE_IS_ENEMY(monsterDefinition *definition, int16 type)
{
    if (static_world->environment_flags & _environment_rebellion_m1)
    {
        if (definition->_class & _class_client_m1)
            return (get_monster_definition(type)->_class & _class_pfhor_m1);
        else if (definition->_class & _class_pfhor_m1)
        {
            if (get_monster_definition(type)->_class & _class_client_m1)
                return true;
        }
    }
    return TYPE_IS_ENEMY(definition, type);
}


Monster *get_monster_data(const ix monster_index)
{
	Monster *monster = GetMemberWithBounds(monsters, monster_index, MAXIMUM_MONSTERS_PER_MAP);
	
	vassert(monster, csprintf(temporary, "monster index #%d is out of range", (int)monster_index));
	vassert(SLOT_IS_USED(monster), csprintf(temporary, "monster index #%d (%p) is unused", (int)monster_index, monster));
	
	return monster;
}

struct Monster& Monster::Get(const ix index)
{
	assert(index < MAXIMUM_MONSTERS_PER_MAP);
	assert(index >= 0);
	return MonsterList[index];
}

monsterDefinition *get_monster_definition(const int16 type)
{
	monsterDefinition* definition = GetMemberWithBounds(monster_definitions, type, NUMBER_OF_MONSTER_TYPES);
	assert(definition);
	
	return definition;
}

//a non-inlined version for external use
monsterDefinition *get_monster_definition_external(const int16 type)
{
	return get_monster_definition(type);
}

static inline int16 get_monster_drop_mask()
{
	switch( dynamic_world->game_information.difficulty_level )
	{
		/* drop every fourth monster */
		case _wuss_level: 
			return 3; 
		/* drop every eighth monster */
		case _easy_level: 
			return 7; 
	}
	/* otherwise, drop no monsters */
	return NONE;
}

static inline int16 get_major_demotion_mask()
{
	switch(dynamic_world->game_information.difficulty_level)
	{
		/* demote every other major */
		case _wuss_level: 
			return 1; 
		/* demote every fourth major */
		case _easy_level: 
			return 3; 
	}
	/* otherwise, demote no monsters */
	return NONE;
}

static inline int16 get_minor_promotion_mask()
{
	switch (dynamic_world->game_information.difficulty_level)
	{
		/* promote every other minor */
		case _major_damage_level: 
			return 1; 
		 /* promote every minor */
		case _total_carnage_level: 
			return 0; 
	}
	/* otherwise, promote no monsters */ 
	return NONE;
}

/* returns new monster index if successful, NONE otherwise */
int16 new_monster(struct object_location *location, int16 monster_type)
{
	monsterDefinition *definition = get_monster_definition(monster_type);
	const auto original_monster_type = monster_type;
	Monster *monster;
	const auto drop_mask = get_monster_drop_mask();
	ix monster_index = NONE;
	int16 flags = _monster_has_never_been_activated;
	

	
	if( !definition->testFlags(_monster_cannot_be_dropped | _monster_is_alien) && !isNONE( drop_mask ) && 
		!(++dynamic_world->new_monster_vanishing_cookie & drop_mask))
	{
		if(static_world->mission_flags & _mission_rescue_m1 && !isNONE(monster_index) && definition->_class & _class_human_civilian_m1) 
			dynamic_world->current_civilian_count++;
		return NONE;
	}

	/* check to see if we should promote or demote this monster based on difficulty level */
	if(definition->testFlags(_monster_major))
	{
		int16 demote_mask = get_major_demotion_mask();
		if(!isNONE( demote_mask ) && !(++dynamic_world->new_monster_mangler_cookie & demote_mask)) 
		{
			definition = get_monster_definition(--monster_type);
			flags |= _monster_was_demoted;
		}
	}
	else if(definition->testFlags(_monster_minor))
	{
		int16 promote_mask = get_minor_promotion_mask();

		if( !isNONE( promote_mask ) && !(++dynamic_world->new_monster_mangler_cookie & promote_mask)) 
		{
			definition= get_monster_definition(++monster_type);
			flags |= _monster_was_promoted;
		}
	}
	
	auto f = [=, &monster_index, &definition, &flags, &location, monster_type](Monster &m)
	{
		if(	!m.slotIsFree()	)
			return 0;
			
		auto object_index = new_map_object(location, BUILD_DESCRIPTOR(definition->collection, definition->stationary_shape));
		
		/*	return nonzero == break	*/
		if( isNONE(object_index) )
			return (monster_index = NONE, -1);
			
		Object*obj = get_object_data( object_index );
		
		obj_set(m, 0x80);
		if( location->isBlind() )
			flags |= _monster_is_blind;
		if( location->isDeaf() )
			flags |= _monster_is_deaf;
		if( location->doesFloat() )
			flags |= _monster_teleports_out_when_deactivated; //?
			
		/* initialize the monster_data structure; we don't touch most of the fields here
			because the monster is initially inactive (and they will be initialized when the
			monster is activated) */
		m.type = monster_type;
		m.activation_bias = DECODE_ACTIVATION_BIAS(location->flags);
		m.vitality = NONE; /* if a monster is activated with vitality==NONE, it will be properly initialized */
		m.object_index = object_index;
		m.flags = flags;
		m.exflags = 0;
		m.instance_definition_index = NONE;
		m.goal_polygon_index = m.activation_bias == _activate_on_goal ?
			nearest_goal_polygon_index(location->polygon_index) : NONE;
		m.sound_polygon_index = obj->polygon;
		m.sound_location = obj->location;
		m.markSlotAsUsed();
		
		monster_instances::new_definition_instance(definition, monster_index);
		
		
		/* initialize the monster's object */
		if(definition->testFlags(_monster_is_invisible))
			obj->transfer_mode = _xfer_invisibility;
			
		if(definition->testFlags(_monster_is_subtly_invisible))
			obj->transfer_mode = _xfer_subtle_invisibility;
			
		if(definition->testFlags(_monster_is_enlarged))
			obj->flags |= _object_is_enlarged;
			
		if(definition->testFlags(_monster_is_tiny))
			obj->flags |= _object_is_tiny;
			
		SET_OBJECT_SOLIDITY(obj, true);
		SET_OBJECT_OWNER(obj, _object_is_monster);
		obj->permutation = monster_index;
		obj->sound_pitch = definition->sound_pitch;

		/* make sure the object frequency stuff keeps track of how many monsters are
			on the map */
		object_was_just_added(_object_is_monster, original_monster_type);
		return 1;	//break
	};
	for(monster_index = 0; monster_index < MAXIMUM_MONSTERS_PER_MAP; ++monster_index)
		if(f(MonsterList[monster_index]))
			break;
			
	
	if(monster_index == MAXIMUM_MONSTERS_PER_MAP) 
		monster_index = NONE;
		
	/* keep track of how many civilians we drop on this level */
	if(static_world->mission_flags & _mission_rescue_m1 && !isNONE(monster_index) && definition->_class & _class_human_civilian_m1) 
		dynamic_world->current_civilian_count++;
	return monster_index;
}


/* assumes ¶t==1 tick */
void move_monsters()
{
	Monster *monster;
	bool monster_got_time = false;
	bool monster_built_path = dynamic_world->tick_count & 3 != 0;
	int monster_index;
	
	foreach_monster(monster_index, monster)
	{
		if( !monster->slotIsUsed() || monster->isPlayer() )
			continue;
			
		Object *object = monster->getObject();
		if(	monster->isActive()	)
		{
			if( object->isInvisible() )
				goto SkipBecauseObjectIsInvisible;
			// AlexJLS patch: effect of dangerous polygons
			cause_polygon_damage(object->polygon, monster_index);

			/* clear the recovering from hit flag, mark the monster as not idle */	
			SET_MONSTER_IDLE_STATUS(monster, false);

			update_monster_vertical_physics_model(monster_index);

			/* 
				update our object's animation unless we're suffering from an external velocity
				or we're airborne (if we're a flying or floating monster, ignore both of these 
			*/
			if(!monster->hasVelocity() || (film_profile.ketchup_fix && monster->isAttacking() ) ||
				((!monster->isBeingHit() || !monster->external_velocity) && monster->canFlyOrFloat()))
				animate_object(monster->object_index);

			/* give this monster time, if we can and he needs it */
			if (!monster_got_time && monster_index > dynamic_world->last_monster_index_to_get_time && !monster->isDying())
			{
				switch (monster->mode)
				{
					case _monster_unlocked:
						/* 
							if this monster is unlocked and we haven't already given a monster time,
							call find_closest_appropriate_target() 
						*/
						change_monster_target(monster_index, find_closest_appropriate_target(monster_index, false));
						monster_got_time = true;
						break;
					
					case _monster_lost_lock:
					case _monster_losing_lock:
						/* 
							if this monster has lost or is losing lock and we haven't already given a monster
							time, check to see if his target has become visible again 
							*/
						if (clear_line_of_sight(monster_index, monster->getTarget(), false))
							change_monster_target( monster_index, monster->getTarget() );
						monster_got_time = true;
						break;
				}
				
				/* if we gave this guy time, make room for the next guy */
				if(monster_got_time) 
					dynamic_world->last_monster_index_to_get_time = monster_index;
			}

			/* if this monster needs a path, generate one (unless we've already generated a
				path this frame in which case we'll wait until next frame, UNLESS the monster
				has no path in which case it needs one regardless) */
			if (MONSTER_NEEDS_PATH(monster) && !monster->isDying() && !monster->isAttacking() &&
				((!monster_built_path && monster_index > dynamic_world->last_monster_index_to_build_path) || !monster->hasPath()))
			{
				generate_new_path_for_monster( monster_index );
				if( !monster_built_path )
				{
					monster_built_path = true;
					dynamic_world->last_monster_index_to_build_path = monster_index;
				}
			}
			
			/* it's possible that we couldn't get where we wanted to go, or that we arrived there
				and deactivated ourselves; if this happens we don't want to continue processing
				the monster as if it were active */
			if(monster->isActive())
			{
				/* move the monster; check to see if we can attack; resolve modes ending; etc. */
				switch (monster->action)
				{
					case _monster_is_waiting_to_attack_again:
					case _monster_is_stationary:
					case _monster_is_moving:
						handle_moving_or_stationary_monster( monster_index );
						break;
					
					case _monster_is_attacking_close:
					case _monster_is_attacking_far:
							if( monster->testAnimationFlags( _obj_keyframe_started ) )
								execute_monster_attack(monster_index);
							if( monster->testAnimationFlags( _obj_last_frame_animated ) )
							{
								monster->attack_repetitions--;
								if( monster->attack_repetitions < 0 || !try_monster_attack(monster_index) )
								{
									/* after an attack has been initiated successfully we need to return to
									_monster_is_moving action, kill our path and ask for a new one
									(because we're pointed in the wrong direction now) */
									set_monster_action(monster_index,
									(monster->attack_repetitions < 0 && monster->testDefinitionFlags(_monster_waits_with_clear_shot) && MONSTER_IS_LOCKED(monster)) ?
									_monster_is_waiting_to_attack_again : _monster_is_moving);
									monster_needs_path( monster_index, true );
									monster->ticks_since_attack = 0;
								}
							}
						break;
					
					case _monster_is_teleporting_in:
						if( !monster->testAnimationFlags( _obj_last_frame_animated ) )
							break;
						monster->setAction( _monster_is_moving );
						set_monster_action(monster_index, _monster_is_moving);
						change_monster_target(monster_index, find_closest_appropriate_target(monster_index, false));
						break;
					case _monster_is_teleporting_out:
						if( !monster->testAnimationFlags(_obj_keyframe_started) )
							break;
						
						monster->setAction( _monster_is_dying_soft ); // to prevent aggressors from relocking
						monster_died( monster_index );
						teleport_object_out( monster->getObjectIndex() );
						remove_map_object( monster->getObjectIndex()  );
						L_Invalidate_Monster( monster_index );
						monster->markSlotAsFree();
						break;
					
					case _monster_is_being_hit:
						update_monster_physics_model( monster_index );
						if( !monster->testAnimationFlags( _obj_last_frame_animated ) )
							break;
						monster_needs_path( monster_index, true );
						set_monster_action( monster_index, _monster_is_moving );
						monster->external_velocity = 0;
						break;
					
					case _monster_is_dying_soft:
					case _monster_is_dying_hard:
					case _monster_is_dying_flaming:
						update_monster_physics_model(monster_index);
						if(monster->testDefinitionFlags(_monster_has_delayed_hard_death) && monster->isDyingSoft() )
						{
							int16 contrail = monster->getDefinition()->contrail_effect;
							if ( !monster->external_velocity && object->location.z == monster->desired_height )
								set_monster_action(monster_index, _monster_is_dying_hard);
								
							else if( !isNONE( contrail ) ) 
								new_effect(&object->location, object->polygon, contrail, object->facing);
							break;
						}

						// LP change: if keyframe is zero, then a monster should not produce shrapnel damage.
						// This fixes a side effect of a fix of the keyframe-never-zero bug,
						// which is that Hunters injure those nearby when they die a soft death.
						if( monster->testAnimationFlags( _obj_keyframe_started ) && (!film_profile.keyframe_fix || GET_SEQUENCE_FRAME(object->sequence) != 0))
							cause_shrapnel_damage( monster_index );
							
						if( monster->testAnimationFlags( _obj_last_frame_animated ) ) 
							kill_monster( monster_index );
						break;
					
					default:
						assert(false);
				}
			}
SkipBecauseObjectIsInvisible:
			;
		}
		/* all inactive monsters get time to scan for targets */
		else if( !monster_got_time && !monster->isBlind() && monster_index > dynamic_world->last_monster_index_to_get_time)
		{
				change_monster_target( monster_index, 
							find_closest_appropriate_target( monster_index, false )
							);
				if( monster->hasValidTarget() ) 
					activate_nearby_monsters( monster->getTarget(), monster_index, _pass_one_zone_border, MONSTER_ALERT_ACTIVATION_RANGE );
				
				monster_got_time = true;
				dynamic_world->last_monster_index_to_get_time = monster_index;
		}
	
	
		/* 
			WARNING: a large number of unusual things could have happened here, including the monster
			being dead, his slot being free, and his object having been removed from the map; in other
			words, it's probably not a good idea to do any postprocessing here 
		*/
	}
	
	/* 
		either there are no unlocked monsters or dynamic_world->last_monster_index_to_get_time is higher than
		all of them (so we reset it to zero) ... same for paths 
	*/
	if( !monster_got_time ) 
		dynamic_world->last_monster_index_to_get_time = NONE;
	if( !monster_built_path ) 
		dynamic_world->last_monster_index_to_build_path = NONE;

	if( !dynamic_world->civilians_killed_by_players )
		return;
	
	unsigned mask = 0;
	
	switch (dynamic_world->game_information.difficulty_level)
	{
		case _wuss_level:
			mask = 0x7F; 
			break;
		case _easy_level: 
			mask = 0xFF; 
			break;
		case _normal_level: 
			mask = 0x1FF; 
			break;
		case _major_damage_level: 
			mask = 0x3FF; 
			break;
		case _total_carnage_level: 
			mask = 0x7FF; 
			break;
	}
	
	if( dynamic_world->tick_count & mask )
		return;
	--dynamic_world->civilians_killed_by_players;
}


/* 
	when a monster dies, all monsters locked on it need to find something better to do; this
	function should be called before the given target is expunged from the monster list but
	after it is marked as dying 
*/
void monster_died(int16 target_index)
{
	Monster &deadMonster = Monster::Get(target_index);
	/* 
		orphan this monster's projectiles if they don't belong to a player (player's monster
		slots are always valid and we want to correctly attribute damage and kills that ocurr
		after a player dies) 
	*/
	if( !deadMonster.isPlayer() ) 
		orphan_projectiles( target_index );
	
	/* active monsters need extant paths deleted and should be marked as unlocked */
	if( deadMonster.isActive() )
	{
		set_monster_mode(target_index, _monster_unlocked, NONE);
		deadMonster.removePath();
	}
	
	/*	best place to do this	*/
	if( deadMonster.hasInstanceDefinition() )
		monster_instances::delete_definition_instance( deadMonster.instance_definition_index) ;
	
	/* anyone locked on this monster needs a clue */
	//foreach_monster( monster_index, monster )
	for(ix i = 0; i < MAXIMUM_MONSTERS_PER_MAP; ++i )
	{
		Monster& monster = Monster::Get(i);
		if( !monster.slotIsUsed() || !monster.isActive() || !monster.isTarget( target_index ))
			continue;
			
		auto closest_target_index = find_closest_appropriate_target(i, true);
		monster.setTarget( NONE );
		monster_needs_path(i, false);
		
		play_object_sound( monster.getObjectIndex(), monster.getDefinition()->kill_sound );
		
		if(	closest_target_index != NONE	)
			change_monster_target( i, closest_target_index );
			
		else 
		{
			if( monster.isWaitingToAttackAgain() ) 
				set_monster_action( i, _monster_is_moving );
			set_monster_mode( i, _monster_unlocked, NONE );
		}
	}

}

void initialize_monsters()
{
	/* initialize our globals to be the same thing on all machines */
	dynamic_world->civilians_killed_by_players = 0;
	dynamic_world->last_monster_index_to_get_time = NONE;
	dynamic_world->last_monster_index_to_build_path = NONE;
	dynamic_world->new_monster_mangler_cookie = global_random();
	dynamic_world->new_monster_vanishing_cookie = global_random();
}

/* call this when a new level is loaded from disk so the monsters can cope with their new world */
void initialize_monsters_for_new_level()
{
	Monster *monster;
	int16 monster_index;

	/* 
		when a level is loaded after being saved all of an active monster's data is still intact,
		but it's path no longer exists.  this function resets all monsters so that they recalculate
		their paths, first thing. 
	*/
	foreach_monster( monster_index, monster )
	{
		if( !monster->slotIsUsed() || !monster->isActive() )
			continue;
		SET_MONSTER_NEEDS_PATH_STATUS( monster, true );
		monster->setPath( NONE );
	}
}

static void load_sound(int16 sound_index)
{
	SoundManager::instance()->LoadSound( sound_index );
}

void load_monster_sounds(int16 monster_type)
{
	if( isNONE( monster_type ) )
		return;
	
	monsterDefinition *definition = get_monster_definition( monster_type );
	process_collection_sounds( definition->collection, load_sound );
	
	load_projectile_sounds( definition->ranged_attack.type );
	load_projectile_sounds( definition->melee_attack.type );
	
	SoundManager::instance()->LoadSounds( &definition->activation_sound, 8 );
}

void mark_monster_collections(int16 monster_type, bool loading)
{
	if( isNONE(monster_type) )
		return;
	monsterDefinition *definition = get_monster_definition( monster_type );

	/* mark the monster collection */
	mark_collection( definition->collection, loading );
	
	/* mark the monster's projectile's collection */
	mark_projectile_collections( definition->ranged_attack.type, loading );
	mark_projectile_collections( definition->melee_attack.type, loading );
}

enum
{
	MAXIMUM_NEED_TARGET_INDEXES = 32
};

void activate_nearby_monsters(
	int16 target_index, /* activate with lock on this target (or NONE for lock-less activation) */
	int16 caller_index, /* start the flood from here */
	int16 flags,
	int32 max_range)
{
	Monster *caller = get_monster_data(caller_index);
    	int32 max_cost = INT32_MAX;
    if( static_world->environment_flags & _environment_activation_ranges )
    {
		if( max_range > 0 )
			max_cost = SQUARE( max_range );
		else if( flags & _activate_glue_monsters )
			max_cost = GLUE_TRIGGER_ACTIVATION_RANGE * GLUE_TRIGGER_ACTIVATION_RANGE;
    }

	if (dynamic_world->tick_count-caller->ticks_since_last_activation > MINIMUM_ACTIVATION_SEPARATION ||
		(flags&_activation_cannot_be_avoided))
	{
		auto polygon_index = get_object_data( caller->getObjectIndex() )->polygon;
		int16 need_target_indexes[ MAXIMUM_NEED_TARGET_INDEXES ];
		int16 need_target_count = 0;
		int32 flood_flags = flags;
		
		/* flood out from the target monster's polygon, searching through the object lists of all
			polygons we encounter */
		polygon_index = flood_map(polygon_index, max_cost, monster_activation_flood_proc, _flagged_breadth_first, &flood_flags);
		while(	!isNONE(polygon_index)	)
		{
			int16 object_index;
			Object *object;
	
			/* loop through all objects in this polygon looking for _hostile inactive or unlocked monsters */
			for (object_index = get_polygon_data(polygon_index)->first_object; object_index != NONE; object_index= object->next_object)
			{
				object = get_object_data(object_index);
				if(GET_OBJECT_OWNER(object) == _object_is_monster && (!object->isInvisible() || flags & _activate_invisible_monsters) )
				{
					auto aggressor_index = object->permutation;
					Monster *aggressor = get_monster_data(aggressor_index);
					// deaf monsters are only deaf to players which have always been hostile, so:
					//   bobs are deaf to friendly players but not hostile ones
					//   monsters are deaf to all players
					// deaf monsters ignore friendly monsters activating on other friendly monsters but
					//   non-deaf ones DO NOT

					/* don't activate players or ourselves, and only activate monsters on glue polygons
						if they have previously been activated or we've been explicitly told to */
					if (!aggressor->isPlayer() && caller_index != aggressor_index && target_index != aggressor_index &&
						(!(flood_flags & _passed_zone_border) || !aggressor->hasNeverBeenActivated() ) &&
						( flood_flags & _activate_deaf_monsters || !aggressor->isDeaf() && !aggressor->isLocked() ))
					{
						bool monster_was_active = aggressor->isActive();
						/* activate the monster if he's inactive */
						if( !monster_was_active )
							activate_monster(aggressor_index);

						
						if(monster_was_active || !(flags & _use_activation_biases) ||
							(aggressor->activation_bias != _activate_on_goal && aggressor->activation_bias!=_activate_randomly))
						{
							if(monster_was_active || aggressor->activation_bias != _activate_on_nearest_hostile)
							{
								/* if we have valid target and this monster thinks that target is hostile, lock on */
								if(get_monster_attitude(aggressor_index, target_index) == _hostile)
									switch_target_check(aggressor_index, target_index, 0);
								/* but hey, if the target isn't hostile, maybe the caller is ...
										(mostly for the automated defenses and the civilians on the ship) */
								else if( get_monster_attitude(aggressor_index, caller_index) == _hostile )
									switch_target_check(aggressor_index, caller_index, 0);
							}
							// must defer find_closest_appropriate_target; pathfinding is not reentrant
							else if( need_target_count < MAXIMUM_NEED_TARGET_INDEXES )
								need_target_indexes[ need_target_count++ ] = aggressor_index;
							
						}
					}
				}
			}
			
			polygon_index = flood_map(NONE, max_cost, monster_activation_flood_proc, _flagged_breadth_first, &flood_flags);
		}

		// deferred find_closest_appropriate_target() calls
		while( --need_target_count >= 0 )
		{
			change_monster_target(
				need_target_indexes[ need_target_count ],
				find_closest_appropriate_target( need_target_indexes[ need_target_count ], true )
				);	
		}
		caller->ticks_since_last_activation = dynamic_world->tick_count;
	}
}

static int32 monster_activation_flood_proc(int16 source_polygon_index, int16 line_index, int16 destination_polygon_index, void *data)
{
	int32 *flags = (int32 *)data;
	Polygon *destination_polygon = get_polygon_data(destination_polygon_index);
	Polygon *source_polygon = get_polygon_data(source_polygon_index);
	line_data *line = get_line_data(line_index);
	bool obey_glue = static_world->environment_flags & _environment_glue_m1 != 0;
	bool limit_activation= (static_world->environment_flags&_environment_activation_ranges);
	auto cost = limit_activation ? source_polygon->area : 1;


	if (destination_polygon->type == _polygon_is_zone_border)
	{
		if (((*flags)&_pass_one_zone_border) && !((*flags)&_passed_zone_border))
			*flags |= _passed_zone_border;
		else
			cost= -1; // can't pass this zone border
	}
	else if ((destination_polygon->type==_polygon_is_superglue) &&
	         ((*flags)&_cannot_pass_superglue) &&
	         obey_glue)
		cost= -1;
	else if ((destination_polygon->type==_polygon_is_glue) &&
	         !((*flags)&_activate_glue_monsters) &&
	         obey_glue)
		cost= -1;
	else if ((destination_polygon->type==_polygon_is_monster_impassable) &&
	         limit_activation)
		cost= -1;
	else if ((destination_polygon->type==_polygon_is_platform) &&
	         (destination_polygon->floor_height==destination_polygon->ceiling_height) &&
	         !((*flags)&_pass_solid_lines) &&
	         limit_activation)
		cost= -1;

	if (!((*flags)&_pass_solid_lines) && LINE_IS_SOLID(line)) 
		cost = -1;

	if(cost > 0 && limit_activation)
	{
		auto delta_height = destination_polygon->floor_height - source_polygon->floor_height;
		cost += SQUARE(delta_height);
	}
	
	return cost;
}

const int16 LIVE_ALIEN_THRESHHOLD = 8;

static std::vector<bool> monster_must_be_exterminated(NUMBER_OF_MONSTER_TYPES, false);

bool live_aliens_on_map()
{
	bool found_alien_which_must_be_killed = false;
	Monster *monster;
	auto live_alien_count = 0;
	auto threshhold = LIVE_ALIEN_THRESHHOLD;
	ix monster_index;
	
	foreach_monster(monster_index, monster)
	{
		if( !monster->slotIsUsed() )
			continue;
			
		found_alien_which_must_be_killed = monster->mustBeExterminated();
		
		if( found_alien_which_must_be_killed )
			break;
		
		if( monster->testDefinitionFlags( _monster_is_alien ) || 
		(static_world->environment_flags & _environment_rebellion && !monster->isPlayer() ))
			live_alien_count++;
	}
	
	if (static_world->environment_flags & _environment_rebellion) 
		threshhold = 0;
	
	return live_alien_count <= threshhold ? found_alien_which_must_be_killed : true;
}

/* activate the given monster (initially unlocked) */
void activate_monster(int16 monster_index)
{
	Monster *monster 		= get_monster_data( monster_index );
	Object *object 		= get_object_data( monster->getObjectIndex() );
	monsterDefinition *definition 	= monster->getDefinition();

	assert( !monster->isActive() );
	assert( !monster->isPlayer() );

	if( object->isInvisible() )
	{
		Polygon *polygon = get_polygon_data(object->polygon);
		
		if( !isNONE( polygon->media_index ) )
		{
			media_data *media = get_media_data(polygon->media_index);
			
			if (media && media->height > object->location.z + definition->height 
				&& !definition->testFlags( _monster_can_teleport_under_media ) )
				return;
		}
	}
	
	CLEAR_MONSTER_RECOVERING_FROM_HIT(monster);
	
	SET_MONSTER_IDLE_STATUS(monster, false);
	monster->setActiveStatus( true );
	SET_MONSTER_BERSERK_STATUS(monster, false);
	SET_MONSTER_HAS_BEEN_ACTIVATED(monster);
	
	monster->flags &= ~(_monster_is_blind|_monster_is_deaf);

	monster->setPath( NONE );
	/* 
		we used to set monster->target_index here, but it is invalid when mode == _monster_unlocked 
	*/
	monster->setMode( _monster_unlocked );
	monster->setTarget( NONE );

	if (!definition->attack_frequency) // IP: Avoid division by zero
		definition->attack_frequency++;	 

	monster->ticks_since_attack = definition->testFlags(_monster_attacks_immediately) 
			?	definition->attack_frequency :	global_random(definition->attack_frequency);
	
	monster->desired_height = object->location.z; /* best guess */
	monster->random_desired_height = INT16_MAX; // to be out of range and recalculated
	monster->external_velocity = monster->vertical_velocity = 0;	
	monster->ticks_since_last_activation = 0;
	
	/* 
		if vitality is NONE (-1) initialize it from the monster_definition, respecting
		the difficulty level if necessary 
	*/
	if( isNONE( monster->getVitality() ) )
	{
		auto vitality = definition->vitality;
		
		if( definition->testFlags( _monster_is_alien ) )
		{
			switch( dynamic_world->game_information.difficulty_level )
			{
				case _wuss_level: 
					vitality -= vitality / 2; 
					break;
				case _easy_level: 
					vitality -= vitality / 4; 
					break;
				case _major_damage_level: 
					vitality += vitality / 4; 
					break;
				case _total_carnage_level: 
					vitality += vitality / 2; 
					break;
			}
		}
		
		monster->setVitality( vitality );
	}

	set_monster_action( monster_index, _monster_is_stationary );
	monster_needs_path( monster_index, true );

	if( object->isInvisible() )
		teleport_object_in( monster->getObjectIndex() );
	else if( definition->testFlags( _monster_makes_sound_when_activated ) )
		play_object_sound( monster->getObjectIndex(), definition->activation_sound );
	
	changed_polygon( object->polygon, object->polygon, NONE );
}

void deactivate_monster(int16 monster_index)
{
	Monster *monster = get_monster_data( monster_index );

	assert( monster->isActive() );

	if( monster->teleportsOutWhenDeactivated() ) 
		monster->vertical_velocity = monster->external_velocity = 0;

	if( monster->vertical_velocity || monster->external_velocity )
		return;
	
	if( monster->teleportsOutWhenDeactivated() && !monster->isTeleportingOut() )
	{
		set_monster_action( monster_index, _monster_is_teleporting_out );
		return;
	}

	/* assume stationary shape before deactivation */
	set_monster_action( monster_index, _monster_is_stationary );
	
	/* get rid of this monster's path if he has one */
	if( !isNONE( monster->getPath() ) )
		delete_path( monster->getPath() );

	monster->setActiveStatus( false );
}

/* 
	returns a list of object indexes of all monsters in or adjacent to the given polygon,
	up to maximum_object_count. 
*/
// LP change: called with growable list
bool possible_intersecting_monsters(vector<int16> *IntersectedObjectsPtr, unsigned maximum_object_count, int16 polygon_index, bool include_scenery)
{
	Polygon *polygon 	= get_polygon_data( polygon_index );
	int16 *neighbor_indexes = get_map_indexes( polygon->first_neighbor_index, polygon->neighbor_count );
	bool found_solid_object = false;

	// Skip this step if neighbor indexes were not found
	if (!neighbor_indexes) 
		return found_solid_object;

	for( ix i = 0; i < polygon->neighbor_count; ++i)
	{
		Polygon *neighboring_polygon = get_polygon_data( *neighbor_indexes++ );
		
		if (POLYGON_IS_DETACHED(neighboring_polygon))
			continue;
	
		auto object_index = neighboring_polygon->first_object;
		
		while( !isNONE(object_index) )
		{
			Object *object = get_object_data(object_index);
			bool solid_object = false;
			
			if (!OBJECT_IS_INVISIBLE(object))
			{
				switch (GET_OBJECT_OWNER(object))
				{
					case _object_is_monster:
					{
						Monster *monster = get_monster_data( object->permutation );
					
						if( !monster->isDying() && !monster->isTeleporting() )
							solid_object = true;
						
						break;
					}
					
					case _object_is_scenery:
						if (include_scenery && OBJECT_IS_SOLID(object)) 
							solid_object = true;
						break;
				}
				
				if (solid_object)
				{
					found_solid_object = true;
					
					// LP change:
					if (IntersectedObjectsPtr && IntersectedObjectsPtr->size() < maximum_object_count) /* do we have enough space to add it? */
					{
						ix j;
						
						/* only add this object_index if it's not already in the list */
						vector<int16>& IntersectedObjects = *IntersectedObjectsPtr;
						
						for( j = 0; j < IntersectedObjects.size() && IntersectedObjects[j] != object_index; ++j )
							;
							
						if( j == IntersectedObjects.size() )
							IntersectedObjects.push_back(object_index);
					}
				}
			}
			
			object_index = object->next_object;
		}
	
	}

	return found_solid_object;
}

/* when a target changes polygons, all monsters locked on it must recalculate their paths.
	target is an index into the monster list. */
void monster_moved(int16 target_index, int16 old_polygon_index)
{
	Monster *monster	= get_monster_data( target_index );
	Object *object	= get_object_data( monster->getObjectIndex() );
	int16 monster_index;
	
	/* cause lights to light, platforms to trigger, etc.; the player does this differently */
	if( !monster->isPlayer() )
		changed_polygon( old_polygon_index, object->polygon, NONE );
	else if ((static_world->environment_flags & _environment_glue_m1) &&
	         (get_polygon_data(object->polygon)->type == _polygon_is_glue_trigger))
	{
		activate_nearby_monsters(target_index, target_index,
			_pass_solid_lines|_activate_deaf_monsters|_activate_invisible_monsters|_use_activation_biases|_cannot_pass_superglue|_activate_glue_monsters);
	}

	foreach_monster(monster_index, monster)
	{
		/* 
			look for active monsters locked (or losing lock) on the given target_index 
		*/
		if( !monster->slotIsUsed() || !monster->hasValidTarget() || !monster->isTarget( target_index ) )
			continue;
		
		if( clear_line_of_sight( monster_index, target_index, true ) )
		{
			if( monster->isLosingLock() ) 
				set_monster_mode( monster_index, _monster_locked, monster->getTarget() );
		}
		else
		{
			monsterDefinition *definition = monster->getDefinition();
			auto intelligence = definition->intelligence;
			
			switch (dynamic_world->game_information.difficulty_level)
			{
				case _wuss_level: 
					intelligence /= 4; 
					break;
				case _easy_level: 
					intelligence /= 2; 
					break;
				case _major_damage_level: 
					intelligence *= 2; 
					break;
				case _total_carnage_level: 
					intelligence *= 4;
					break;
			}
			
			/* 
				we can't see our target: if this is first time, change from _monster_locked
				to _monster_losing_lock, if this isn't the first time and our target has
				switched polygons more times out of our sight than we have intelligence points,
				go to _lost_lock (which means we won't get any more new paths when our target
				switches polygons, but we won't clear our last one until we reach the end). 
			*/
			if( monster->isLocked() ) 
				monster->changes_until_lock_lost = 0;
				
			if( monster->isLosingLock() ) 
				monster->changes_until_lock_lost++;
				
			set_monster_mode(
				monster_index, 
				monster->changes_until_lock_lost >= definition->intelligence 
					? _monster_lost_lock 
					: _monster_losing_lock, 
				NONE);
		}
		
		/* if we're losing lock, don't recalculate our path (we're headed towards the target's
			last-known location) */
		if( !monster->isLosingLock() ) 
			monster_needs_path( monster_index, false );
	
	}
}

/* returns NONE or a monster_index that prevented us from moving */
int16 legal_player_move(int16 monster_index, world_point3d *new_location, world_distance *object_floor) /* must be set on entry */
{
	Monster *monster = get_monster_data(monster_index);
	Object *object = get_object_data(monster->object_index);
	world_point3d *old_location = &object->location;
	size_t monster_count;
	world_distance radius, height;
	int16 obstacle_index = NONE;

	get_monster_dimensions( monster_index, &radius, &height );	
	
	IntersectedObjects.clear();
	possible_intersecting_monsters(&IntersectedObjects, LOCAL_INTERSECTING_MONSTER_BUFFER_SIZE, object->polygon, true);
	monster_count = IntersectedObjects.size();
	for( ix i = 0; i < monster_count; ++i )
	{
		Object *obstacle = get_object_data(IntersectedObjects[i]);
		world_distance obstacle_radius, obstacle_height;
		
		switch (GET_OBJECT_OWNER(obstacle))
		{
			case _object_is_monster: 
				get_monster_dimensions(obstacle->permutation, &obstacle_radius, &obstacle_height); 
				break;
			case _object_is_scenery: 
				get_scenery_dimensions(obstacle->permutation, &obstacle_radius, &obstacle_height); 
				break;
			default:
				assert(false);
		}
		
		if ( monster->isObject( IntersectedObjects[ i ] ) ) /* no self-intersection */
			continue;
			
		world_point3d *obstacle_location = &obstacle->location;

		auto separation = radius + obstacle_radius;
		auto separation_squared = SQUARE( separation );

		auto new_dx = obstacle_location->x - new_location->x;
		auto new_dy = obstacle_location->y - new_location->y;
		auto new_distance_squared = SQUARE( new_dx ) + SQUARE( new_dy );
		
		if(new_distance_squared >= separation_squared)
			continue;
		
		auto old_dx = obstacle_location->x - old_location->x;
		auto old_dy = obstacle_location->y - old_location->y;
		auto old_distance_squared = SQUARE( old_dx ) + SQUARE( old_dy );

		if( old_distance_squared > new_distance_squared )
		{
			auto this_object_floor = obstacle_location->z + obstacle_height;
			
			/* 
				it's possible we don't intersect in z 
			*/
			if( new_location->z + height < obstacle_location->z )
				continue; 
			if( new_location->z > this_object_floor )
			{
				if( this_object_floor > *object_floor) 
					*object_floor = this_object_floor;
				continue;
			}
			obstacle_index = IntersectedObjects[ i ];
			break;
		}
	}
	return obstacle_index;
}

/* returns NONE or a monster_index that prevented us from moving */
int16 legal_monster_move(int16 monster_index,
	angle facing, /* could be different than object->facing for players and flying (heh heh) monsters */
	world_point3d *new_location)
{
	Monster *monster 	= get_monster_data(monster_index);
	Object *object 	= get_object_data( monster->getObjectIndex() );

	world_distance radius, height;
	int16 obstacle_index = NONE;

	get_monster_dimensions(monster_index, &radius, &height);	
	
	IntersectedObjects.clear();
	
	possible_intersecting_monsters( &IntersectedObjects, 
		LOCAL_INTERSECTING_MONSTER_BUFFER_SIZE, 
		object->polygon, 
		true);
		
	auto monster_count = IntersectedObjects.size();
	
	for( ix i = 0; i < monster_count; ++i )
	{
		Object *obstacle	= get_object_data( IntersectedObjects[i] );
		world_distance obstacle_radius, obstacle_height;
			
		switch( GET_OBJECT_OWNER( obstacle ) )
		{
			case _object_is_monster: 
				get_monster_dimensions( obstacle->permutation, &obstacle_radius, &obstacle_height ); 
				break;
			case _object_is_scenery: 
				get_scenery_dimensions( obstacle->permutation, &obstacle_radius, &obstacle_height ); 
				break;
			default:
				assert( false );
		}
			
		// LP change:
		if ( monster->isObject( IntersectedObjects[ i ] ) ) /* no self-intersection */
			continue;
		
		world_point3d *obstacle_location = &obstacle->location;
		
		if (obstacle_location->z < new_location->z + height && 
			obstacle_location->z + obstacle_height > new_location->z)
		{
			auto separation = radius + obstacle_radius;
			auto dx = obstacle_location->x - new_location->x;
			auto dy = obstacle_location->y - new_location->y;
			
			if (GET_OBJECT_OWNER(obstacle) != _object_is_scenery && obstacle->permutation > monster_index 
				&& !MONSTER_IS_PLAYER( get_monster_data( obstacle->permutation ) )) 
				separation = ( separation / 2 ) + ( separation / 4 );
				
			if (dx > -separation && dx < separation && dy > -separation && dy < separation)
			{
				/* 
					we intersect sloppily; get arctan to be sure 
				*/
				auto theta = NORMALIZE_ANGLE( arctangent(dx, dy) - facing );
				
				if (theta < EIGHTH_CIRCLE || theta > FULL_CIRCLE - EIGHTH_CIRCLE)
				{
					obstacle_index = IntersectedObjects[i];
					break;
				}
			}
		}
	
	}
	
	return obstacle_index;
}

void get_monster_dimensions(int16 monster_index, world_distance *radius, world_distance *height)
{
	Monster *monster 		= get_monster_data( monster_index );
	monsterDefinition *definition 	= monster->getDefinition();

	*radius = definition->radius;
	*height = definition->height;
}

void damage_monsters_in_radius(
	int16 primary_target_index,
	int16 aggressor_index,
	int16 aggressor_type,
	world_point3d *epicenter,
	int16 epicenter_polygon_index,
	world_distance radius,
	struct damage_definition *damage,
	int16 projectile_index)
{
	bool aggressor_is_live_player = false;

	(void) (primary_target_index);
	
	IntersectedObjects.clear();
	possible_intersecting_monsters(&IntersectedObjects, LOCAL_INTERSECTING_MONSTER_BUFFER_SIZE, epicenter_polygon_index, false);
	
	auto object_count = IntersectedObjects.size();
        Object *aggressor = nullptr;
        
	if( film_profile.infinity_tag_fix && !isNONE(aggressor_index) )
	{
		Monster* monster = get_monster_data(aggressor_index);
		if( monster->isPlayer() )
		{
			player_data* player = get_player_data( monster_index_to_player_index( aggressor_index ) );
			
			if (!PLAYER_IS_DEAD(player)) 
				aggressor_is_live_player = true;
		}
	}

	for( ix i = 0; i < object_count; ++i )
	{
		Object *object = get_object_data(IntersectedObjects[i]);
                if (film_profile.damage_aggressor_last_in_tag && 
		    GET_GAME_TYPE() == _game_of_tag && 
		    object->permutation == aggressor_index) 
		{
                        // damage the aggressor last, so tag suicides are handled correctly
                        aggressor = object;
                        continue;
                } 
                
                auto distance = distance2d((world_point2d*)epicenter, (world_point2d*)&object->location);
                world_distance monster_radius, monster_height;
                
                get_monster_dimensions(object->permutation, &monster_radius, &monster_height);

                /* make sure we intersect the monster's radius in the x,y-plane and that we intersect
                        his cylinder in z */
                if (distance >= radius + monster_radius)
                	continue;
                if (epicenter->z + radius - distance > object->location.z && 
                	epicenter->z - radius + distance < object->location.z + monster_height)
                {
                        if (!line_is_obstructed(epicenter_polygon_index, (world_point2d*)epicenter, object->polygon, (world_point2d*)&object->location))
                                damage_monster(object->permutation, aggressor_index, aggressor_type, epicenter, damage, projectile_index);
                }
	}

        // damage the aggressor
        if (film_profile.damage_aggressor_last_in_tag && aggressor != nullptr) 
	{
                auto distance = distance2d((world_point2d*)epicenter, (world_point2d*)&aggressor->location);
                world_distance monster_radius, monster_height;
                
                get_monster_dimensions(aggressor->permutation, &monster_radius, &monster_height);
                if( distance < radius + monster_radius )
                {
                        if(epicenter->z + radius-distance > aggressor->location.z && 
                        	epicenter->z - radius + distance < aggressor->location.z + monster_height)
                        {
                                if ( !line_is_obstructed( epicenter_polygon_index, (world_point2d*)epicenter, aggressor->polygon, (world_point2d*)&aggressor->location))
                                        damage_monster(aggressor->permutation, aggressor_index, aggressor_type, epicenter, damage, projectile_index);
			}
		}
	}

	// or, just make him it
	if (GET_GAME_TYPE() == _game_of_tag && aggressor_is_live_player)
	{
		Monster* monster = get_monster_data(aggressor_index);
		if( monster->isPlayer() )
		{
			auto player_index 	= monster_index_to_player_index(aggressor_index);
			player_data* player 	= get_player_data(player_index);
			
			// he blew himself up, so make sure he's it
			if (PLAYER_IS_DEAD(player))
				dynamic_world->game_player_index = player_index;
		}
	}
}

void damage_monster(int16 target_index, int16 aggressor_index, int16 aggressor_type, world_point3d *epicenter,
	struct damage_definition *damage, int16 projectile_index)
{
	Monster *monster 		= get_monster_data(target_index);
	monsterDefinition *definition 	= monster->getDefinition();
	Monster *aggressor_monster = aggressor_index != NONE ? get_monster_data(aggressor_index) : nullptr;

	auto delta_vitality = calculate_damage(damage);

	auto external_velocity = 0;
	bool vertical_component = false;

	if (!(definition->immunities & FLAG(damage->type)))
	{
		// double damage for weaknesses
		if( definition->weaknesses & FLAG(damage->type) ) 
			delta_vitality *= 2;
		
		// if this player was shot by a friendly, make him apologise
		if (aggressor_index!=NONE && get_monster_attitude(aggressor_index, target_index)==_friendly)
			play_object_sound(aggressor_monster->object_index, aggressor_monster->getDefinition()->apology_sound);
		
		if( monster->isPlayer() )
			damage_player(target_index, aggressor_index, aggressor_type, damage, projectile_index);
		else
		{
			player_data *aggressor_player = nullptr;
			
			/* only active monsters can take damage */
			if( !monster->isActive() ) 
				activate_monster(target_index);
			
			/* convert aggressor monster index to a player index, if possible, to record damage */
			if( !isNONE(aggressor_index) && aggressor_monster->isPlayer() )
			{
				aggressor_player = get_player_data(monster_index_to_player_index(aggressor_index));
				aggressor_player->monster_damage_given.damage += MAX(monster->vitality, delta_vitality);
				team_monster_damage_given[aggressor_player->team].damage += MAX(monster->vitality, delta_vitality);
			}
			

			// LP change: pegging to maximum value
			monster->vitality = MIN(int32(monster->vitality) - int32(delta_vitality), int32(INT16_MAX));
			L_Call_Monster_Damaged(target_index, aggressor_index, damage->type,  delta_vitality, projectile_index);
			
			if (monster->vitality > 0)
			{
				set_monster_action(target_index, _monster_is_being_hit);
				
				if(definition->testFlags(_monster_is_berserker) && monster->vitality < definition->vitality / 4 )
					SET_MONSTER_BERSERK_STATUS(monster, true);
					
				if( !isNONE(aggressor_index) )
					switch_target_check(target_index, aggressor_index, delta_vitality);
				
				// if a player shoots a monster who thinks the player is friendly; ask him what the fuck is up
				if (aggressor_player && get_monster_attitude(target_index, aggressor_index)==_friendly) 
					play_object_sound(monster->object_index, definition->friendly_fire_sound);
			}
			else
			{
				if ( !monster->isDying() )
				{
					int16 action;
					
					if ((damage_kick_definitions[damage->type].death_action == _monster_is_dying_flaming) && (definition->flags&_monster_can_die_in_flames))
						action = _monster_is_dying_flaming;
					else
					{
						if ((damage_kick_definitions[damage->type].death_action == _monster_is_dying_hard || ((FLAG(damage->type)&definition->weaknesses) && !(definition->flags & _monster_weaknesses_cause_soft_death)) ||
							definition->soft_dying_shape==UNONE) && definition->hard_dying_shape!=UNONE && !(definition->flags&_monster_has_delayed_hard_death))
							action = _monster_is_dying_hard;
						else
							action= _monster_is_dying_soft;
						if (definition->flags&_monster_has_delayed_hard_death) 
							monster->vertical_velocity = -1;
					}
					
					if (action==_monster_is_dying_flaming || (damage->type == _damage_crushing && (definition->flags & _monster_screams_when_crushed))) 
						play_object_sound(monster->object_index, definition->flaming_sound);
					set_monster_action(target_index, action);
					monster_died(target_index); /* orphan projectile, recalculate aggressor paths */
					
					if (aggressor_player)
					{
						aggressor_player->monster_damage_given.kills++;
						team_monster_damage_given[aggressor_player->team].kills++;
						
						if(definition->_class & _class_human_civilian) 
							dynamic_world->civilians_killed_by_players++;
					}

					if ((static_world->mission_flags & _mission_rescue_m1) && (definition->_class & _class_human_civilian_m1))
						dynamic_world->current_civilian_causalties++;
				}
				
				// Lua script hook
				auto aggressor_player_index = -1;
				if (aggressor_index!=NONE)
					if (MONSTER_IS_PLAYER(aggressor_monster))
						aggressor_player_index = monster_index_to_player_index(aggressor_index);
				L_Call_Monster_Killed (target_index, aggressor_player_index, projectile_index);
			}
		}
		

		if(damage->type >= 0 && damage->type < NUMBER_OF_DAMAGE_TYPES)
		{
			damage_kick_definition& kick_def = damage_kick_definitions[damage->type];
			
			external_velocity = (world_distance)(kick_def.base_value + kick_def.delta_vitality_multiplier*delta_vitality);
			vertical_component = kick_def.is_also_vertical;
			if (film_profile.use_vertical_kick_threshold && kick_def.vertical_threshold && delta_vitality > kick_def.vertical_threshold)
				vertical_component = true;
		}
	
		if( monster->isDying() && external_velocity < MINIMUM_DYING_EXTERNAL_VELOCITY ) 
			external_velocity = MINIMUM_DYING_EXTERNAL_VELOCITY;
			
		external_velocity = (external_velocity * definition->external_velocity_scale) >> FIXED_FRACTIONAL_BITS;
		
		if(external_velocity && epicenter)
		{
			Object *object = get_object_data(monster->object_index);
			auto dx = object->location.x - epicenter->x;
			auto dy = object->location.y - epicenter->y;
			auto dz = object->location.z + (definition->height / 2) - epicenter->z;
			auto direction = arctangent(dx, dy);
			auto radius = isqrt( SQUARE(dx) + SQUARE(dy) + SQUARE(dz) );
			
			auto vertical_velocity = vertical_component && radius 
						?  (external_velocity * dz) / radius  
						: 0;
	
			accelerate_monster(target_index, vertical_velocity, direction, external_velocity);
		}
	}
}

bool bump_monster(int16 aggressor_index, int16 monster_index)
{
	return switch_target_check(monster_index, aggressor_index, 0);
}


bool legal_polygon_height_change(int16 polygon_index, world_distance new_floor_height, world_distance new_ceiling_height,
	struct damage_definition *damage)
{
	auto new_polygon_height = new_ceiling_height - new_floor_height;
	Polygon *polygon 	= get_polygon_data(polygon_index);
	
	auto object_index 	= polygon->first_object;
	
	auto minimum_height 	= dead_player_minimum_polygon_height(polygon_index);
	bool legal_change 	= true;
	
	while( !isNONE(object_index) )
	{
		Object *object = get_object_data(object_index);
		
		if( GET_OBJECT_OWNER(object) == _object_is_monster && OBJECT_IS_VISIBLE(object))
		{
			world_distance radius, height;
			
			get_monster_dimensions( object->permutation, &radius, &height );
			if( height >= new_polygon_height )
			{
				if( damage )
				{
					damage_monster( object->permutation, NONE, NONE, nullptr, damage, NONE );
					play_object_sound( object_index, Sound_Crunched() );
				}
				legal_change = false;
			}
		}
		
		object_index = object->next_object;
	}
	
	return new_polygon_height < minimum_height ? false : legal_change;
}

/* 
	we've already checked and this monster is not obstructing the polygon from changing heights 
*/
void adjust_monster_for_polygon_height_change(int16 monster_index, int16 polygon_index, world_distance new_floor_height, 
			world_distance new_ceiling_height)
{
	Polygon *polygon = get_polygon_data( polygon_index );
	Monster *monster = get_monster_data( monster_index );
	world_distance radius, height;
	
	get_monster_dimensions(monster_index, &radius, &height);
	
	if( monster->isPlayer() )
	{
		adjust_player_for_polygon_height_change( monster_index, polygon_index, new_floor_height, new_ceiling_height);
		return;
	}
	Object *object = get_object_data( monster->getObjectIndex() );
	
	if( object->location.z == polygon->floor_height ) 
		object->location.z = new_floor_height;
}

void accelerate_monster(int16 monster_index, world_distance vertical_velocity, angle direction, world_distance velocity)
{
	Monster *monster = get_monster_data(monster_index);
	
	if( monster->isPlayer() )
	{
		accelerate_player(monster_index, vertical_velocity, direction, velocity);
		return;
	}
	monster->getObject()->facing = NORMALIZE_ANGLE( direction + HALF_CIRCLE );
	monster->external_velocity += velocity;
	monster->vertical_velocity += PIN(monster->vertical_velocity + vertical_velocity, 
		-TERMINAL_VERTICAL_MONSTER_VELOCITY, 
		TERMINAL_VERTICAL_MONSTER_VELOCITY);
}

int16 get_monster_impact_effect(int16 monster_index)
{
	Monster *monster 		= get_monster_data(monster_index);
	monsterDefinition* definition 	= monster->getDefinition();
	auto impact_effect_index 	= definition->impact_effect;
	
	if( monster->isPlayer() )
	{
		switch (monster->getObject()->transfer_mode)
		{
			case _xfer_static:
				impact_effect_index = NONE;
				break;
		}
	}
	
	return impact_effect_index;
}

int16 get_monster_melee_impact_effect(int16 monster_index)
{
	Monster *monster 		= get_monster_data(monster_index);
	monsterDefinition* definition 	= monster->getDefinition();
	return definition->melee_impact_effect;
}

/* ---------- private code */

static void cause_shrapnel_damage(int16 monster_index)
{
	Monster *monster		= get_monster_data(monster_index);
	Object *object		= get_object_data( monster->getObjectIndex() );
	monsterDefinition* definition	= monster->getDefinition();

	if( !isNONE( definition->shrapnel_radius ) )
		damage_monsters_in_radius(NONE, NONE, NONE, &object->location, object->polygon,
			definition->shrapnel_radius, &definition->shrapnel_damage, NONE);
}

static void update_monster_vertical_physics_model(int16 monster_index)
{
	Monster *monster 		= get_monster_data( monster_index );
	monsterDefinition* definition 	= monster->getDefinition();
	Object *object 		= get_object_data( monster->getObjectIndex() );
	Polygon *polygon 		= get_polygon_data( object->polygon );
	
	media_data *media = isNONE(polygon->media_index) ? nullptr : get_media_data(polygon->media_index);
	auto moving_flags = MONSTER_IS_DYING(monster) ? 0 : (definition->flags&(_monster_flys|_monster_floats));
	
	auto gravity = static_world->environment_flags & _environment_low_gravity ? 
			definition->gravity / 2 : definition->gravity;
			
	auto floor_height = polygon->floor_height;
	
	world_distance desired_height;
	
	auto old_height = object->location.z;
	bool above_ground, below_ground;

	if (media)
	{
		// flying and floating monsters treat media as the floor
		if (moving_flags && media->height > floor_height) 
			floor_height = media->height + WORLD_ONE / 16;
		
		// take damage if necessary
		if (media->height > object->location.z)
		{
			damage_definition *damage = get_media_damage( polygon->media_index, FIXED_ONE );
			
			if (damage) 
				damage_monster(monster_index, NONE, NONE, nullptr, damage, NONE);
		}
	}
	desired_height= (monster->desired_height==NONE||MONSTER_IS_DYING(monster)) ? polygon->floor_height : monster->desired_height;
	above_ground= object->location.z>desired_height;
	below_ground= object->location.z<desired_height;

	switch (moving_flags)
	{
		case 0:
			/* if we're above the floor, adjust vertical velocity */
			if (above_ground) 
				monster->vertical_velocity = 
					FLOOR( monster->vertical_velocity-gravity, -definition->terminal_velocity );
			if (below_ground) 
			{
				monster->vertical_velocity = 0;
				object->location.z = desired_height;
			}
			break;
		
		case _monster_flys:
			if (above_ground && !MONSTER_IS_ATTACKING(monster)) monster->vertical_velocity= FLOOR(monster->vertical_velocity-gravity, -definition->terminal_velocity);
			if (below_ground) monster->vertical_velocity= CEILING(monster->vertical_velocity+gravity, definition->terminal_velocity);
			break;

		case _monster_floats:
			if (above_ground && !MONSTER_IS_ATTACKING(monster)) monster->vertical_velocity= FLOOR(monster->vertical_velocity-gravity, -definition->terminal_velocity);
			if (below_ground) monster->vertical_velocity= CEILING(monster->vertical_velocity+gravity, definition->terminal_velocity);
			break;
		
		default:
			/* can't fly and float, beavis */
			// LP change: this stuff put in to handle the map "Aqualung" correctly
			if (above_ground && !MONSTER_IS_ATTACKING(monster)) monster->vertical_velocity= FLOOR(monster->vertical_velocity-gravity, -definition->terminal_velocity);
			if (below_ground) monster->vertical_velocity= CEILING(monster->vertical_velocity+gravity, definition->terminal_velocity);
			break;
	}
	
	/* add our vertical velocity to z */
	object->location.z= PIN(object->location.z+monster->vertical_velocity, polygon->floor_height, polygon->ceiling_height-definition->height);

	/* if we're under the floor moving down, put us on the floor and clear our velocity;
		if we're above the floor moving up, put us on the floor and clear our velocity if we were previously below ground */
	switch (moving_flags)
	{
		case 0:
		case _monster_floats:
			if (object->location.z<=desired_height && monster->vertical_velocity<0) monster->vertical_velocity= 0, object->location.z= desired_height;
			if (object->location.z>=desired_height && monster->vertical_velocity>0 && below_ground) monster->vertical_velocity= 0, object->location.z= desired_height;
			break;
		
		case _monster_flys:
		default: // LP: added this case to handle "Aqualung" correctly
			if (object->location.z<=desired_height && above_ground) monster->vertical_velocity>>= 1, object->location.z= desired_height;
			if (object->location.z>=desired_height && below_ground) monster->vertical_velocity>>= 1, object->location.z= desired_height;
			break;
	}

	/* reset desired height (flying and floating monsters often change this later) */
	if(moving_flags & _monster_flys)
	{
		/* we're flying!: if we have no target, take the middle ground; if we have a target aim
			for his midsection */
		if ( monster->hasValidTarget() )
		{
			Monster *target = get_monster_data(monster->getTarget());
			monsterDefinition* target_definition = target->getDefinition();
			
			monster->desired_height= get_object_data(target->object_index)->location.z + ((target_definition->height-definition->height)>>1) + definition->preferred_hover_height;
			monster->desired_height= PIN(monster->desired_height, floor_height+(definition->height>>2), polygon->ceiling_height-definition->height);
		}
		else
		{
			if (monster->random_desired_height<floor_height || monster->random_desired_height>polygon->ceiling_height)
			{
				auto delta = polygon->ceiling_height - floor_height-definition->height;
				
				monster->random_desired_height= floor_height + ( delta > 0  ? global_random() % delta : 0);
			}
			
			monster->desired_height = MONSTER_IS_DYING(monster) ? polygon->floor_height : monster->random_desired_height;
		}
	}
	else
		monster->desired_height = floor_height;

	monster->sound_location = object->location;
	monster->sound_polygon_index = object->polygon;
	monster->sound_location.z += definition->height - ( definition->height / 2 );

	if (!media)
		return;

	world_point3d location = object->location;
	int16 media_effect_type = NONE;
	
	location.z = media->height;
	
	if(old_height >= media->height && object->location.z < media->height)
		media_effect_type = _large_media_detonation_effect;

	if(old_height < media->height && object->location.z >= media->height)
		media_effect_type = _large_media_emergence_effect;
	
	if(media_effect_type != NONE)
	{
		int16 effect_type = NONE;
		
		get_media_detonation_effect(polygon->media_index, media_effect_type, &effect_type);
		new_effect(&location, object->polygon, effect_type, object->facing);
	}

}

static void update_monster_physics_model(int16 monster_index)
{
	Monster *monster = get_monster_data(monster_index);
	monsterDefinition* definition = monster->getDefinition();
	Object *object = get_object_data(monster->object_index);
	
	if(!monster->external_velocity)
		return;
	
	world_point3d new_location = object->location;
	
	world_distance adjusted_floor_height, adjusted_ceiling_height;
	
	auto negative_facing = NORMALIZE_ANGLE(HALF_CIRCLE + object->facing);
	int16 supporting_polygon_index;

	/* move the monster */		
	translate_point2d((world_point2d*)&new_location, monster->external_velocity, negative_facing);
	
	keep_line_segment_out_of_walls(object->polygon, &object->location, &new_location,
		0, definition->height, &adjusted_floor_height, &adjusted_ceiling_height, &supporting_polygon_index);
		
	if (legal_monster_move(monster_index, negative_facing, &new_location)==NONE)
	{
		auto old_polygon_index = object->polygon;
		
		if (translate_map_object(monster->object_index, &new_location, NONE)) 
			monster_moved(monster_index, old_polygon_index);
	}
	
	/* slow him down if he's touching the ground or flying */
	Polygon *polygon = get_polygon_data( object->polygon );
	
	if( object->location.z <= polygon->floor_height || definition->testFlags(_monster_flys|_monster_floats) )
	{
		if( (monster->external_velocity -= MONSTER_EXTERNAL_DECELERATION) < MONSTER_MINIMUM_EXTERNAL_VELOCITY)
			monster->external_velocity = 0;
	}

}

static void monster_needs_path(int16 monster_index, bool immediately)
{
	Monster *monster = get_monster_data(monster_index);
	
	if (!monster->isPath(NONE) && immediately) 
	{
		delete_path(monster->path);
		monster->setPath(NONE);
	}
	if (monster->isMoving() && immediately) 
		set_monster_action(monster_index, _monster_is_stationary);
		
	SET_MONSTER_NEEDS_PATH_STATUS(monster, true);
}

void set_monster_mode(int16 monster_index, int16 new_mode, int16 target_index)
{
	Monster *monster= get_monster_data(monster_index);

	/* if we were locked on a monster in our own polygon and we lost him then we don't have a path
		and going anywhere would be dangerous so we need to ask for a new path */
	if( monster->isLocked() && new_mode != _monster_locked && monster->isPath(NONE) )
		monster_needs_path(monster_index, false);

	switch (new_mode)
	{
		case _monster_locked:
			(void)get_monster_data(target_index); /* for bounds checking only */
			monster->setTarget(target_index);
			CLEAR_TARGET_DAMAGE_FLAG(monster);

			break;
		
		case _monster_losing_lock: /* target_index ignored, but still valid */
		case _monster_lost_lock:
			(void)get_monster_data(monster->getTarget()); /* for bounds checking only */
			break;
		
		case _monster_unlocked:
			monster->setTarget(NONE);
			break;
		
		default:
			assert(false);
			break;
	}
	monster->mode = new_mode;
}

/* this function decides what the given monster actually wants to do, and then generates a path
	to get him there; if a monster who has lost lock calls this function, he will be forced to
	wander randomly or follow a guard path. */
static void generate_new_path_for_monster(int16 monster_index)
{
	Monster *monster 		= get_monster_data(monster_index);
	Object *object 		= get_object_data(monster->object_index);
	monsterDefinition* definition 	= monster->getDefinition();
	monster_pathfinding_data data;
	int16 destination_polygon_index;
	world_point2d *destination;
	world_vector2d bias;

	/* delete this monster's old path, if one exists, and clear the need path flag */
	if (monster->path!=NONE) 
	{
		delete_path(monster->path);
		monster->path= NONE;
	}
	SET_MONSTER_NEEDS_PATH_STATUS(monster, false);

	switch (monster->mode)
	{
		case _monster_losing_lock:
			/* our target is out of sight, but we're still zen-ing his position until we run out
				of intelligence points */
		case _monster_locked:
		{
			Monster *target = get_monster_data(monster->getTarget());
			Object *target_object= get_object_data(target->object_index);

			if (definition->random_sound_mask && !(global_random()&definition->random_sound_mask)) play_object_sound(monster->object_index, definition->random_sound);

			/* if we can't attack, run away, otherwise go for the target */
			if (definition->flags&_monster_cannot_attack)
			{
				// LP changed: unnecessary to interrupt for this
				// dprintf("%p", monster);
				destination= (world_point2d *) &bias;
				bias.i= object->location.x - target_object->location.x;
				bias.j= object->location.y - target_object->location.y;
				destination_polygon_index= NONE;
			}
			else
			{
				/* if we still have lock, just build a new path and keep charging */
				destination= (world_point2d *) &target_object->location;
				destination_polygon_index= MONSTER_IS_PLAYER(target) ?
					get_polygon_index_supporting_player( monster->getTarget() ) :
					target_object->polygon;
			}
			break;
		}
		
		case _monster_lost_lock:
			/* if we lost lock during this path and we went as far as we could go, unlock */
			set_monster_mode(monster_index, _monster_unlocked, NONE);

		case _monster_unlocked:
			/* if we're unlocked and need a new path, follow our guard path if we have one and
				run around randomly if we don't */
			if ((destination_polygon_index= monster->goal_polygon_index) != NONE)
				destination = &get_polygon_data(destination_polygon_index)->center;
			else
				destination = nullptr;
			break;
		
		default:
			assert(false);
	}


	data.definition = definition;
	data.monster = monster;
	data.cross_zone_boundaries = destination_polygon_index==NONE ? false : true;

	monster->path= new_path((world_point2d *)&object->location, object->polygon, destination,
		destination_polygon_index, 3*definition->radius, monster_pathfinding_cost_function, &data);
	if (monster->path==NONE)
	{
		if (monster->action!=_monster_is_being_hit || MONSTER_IS_DYING(monster)) 
			set_monster_action(monster_index, _monster_is_stationary);
		set_monster_mode(monster_index, _monster_unlocked, NONE);
	}
	else
		advance_monster_path(monster_index);
}


/* somebody just did damage to us; see if we should start attacking them or not.  berserk
	monsters always switch targets.  this is where we check to see if we go berserk, right?
	monster->vitality has already been changed (a monster who just bumped into another monster
	also calls this, with a delta_vitality of zero).  returns true if an attack was started. */
static bool switch_target_check(int16 monster_index, int16 attacker_index, int16 delta_vitality)
{
	Monster *monster = get_monster_data(monster_index);
	bool switched_target = false;
	
	/* don't mess with players or dying monsters */
	if (monster->isPlayer() || monster->isDying()) 
		return false;
		
	
	if( monster->hasValidTarget() && monster->isTarget( attacker_index ) )
	{
		/* 
			if we didn't know where our target was and he just shot us, we sort of like, know
			where he is now 
		*/
		if( monster->isLosingLock() )
		{
			set_monster_mode(monster_index, _monster_locked, attacker_index);
			monster_needs_path(monster_index, false);
		}

		/* if we're already after this guy and he just did damage to us, remember that */
		if (delta_vitality)
			SET_TARGET_DAMAGE_FLAG(monster);
		
		return true;
	}

	monsterDefinition* definition = monster->getDefinition();
	int16 target_index = monster->hasValidTarget() ? monster->getTarget() : NONE;
	Monster *attacker = get_monster_data(attacker_index);
	int16 attacker_attitude, target_attitude;
	
	CLEAR_TARGET_DAMAGE_FLAG(monster);

	if ( attacker->isDying() || definition->testFlags(_monster_cannot_attack) )
		return switched_target;
	
	/* if our attacker is an enemy (or a neutral doing non-zero damage or we are berserk) and
			a) we're inactive, or,
			b) idle, or,
			c) unlocked, or,
			d) our current target has not done any damage, or,
					e) attacker is an enemy and our current target is neutral or friendly, or,
					f) attacker is a neutral and our current target is friendly, or,
			g) we can't attack and somebody just did damage to us
		then go kick his ass. */
	attacker_attitude = get_monster_attitude(monster_index, attacker_index);
	
	if( !isNONE(target_index) )
		target_attitude = get_monster_attitude(monster_index, target_index);
		
	if(mTYPE_IS_ENEMY(definition, attacker->type) || (TYPE_IS_NEUTRAL(definition, attacker->type) && delta_vitality) || monster->isBerserk() )
	{
		if( !monster->isActive() || monster->isIdle() || !monster->isLocked() || !monster->hasTargetDoneDamage() )
		{
			change_monster_target(monster_index, attacker_index);
			if (delta_vitality) 
				SET_TARGET_DAMAGE_FLAG(monster);
			switched_target = true;
		}
	}

	return switched_target;
}

static int16 get_monster_attitude(int16 monster_index, int16 target_index)
{
	Monster *monster = get_monster_data(monster_index);
	monsterDefinition* definition = monster->getDefinition();
	Monster *target = get_monster_data(target_index);
	auto target_type = target->type;
	int16 attitude;

	/* berserk monsters are hostile toward everything */
	if (mTYPE_IS_ENEMY(definition, target_type) || monster->isBerserk() ||
		(monster->hasValidTarget() && monster->isTarget(target_index)) ||
		((definition->_class&_class_human_civilian) && target->isPlayer() && 
		dynamic_world->civilians_killed_by_players >= CIVILIANS_KILLED_BY_PLAYER_THRESHHOLD))
	{
		attitude= _hostile;
	}
	else
	{
		attitude = (TYPE_IS_FRIEND(definition, target_type)) ? _friendly : _neutral;
	}

	return attitude;
}

/* find_closest_appropriate_target() tries to do just that.  it is a little broken in that it
	treats all monsters in a given polygon as if they were the same distance away, which could
	result in strange behavior.  the assumption is that if there is a more accessable hostile monster
	nearby, that monster will attack and thus end a possible wild goose chase.  if there is a
	closer hostile target which the aggressor subsequently attempts to move through, he will
	change lock and attack the obstruction instead, which will help minimize weirdness.
	full_circle is passed directly to clear_line_of_sight(). */
int16 find_closest_appropriate_target(int16 aggressor_index, bool full_circle)
{
	Monster *aggressor		= get_monster_data(aggressor_index);
	monsterDefinition* definition	= aggressor->getDefinition();
	int16 closest_hostile_target_index = NONE;
	
	if (MONSTER_IS_ACTIVE(aggressor))
	{
		int32 flood_flags = _pass_one_zone_border;
		auto polygon_index = get_object_data(get_monster_data(aggressor_index)->object_index)->polygon;
		
		/* flood out from the aggressor monster's polygon, searching through the object lists of all
			polygons we encounter */
		polygon_index= flood_map(polygon_index, INT32_MAX, monster_activation_flood_proc, _flagged_breadth_first, &flood_flags);
		while (polygon_index!=NONE && closest_hostile_target_index==NONE)
		{
			int16 object_index;
			Object *object;
	
			/* loop through all objects in this polygon looking for hostile monsters we can see */
			for (object_index= get_polygon_data(polygon_index)->first_object; object_index!=NONE; object_index= object->next_object)
			{
				object= get_object_data(object_index);
				if (GET_OBJECT_OWNER(object)==_object_is_monster && OBJECT_IS_VISIBLE(object))
				{
					auto target_monster_index = object->permutation;
					Monster *target_monster= get_monster_data(target_monster_index);
	
					if (!MONSTER_IS_DYING(target_monster) && target_monster_index!=aggressor_index)
					{
						if (get_monster_attitude(aggressor_index, target_monster_index)==_hostile)
						{
							if (((definition->flags&_monster_is_omniscent) || clear_line_of_sight(aggressor_index, target_monster_index, full_circle)) &&
								(MONSTER_IS_ACTIVE(target_monster) || MONSTER_IS_PLAYER(target_monster) || (static_world->environment_flags&_environment_rebellion)))
							{
								/* found hostile, live, visible monster */
								closest_hostile_target_index= target_monster_index;
								break;
							}
						}
					}
				}
			}
			
			polygon_index = flood_map(NONE, INT32_MAX, monster_activation_flood_proc, _flagged_breadth_first, &flood_flags);
		}
	}
	else
	{
		int16 player_index;
		
		/* if this monster is deactivated, only seeing a player will activate him */
		
		for (player_index = 0; player_index<dynamic_world->player_count; ++player_index)
		{
			player_data *player = get_player_data(player_index);
			
			if (get_monster_attitude(aggressor_index, player->monster_index)==_hostile &&
				clear_line_of_sight(aggressor_index, player->monster_index, full_circle))
			{
				closest_hostile_target_index= player->monster_index;
				
				break;
			}
		}
	}

	return closest_hostile_target_index;
}

/* if Ôfull_circle' is true, the monster can see in all directions.  if Ôfull_circle' is false
	the monster respects his visual_arc and current facing.  clear_line_of_sight() is implemented
	wholly in 2D and only attempts to connect the centers of the two monsters by a line. */
static bool clear_line_of_sight(int16 viewer_index, int16 target_index, bool full_circle)
{
	Monster *viewer 				= get_monster_data(viewer_index);
	Object *viewer_object			= get_object_data(viewer->object_index);
	monsterDefinition* viewer_definition
										= viewer->getDefinition();
	Monster *target 				= get_monster_data(target_index);
	Object *target_object 			= get_object_data(target->object_index);
	
	bool target_visible = true;
	
	
	world_point3d *origin = &viewer_object->location;
	world_point3d *destination = &target_object->location;
	// LP change: made this long-distance friendly
	int32 dx = int32(destination->x) - int32(origin->x);
	int32 dy = int32(destination->y) - int32(origin->y);
	
	int32 dz = destination->z - origin->z;
	int32 distance2d = GUESS_HYPOTENUSE(ABS(dx), ABS(dy));

	/* if we can't see full circle, make sure the target is in our visual arc */
	if (!full_circle)
	{
		auto theta = arctangent(dx, dy) - viewer_object->facing;
		auto phi = arctangent(distance2d, ABS(dz));
		
		if (ABS(theta) > viewer_definition->half_visual_arc) 
			target_visible = false;
		if (phi >= viewer_definition->half_vertical_visual_arc && phi < FULL_CIRCLE - viewer_definition->half_vertical_visual_arc) 
			target_visible = false;
	}

	/* we can't see some transfer modes */
	switch (target_object->transfer_mode)
	{
		case _xfer_invisibility:
		case _xfer_subtle_invisibility:
			if (distance2d > viewer_definition->dark_visual_range) 
				target_visible = false;
			break;
	}
	
	/* make sure the target is within our visual_range (taking any of his active
		effects, i.e. invisibility, into account) and that he isn't standing in a
		dark polygon beyond our dark_visual_range. */
	if (target_visible && distance2d > viewer_definition->visual_range)
		target_visible = false;
		
	

	/* make sure there are no non-transparent lines between the viewer and the target */
	if(!target_visible)
		return false;
	
	auto polygon_index = viewer_object->polygon;
	int16 line_index;
	
	do
	{
		line_index = find_line_crossed_leaving_polygon(polygon_index, (world_point2d *)origin, (world_point2d *)destination);
		if(!isNONE(line_index))
		{
			if( LINE_IS_TRANSPARENT(get_line_data(line_index)) )
			{
				/* transparent line, find adjacent polygon */
				polygon_index = find_adjacent_polygon(polygon_index, line_index);
				// LP change: make no polygon act like a non-transparent line
				if(isNONE(polygon_index))
					target_visible = false;
			}
			/* non-transparent line, target not visible */
			else
				target_visible = false;
		}
		/* we got to the target's (x,y) location, but we're in a different polygon;
			he's invisible */
		else if(polygon_index != target_object->polygon) 
			target_visible = false;
	}
	while(target_visible && !isNONE(line_index));


	
	return target_visible;
}

/* lock the given monster onto the given target, playing a locking sound if the monster
	previously didn't have a lock */
void change_monster_target(int16 monster_index, int16 target_index)
{
	/* locking on ourselves would be cool, but ... */
	if(monster_index == target_index)
		return;
	
	Monster *monster = get_monster_data(monster_index);

	if(isNONE(target_index))
	{
		/* no target, if we're not unlocked mark us as unlocked and ask for a new path */
		if(monster->isActive() and not monster->isUnlocked())
		{
			set_monster_mode(monster_index, _monster_unlocked, NONE);
			monster_needs_path(monster_index, false);
		}
		return;
	}
	
	/* only active monsters can have lock, so activate inactive monsters */
	if( !monster->isActive() ) 
		activate_monster(monster_index);

	/* play activation sounds (including activating on a friendly) */
	if( !monster->isTarget( target_index ) && TYPE_IS_FRIEND(monster->getDefinition(), get_monster_data(target_index)->type))
		play_object_sound(monster->object_index, monster->getDefinition()->friendly_activation_sound);
		
	else if(!monster->testDefinitionFlags(_monster_makes_sound_when_activated) && monster->isUnlocked() ) 
		play_object_sound(monster->object_index, monster->getDefinition()->activation_sound);
		
	/* instantiate the new target and ask for a new path */
	if( monster->hasValidTarget() && !monster->isTarget( target_index ) ) 
		CLEAR_TARGET_DAMAGE_FLAG(monster);
		
	monster_needs_path( monster_index, false );
	set_monster_mode( monster_index, _monster_locked, target_index );
}

static void handle_moving_or_stationary_monster(int16 monster_index)
{
	Monster *monster 		= get_monster_data(monster_index);
	Object *object 		= monster->getObject();
	monsterDefinition* definition 	= monster->getDefinition();
	
	/* stationary, unlocked monsters without paths cannot move */
	if( isNONE( monster->getPath() ) && !monster->isLocked() && monster->isStationary() )
	{
		monster_needs_path(monster_index, false);
		return;
	}

	auto distance_moved = definition->speed;
	
	/* base speed on difficulty level (for aliens) and berserk status */
	if( definition->testFlags( _monster_is_alien ) )
	{
		switch (dynamic_world->game_information.difficulty_level)
		{
			case _wuss_level: 
				distance_moved -= distance_moved / 8; 
				break;
			case _easy_level: 
				distance_moved -= distance_moved / 16; 
				break;
			case _major_damage_level: 
				distance_moved += distance_moved / 8; 
				break;
			case _total_carnage_level: 
				distance_moved += distance_moved / 4; 
				break;
		}
	}
	if ( monster->isBerserk() ) 
		distance_moved += distance_moved / 2;
	
	if (monster->action!=_monster_is_waiting_to_attack_again)
	{
		if (translate_monster(monster_index, distance_moved))
		{
			/* we moved: _monster_is_stationary becomes _monster_is_moving */
			if( monster->isStationary() ) 
				set_monster_action(monster_index, _monster_is_moving);
		}
		else
		{
			/* we couldn't move: _monster_is_moving becomes _monster_is_stationary */
			if ( monster->isMoving() ) 
				set_monster_action(monster_index, _monster_is_stationary);
			monster->ticks_since_attack++; /* attacks occur twice as frequently if we can't move (damnit!) */
		}
	}
	else
		monster->ticks_since_attack++;

	/* whether we moved or not, see if we can attack if we have lock */
	monster->ticks_since_attack += monster->isBerserk() ? 3 : 1;
	
	if( !OBJECT_WAS_ANIMATED(object) && !monster->isLocked() )
		return;
	
	auto attack_frequency = definition->attack_frequency;
	
	if(definition->flags & _monster_is_alien)
	{
		switch (dynamic_world->game_information.difficulty_level)
		{
			case _wuss_level: 
				attack_frequency *= 3; 
				break;
			case _easy_level: 
				attack_frequency *= 2; 
				break;
			case _major_damage_level: 
				attack_frequency /= 2; 
				break;
			case _total_carnage_level: 
				attack_frequency /= 4; 
				break;
		}
	}

	if( monster->ticks_since_attack <= attack_frequency )
		return;	//can't attack yet.
	
	/* activate with lock nearby monsters on our target */
	if( try_monster_attack(monster_index) )
		activate_nearby_monsters( monster->getTarget(), monster_index,
			_pass_one_zone_border, MONSTER_ALERT_ACTIVATION_RANGE );
			
	else if( monster->isWaitingToAttackAgain() )
	{
		set_monster_action( monster_index, _monster_is_stationary );
		monster_needs_path( monster_index, true );
	}
	
}

void set_monster_action(int16 monster_index, int16 action)
{
	Monster *monster 		= get_monster_data( monster_index );
	monsterDefinition* definition 	= monster->getDefinition();
	shape_descriptor shape;

	/* what shape should we use? */
	if( action == _monster_is_dying_flaming )
		shape = FLAMING_DYING_SHAPE;
	else
	{
		assert( (unsigned int)action < NUMBER_OF_MONSTER_ACTIONS ); //so we can remove the default case
		switch (action)
		{
			case _monster_is_waiting_to_attack_again:
			case _monster_is_stationary: 
				shape = definition->stationary_shape; 
				break;
			case _monster_is_moving: 
				shape = definition->moving_shape; 
				break;
			case _monster_is_attacking_close: 
				shape = definition->melee_attack.attack_shape; 
				break;
			case _monster_is_attacking_far: 
				shape = definition->ranged_attack.attack_shape; 
				break;
			case _monster_is_being_hit: 
				shape = definition->hit_shapes; 
				break;
			case _monster_is_dying_hard: 
				shape = definition->hard_dying_shape; 
				break;
			case _monster_is_dying_soft: 
				shape = definition->soft_dying_shape; 
				break;
			case _monster_is_teleporting_in: 
				shape = definition->teleport_in_shape; 
				break;
			case _monster_is_teleporting_out: 
				shape = definition->teleport_out_shape; 
				break;
		}
		
		shape = isUNONE(shape) ? UNONE : BUILD_DESCRIPTOR(definition->collection, shape);
	}

	/* only set the action of the shape is not UNONE */
	if( isUNONE( shape ) )
		return;
	
	monster->setAction(action);
	set_object_shape_and_transfer_mode(
		monster->getObjectIndex(), shape, NONE
		);

	/* if this monster does shrapnel damage, do it */
	if( action == _monster_is_dying_hard )
	{
		if( definition->testFlags(_monster_has_delayed_hard_death) )
			cause_shrapnel_damage(monster_index);
		else if (film_profile.key_frame_zero_shrapnel_fix)
		{
			const Object*object = monster->getObject();
			const shape_animation_data* animation = get_shape_animation_data( object->shape );
			if( animation && !animation->key_frame )
				cause_shrapnel_damage( monster_index );
		}
	}
	
	if( definition->testFlags( _monster_has_nuclear_hard_death ) && action == _monster_is_dying_hard )
	{
		start_fade( _fade_long_bright );
		SoundManager::instance()->PlayLocalSound( Sound_Exploding() );
	}

}

/* do whatever needs to be done when this monster dies and remove it from the monster list */
static void kill_monster(int16 monster_index)
{
	Monster *monster 		= get_monster_data( monster_index );
	monsterDefinition* definition 	= monster->getDefinition();
	Object *object 		= monster->getObject();
	shape_descriptor shape;
	
	assert( monster->isDying() ); //eliminates default case
	
	switch( monster->getAction() )
	{
		case _monster_is_dying_soft:
			shape = isUNONE(definition->soft_dead_shapes) ? UNONE : BUILD_DESCRIPTOR(definition->collection, definition->soft_dead_shapes);
			break;
		case _monster_is_dying_hard:
			shape = isUNONE(definition->hard_dead_shapes) ? UNONE : BUILD_DESCRIPTOR(definition->collection, definition->hard_dead_shapes);
			break;
		case _monster_is_dying_flaming:
			shape = FLAMING_DEAD_SHAPE;
			break;
	}

	/* add an item if we're supposed to be carrying something */
	if( !isNONE( definition->carrying_item_type ) && monster->isDyingSoft() )
	{
		world_distance radius, height;
		world_point3d random_point;
		int16 random_polygon_index;
		
		get_monster_dimensions(monster_index, &radius, &height);
		random_point_on_circle(&object->location, object->polygon, radius, &random_point, &random_polygon_index);
		if( !isNONE(random_polygon_index) )
		{
			Polygon *random_polygon = get_polygon_data(random_polygon_index);
			object_location location;
			
			switch(random_polygon->type)
			{
				case _polygon_is_platform:
				case _polygon_is_item_impassable:
				case _polygon_is_monster_impassable:
				case _polygon_is_teleporter:
					break;
				
				default:
					location.polygon_index = random_polygon_index;
					location.p.x = random_point.x;
					location.p.y = random_point.y; 
					location.flags = location.yaw = location.p.z = 0;
					
					new_item(&location, definition->carrying_item_type);
					break;
			}
		}
	}
	
	/* stuff in an appropriate dead shape (or remove our object if we don't have a dead shape) */
    bool remove_object = isUNONE(shape);
    if (!remove_object && (static_world->environment_flags & _environment_ouch_m1))
    {
        Polygon *polygon = get_polygon_data(object->polygon);
        switch (polygon->type)
        {
            case _polygon_is_major_ouch:
            case _polygon_is_minor_ouch:
                remove_object = true;
                break;
            case _polygon_is_platform:
                if (PLATFORM_IS_FLOODED(get_platform_data(polygon->permutation)) &&
                    find_flooding_polygon(object->polygon) != NONE)
                    remove_object = true;
                break;
        }
    }
    
	if (remove_object)
		remove_map_object( monster->getObjectIndex() );
	else
	{
		turn_object_to_shit( monster->getObjectIndex() );
		randomize_object_sequence( monster->getObjectIndex(), shape );
	}

	/* recover original type and notify the object stuff a monster died */
	if( monster->wasPromoted() ) 
		monster->type--;
	if( monster->wasDemoted() ) 
		monster->type++;

	object_was_just_destroyed( _object_is_monster, monster->getType() );

	L_Invalidate_Monster(monster_index);
	monster->markSlotAsFree();
}
		
/* 
	move the monster along his current heading; if he reaches the center of his destination square,
	then point him at the next square and send him off.  this used to chuck if the monster moved
	too far during a certain turn (which was completely possible when the player was wearing the
	red cloak in Pathways), but that was fixed.  i just recoded this for marathon and it looks
	a hell of a lot better now. 
	*/
static bool translate_monster(int16 monster_index, world_distance distance)
{
	Monster *monster 		= get_monster_data(monster_index);
	Object *object 		= monster->getObject();
	monsterDefinition* definition 	= monster->getDefinition();
	world_point3d new_location;
	bool legal_move = false;

	new_location = object->location;
	translate_point2d((world_point2d *)&new_location, distance, object->facing);
	auto obstacle_index = legal_monster_move(monster_index, object->facing, &new_location);
	/* find out where we're going and see if we could actually move there */
	if( isNONE(obstacle_index) )
	{
		/* legal move: see if there is a platform that we have to open or wait for,
			if not move, if so, wait */
		
		int16 feature_index;
		int16 relevant_polygon_index;

		legal_move = true;
		switch (find_obstructing_terrain_feature(monster_index, &feature_index, &relevant_polygon_index))
		{
			case _entering_platform_polygon:
				switch ( monster_can_enter_platform(feature_index, relevant_polygon_index, definition->height, definition->minimum_ledge_delta, definition->maximum_ledge_delta))
				{
					case _platform_will_never_be_accessable:
						monster_needs_path(monster_index, true);
						break;
						
					case _platform_will_be_accessable:
						/* we avoid vidding the door by only trying to open it every door_retry_mask+1 ticks */
						if (!(dynamic_world->tick_count&definition->door_retry_mask)) 
							try_and_change_platform_state(feature_index, true);
						SET_MONSTER_IDLE_STATUS(monster, true);
						legal_move = false;
						break;
					
					/* _platform_is_accessable */
				}
				break;
				
			case _leaving_platform_polygon:
				switch (monster_can_leave_platform(feature_index, relevant_polygon_index, definition->height, definition->minimum_ledge_delta, definition->maximum_ledge_delta))
				{
					case _exit_will_never_be_accessable:
						monster_needs_path(monster_index, true);
						break;
					
					case _exit_will_be_accessable:
						SET_MONSTER_IDLE_STATUS(monster, true);
						legal_move= false;
						break;
					
					/* _exit_is_accessable, ignored */
				}
				break;

			case _flying_or_floating_transition:
				/* there is a wall in our way which we have to rise (or fall) along, so don't
					go anywhere unless we're over it (or under it) */
				if (ABS(object->location.z-monster->desired_height)>MINIMUM_FLOATING_HEIGHT) 
					legal_move = false;
				break;
			
			case _standing_on_sniper_ledge:
				/* we've been told to freeze on a sniper ledge (no saving throw) */
				legal_move= false;
				break;
		}
		
		if (legal_move)
		{
			if( ( monster->path_segment_length -= distance ) <= 0 )
				advance_monster_path(monster_index);
			else
			{
				auto old_polygon_index = object->polygon;
				
				/* update the monster's object to reflect his new position */
				if (translate_map_object(monster->object_index, &new_location, NONE)) 
					monster_moved(monster_index, old_polygon_index);
			}
			legal_move = true;
		}
	}
	else
	{
		Object *obstacle_object = get_object_data(obstacle_index);
		
		if (GET_OBJECT_OWNER(obstacle_object)!=_object_is_monster)
		{
			attempt_evasive_manouvers( monster_index ); // to avoid the scenery
			return legal_move;
		}
		Monster *obstacle_monster = get_monster_data(obstacle_object->permutation);

		/* we collided with another monster: see if we want to attack him; if not, see if we
			can attack his current target (if he is locked or losing_lock); if not, drop lock
			and ask for a new path. */
		
		if(!mTYPE_IS_ENEMY(definition, obstacle_monster->type) && !( monster->hasValidTarget() && monster->isTarget(obstacle_object->permutation) ) &&
			!MONSTER_IS_BERSERK(monster))
		{
			if( !obstacle_monster->isPlayer() )
			{
				if( !monster->isLocked() )
				{
					if( !obstacle_monster->hasValidTarget() || !switch_target_check(monster_index, obstacle_monster->getTarget(), 0))
					{
						if (monster->isUnlocked() && !(global_random()&OBSTRUCTION_DEACTIVATION_MASK) &&
							(monster->goal_polygon_index == NONE || monster->goal_polygon_index == object->polygon))
							deactivate_monster(monster_index);
						else
						{
							monster_needs_path(monster_index, false);
							/* 
								if we're not locked, we might want to think about deactivating here, but
								for now we just build a new random path by forcing our state to _unlocked. 
							*/
							if( !monster->isLocked() )
								set_monster_mode( monster_index, _monster_unlocked, NONE );
						}
					}
				}
				else
					attempt_evasive_manouvers( monster_index );
			}

			SET_MONSTER_IDLE_STATUS(monster, true);
		}
		else
		{
			monsterDefinition* obstacle_definition = obstacle_monster->getDefinition();
			auto key_height = obstacle_object->location.z + ( obstacle_definition->height / 2 );
			
			change_monster_target(monster_index, obstacle_object->permutation);
			
			/* if we're a kamakazi and we're within range, pop */
			if( definition->testFlags( _monster_is_kamakazi ) && object->location.z < key_height)
			{
				bool in_range = object->location.z + definition->height > key_height;
				
				/* if we're int16 and can't float, take out their knees! */
				if (!in_range && film_profile.allow_short_kamikaze && !definition->testFlags(_monster_floats))
					in_range = object->location.z >= obstacle_object->location.z;
				
				if (in_range)
				{
					set_monster_action(monster_index, _monster_is_dying_hard);
					monster_died(monster_index);
				}
			}
			
			/* if we float and this is our target, go up */
			if( definition->testFlags( _monster_floats ) )
				monster->desired_height = obstacle_object->location.z;
		}
	}
	
	return legal_move;
}

static bool attempt_evasive_manouvers(int16 monster_index)
{
	Monster *monster 		= get_monster_data(monster_index);
	Object *object 		= monster->getObject();
	
	world_point2d destination 	= *((world_point2d*)&object->location);
	
	auto new_facing 		= NORMALIZE_ANGLE(object->facing + ((global_random()&1) ? QUARTER_CIRCLE : -QUARTER_CIRCLE));
	auto original_floor_height 	= get_polygon_data(object->polygon)->floor_height;
	auto polygon_index 		= object->polygon;
	bool successful = true;
	
	translate_point2d(&destination, EVASIVE_MANOUVER_DISTANCE, new_facing);
	do
	{
		auto line_index = find_line_crossed_leaving_polygon(polygon_index, (world_point2d *)&object->location, &destination);
		
		if (line_index==NONE)
		{
			polygon_index = NONE;
			continue;
		}

		/* if we ran off the map, we failed */
		if (LINE_IS_SOLID(get_line_data(line_index)) || (polygon_index= find_adjacent_polygon(polygon_index, line_index))==NONE)
		{
			polygon_index= NONE;
			successful= false;
		}
		else
		{
			Polygon *polygon = get_polygon_data(polygon_index);
			if (polygon->floor_height != original_floor_height || polygon->type == _polygon_is_monster_impassable)
			{
				polygon_index = NONE;
				successful = false;
			}
		}
	
	}
	while (	!isNONE(polygon_index)	);
	
	if(!successful)
		return false;
	
	object->facing = new_facing;
	if( !monster->isPath( NONE ) ) 
	{
		delete_path( monster->getPath() );
		monster->setPath( NONE );
	}
	monster->path_segment_length = EVASIVE_MANOUVER_DISTANCE;

	
	return true;
}

void advance_monster_path(int16 monster_index)
{
	Monster *monster 	= get_monster_data(monster_index);
	Object *object 	= get_object_data( monster->getObjectIndex() );
	world_point2d path_goal;
	bool done = true;

	if(isNONE(monster->path))
	{
		/* only locked monsters in their target's polygon can advance without paths */
		if(!monster->isLocked() || object->polygon != get_object_data(get_monster_data(monster->getTarget())->object_index)->polygon)
		{
			monster_needs_path(monster_index, true);
			return;
		}
	}
	else if( (done = move_along_path(monster->getPath(), &path_goal)) )
		monster->setPath(NONE);

	/* if we're locked without a path, head right for the bastard (he's in our polygon) */
	if ((done || monster->isPath(NONE) ) && monster->isLocked() )
	{
		Monster *target = get_monster_data( monster->getTarget() );
		Object *target_object = get_object_data( target->getObjectIndex() );
		
		if (object->polygon == target_object->polygon)
		{
			path_goal= *(world_point2d *)&get_object_data(get_monster_data(monster->getTarget())->object_index)->location;
			done = false;
		}
	}
	
	if (done)
	{
		/* ask for a new path (never happens to locked monsters) */
		monster_needs_path( monster_index, false );
		if( monster->isUnlocked() )
		{
			monster->goal_polygon_index = NONE;
			if (MONSTER_TELEPORTS_OUT_WHEN_DEACTIVATED(monster)) 
				deactivate_monster(monster_index);
		}
	}
	else
	{
		/* point ourselves at this new point in the path */
		object->facing = arctangent( path_goal.x-object->location.x, path_goal.y-object->location.y );
		monster->path_segment_length = distance2d( &path_goal, (world_point2d *)&object->location );
	}
}

static bool try_monster_attack(int16 monster_index)
{
	Monster *monster 		= get_monster_data(monster_index);
	Object *object 			= get_object_data(monster->object_index);
	monsterDefinition* definition 	= monster->getDefinition();
	
	int16 repetitions 	= NONE;
	int16 new_action 	= NONE;
	int16 obstruction_index = NONE;
	
	angle theta = 0;

	if( monster->hasValidTarget() )
	{
		Object *target_object = get_object_data(
			get_monster_data(monster->getTarget())->getObjectIndex()
			);
			
		world_point3d origin = object->location;
		world_point3d destination = target_object->location;
		
		auto range = distance2d( (world_point2d *)&origin, (world_point2d *)&destination );
		int16 polygon_index;
		world_point3d _vector;
	
		theta = arctangent(destination.x - origin.x, destination.y - origin.y);
		auto delta_theta = NORMALIZE_ANGLE(theta - object->facing);
		
		if (!(definition->flags&_monster_cant_fire_backwards) || (delta_theta<QUARTER_CIRCLE+QUARTER_CIRCLE/2 || delta_theta>FULL_CIRCLE-QUARTER_CIRCLE-QUARTER_CIRCLE/2))
		{
			switch (monster->action)
			{
				case _monster_is_attacking_close:
				case _monster_is_attacking_far:
					new_action= monster->action;
					break;
				
				default:
					if (definition->ranged_attack.type!=NONE && range<definition->ranged_attack.range) new_action= _monster_is_attacking_far;
					if (definition->melee_attack.type!=NONE && range<definition->melee_attack.range)
					{
						new_action= _monster_is_attacking_close;
		
						if (definition->flags&_monster_chooses_weapons_randomly)
						{
							bool switch_to_ranged = true;
							if (film_profile.validate_random_ranged_attack)
							{
								if (definition->ranged_attack.type == NONE)
								{
									logWarning("Monster chooses weapons randomly, but has no ranged attack");
									definition->flags &= ~_monster_chooses_weapons_randomly;
									switch_to_ranged = false;
								}
								else
									switch_to_ranged = (range<definition->ranged_attack.range);
							}
							if (switch_to_ranged && global_random()&1)
								new_action= _monster_is_attacking_far;
						}
					}
					break;
			}

			/* if we have a melee attack and we're at int16 range, use it */
			if (new_action==_monster_is_attacking_close)
			{
				/* make sure this is a valid projectile, that we don't hit any walls and that whatever
					we did hit is _hostile. */
				polygon_index= position_monster_projectile(monster_index, monster->getTarget(), &definition->melee_attack, &origin, &destination, &_vector, theta);
				if (preflight_projectile(&origin, polygon_index, &destination, definition->melee_attack.error,
					definition->melee_attack.type, monster_index, monster->type, &obstruction_index))
				{
					if ((obstruction_index!=NONE && get_monster_attitude(monster_index, obstruction_index)==_hostile) ||
						!line_is_obstructed(object->polygon, (world_point2d *) &object->location, target_object->polygon, (world_point2d *) &target_object->location))
					{
						repetitions = definition->melee_attack.repetitions;
					}
				}
			}
			else
			{
				/* make sure we have a ranged attack and our target is within range */
				if (new_action==_monster_is_attacking_far)
				{
					/* make sure this is a valid projectile, that we don't hit any walls and that whatever
						we did hit is _hostile. */
					polygon_index= position_monster_projectile(monster_index, monster->getTarget(), &definition->ranged_attack, &origin, &destination, &_vector, theta);
					if (preflight_projectile(&origin, polygon_index, &destination, definition->ranged_attack.error,
						definition->ranged_attack.type, monster_index, monster->type, &obstruction_index))
					{
						if ((obstruction_index!=NONE && get_monster_attitude(monster_index, obstruction_index)==_hostile) ||
							(obstruction_index==NONE && !line_is_obstructed(object->polygon, (world_point2d *) &object->location, target_object->polygon, (world_point2d *) &target_object->location)))
						{
							repetitions= definition->ranged_attack.repetitions;
						}
					}
				}
			}
		}
	}
	
	if (repetitions != NONE)
	{
		/* we can attack; set monster facing, start the attack action and reset ticks_since_attack */
		object->facing = theta;
		if( monster->getAction() != new_action ) /* if we're already attacking, this is a chained attack */
		{
			switch (dynamic_world->game_information.difficulty_level)
			{
				case _wuss_level: 
				case _easy_level: 
					repetitions /= 2;
				case _normal_level: 
					repetitions = repetitions <= 1 ? repetitions : repetitions - 1; 
					break;
			}
			
			set_monster_action( monster_index, new_action );
			monster->attack_repetitions = repetitions;
		}
		
		/* on the highest level, hitting a monster in the middle of an attack doesn't really
			stop him from continuing to attack because ticks_since_attack is never reset */
		switch (dynamic_world->game_information.difficulty_level)
		{
			case _total_carnage_level:
				break;
			
			default:
				monster->ticks_since_attack= 0;
				break;
		}
	}
	else
	{
		/* we can't attack (for whatever reason), halve ticks_since_attack so we try again soon */
		monster->ticks_since_attack = 0;
			
		if (obstruction_index!=NONE && get_monster_attitude(monster_index, obstruction_index)==_friendly &&
			MONSTER_IS_PLAYER(get_monster_data(obstruction_index)))
			play_object_sound(monster->object_index, definition->clear_sound);
	}
	
	return isNONE(new_action) ? false : true;
}

static void execute_monster_attack(int16 monster_index)
{
	Monster *monster = get_monster_data(monster_index);
	
	/* we used to assert that the attacking monster was locked, but monsters can be deactivated
		or lose lock during an attack (!) so we just abort if we no longer have a valid target */
	if (!monster->hasValidTarget())
		return;
	
	monsterDefinition* definition = monster->getDefinition();
	Object *object = get_object_data(monster->object_index);
	attack_definition *attack = monster->isAttackingClose() ? &definition->melee_attack : &definition->ranged_attack;
	int16 projectile_polygon_index;
	world_point3d origin = object->location;
	world_point3d _vector;
	
	projectile_polygon_index = position_monster_projectile(monster_index, monster->getTarget(), attack, &origin, nullptr, &_vector, object->facing);
	if (projectile_polygon_index != NONE)
		new_projectile(&origin, projectile_polygon_index, &_vector, attack->error, attack->type, monster_index, monster->type, monster->getTarget(), FIXED_ONE);
	
	if (definition->flags&_monster_fires_symmetrically)
	{
		attack->dy = -attack->dy;
		projectile_polygon_index= position_monster_projectile(monster_index, monster->getTarget(), attack, &origin, nullptr, &_vector, object->facing);
		if (projectile_polygon_index != NONE) 
			new_projectile(&origin, projectile_polygon_index, &_vector, attack->error, attack->type, monster_index, monster->type, monster->getTarget(), FIXED_ONE);
		attack->dy = -attack->dy;
	}

}

int32 monster_pathfinding_cost_function(int16 source_polygon_index, int16 line_index, int16 destination_polygon_index, void *vdata)
{
	monster_pathfinding_data *data = (monster_pathfinding_data *)vdata;
	monsterDefinition* definition = data->definition;
	Polygon *destination_polygon = get_polygon_data(destination_polygon_index);
	Polygon *source_polygon = get_polygon_data(source_polygon_index);
	line_data *line = get_line_data(line_index);
	bool respect_polygon_heights = true;
	Object *object;
	int16 object_index;
	int32 cost = source_polygon->area;
		
	/* base cost is the area of the polygon we're leaving */

	/* no solid lines (baby) */
	if (LINE_IS_SOLID(line) && !LINE_IS_VARIABLE_ELEVATION(line)) 
		cost = -1;

	/* count up the monsters in destination_polygon and add a constant cost, MONSTER_PATHFINDING_OBSTRUCTION_PENALTY,
		for each of them to discourage overcrowding */
	for (object_index= destination_polygon->first_object; object_index!=NONE; object_index=  object->next_object)
	{
		object= get_object_data(object_index);
		if (GET_OBJECT_OWNER(object)==_object_is_monster) cost+= MONSTER_PATHFINDING_OBSTRUCTION_COST;
	}

	/* if we're trying to move into a polygon with an area smaller than MINIMUM_MONSTER_PATHFINDING_POLYGON_AREA, disallow the move */
	if (source_polygon->area<MINIMUM_MONSTER_PATHFINDING_POLYGON_AREA) cost= -1;

	// do platform stuff	
	if (cost > 0)
	{
		if (destination_polygon->type == _polygon_is_platform)
		{
			switch (monster_can_enter_platform(destination_polygon->permutation, source_polygon_index, definition->height, definition->minimum_ledge_delta, definition->maximum_ledge_delta))
			{
				case _platform_will_never_be_accessable: 
					cost= -1; 
					break;
				default: 
					cost += MONSTER_PATHFINDING_PLATFORM_COST; 
					respect_polygon_heights = false; 
					break;
			}
            
            // don't move into flooded platforms
            if ((static_world->environment_flags&_environment_ouch_m1) &&
                !(definition->flags&(_monster_flys|_monster_floats)) &&
                PLATFORM_IS_FLOODED(get_platform_data(destination_polygon->permutation)) &&
                (find_flooding_polygon(destination_polygon_index) != NONE))
                cost= -1;
		}
		if (source_polygon->type == _polygon_is_platform)
		{
			switch (monster_can_leave_platform(source_polygon->permutation, destination_polygon_index, definition->height, definition->minimum_ledge_delta, definition->maximum_ledge_delta))
			{
				case _exit_will_never_be_accessable: 
					cost = -1; 
					break;
				default: 
					respect_polygon_heights = false; 
					break;
			}
		}
	}
		
	/* if the ledge between polygons is too high, the fall is too far, or there just
		isn't enough vertical space, disallow the move (and ignore this if we're dealing with
		platforms or doors) */
	if( respect_polygon_heights )
	{
		auto delta_height = destination_polygon->floor_height - source_polygon->floor_height;
		
		if( delta_height < definition->minimum_ledge_delta || delta_height>definition->maximum_ledge_delta ) 
			cost = -1;
		if( line->lowest_adjacent_ceiling - line->highest_adjacent_floor < definition->height ) 
			cost = -1;
		
		if( cost > 0 ) 
			cost += SQUARE( delta_height ); /* prefer not to change heights */
	}
	
	/* if this line not wide enough, disallow the move */
	if( line->length < 2 * definition->radius ) 
		cost = -1;

	if (cost>0)
	{
		/* if we're trying to move into an impassable polygon, disallow the move */
		switch (destination_polygon->type)
		{
			case _polygon_is_zone_border:
				if (!data->cross_zone_boundaries) cost= -1;
				break;
			
			case _polygon_is_monster_impassable:
			case _polygon_is_teleporter:
				cost= -1;
				break;
            
			case _polygon_is_minor_ouch:
			case _polygon_is_major_ouch:
				if ((static_world->environment_flags&_environment_ouch_m1) &&
				    !(definition->flags&(_monster_flys|_monster_floats)))
					cost= -1;
				break;
		}
	}

	if (cost>0)
	{
		/* if we're trying to move into media, pay the penalty */
		if (destination_polygon->media_index != NONE)
		{
			media_data *media = get_media_data(destination_polygon->media_index);
			
			// LP change: idiot-proofed this
			if (media)
			{
				if (media->height>destination_polygon->floor_height)
					cost += 2 * destination_polygon->area;
			}
		}
	}

	return cost;
}

/* returns the type and index of any interesting terrain feature (platform or door) in front
	of the given monster in his current direction; this lets us open doors and wait for
	platforms.  relevant_polygon_index is the polygon_index we have to pass to platform_is_accessable */
static int16 find_obstructing_terrain_feature(int16 monster_index, int16 *feature_index, int16 *relevant_polygon_index)
{
	Monster *monster 		= get_monster_data(monster_index);
	monsterDefinition* definition 	= monster->getDefinition();
	Object *object 			= get_object_data( monster->getObjectIndex() );
	int16 polygon_index, feature_type;
	world_point2d p1;

	ray_to_line_segment((world_point2d *)&object->location, &p1, object->facing, MONSTER_PLATFORM_BUFFER_DISTANCE+definition->radius);
	
	feature_type = NONE;
	*relevant_polygon_index = polygon_index = object->polygon;
	do
	{
		Polygon *polygon = get_polygon_data(polygon_index);
		auto line_index = find_line_crossed_leaving_polygon(polygon_index, (world_point2d *)&object->location, &p1);
		
		switch (polygon->type)
		{
			case _polygon_is_platform:
				if (object->polygon == polygon_index)
				{
					/* 
						we're standing on the platform: find out where we're headed (if we're
						going nowhere then pretend like everything is o.k.) 
					*/

					polygon_index = line_index == NONE ? NONE : find_adjacent_polygon(polygon_index, line_index);
					if (polygon_index != NONE)
					{
						*relevant_polygon_index = polygon_index;
						*feature_index = polygon->permutation;
						feature_type = _leaving_platform_polygon;
						assert(*feature_index != NONE);
					}
				}
				else
				{
					feature_type = _entering_platform_polygon;
					*feature_index = polygon->permutation;
					assert(*feature_index != NONE);
				}
				break;
			
			default:
				if (((definition->flags&_monster_floats) && polygon->floor_height>monster->desired_height) ||
					object->location.z+definition->height>polygon->ceiling_height)
				{
					monster->desired_height= polygon->floor_height;
					feature_type= _flying_or_floating_transition;
					*feature_index= 0;
				}
				if( definition->testFlags(_monster_flys) )
				{
					if ((polygon->floor_height>monster->desired_height) ||
						(polygon->ceiling_height<monster->desired_height+definition->height))
					{
						monster->desired_height= (polygon->floor_height>monster->desired_height) ?
							polygon->floor_height : (polygon->ceiling_height - definition->height);
						feature_type= _flying_or_floating_transition;
						*feature_index= 0;
					}
					
					if( object->location.z < polygon->floor_height || object->location.z + definition->height > polygon->ceiling_height )
					{
						feature_type = _flying_or_floating_transition;
						*feature_index = 0;
					}
				}
				if( definition->testFlags(_monster_uses_sniper_ledges) )
				{
					if (polygon->floor_height + MINIMUM_SNIPER_ELEVATION < monster->desired_height && monster->isLocked() )
						feature_type= _standing_on_sniper_ledge;
				}
				if (!definition->testFlags( _monster_floats | _monster_flys ) && !isNONE(polygon->media_index) )
				{
					media_data *media = get_media_data( polygon->media_index );
					// Monster will normally wade to half-height
					auto height = definition->height / 2;
					// LP change: idiot-proofing
					if (media)
					{
						// In dangerous media, wade only to zero height (that is, don't wade at all)
						if( IsMediaDangerous(media->type) ) 
							height = 0;
						
						switch (media->type)
						{
							case _media_water: 
								if( definition->testFlags(_monster_is_not_afraid_of_water) )
									media = nullptr; 
								break;
								
							case _media_jjaro: // LP addition: monsters treat Jjaro goo like sewage
							case _media_sewage: 
								if( definition->testFlags(_monster_is_not_afraid_of_sewage) )
									media = nullptr; 
								break;
								
							case _media_lava: /* height= 0; */ 
								if( definition->testFlags(_monster_is_not_afraid_of_lava) )
									media = nullptr; 
								break;
								
							case _media_goo: /* height= 0; */ 
								if( definition->testFlags(_monster_is_not_afraid_of_goo) )
									media = nullptr;
								break;
						}
					}
					if( media && media->height - polygon->floor_height > height )
					{
						if( get_polygon_data(object->polygon)->floor_height > polygon->floor_height )
						{
							feature_type = _standing_on_sniper_ledge;
							if ( !monster->isLocked() ) 
								monster_needs_path( monster_index, false );
						}
					}
				}
				polygon_index = isNONE(line_index) ? NONE : find_adjacent_polygon(polygon_index, line_index);
				break;
		}
		
		if (line_index!=NONE && polygon_index==NONE)
		{
			if (monster->path_segment_length < MONSTER_PLATFORM_BUFFER_DISTANCE)
				monster->path_segment_length = 0;
			else
			{
				/* we're headed for a wall solid; freeze and get a new path, pronto */
				feature_type= _standing_on_sniper_ledge;
				monster_needs_path(monster_index, true);
			}
		}
	}
	while (polygon_index!=NONE&&(feature_type==NONE||feature_type==_flying_or_floating_transition));
	
	return feature_type;
}

/* 
	returns new polygon index; if destination is NULL then we fire along the monster's facing
	and elevation, if destination is not NULL then we set it correctly and save the elevation angle 
*/
static int16 position_monster_projectile(
	int16 aggressor_index,
	int16 target_index,
	struct attack_definition *attack,
	world_point3d *origin,
	world_point3d *destination,
	world_point3d *_vector,
	angle theta)
{
	Monster *aggressor 		= get_monster_data(aggressor_index);
	Monster *target 		= get_monster_data(target_index);
	Object *aggressor_object 	= get_object_data(aggressor->object_index);
	Object *target_object 		= get_object_data(target->object_index);
	world_distance radius, height;

	/* adjust origin */
	*origin = aggressor_object->location;
	origin->z += attack->dz;
	translate_point2d((world_point2d *)origin, attack->dy, NORMALIZE_ANGLE(theta + QUARTER_CIRCLE));
	translate_point2d((world_point2d *)origin, attack->dx, theta);
	
	if (destination)
	{
		world_distance distance;
		
		/* adjust destination */
		get_monster_dimensions(target_index, &radius, &height);
		*destination = target_object->location;
		destination->z += (height / 2) + (height / 4); /* shoot 3/4ths up the target */
		
		/* calculate outbound vector */
		_vector->x = destination->x - origin->x;
		_vector->y = destination->y - origin->y;
		_vector->z = destination->z - origin->z;
		
		distance = isqrt( SQUARE( _vector->x ) + SQUARE( _vector->y ) );
		aggressor->elevation = distance ? (_vector->z * TRIG_MAGNITUDE) / distance : 0;
	}
	else
	{
		_vector->x = cosine_table[ theta ];
		_vector->y = sine_table[ theta ];
		_vector->z = aggressor->elevation;
	}
	
	/* return polygon_index of the new origin point */
	return find_new_object_polygon((world_point2d *)&aggressor_object->location,
		(world_point2d *)origin, aggressor_object->polygon);
}

int16 nearest_goal_polygon_index(int16 polygon_index)
{
	polygon_index = flood_map( polygon_index, INT32_MAX, nearest_goal_cost_function, _breadth_first, nullptr );
	
	while( !isNONE(polygon_index) )
	{
		Polygon *polygon = get_polygon_data(polygon_index);

		if (polygon->type == _polygon_is_goal) 
			break;

		polygon_index = flood_map( NONE, INT32_MAX, nearest_goal_cost_function, _breadth_first, nullptr );
	}
	
	return polygon_index;
}

static int32 nearest_goal_cost_function(int16 source_polygon_index, int16 line_index, int16 destination_polygon_index, void *unused)
{
	Polygon *destination_polygon = get_polygon_data(destination_polygon_index);
	auto cost = 1;
	
	(void) (unused);
	(void) (source_polygon_index);
	(void) (line_index);

	if( destination_polygon->type == _polygon_is_zone_border ) 
		cost = -1;
	
	return cost;
}


// LP: will set player view attributes when trying to shoot a guided projectile.
void SetPlayerViewAttribs(int16 half_visual_arc, int16 half_vertical_visual_arc,
	world_distance visual_range, world_distance dark_visual_range)
{
	// Added a modified version of AlexJS's changes: change only if necessary
	// Restoring AlexJLS's changes
	monster_definition& PlayerAsMonster = monster_definitions[_monster_marine];
	if (half_visual_arc > 0 || PlayerAsMonster.half_visual_arc > 0)
		PlayerAsMonster.half_visual_arc = half_visual_arc;
	if (half_vertical_visual_arc > 0 || PlayerAsMonster.half_vertical_visual_arc > 0)
		PlayerAsMonster.half_vertical_visual_arc = half_vertical_visual_arc;
	if (visual_range > 0 || PlayerAsMonster.visual_range > 0)
		PlayerAsMonster.visual_range = visual_range;
	if (dark_visual_range > 0 || PlayerAsMonster.dark_visual_range > 0)
		PlayerAsMonster.dark_visual_range = dark_visual_range;
}


uint8 *unpack_monster_data(uint8 *Stream, monster_data *Objects, size_t Count)
{
	uint8* S = Stream;
	monster_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->type);
		StreamToValue(S,ObjPtr->vitality);
		StreamToValue(S,ObjPtr->flags);
		
		StreamToValue(S,ObjPtr->path);
		StreamToValue(S,ObjPtr->path_segment_length);
		StreamToValue(S,ObjPtr->desired_height);
		
		StreamToValue(S,ObjPtr->mode);
		StreamToValue(S,ObjPtr->action);
		StreamToValue(S,ObjPtr->target_index);
		StreamToValue(S,ObjPtr->external_velocity);
		StreamToValue(S,ObjPtr->vertical_velocity);
		StreamToValue(S,ObjPtr->ticks_since_attack);
		StreamToValue(S,ObjPtr->attack_repetitions);
		StreamToValue(S,ObjPtr->changes_until_lock_lost);
		
		StreamToValue(S,ObjPtr->elevation);
		
		StreamToValue(S,ObjPtr->object_index);
		
		StreamToValue(S,ObjPtr->ticks_since_last_activation);
		
		StreamToValue(S,ObjPtr->activation_bias);
		
		StreamToValue(S,ObjPtr->goal_polygon_index);
		
		StreamToValue(S,ObjPtr->sound_location.x);
		StreamToValue(S,ObjPtr->sound_location.y);
		StreamToValue(S,ObjPtr->sound_location.z);
		StreamToValue(S,ObjPtr->sound_polygon_index);
		
		StreamToValue(S,ObjPtr->random_desired_height);
		
		S += 7*2;
	}
	
	assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_monster_data));
	return S;
}

uint8 *pack_monster_data(uint8 *Stream, monster_data *Objects, size_t Count)
{
	uint8* S = Stream;
	Monster* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		ValueToStream(S,ObjPtr->vitality);
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->path);
		ValueToStream(S,ObjPtr->path_segment_length);
		ValueToStream(S,ObjPtr->desired_height);
		
		ValueToStream(S,ObjPtr->mode);
		ValueToStream(S,ObjPtr->action);
		ValueToStream(S,ObjPtr->target_index);
		ValueToStream(S,ObjPtr->external_velocity);
		ValueToStream(S,ObjPtr->vertical_velocity);
		ValueToStream(S,ObjPtr->ticks_since_attack);
		ValueToStream(S,ObjPtr->attack_repetitions);
		ValueToStream(S,ObjPtr->changes_until_lock_lost);
		
		ValueToStream(S,ObjPtr->elevation);
		
		ValueToStream(S,ObjPtr->object_index);
		
		ValueToStream(S,ObjPtr->ticks_since_last_activation);
		
		ValueToStream(S,ObjPtr->activation_bias);
		
		ValueToStream(S,ObjPtr->goal_polygon_index);
		
		ValueToStream(S,ObjPtr->sound_location.x);
		ValueToStream(S,ObjPtr->sound_location.y);
		ValueToStream(S,ObjPtr->sound_location.z);
		ValueToStream(S,ObjPtr->sound_polygon_index);
		
		ValueToStream(S,ObjPtr->random_desired_height);
		
		S += 7*2;
	}
	
	assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_monster_data));
	return S;
}


inline void StreamToAttackDef(uint8* &S, attack_definition& Object)
{
	StreamToValue(S,Object.type);
	StreamToValue(S,Object.repetitions);
	StreamToValue(S,Object.error);
	StreamToValue(S,Object.range);
	StreamToValue(S,Object.attack_shape);
	
	StreamToValue(S,Object.dx);
	StreamToValue(S,Object.dy);
	StreamToValue(S,Object.dz);
}

inline void AttackDefToStream(uint8* &S, attack_definition& Object)
{
	ValueToStream(S,Object.type);
	ValueToStream(S,Object.repetitions);
	ValueToStream(S,Object.error);
	ValueToStream(S,Object.range);
	ValueToStream(S,Object.attack_shape);
	
	ValueToStream(S,Object.dx);
	ValueToStream(S,Object.dy);
	ValueToStream(S,Object.dz);
}


uint8 *unpack_monster_definition(uint8 *Stream, size_t Count)
{
	return unpack_monster_definition(Stream,monster_definitions,Count);
}

uint8 *unpack_monster_definition(uint8 *Stream, monster_definition* Objects, size_t Count)
{
	uint8* S = Stream;
	monster_definition* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->collection);
		
		StreamToValue(S,ObjPtr->vitality);
		StreamToValue(S,ObjPtr->immunities);
		StreamToValue(S,ObjPtr->weaknesses);
		StreamToValue(S,ObjPtr->flags);
		
		StreamToValue(S,ObjPtr->_class);
		StreamToValue(S,ObjPtr->friends);
		StreamToValue(S,ObjPtr->enemies);
		
		StreamToValue(S,ObjPtr->sound_pitch);
		StreamToValue(S,ObjPtr->activation_sound);
		StreamToValue(S,ObjPtr->friendly_activation_sound);
		StreamToValue(S,ObjPtr->clear_sound);
		StreamToValue(S,ObjPtr->kill_sound);
		StreamToValue(S,ObjPtr->apology_sound);
		StreamToValue(S,ObjPtr->friendly_fire_sound);
		StreamToValue(S,ObjPtr->flaming_sound);
		StreamToValue(S,ObjPtr->random_sound);
		StreamToValue(S,ObjPtr->random_sound_mask);
		
		StreamToValue(S,ObjPtr->carrying_item_type);
		
		StreamToValue(S,ObjPtr->radius);
		StreamToValue(S,ObjPtr->height);
		StreamToValue(S,ObjPtr->preferred_hover_height);	
		StreamToValue(S,ObjPtr->minimum_ledge_delta);
		StreamToValue(S,ObjPtr->maximum_ledge_delta);
		StreamToValue(S,ObjPtr->external_velocity_scale);
		StreamToValue(S,ObjPtr->impact_effect);
		StreamToValue(S,ObjPtr->melee_impact_effect);
		StreamToValue(S,ObjPtr->contrail_effect);
		
		StreamToValue(S,ObjPtr->half_visual_arc);
		StreamToValue(S,ObjPtr->half_vertical_visual_arc);
		StreamToValue(S,ObjPtr->visual_range);	
		StreamToValue(S,ObjPtr->dark_visual_range);
		StreamToValue(S,ObjPtr->intelligence);
		StreamToValue(S,ObjPtr->speed);
		StreamToValue(S,ObjPtr->gravity);
		StreamToValue(S,ObjPtr->terminal_velocity);
		StreamToValue(S,ObjPtr->door_retry_mask);
		StreamToValue(S,ObjPtr->shrapnel_radius);
		S = unpack_damage_definition(S,&ObjPtr->shrapnel_damage,1);
		
		StreamToValue(S,ObjPtr->hit_shapes);
		StreamToValue(S,ObjPtr->hard_dying_shape);
		StreamToValue(S,ObjPtr->soft_dying_shape);
		StreamToValue(S,ObjPtr->hard_dead_shapes);
		StreamToValue(S,ObjPtr->soft_dead_shapes);
		StreamToValue(S,ObjPtr->stationary_shape);
		StreamToValue(S,ObjPtr->moving_shape);
		StreamToValue(S,ObjPtr->teleport_in_shape);
		StreamToValue(S,ObjPtr->teleport_out_shape);
		
		StreamToValue(S,ObjPtr->attack_frequency);
		StreamToAttackDef(S,ObjPtr->melee_attack);
		StreamToAttackDef(S,ObjPtr->ranged_attack);
	}
	
	assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_monster_definition));
	return S;
}

uint8* unpack_m1_monster_definition(uint8 *Stream, size_t Count)
{
	uint8* S = Stream;
	monster_definition* ObjPtr = monster_definitions;

	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S, ObjPtr->collection);

		StreamToValue(S, ObjPtr->vitality);
		StreamToValue(S, ObjPtr->immunities);
		StreamToValue(S, ObjPtr->weaknesses);
		StreamToValue(S, ObjPtr->flags);

		StreamToValue(S, ObjPtr->_class);
		StreamToValue(S, ObjPtr->friends);
		StreamToValue(S, ObjPtr->enemies);
		
		ObjPtr->sound_pitch = FIXED_ONE;
		StreamToValue(S, ObjPtr->activation_sound);
		S += 2; // ignore conversation sound

		// Marathon doesn't have these
		ObjPtr->friendly_activation_sound = NONE;
		ObjPtr->clear_sound = NONE;
		ObjPtr->kill_sound = NONE;
		ObjPtr->apology_sound = NONE;
		ObjPtr->friendly_fire_sound = NONE;
		
		StreamToValue(S, ObjPtr->flaming_sound);
		StreamToValue(S, ObjPtr->random_sound);
		StreamToValue(S, ObjPtr->random_sound_mask);

		StreamToValue(S, ObjPtr->carrying_item_type);

		StreamToValue(S, ObjPtr->radius);
		StreamToValue(S, ObjPtr->height);
		StreamToValue(S, ObjPtr->preferred_hover_height);
		StreamToValue(S, ObjPtr->minimum_ledge_delta);
		StreamToValue(S, ObjPtr->maximum_ledge_delta);
		StreamToValue(S, ObjPtr->external_velocity_scale);

		StreamToValue(S, ObjPtr->impact_effect);
		StreamToValue(S, ObjPtr->melee_impact_effect);
		ObjPtr->contrail_effect = NONE;
		
		StreamToValue(S,ObjPtr->half_visual_arc);
		StreamToValue(S,ObjPtr->half_vertical_visual_arc);
		StreamToValue(S,ObjPtr->visual_range);	
		StreamToValue(S,ObjPtr->dark_visual_range);
		StreamToValue(S,ObjPtr->intelligence);
		StreamToValue(S,ObjPtr->speed);
		StreamToValue(S,ObjPtr->gravity);
		StreamToValue(S,ObjPtr->terminal_velocity);
		StreamToValue(S,ObjPtr->door_retry_mask);
		StreamToValue(S,ObjPtr->shrapnel_radius);

		S = unpack_damage_definition(S, &ObjPtr->shrapnel_damage, 1);

		StreamToValue(S,ObjPtr->hit_shapes);
		StreamToValue(S,ObjPtr->hard_dying_shape);
		StreamToValue(S,ObjPtr->soft_dying_shape);
		StreamToValue(S,ObjPtr->hard_dead_shapes);
		StreamToValue(S,ObjPtr->soft_dead_shapes);
		StreamToValue(S,ObjPtr->stationary_shape);
		StreamToValue(S,ObjPtr->moving_shape);

		ObjPtr->teleport_in_shape = ObjPtr->stationary_shape;
		ObjPtr->teleport_out_shape = ObjPtr->teleport_out_shape;

		StreamToValue(S, ObjPtr->attack_frequency);
		StreamToAttackDef(S, ObjPtr->melee_attack);
		StreamToAttackDef(S, ObjPtr->ranged_attack);

		ObjPtr->flags |= _monster_weaknesses_cause_soft_death;
		ObjPtr->flags |= _monster_screams_when_crushed;
		ObjPtr->flags |= _monster_makes_sound_when_activated;
		ObjPtr->flags |= _monster_can_grenade_climb;
	}

	return S;
}

uint8 *pack_monster_definition(uint8 *Stream, size_t Count)
{
	return pack_monster_definition(Stream,monster_definitions,Count);
}

uint8 *pack_monster_definition(uint8 *Stream, monsterDefinition* Objects, size_t Count)
{
	uint8* S = Stream;
	monster_definition* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->collection);
		
		ValueToStream(S,ObjPtr->vitality);
		ValueToStream(S,ObjPtr->immunities);
		ValueToStream(S,ObjPtr->weaknesses);
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->_class);
		ValueToStream(S,ObjPtr->friends);
		ValueToStream(S,ObjPtr->enemies);
		
		ValueToStream(S,ObjPtr->sound_pitch);
		ValueToStream(S,ObjPtr->activation_sound);
		ValueToStream(S,ObjPtr->friendly_activation_sound);
		ValueToStream(S,ObjPtr->clear_sound);
		ValueToStream(S,ObjPtr->kill_sound);
		ValueToStream(S,ObjPtr->apology_sound);
		ValueToStream(S,ObjPtr->friendly_fire_sound);
		ValueToStream(S,ObjPtr->flaming_sound);
		ValueToStream(S,ObjPtr->random_sound);
		ValueToStream(S,ObjPtr->random_sound_mask);
		
		ValueToStream(S,ObjPtr->carrying_item_type);
		
		ValueToStream(S,ObjPtr->radius);
		ValueToStream(S,ObjPtr->height);
		ValueToStream(S,ObjPtr->preferred_hover_height);	
		ValueToStream(S,ObjPtr->minimum_ledge_delta);
		ValueToStream(S,ObjPtr->maximum_ledge_delta);
		ValueToStream(S,ObjPtr->external_velocity_scale);
		ValueToStream(S,ObjPtr->impact_effect);
		ValueToStream(S,ObjPtr->melee_impact_effect);
		ValueToStream(S,ObjPtr->contrail_effect);
		
		ValueToStream(S,ObjPtr->half_visual_arc);
		ValueToStream(S,ObjPtr->half_vertical_visual_arc);
		ValueToStream(S,ObjPtr->visual_range);	
		ValueToStream(S,ObjPtr->dark_visual_range);
		ValueToStream(S,ObjPtr->intelligence);
		ValueToStream(S,ObjPtr->speed);
		ValueToStream(S,ObjPtr->gravity);
		ValueToStream(S,ObjPtr->terminal_velocity);
		ValueToStream(S,ObjPtr->door_retry_mask);
		ValueToStream(S,ObjPtr->shrapnel_radius);
		S = pack_damage_definition(S,&ObjPtr->shrapnel_damage,1);
		
		ValueToStream(S,ObjPtr->hit_shapes);
		ValueToStream(S,ObjPtr->hard_dying_shape);
		ValueToStream(S,ObjPtr->soft_dying_shape);
		ValueToStream(S,ObjPtr->hard_dead_shapes);
		ValueToStream(S,ObjPtr->soft_dead_shapes);
		ValueToStream(S,ObjPtr->stationary_shape);
		ValueToStream(S,ObjPtr->moving_shape);
		ValueToStream(S,ObjPtr->teleport_in_shape);
		ValueToStream(S,ObjPtr->teleport_out_shape);
		
		ValueToStream(S,ObjPtr->attack_frequency);
		AttackDefToStream(S,ObjPtr->melee_attack);
		AttackDefToStream(S,ObjPtr->ranged_attack);
	}
	
	assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_monster_definition));
	return S;
}

void init_monster_definitions()
{
	memcpy(monster_definitions, original_monster_definitions, sizeof(monster_definitions));
}

struct damage_kick_definition *original_damage_kick_definitions = NULL;
class XML_DamageKickParser: public XML_ElementParser
{
	int16 Index;
	damage_kick_definition Data;
	
	// What is present?
	bool IndexPresent;
	enum {NumberOfValues = 4};
	bool IsPresent[NumberOfValues];
	
public:
	bool Start();
	bool HandleAttribute(const char *Tag, const char *Value);
	bool AttributesDone();
	bool ResetValues();

	XML_DamageKickParser(): XML_ElementParser("kick") {}
};

bool XML_DamageKickParser::Start()
{
	// back up old values first
	if (!original_damage_kick_definitions) {
		original_damage_kick_definitions = (struct damage_kick_definition *) malloc(sizeof(struct damage_kick_definition) * NUMBER_OF_DAMAGE_TYPES);
		assert(original_damage_kick_definitions);
		for (unsigned i = 0; i < NUMBER_OF_DAMAGE_TYPES; i++)
			original_damage_kick_definitions[i] = damage_kick_definitions[i];
	}

	IndexPresent = false;
	for (int k=0; k<NumberOfValues; k++)
		IsPresent[k] = false;
	
	return true;
}

bool XML_DamageKickParser::HandleAttribute(const char *Tag, const char *Value)
{
	if (StringsEqual(Tag,"index"))
	{
		if (ReadBoundedInt16Value(Value,Index,0,NUMBER_OF_DAMAGE_TYPES-1))
		{
			IndexPresent = true;
			return true;
		}
		else return false;
	}
	else if (StringsEqual(Tag,"base"))
	{
		if (ReadInt16Value(Value,Data.base_value))
		{
			IsPresent[0] = true;
			return true;
		}
		else return false;
	}
	else if (StringsEqual(Tag,"mult"))
	{
		if (ReadFloatValue(Value,Data.delta_vitality_multiplier))
		{
			IsPresent[1] = true;
			return true;
		}
		else return false;
	}
	else if (StringsEqual(Tag,"vertical"))
	{
		if (ReadBooleanValue(Value,Data.is_also_vertical))
		{
			IsPresent[2] = true;
			return true;
		}
		else return false;
	}
	else if (StringsEqual(Tag,"death_action"))
	{
		if (ReadInt16Value(Value,Data.death_action))
		{
			IsPresent[3] = true;
			return true;
		}
		else return false;
	}
	UnrecognizedTag();
	return false;
}

bool XML_DamageKickParser::AttributesDone()
{
	// Verify...
	if (!IndexPresent)
	{
		AttribsMissing();
		return false;
	}
	damage_kick_definition& OrigData = damage_kick_definitions[Index];
	
	if (IsPresent[0]) OrigData.base_value = Data.base_value;
	if (IsPresent[1]) OrigData.delta_vitality_multiplier = Data.delta_vitality_multiplier;
	if (IsPresent[2]) OrigData.is_also_vertical = Data.is_also_vertical;
	if (IsPresent[3]) OrigData.death_action = Data.death_action;
	
	return true;
}

bool XML_DamageKickParser::ResetValues()
{
	if (original_damage_kick_definitions) {
		for (unsigned i = 0; i < NUMBER_OF_DAMAGE_TYPES; i++)
			damage_kick_definitions[i] = original_damage_kick_definitions[i];
		free(original_damage_kick_definitions);
		original_damage_kick_definitions = NULL;
	}
	return true;
}

static XML_DamageKickParser DamageKickParser;


static XML_ElementParser DamageKicksParser("damage_kicks");


// XML-parser support
XML_ElementParser *DamageKicks_GetParser()
{
	DamageKicksParser.AddChild(&DamageKickParser);
	
	return &DamageKicksParser;
}

class XML_MonsterParser : public XML_ElementParser
{
	int16 Index;
	bool MustBeExterminated;

	bool IndexPresent;
	bool ValuePresent;

public:
	bool Start();
	bool HandleAttribute(const char* Tag, const char* Value);
	bool AttributesDone();
	bool ResetValues();

	XML_MonsterParser() : XML_ElementParser("monster") { }
};

bool XML_MonsterParser::Start()
{
	IndexPresent = false;
	ValuePresent = false;

	return true;
}

bool XML_MonsterParser::HandleAttribute(const char* Tag, const char* Value)
{
	if (StringsEqual(Tag, "Index"))
	{
		if (ReadBoundedInt16Value(Value, Index, 0, NUMBER_OF_MONSTER_TYPES-1))
		{
			IndexPresent = true;
			return true;
		}
		else return false;
	}
	else if (StringsEqual(Tag, "must_be_exterminated"))
	{
		if (ReadBooleanValue(Value, MustBeExterminated))
		{
			ValuePresent = true;
			return true;
		}
		else return false;
	}
	UnrecognizedTag();
	return false;
}

bool XML_MonsterParser::AttributesDone()
{
	if (!IndexPresent)
	{
		AttribsMissing();
		return false;
	}

	if (ValuePresent)
	{
		monster_must_be_exterminated[Index] = MustBeExterminated;
	}

	return true;
}

bool XML_MonsterParser::ResetValues()
{
	monster_must_be_exterminated.clear();
	monster_must_be_exterminated.resize(NUMBER_OF_MONSTER_TYPES, false);
}

static XML_MonsterParser MonsterParser;

static XML_ElementParser MonstersParser("monsters");

XML_ElementParser* Monsters_GetParser()
{
	MonstersParser.AddChild(&MonsterParser);
	return &MonstersParser;
}

struct monster_definition*	Monster::getDefinition()
{
	return	(	hasInstanceDefinition()	) 
		? 	monster_instances::get_instance(	instance_definition_index	)->getInstanceDefinition()
		:	get_monster_definition(type);
}

uint32 Monster::testDefinitionFlags(uint32 flagtest)	
{	
	return getDefinition()->flags & flagtest;	
}

bool Monster::canFlyOrFloat()	
{	
	return testDefinitionFlags(_monster_floats|_monster_flys);	
}

struct Object* Monster::getObject()
{
	return get_object_data(object_index);
}

uint16 Monster::testObjectFlags(uint16 flagtest)
{
	return getObject()->flags & flagtest;
}

uint16 Monster::testAnimationFlags(uint16 flagtest)
{
	return GET_OBJECT_ANIMATION_FLAGS(getObject()) & flagtest;
}

void Monster::removePath()
{
	if(	hasPath()	) 
		delete_path( getPath() );
	setNeedsPathStatus(false);
	setPath(NONE);
}

bool Monster::mustBeExterminated()
{
	return monster_must_be_exterminated[type];
}

bool Monster::slotIsUsed()
{
	return SLOT_IS_USED(this);
}

bool Monster::slotIsFree()
{
	return SLOT_IS_FREE(this);
}

void Monster::markSlotAsUsed()
{
	MARK_SLOT_AS_USED(this);
}
void Monster::markSlotAsFree()
{
	MARK_SLOT_AS_FREE(this);
}
