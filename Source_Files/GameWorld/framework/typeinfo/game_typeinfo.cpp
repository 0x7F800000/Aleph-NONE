/*
	GAME_TYPEINFO.CPP
	
*/
#include "cseries.h"

#include "map.h"
#include "effects.h"
#include "monsters.h"
#include "projectiles.h"
#include "player.h"
#include "scenery.h"
#include "platforms.h"
#include "lightsource.h"
#include "media.h"
#include "items.h"
#include "weapons.h"
#include "world.h"

#include "./damage/doors.hpp"

#include <typeinfo>
#include <type_traits>

#include <string>
#include <limits.h>

#include "game_typeinfo.hpp"

#define		__declmemberInfo(classname, membername)	\
		\
		alephType::alephMemberInfo<\
		alephType::offset_of<classname, decltype(classname::membername), &classname::membername>()\
		, decltype(classname::membername)>

void alephType::initTypeInfo()
{
	__declmemberInfo(Monster, type) test;
}

