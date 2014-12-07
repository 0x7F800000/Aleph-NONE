int16 long_legal_monster_move(int16 monster_index, int a2, world_point3d *newLocation)
{
	monster_data *monster;
	int monsterObjectIndex; 
	object_data *object;
	int16 intersectingMonsterIndex;
	int intersectingObjectIndex; 
	object_data *intersectingObject; 
	uint16 maskedFlags; 
	int newLocationZ;
	int intersectingObjectZ;
	int v13;
	int16 intersectingObjects[16]; 
	int lSegmentDistance;
	int16 numberOfIntersectingObjects; 
	int16 radius;
	int16 intersectingRadius; 
	int16 intersectingHeight; 

	int16 height;
	
	monster_index = monster_index;
	monster = &monsters[monster_index];
	monsterObjectIndex = monster->object_index;
	object = &objects[monsterObjectIndex];

	get_monster_dimensions(monster_index, &radius, &height);
	numberOfIntersectingObjects = 0;
	intersectingMonsterIndex = 0;
	possible_intersecting_monsters(intersectingObjects, &numberOfIntersectingObjects, 16, object->polygon, true);

	while(intersectingMonsterIndex < numberOfIntersectingObjects)
	{
		intersectingObjectIndex = intersectingObjects[intersectingMonsterIndex];
		if ( intersectingObjectIndex == monster->object_index )
			continue;
		
		intersectingObject = &objects[intersectingObjectIndex];
		lSegmentDistance = point_to_line_segment_distance_squared(
			     (world_point2d*)&intersectingObject->location,
			     (world_point2d*)&object->location,
			     (world_point2d*)&newLocation
		     );
		maskedFlags = intersectingObject->flags & 7;
		if( maskedFlags >= 2 )
		{
			if( maskedFlags <= 2 )
				get_monster_dimensions(intersectingObject->permutation, &intersectingRadius, &intersectingHeight);
		}
		else
		{
			if( maskedFlags == 1 )
				get_scenery_dimensions(intersectingObject->permutation, &intersectingRadius, &intersectingHeight);
		}
		if( (intersectingRadius + radius) * (intersectingRadius + radius) > lSegmentDistance )
		{
			newLocationZ = newLocation->z;
			intersectingObjectZ = intersectingObject->location.z;
			if( intersectingObjectZ < newLocationZ + height && intersectingObjectZ + intersectingHeight > newLocationZ )
			{
				v13 = intersectingMonsterIndex;
				v13 = intersectingObjects[intersectingMonsterIndex];
				return v13;
			}
		
		}
		
		++intersectingMonsterIndex;
	}
	return NONE;
}
