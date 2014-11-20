#include <limits.h>
#include <cstdio>

#include "cseries.h"
#include "map.h"
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

/*	we dont want to redeclare the monster definitions. we just want the struct	*/
#define		DONT_REPEAT_DEFINITIONS
#include "monster_definitions.h"
#include "monsters_ext.h"

//#define	MONSTERS_EXT_REDUNDANT_ASSERTIONS

static	vector<monster_instance_data*> MonsterInstanceList;

monster_instance_data::monster_instance_data(const struct monster_definition* def, int16 index)	:	monster_data_index(	index	)
{
	
	memcpy((void*)&collection, (void*)def, sizeof(monster_definition));
}

monster_definition* monster_instance_data::getInstanceDefinition()	
{
	return (monster_definition*)&collection;
}

bool monster_instance_data::isMe(monster_definition* other)	
{
	return other == this;
}

monster_data* monster_instance_data::getMonsterData()	
{
	assert(monster_data_index != NONE);
	monster_data* mdata = get_monster_data(monster_data_index);
	
	assert(	mdata 
			&&	mdata->instance_definition_index != NONE 
			&&	mdata->instance_definition_index < int16(monster_instances::max_monster_instances)
			&&	MonsterInstanceList[mdata->instance_definition_index]
			&&	MonsterInstanceList[mdata->instance_definition_index]->isMe(this)
	);
	return mdata;
}

void monster_instances::new_definition_instance(const struct monster_definition* def, int16 index	)
{
	assert(	def && index != NONE	);
	
	for( ix i = 0; i < monster_instances::max_monster_instances; ++i )
	{
			if(	MonsterInstanceList[i]	)
			#ifndef	MONSTERS_EXT_REDUNDANT_ASSERTIONS
				continue;
			#else
			{
				monster_instance_data* inst = MonsterInstanceList[i];
				assert(	inst->monster_data_index	!=	NONE
						&&	get_monster_data(inst->monster_data_index)->instance_definition_index == int16(i)
						);
				continue;
			}
			#endif
			assert(MonsterInstanceList[i] = new monster_instance_data(def, index));
			monster_data* mdata = get_monster_data(index);
			mdata->setHasInstanceDefinition(true);
			mdata->instance_definition_index = int16(i);
			return;
	}
	assert(false);
}

static bool allocated_monster_instances = false;
void monster_instances::initialize()
{
	if(not allocated_monster_instances)
	{
		MonsterInstanceList.reserve(monster_instances::max_monster_instances);
		allocated_monster_instances = true;
	}
	memset((void*)&MonsterInstanceList[0], 0, sizeof( decltype( MonsterInstanceList[0] ) ) * monster_instances::max_monster_instances);
}

void		monster_instances::delete_definition_instance(	int16	index	)	
{
	assert(	index != NONE && index < int16(monster_instances::max_monster_instances) && index >= 0	);
	monster_instance_data*	inst	=	MonsterInstanceList[index];
	#ifdef	ALLOW_CRASHES_FOR_DEBUGGING
	assert(inst);
	#else
	if(!inst)
	{
		printf("No instance for index %d.\n", index);
		return;
	}
	#endif
	monster_data* mdata = inst->getMonsterData();
	
	mdata->setHasInstanceDefinition(false);
	mdata->instance_definition_index = NONE;
	delete inst;
	MonsterInstanceList[index] = nullptr;
}

class monster_instance_data*	monster_instances::get_instance(	int16	index	)
{
	#ifdef		MONSTERS_EXT_REDUNDANT_ASSERTIONS
		assert(	index != NONE	); 
		assert(index >= 0);
		assert(index < monster_instances::max_monster_instances );
		assert(MonsterInstanceList[index]);
	#endif
	return MonsterInstanceList[index];
}
