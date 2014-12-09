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
#include <map>
#include <limits.h>

#include "game_typeinfo.hpp"

template<typename T> 
struct memberTypeInfo
{
	char* name;
	size_t offset;
	
	virtual const std::type_info& getInfo()
	{
		return typeid(T);
	}
};

template<typename T>
class alephTypeInfo : public vector< memberTypeInfo<void> > 
{
public:
	const char* const name;
	
	alephTypeInfo() : name( getInfo().name() ) {}
	
	virtual const std::type_info& getInfo()
	{
		return typeid(T);
	}
	
	template<typename memberType> bool add(memberTypeInfo<memberType>* member)
	{
		push_back( *static_cast< memberTypeInfo<void>* >(member) );
		return true;
	}
	
	virtual ix hasField(const char* fieldName)
	{
		for(memberTypeInfo<void> member : *this)
		{
			if( !strcmp(fieldName, member.name) )
				return ix(reinterpret_cast<size_t>(&member) - reinterpret_cast<size_t>( &(*this)[0] ));
		}
		return NONE;
	}

};

static std::map< size_t, //the type's hash code
		alephTypeInfo<void> * //and the corresponding typeinfo
> aTypeMap;

#define		getOffset(field)	offset = size_t(&dummy.field) - size_t(&dummy) 

#define		addMember(memberName)		\
		{\
			memberTypeInfo<decltype(dummy.memberName)> member;\
			member.name = #memberName;\
			member.offset = getOffset(memberName);\
			mTypeInfo->add<decltype(dummy.memberName)>(&member);\
		}
		

static void initMonsterTypeInfo()
{
	static Monster dummy = {};
	size_t offset = 0;
	
	alephTypeInfo<Monster>* mTypeInfo = new alephTypeInfo<Monster>();
	
	addMember(type)
	addMember(vitality)
	addMember(flags)
	addMember(path)
	addMember(path_segment_length)
	addMember(desired_height)
	addMember(mode)
	addMember(action)
	addMember(target_index)
	addMember(external_velocity)
	addMember(vertical_velocity)
	addMember(ticks_since_attack)
	addMember(attack_repetitions)
	addMember(changes_until_lock_lost)
	addMember(elevation)
	addMember(object_index)
	addMember(ticks_since_last_activation)
	addMember(activation_bias)
	addMember(goal_polygon_index)
	addMember(sound_location)
	addMember(sound_polygon_index)
	addMember(random_desired_height)
	
	aTypeMap[ typeid(Monster).hash_code() ] = reinterpret_cast<alephTypeInfo<void>*>(mTypeInfo);
}


void alephType::initTypeInfo()
{

}

