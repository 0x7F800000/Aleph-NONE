#pragma once
/*
	WEAPONS.H

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

	Saturday, May 13, 1995 4:41:42 PM- rdm created.

Feb 4, 2000 (Loren Petrich):
	Added SMG stuff

Feb 6, 2000 (Loren Petrich):
	Added access to size of weapon-definition structure and to the number of weapon types

May 26, 2000 (Loren Petrich):
	Added XML support for configuring various weapon features

Aug 31, 2000 (Loren Petrich):
	Added stuff for unpacking and packing
*/

#ifndef		weapon_data
	#define		weapon_data			Weapon
#endif

#ifndef		trigger_data
	#define		trigger_data			Trigger
#endif

#ifndef		shell_casing_data
	#define		shell_casing_data		shellCasing
#endif

#ifndef		player_weapon_data
	#define		player_weapon_data		playerWeapons
#endif

#ifndef		weapon_display_information
	#define		weapon_display_information	weaponDisplayInfo
#endif

/* enums for player.cpp */
enum 
{ /* Weapons */
	_weapon_fist,
	_weapon_pistol,
	_weapon_plasma_pistol,
	_weapon_assault_rifle,
	_weapon_missile_launcher,
	_weapon_flamethrower,
	_weapon_alien_shotgun,
	_weapon_shotgun,
	_weapon_ball, // or something
	// LP addition:
	_weapon_smg,
	MAXIMUM_NUMBER_OF_WEAPONS,
	
	_weapon_doublefisted_pistols = MAXIMUM_NUMBER_OF_WEAPONS, /* This is a pseudo-weapon */
	_weapon_doublefisted_shotguns,
	PLAYER_TORSO_SHAPE_COUNT
};

enum 
{
	_shape_weapon_idle,
	_shape_weapon_charging,
	_shape_weapon_firing,
        PLAYER_TORSO_WEAPON_ACTION_COUNT	// ZZZ: added this one
};

enum 
{
	_primary_weapon,
	_secondary_weapon,
	NUMBER_OF_TRIGGERS
};

enum /* weapon display positioning modes */
{
	_position_low, /* position==0 is invisible, position==FIXED_ONE is sticking out from left/bottom */
	_position_center, /* position==0 is off left/bottom, position==FIXED_ONE is off top/right */
	_position_high /* position==0 is invisible, position==FIXED_ONE is sticking out from right/top
		(mirrored, whether you like it or not) */
};

/* ----------------- structures */

class weaponDisplayInfo
{
	// Has sequence info for 3D-model weapon display
	int16 collection, shape_index, low_level_shape_index;
	
	_fixed vertical_position, horizontal_position;
	int16 vertical_positioning_mode, horizontal_positioning_mode;
	int16 transfer_mode;
	_fixed transfer_phase;
	
	bool flip_horizontal, flip_vertical;
	
	// Needed for animated models: which frame in an individual sequence (0, 1, 2, ...)
	int16 Frame, NextFrame;
	
	// Needed for animated models: which tick in a frame, and total ticks per frame
	int16 Phase, Ticks;
};

// SB: This needs to be accessed in lua_script.cpp


const unsigned MAXIMUM_SHELL_CASINGS = 4;

class Trigger 
{
	int16 state, phase;
	int16 rounds_loaded;
	int16 shots_fired, shots_hit;
	int16 ticks_since_last_shot; /* used to play shell casing sound, and to calculate arc for shell casing drawing... */
	int16 ticks_firing; /* How long have we been firing? (only valid for automatics) */
	uint16 sequence; /* what step of the animation are we in? (NOT guaranteed to be in sync!) */
};

class Weapon 
{
	int16 weapon_type; /* stored here to make life easier.. */
	uint16 flags;
	uint16 unused; /* non zero-> weapon is powered up */
	Trigger triggers[NUMBER_OF_TRIGGERS];
};

class shellCasing
{
	int16 type;
	int16 frame;
  
	uint16 flags;
  
	_fixed x, y;
	_fixed vx, vy;
};

class playerWeapons 
{
	int16 current_weapon;
	int16 desired_weapon;
	
	Weapon weapons[MAXIMUM_NUMBER_OF_WEAPONS];
	shellCasing shell_casings[MAXIMUM_SHELL_CASINGS];
};

// For external access:
const int SIZEOF_weapon_definition = 134;

const int SIZEOF_player_weapon_data = 472;

/* ----------------- prototypes */
/* called once at startup */
void initialize_weapon_manager();

/* Initialize the weapons for a completely new game. */
void initialize_player_weapons_for_new_game(int16 player_index);

/* initialize the given players weapons-> called after creating a player */
void initialize_player_weapons(int16 player_index);

// Old external-access stuff: superseded by the packing and unpacking routines below
void *get_weapon_array();
int32 calculate_weapon_array_length();

/* while this returns true, keep calling.. */
bool get_weapon_display_information(int16 *count, weaponDisplayInfo *data);

/* When the player runs over an item, check for reloads, etc. */
void process_new_item_for_reloading(int16 player_index, int16 item_type);

/* Update the given player's weapons */
void update_player_weapons(int16 player_index, uint32 action_flags);

/* Mark the weapon collections for loading or unloading.. */
void mark_weapon_collections(bool loading);

/* Called when a player dies to discharge the weapons that they have charged up. */
void discharge_charged_weapons(int16 player_index);

/* Called on entry to a level, and will change weapons if this one doesn't work */
/*  in the given environment. */
void check_player_weapons_for_environment_change();

/* Tell me when one of my projectiles hits, and return the weapon_identifier I passed */
/*  to new_projectile... */
void player_hit_target(int16 player_index, int16 weapon_identifier);

/* for drawing the player */
void get_player_weapon_mode_and_type(int16 player_index, int16 *shape_weapon_type,
	int16 *shape_mode);

/* For the game window to update properly */
int16 get_player_desired_weapon(int16 player_index);

/* This is pinned to the maximum I think I can hold.. */
int16 get_player_weapon_ammo_count(int16 player_index, int16 which_weapon, int16 which_trigger);

int16 	get_player_weapon_ammo_maximum(	int16 player_index, int16 which_weapon, int16 which_trigger);
int16 	get_player_weapon_ammo_type(	int16 player_index, int16 which_weapon, int16 which_trigger);
bool 	get_player_weapon_drawn(	int16 player_index, int16 which_weapon, int16 which_trigger);


// LP: to pack and unpack this data;
// these do not make the definitions visible to the outside world

uint8 *unpack_player_weapon_data(uint8 *Stream, size_t Count);
uint8 *pack_player_weapon_data(uint8 *Stream, size_t Count);
uint8 *unpack_weapon_definition(uint8 *Stream, size_t Count);
uint8 *pack_weapon_definition(uint8 *Stream, size_t Count);
uint8* unpack_m1_weapon_definition(uint8* Stream, size_t Count);
void init_weapon_definitions();

// LP additions: get number of weapon types;
size_t get_number_of_weapon_types();

// LP addition: XML-parser support
XML_ElementParser *Weapons_GetParser();
