__int16 __fastcall long_legal_monster_move(__int16 monster_index, int a2, world_point3d *newLocation)
{
  monster_data *monster; // eax@1
  int monsterObjectIndex; // edx@1
  object_data *object; // ebp@1
  __int16 intersectingMonsterIndex; // di@1
  int intersectingObjectIndex; // eax@3
  object_data *intersectingObject; // ecx@5
  unsigned __int16 maskedFlags; // ax@5
  int newLocationZ; // eax@9
  int intersectingObjectZ; // ecx@9
  int v13; // eax@11
  __int16 intersectingObjects[16]; // [sp+2h] [bp-50h]@1
  monster_data *_monster; // [sp+22h] [bp-30h]@1
  int lSegmentDistance; // [sp+26h] [bp-2Ch]@5
  __int16 numberOfIntersectingObjects; // [sp+2Ah] [bp-28h]@1
  __int16 radius; // [sp+2Eh] [bp-24h]@1
  __int16 intersectingRadius; // [sp+32h] [bp-20h]@7
  __int16 intersectingHeight; // [sp+36h] [bp-1Ch]@7
  None v25; // [sp+3Ah] [bp-18h]@1
  __int16 height; // [sp+3Eh] [bp-14h]@1

  monster_index = monster_index;
  monster = &monsters[monster_index];
  monsterObjectIndex = monster->object_index;
  _monster = monster;
  object = &objects[monsterObjectIndex];
  v25 = NONE;
  get_monster_dimensions(monster_index, &radius, &height);
  numberOfIntersectingObjects = 0;
  intersectingMonsterIndex = 0;
  possible_intersecting_monsters(intersectingObjects, &numberOfIntersectingObjects, 16, object->polygon, true);
  while ( intersectingMonsterIndex < numberOfIntersectingObjects )
  {
    intersectingObjectIndex = intersectingObjects[intersectingMonsterIndex];
    if ( intersectingObjectIndex != _monster->object_index )
    {
      intersectingObject = &objects[intersectingObjectIndex];
      lSegmentDistance = point_to_line_segment_distance_squared(
                           &intersectingObject->location.base,
                           &object->location.base,
                           &newLocation->base);
      maskedFlags = intersectingObject->flags & 7;
      if ( maskedFlags >= 2u )
      {
        if ( maskedFlags <= 2u )
          get_monster_dimensions(intersectingObject->permutation, &intersectingRadius, &intersectingHeight);
      }
      else
      {
        if ( maskedFlags == 1 )
          get_scenery_dimensions(intersectingObject->permutation, &intersectingRadius, &intersectingHeight);
      }
      if ( (intersectingRadius + radius) * (intersectingRadius + radius) > lSegmentDistance )
      {
        newLocationZ = newLocation->z;
        intersectingObjectZ = intersectingObject->location.z;
        if ( intersectingObjectZ < newLocationZ + height )
        {
          if ( intersectingObjectZ + intersectingHeight > newLocationZ )
          {
            v13 = intersectingMonsterIndex;
            LOWORD(v13) = intersectingObjects[intersectingMonsterIndex];
            return v13;
          }
        }
      }
    }
    ++intersectingMonsterIndex;
  }
  return v25;
}
