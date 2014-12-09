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
		push_back( *reinterpret_cast< memberTypeInfo<void>* >(member) );
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

#define		getOffset(field)	(size_t(&dummy.field) - size_t(&dummy) )

#define		addMember(memberName)		\
		{\
			memberTypeInfo<decltype(dummy.memberName)> member;\
			member.name = #memberName;\
			member.offset = getOffset(memberName);\
			mTypeInfo->add<decltype(dummy.memberName)>(&member);\
		}

#define		INIT_TYPEINFO_START(typename)	 \
		typename dummy = {}; \
		alephTypeInfo<typename>* mTypeInfo = new alephTypeInfo<typename>()
		
#define		INIT_TYPEINFO_END(typename)	\
	aTypeMap[ typeid(typename).hash_code() ] = reinterpret_cast<alephTypeInfo<void>*>(mTypeInfo)
	

static void initPoint2DTypeInfo()
{
	INIT_TYPEINFO_START(world_point2d);
		addMember(x)
		addMember(y)
	INIT_TYPEINFO_END(world_point2d);
}

static void initPoint3DTypeInfo()
{
	INIT_TYPEINFO_START(world_point3d);
		addMember(x)
		addMember(y)
		addMember(z)
	INIT_TYPEINFO_END(world_point3d);
}

static void initFixedPoint3DTypeInfo()
{
	INIT_TYPEINFO_START(fixed_point3d);
		addMember(x)
		addMember(y)
		addMember(z)
	INIT_TYPEINFO_END(fixed_point3d);
}

static void initVector2DTypeInfo()
{
	INIT_TYPEINFO_START(world_vector2d);
		addMember(i)
		addMember(j)
	INIT_TYPEINFO_END(world_vector2d);
}

static void initVector3DTypeInfo()
{
	INIT_TYPEINFO_START(world_vector3d);
		addMember(i)
		addMember(j)
		addMember(k)
	INIT_TYPEINFO_END(world_vector3d);
}

static void initFixedVector3DTypeInfo()
{
	INIT_TYPEINFO_START(fixed_vector3d);
		addMember(i)
		addMember(j)
		addMember(k)
	INIT_TYPEINFO_END(fixed_vector3d);
}

static void initLocation3DTypeInfo()
{
	INIT_TYPEINFO_START(world_location3d);
		addMember(point)
		addMember(polygon_index)
		addMember(yaw)
		addMember(pitch)
		addMember(velocity)
	INIT_TYPEINFO_END(world_location3d);
}

static inline void initMathTypeInfo()
{
	initPoint2DTypeInfo();
	initPoint3DTypeInfo();
	
	initFixedPoint3DTypeInfo();
	
	initVector2DTypeInfo();
	initVector3DTypeInfo();
	
	initFixedVector3DTypeInfo();
	
	initLocation3DTypeInfo();
}

static void initObjectTypeInfo()
{
	INIT_TYPEINFO_START(Object);
		addMember(location)
		addMember(polygon)
		addMember(facing)
		addMember(shape)
		addMember(sequence)
		addMember(flags)
		addMember(transfer_mode)
		addMember(transfer_period)
		addMember(transfer_phase)
		addMember(permutation)
		addMember(next_object)
		addMember(parasitic_object)
		addMember(sound_pitch)
	INIT_TYPEINFO_END(Object);
}



static void initMonsterTypeInfo()
{
	INIT_TYPEINFO_START(Monster);
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
	INIT_TYPEINFO_END(Monster);
}

static void initPolygonTypeInfo()
{
	INIT_TYPEINFO_START(Polygon);
		addMember(type)
		addMember(flags)
		addMember(permutation)
		addMember(vertex_count)
		addMember(endpoint_indexes)
		addMember(line_indexes)
		addMember(floor_texture)
		addMember(ceiling_texture)
		addMember(floor_height)
		addMember(ceiling_height)
		addMember(floor_lightsource_index)
		addMember(ceiling_lightsource_index)
		addMember(area)
		addMember(first_object)
		addMember(first_exclusion_zone_index)
		addMember(line_exclusion_zone_count)
		addMember(point_exclusion_zone_count)
		addMember(floor_transfer_mode)
		addMember(ceiling_transfer_mode)
		addMember(adjacent_polygon_indexes)
		addMember(first_neighbor_index)
		addMember(neighbor_count)
		addMember(center)
		addMember(side_indexes)
		addMember(floor_origin)
		addMember(ceiling_origin)
		addMember(media_index)
		addMember(media_lightsource_index)
		addMember(sound_source_indexes)
		addMember(ambient_sound_image_index)
		addMember(random_sound_image_index)
	INIT_TYPEINFO_END(Polygon);
}

static void initMediaTypeInfo()
{
	INIT_TYPEINFO_START(Media);
		addMember(type)
		addMember(flags)
		addMember(light_index)
		addMember(current_direction)
		addMember(current_magnitude)
		addMember(low)
		addMember(high)
		addMember(origin)
		addMember(height)
		addMember(minimum_light_intensity)
		addMember(texture)
		addMember(transfer_mode)
	INIT_TYPEINFO_END(Media);
}

static void initStaticPlatformDataTypeInfo()
{
	INIT_TYPEINFO_START(static_platform_data);
		addMember(type)
		addMember(speed)
		addMember(delay)
		addMember( maximum_height)
		addMember( minimum_height )
		addMember(static_flags)
		addMember(polygon_index)
		addMember(tag)
	INIT_TYPEINFO_END(static_platform_data);
}

static void initGameTypeInfo()
{
	initPolygonTypeInfo();
	initMediaTypeInfo();
	initStaticPlatformDataTypeInfo();
	
	initObjectTypeInfo();
	initMonsterTypeInfo();
}

void alephType::initTypeInfo()
{
	initMathTypeInfo();
	initGameTypeInfo();
}

