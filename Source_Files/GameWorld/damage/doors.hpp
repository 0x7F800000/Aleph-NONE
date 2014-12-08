#pragma once

/*
	DOORS.HPP
*/

#define		MAX_SWINGING_DOORS	32
#define		MAX_SLIDING_DOORS	64

struct swinging_door_data
{
	int16 textured_objects[4];
	int16 center_object, top_object, bottom_object;
	
	world_point3d p0, p1, p2, p3;
	
	int16 owner_polygon;
	int16 definition_index;
	int16 flags;
	int16 field_2C;
	int16 field_2E;
	
	int16 radius, height, length, width, yaw;
	
	int16 field_3A;
	int16 related_to_direction0;
	int16 related_to_direction1;
	int16 related_to_direction2;
	int16 field_42;
	bool a_boolean;
	bool door_open;
};

struct sliding_door_data
{
	int16 sound_index;
	int16 flags;
	int16 field_4;
	int16 polygon_index1;
	int16 polygon_index;
	int16 field_A;
	int16 line_data_index0;
	
	union
	{
		struct
		{
			int16 endpoint_data_index0;
			int16 endpoint_data_index1;
		};
		int16 endpoint_indexes[2];
	};
	
	int field_12;
	int16 field_16;
	int16 field_18;
	union
	{
		struct
		{
			int16 line_data_index1;
			int16 line_data_index2;
			int16 line_data_index3;
			int16 line_data_index4;
		};
		int16 line_indexes[4];
	};
	int field_22;
	int field_26;
	int field_2A;
	int field_2E;
	int16 endpoint_x_offset;
	int16 endpoint_y_offset;
	int field_36;
	int field_3A;
	int8 field_3E;
	int8 field_3F;
	int8 other_flags;
	int8 field_41;
	int16 field_42;
	int16 field_44;
	bool abool;
	int8 field_47;
	int16 field_48;
};

extern vector<swinging_door_data> SwingingDoorList;
extern vector<sliding_door_data> SlidingDoorList;

#define		swinging_doors	(&SwingingDoorList[0])
#define		sliding_doors	(&SlidingDoorList[0])

/*	prototypes	*/

void update_doors();
void update_swinging_doors();
