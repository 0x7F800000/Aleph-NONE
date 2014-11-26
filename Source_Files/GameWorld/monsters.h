#pragma once

/*
MONSTERS.H

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

Tuesday, June 28, 1994 7:07:59 PM

Feb 3, 2000 (Loren Petrich):
	Added VacBobs

Feb 6, 2000 (Loren Petrich):
	Added access to size of monster-definition structure

Feb 10, 2000 (Loren Petrich):
	Added dynamic-limits setting of MAXIMUM_MONSTERS_PER_MAP

Feb 19, 2000 (Loren Petrich):
	Added growable lists of indices of objects to be checked for collisions

Aug 30, 2000 (Loren Petrich):
	Added stuff for unpacking and packing
	
Oct 13, 2000 (Loren Petrich)
	Converted the intersected-objects list into a Standard Template Library vector

Oct 24, 2000 (Mark Levin)
	Revealed some functions for P-tran
*/
#include "access.hpp"
// LP additions:
#include "dynamic_limits.h"
#include "XML_ElementParser.h"
#include <vector>

/* ---------- constants */

#ifndef	monster_definition
	#define	monster_definition monsterDefinition
#endif

#define FLAMING_DEAD_SHAPE BUILD_DESCRIPTOR(_collection_rocket, 7)
#define FLAMING_DYING_SHAPE BUILD_DESCRIPTOR(_collection_rocket, 8)

enum /* constants for activate_nearby_monsters */
{
	_pass_one_zone_border= 0x0001,
	_passed_zone_border= 0x0002,
	_activate_invisible_monsters= 0x0004, // sound or teleport trigger
	_activate_deaf_monsters= 0x0008, // i.e., trigger
	_pass_solid_lines= 0x0010, // i.e., not a sound (trigger)
	_use_activation_biases= 0x0020, // inactive monsters follow their editor instructions (trigger)
	_activation_cannot_be_avoided= 0x0040, // cannot be suppressed because of recent activation (trigger)
	_cannot_pass_superglue= 0x0080, // i.e., glue trigger
	_activate_glue_monsters= 0x0100 // glue trigger
};

/* activation biases are only used when the monster is activated by a trigger */
enum /* activation biases (set in editor) */
{
	_activate_on_player,
	_activate_on_nearest_hostile,
	_activate_on_goal,
	_activate_randomly
};

/* ---------- monsters */

// LP change: made this settable from the resource fork
#define MAXIMUM_MONSTERS_PER_MAP (get_dynamic_limit(_dynamic_limit_monsters))

/* player monsters are never active */

#define MONSTER_IS_PLAYER(m) ((m)->type==_monster_marine)
enum /* monster types */
{
	_monster_marine,
	_monster_tick_energy,
	_monster_tick_oxygen,
	_monster_tick_kamakazi,
	_monster_compiler_minor,
	_monster_compiler_major,
	_monster_compiler_minor_invisible,
	_monster_compiler_major_invisible,
	_monster_fighter_minor,
	_monster_fighter_major,
	_monster_fighter_minor_projectile,
	_monster_fighter_major_projectile,
	_civilian_crew,
	_civilian_science,
	_civilian_security,
	_civilian_assimilated,
	_monster_hummer_minor, // slow hummer
	_monster_hummer_major, // fast hummer
	_monster_hummer_big_minor, // big hummer
	_monster_hummer_big_major, // angry hummer
	_monster_hummer_possessed, // hummer from durandal
	_monster_cyborg_minor,
	_monster_cyborg_major,
	_monster_cyborg_flame_minor,
	_monster_cyborg_flame_major,
	_monster_enforcer_minor,
	_monster_enforcer_major,
	_monster_hunter_minor,
	_monster_hunter_major,
	_monster_trooper_minor,
	_monster_trooper_major,
	_monster_mother_of_all_cyborgs,
	_monster_mother_of_all_hunters,
	_monster_sewage_yeti,
	_monster_water_yeti,
	_monster_lava_yeti,
	_monster_defender_minor,
	_monster_defender_major,
	_monster_juggernaut_minor,
	_monster_juggernaut_major,
	_monster_tiny_fighter,
	_monster_tiny_bob,
	_monster_tiny_yeti,
	// LP addition:
	_civilian_fusion_crew,
	_civilian_fusion_science,
	_civilian_fusion_security,
	_civilian_fusion_assimilated,
	NUMBER_OF_MONSTER_TYPES
};

/* uses SLOT_IS_USED(), SLOT_IS_FREE(), MARK_SLOT_AS_FREE(), MARK_SLOT_AS_USED() macros (0x8000 bit) */

#define MONSTER_NEEDS_PATH(m) ((m)->flags&(uint16)0x4000)
#define SET_MONSTER_NEEDS_PATH_STATUS(m,v) ((void)((v)?((m)->flags|=(uint16)0x4000):((m)->flags&=(uint16)~0x4000)))

/* the recovering from hit flag is set randomly after a monster finishes his being-hit
	animation, and when set it prevents him from being immediately dragged into another
	being-hit animation.  this makes monsters twitch when being pinned down by a big gun,
	and allows them a small chance to react.  the flag is cleared every frame. */
#define MONSTER_RECOVERING_FROM_HIT(m) ((m)->flags&(uint16)0x2000)
#define CLEAR_MONSTER_RECOVERING_FROM_HIT(m) ((m)->flags&=(uint16)~0x2000)
#define SET_MONSTER_RECOVERING_FROM_HIT(m) ((m)->flags|=(uint16)0x2000)

#define MONSTER_IS_ACTIVE(m) ((m)->flags&(uint16)0x1000)
#define SET_MONSTER_ACTIVE_STATUS(m,v) ((void)((v)?((m)->flags|=(uint16)0x1000):((m)->flags&=(uint16)~0x1000)))

/* berserk monsters will only switch targets when their target dies and then choose the
	geometerically closest monster.  what sets this bit is still unclear.  */
#define MONSTER_IS_BERSERK(m) ((m)->flags&(uint16)0x0400)
#define SET_MONSTER_BERSERK_STATUS(m,v) ((void)((v)?((m)->flags|=(uint16)0x0400):((m)->flags&=(uint16)~0x0400)))

#define MONSTER_IS_IDLE(m) ((m)->flags&(uint16)0x0800)
#define SET_MONSTER_IDLE_STATUS(m,v) ((void)((v)?((m)->flags|=(uint16)0x0800):((m)->flags&=(uint16)~0x0800)))

/* this flag is set if our current target has inflicted damage on us (because if he hasnÕt, weÕll
	probably go after somebody else if they hit us first) */
#define TARGET_HAS_DONE_DAMAGE(m) ((m)->flags&(uint16)0x0200)
#define CLEAR_TARGET_DAMAGE_FLAG(m) ((m)->flags&=(uint16)~0x0200)
#define SET_TARGET_DAMAGE_FLAG(m) ((m)->flags|=(uint16)0x0200)

#define SET_MONSTER_HAS_BEEN_ACTIVATED(m) ((m)->flags&=(uint16)~_monster_has_never_been_activated)

#define MONSTER_IS_BLIND(m) ((m)->flags&(uint16)_monster_is_blind)
#define MONSTER_IS_DEAF(m) ((m)->flags&(uint16)_monster_is_deaf)
#define MONSTER_TELEPORTS_OUT_WHEN_DEACTIVATED(m) ((m)->flags&(uint16)_monster_teleports_out_when_deactivated)

#define MONSTER_IS_DYING(m) ((m)->action==_monster_is_dying_hard||(m)->action==_monster_is_dying_soft||(m)->action==_monster_is_dying_flaming)
#define MONSTER_IS_ATTACKING(m) ((m)->action==_monster_is_attacking_close||(m)->action==_monster_is_attacking_far)
#define MONSTER_IS_TELEPORTING(m) ((m)->action==_monster_is_teleporting)
enum /* monster actions */
{
	_monster_is_stationary,
	_monster_is_waiting_to_attack_again,
	_monster_is_moving,
	_monster_is_attacking_close, /* melee */
	_monster_is_attacking_far, /* ranged */
	_monster_is_being_hit,
	_monster_is_dying_hard,
	_monster_is_dying_soft,
	_monster_is_dying_flaming,
	_monster_is_teleporting, // transparent
	_monster_is_teleporting_in,
	_monster_is_teleporting_out,
	NUMBER_OF_MONSTER_ACTIONS
};

#define MONSTER_IS_LOCKED(m) ((m)->mode==_monster_locked||(m)->mode==_monster_losing_lock)
#define MONSTER_HAS_VALID_TARGET(m) (MONSTER_IS_ACTIVE(m)&&MONSTER_IS_LOCKED(m))


enum /* monster flags */
{
	_monster_no_flags,		//is 0
	_monster_was_promoted, //is 1
	_monster_was_demoted = _monster_was_promoted << 1,
	_monster_has_never_been_activated = _monster_was_demoted << 1,
	_monster_is_blind = _monster_has_never_been_activated << 1,
	_monster_is_deaf = _monster_is_blind << 1,
	_monster_teleports_out_when_deactivated = _monster_is_deaf << 1	//is 0x20
};

enum /*	monster extended flags	*/
{
	_monster_no_exflags,
	_monster_has_instance_definition, //is 1
};
enum /* monster modes */
{
	_monster_locked,
	_monster_losing_lock,
	_monster_lost_lock,
	_monster_unlocked,
	_monster_running,
	NUMBER_OF_MONSTER_MODES
};

#define		IS_ACTION(gettername, settername, chkaction)	inline bool gettername()	{	return action == chkaction;	} \
	inline void settername()	{	action = chkaction;	}

#define		IS_MODE(gettername, settername, chkmode)		inline bool gettername()	{	return mode == chkmode;	}\
	inline void settername()	{	mode = chkmode;	}

#define		FLAGTEST(gettername, settername, chkflag)		inline bool gettername()	{	return flags & chkflag;	}	\
	inline void settername(bool set)	{	flags = (set) ? flags | chkflag : flags & (~chkflag);	}

#define		EXFLAGTEST(gettername, settername, chkflag)		inline bool gettername()	{	return exflags & chkflag;	}	\
	inline void settername(bool set)	{	exflags = (set) ? exflags | chkflag : exflags & (~chkflag);	}


#define	monster_data	Monster
class Monster 
{
public:
	static class Monster& Get(const ix index);
	
	IS_MODE(isLocked, setLocked, _monster_locked)
	IS_MODE(isLosingLock, setLosingLock, _monster_losing_lock)
	IS_MODE(didLoseLock, setLostLock, _monster_lost_lock)
	IS_MODE(isUnlocked, setUnlocked, _monster_unlocked)
	IS_MODE(isRunning, setRunning, _monster_running)
	
	IS_ACTION(isStationary, setStationary,  _monster_is_stationary)
	IS_ACTION(isWaitingToAttackAgain,setWaitingToAttackAgain, _monster_is_waiting_to_attack_again)
	IS_ACTION(isMoving,setMoving, _monster_is_moving)
	IS_ACTION(isAttackingClose, setAttackingClose, _monster_is_attacking_close)
	IS_ACTION(isAttackingFar, setAttackingFar, _monster_is_attacking_far)
	IS_ACTION(isBeingHit, setBeingHit, _monster_is_being_hit)
	IS_ACTION(isDyingHard, setDyingHard, _monster_is_dying_hard)
	IS_ACTION(isDyingSoft, setDyingSoft, _monster_is_dying_soft)
	IS_ACTION(isDyingFlaming, setDyingFlaming, _monster_is_dying_flaming)
	IS_ACTION(isTeleporting, setTeleporting, _monster_is_teleporting)
	IS_ACTION(isTeleportingIn, setTeleportingIn, _monster_is_teleporting_in)
	IS_ACTION(isTeleportingOut, setTeleportingOut, _monster_is_teleporting_out)
	
	FLAGTEST(wasPromoted, setWasPromoted, _monster_was_promoted)
	FLAGTEST(wasDemoted, setWasDemoted, _monster_was_demoted)
	FLAGTEST(hasNeverBeenActivated, setHasNeverBeenActivated, _monster_has_never_been_activated)
	FLAGTEST(isBlind, setBlind, _monster_is_blind)
	FLAGTEST(isDeaf, setDeaf, _monster_is_deaf)
	FLAGTEST(teleportsOutWhenDeactivated, setTeleportsOutWhenDeactivated, _monster_teleports_out_when_deactivated)
	
	EXFLAGTEST(hasInstanceDefinition, setHasInstanceDefinition, _monster_has_instance_definition)
	inline void setNeedsPathStatus(bool needs)	{	SET_MONSTER_NEEDS_PATH_STATUS(this, needs);	}
	inline bool isPlayer()		{	return type == _monster_marine;						}
	inline bool needsPath()		{	return MONSTER_NEEDS_PATH(this);					}
	inline bool isActive()		{	return MONSTER_IS_ACTIVE(this);						}
	inline bool isIdle()		{	return MONSTER_IS_IDLE(this);						}
	inline bool isDying()		{	return MONSTER_IS_DYING(this);						}
	inline bool isBerserk()		{	return MONSTER_IS_BERSERK(this);					}
	inline bool hasTargetDoneDamage()	{	return TARGET_HAS_DONE_DAMAGE(this);		}
	inline bool isAttacking()	{	return isAttackingClose()	||	isAttackingFar();	}
	inline bool hasVelocity()	{	return external_velocity || vertical_velocity;		}
	inline bool hasValidTarget(){	return MONSTER_HAS_VALID_TARGET(this);				}
	
	bool mustBeExterminated();
	
	bool slotIsUsed();
	bool slotIsFree();
	void markSlotAsUsed();
	void markSlotAsFree();
	
	bool canFlyOrFloat();

	
	inline int16 getObjectIndex()		{	return object_index;			}
	inline void setObjectIndex(int16 o)	{	object_index = o;			}
	inline bool isObject(int16 index)	{	return getObjectIndex() == index;	}
	
	void removePath();
	
	inline void setActiveStatus(bool s)	{	SET_MONSTER_ACTIVE_STATUS(this, s);	}
	
	struct object_data* getObject();
	
	ix getIndex();
	
	void accelerate(world_distance v_velocity, angle direction, world_distance velocity);
	void changeTarget(const int16 targetIndex);
	void changeMode(const int16 newMode, const int16 targetIndex);
	void changeAction(const int16 newAction);
	
	void activate();
	void deactivate();
	void kill();
	
	void getDimensions(world_distance* radius, world_distance* height);
	
	int16 getImpactEffect();
	int16 getMeleeImpactEffect();
	
	
	uint32 testDefinitionFlags(uint32 flagtest);
	uint16 testObjectFlags(uint16 flagtest);
	uint16 testAnimationFlags(uint16 flagtest);
	struct monster_definition*	getDefinition();
	
	__accessordecl(Type, int16, type)
	int16 type;
	
	__accessordecl(Vitality, int16, vitality)
	int16 vitality; /* if ==NONE, will be properly initialized when the monster is first activated */
	
	uint16 flags; /* [slot_used.1] [need_path.1] [recovering_from_hit.1] [active.1] [idle.1] [berserk.1] [target_damage.1] [unused.6] [never_activated.1] [demoted.1] [promoted.1] */
	
	inline bool hasPath()		{	return getPath() != NONE;	}
	__accessordecl(Path, int16, path)
	int16 path; /* NONE is no path (the need path bit should be set in this case) */
	
	__accessordecl(PathSegmentLength, world_distance, path_segment_length)
	world_distance path_segment_length; /* distance until weÕre through with this segment of the path */
	
	__accessordecl(DesiredHeight, world_distance, desired_height)
	world_distance desired_height;
	
	__accessordecl(Mode, int16, mode)
	int16 mode;
	
	__accessordecl(Action, int16, action)
	int16 action;
	
	inline bool hasTarget()		{	return getTarget() != NONE;	}
	__accessordecl(Target, int16, target_index)
	int16 target_index; /* a monster_index */
	
	
	__accessordecl(ExternalVelocity, world_distance, external_velocity)
	world_distance external_velocity; /* per tick, in the direction -facing, only updated during hit/death animations */
	
	__accessordecl(VerticalVelocity, world_distance, vertical_velocity)
	world_distance vertical_velocity; /* per tick, is rarely positive; absorbed immediately on contact with ground */
	
	__accessordecl(TicksSinceAttack, int16, ticks_since_attack)
	int16 ticks_since_attack;
	
	__accessordecl(AttackRepetitions, int16, attack_repetitions)
	int16 attack_repetitions;
	
	__accessordecl(ChangesUntilLockLost, int16, changes_until_lock_lost)
	int16 changes_until_lock_lost; /* number of times more the target can change polygons until _losing_lock becomes _lost_lock */

	__accessordecl(Elevation, world_distance, elevation)
	world_distance elevation; /* valid when attacking; z-component of projectile vector */

	int16 object_index;
	
	__accessordecl(TicksSinceLastActivation, int16, ticks_since_last_activation)
	int32 ticks_since_last_activation;
	
	__accessordecl(ActivationBias, int16, activation_bias)
	int16 activation_bias;
	
	__accessordecl(GoalPolygonIndex, int16, goal_polygon_index)
	int16 goal_polygon_index; // used instead of NONE when generating random paths

	// copied from monsterÕs object every tick but updated with height
	inline world_point3d& getSoundLocation()	{	return sound_location;	}
	world_point3d sound_location;
	
	__accessordecl(SoundPolygonIndex, int16, sound_polygon_index)
	int16 sound_polygon_index;
	
	__accessordecl(RandomDesiredHeight, int16, random_desired_height)
	int16 random_desired_height;
	
	int16 instance_definition_index;
	int16 exflags;
	int16 unused[6];
	
	/*	overloaded operators	*/
	const struct monsterDefinition* const operator ->()
	{
		return getDefinition();
	}

};
const int SIZEOF_monster_data = 64;

const int SIZEOF_monster_definition = 156;

/* ---------- globals */

// Turned the list of active monsters into a variable array

extern vector<Monster> MonsterList;
#define monsters (&MonsterList[0])

// extern struct monster_data *monsters;

/* ---------- prototypes/MONSTERS.CPP */

void initialize_monsters();
void initialize_monsters_for_new_level(); /* when a map is loaded */

void move_monsters(); /* assumes ¶t==1 tick */

short new_monster(struct object_location *location, short monster_code);
void remove_monster(short monster_index);

void activate_monster(short monster_index);
void deactivate_monster(short monster_index);
short find_closest_appropriate_target(short aggressor_index, bool full_circle);

void mark_monster_collections(short type, bool loading);
void load_monster_sounds(short monster_type);

void monster_moved(short target_index, short old_polygon_index);
short legal_monster_move(short monster_index, angle facing, world_point3d *new_location);
short legal_player_move(short monster_index, world_point3d *new_location, world_distance *object_floor);

//ML: Revealed these function for Pfhortran
void change_monster_target(short monster_index, short target_index);
struct monster_definition *get_monster_definition_external(const short type);


// LP change: made these settable from the resource fork
// and used a growable list for the indices
#define LOCAL_INTERSECTING_MONSTER_BUFFER_SIZE (get_dynamic_limit(_dynamic_limit_local_collision))
#define GLOBAL_INTERSECTING_MONSTER_BUFFER_SIZE (get_dynamic_limit(_dynamic_limit_global_collision))
bool possible_intersecting_monsters(vector<short> *IntersectedObjectsPtr, unsigned maximum_object_count, short polygon_index, bool include_scenery);
#define monsters_nearby(polygon_index) possible_intersecting_monsters(0, 0, (polygon_index), false)

void get_monster_dimensions(short monster_index, world_distance *radius, world_distance *height);

void activate_nearby_monsters(short target_index, short caller_index, short flags, int32 max_range = -1);

void damage_monsters_in_radius(short primary_target_index, short aggressor_index, short aggressor_type,
	world_point3d *epicenter, short epicenter_polygon_index, world_distance radius, struct damage_definition *damage, short projectile_index);
void damage_monster(short monster_index, short aggressor_index, short aggressor_type, world_point3d *epicenter, struct damage_definition *damage, short projectile_index);

Monster* get_monster_data(const ix monster_index);
struct monster_definition *get_monster_definition(const short type);

bool bump_monster(short aggressor_index, short monster_index);

bool legal_polygon_height_change(short polygon_index, world_distance new_floor_height, world_distance new_ceiling_height, struct damage_definition *damage);
void adjust_monster_for_polygon_height_change(short monster_index, short polygon_index, world_distance new_floor_height, world_distance new_ceiling_height);
void accelerate_monster(short monster_index, angle direction, angle elevation, world_distance velocity);

void monster_died(short target_index);

short monster_placement_index(short monster_type);
short placement_index_to_monster_type(short placement_index);
void try_to_add_random_monster(short monster_type, bool activate);

short get_monster_impact_effect(short monster_index);
short get_monster_melee_impact_effect(short monster_index);

bool live_aliens_on_map();

// LP: will set player view attributes when trying to shoot a guided projectile.
void SetPlayerViewAttribs(int16 half_visual_arc, int16 half_vertical_visual_arc,
	world_distance visual_range, world_distance dark_visual_range);


// LP: to pack and unpack this data;
// these do not make the definitions visible to the outside world

uint8 *unpack_monster_data(uint8 *Stream, monster_data *Objects, size_t Count);
uint8 *pack_monster_data(uint8 *Stream, monster_data *Objects, size_t Count);
uint8 *unpack_monster_definition(uint8 *Stream, size_t Count);
uint8 *pack_monster_definition(uint8 *Stream, size_t Count);
uint8* unpack_m1_monster_definition(uint8* Stream, size_t Count);
void init_monster_definitions();

XML_ElementParser *DamageKicks_GetParser();
XML_ElementParser* Monsters_GetParser();
