#pragma once


//#define		ALLOW_CRASHES_FOR_DEBUGGING

#ifndef		EXCLUDE_INSTANCE_DATA_DECLARATION

class monster_instance_data : public monster_definition
{
public:
	monster_instance_data(	const struct	monster_definition* def, int16 index	);
	monster_definition*	getInstanceDefinition();
	monster_data*		getMonsterData();
	bool				isMe(struct monster_definition* other);
	
	
	const int16 monster_data_index;
};
#endif

namespace monster_instances
{
	void					new_definition_instance(	const struct	monster_definition* def, int16 index	);
	void					delete_definition_instance(	int16	index	);
	void					initialize();
	class monster_instance_data*	get_instance(	int16	index	);
	/*	AM	-	picked an arbitrary number. will change later	*/
	const size_t	max_monster_instances	=		2048;
};

