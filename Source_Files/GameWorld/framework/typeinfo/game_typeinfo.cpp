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

#define		getOffset(field)	offset = size_t(&dummy.field) - size_t(&dummy) 

template<typename T> 
struct memberTypeInfo
{
	const char* name;
	const size_t offset;
	
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
	
	const std::type_info& getInfo()
	{
		return typeid(T);
	}
	
	template<typename memberType> bool add(memberTypeInfo<memberType>* member)
	{
		push_back( *static_cast< memberTypeInfo<void>* >(member) );
		return true;
	}
	
	ix hasField(const char* fieldName)
	{
		for(memberTypeInfo<void> member : *this)
		{
			if( !strcmp(fieldName, member.name) )
				return ix(reinterpret_cast<size_t>(&member) - reinterpret_cast<size_t>( &(*this)[0] ));
		}
		return NONE;
	}

};

static void initMonsterTypeInfo()
{
	static Monster dummy = {};
	size_t offset = 0;
	alephTypeInfo<Monster>* mTypeInfo = new alephTypeInfo<Monster>();
}


void alephType::initTypeInfo()
{

}

