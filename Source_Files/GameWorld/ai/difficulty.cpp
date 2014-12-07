#include <string.h>
#include <limits.h>

#include "cseries.h"
#include "map.h"
#include "render.h"
#include "interface.h"
#include "FilmProfile.h"
#include "effects.h"
#include "monsters.h"
#include "projectiles.h"
#include "player.h"
#include "platforms.h"
#include "difficulty.hpp"

int16 get_monster_drop_mask()
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

int16 get_major_demotion_mask()
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

int16 get_minor_promotion_mask()
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
