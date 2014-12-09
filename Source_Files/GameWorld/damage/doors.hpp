#pragma once

/*
	DOORS.HPP
*/

#define		MAX_SWINGING_DOORS	32
#define		MAX_SLIDING_DOORS	64

/*	in original Damage this is a field of dynamic_data.	*/
extern int16 sliding_door_count;

//enum for sliding_door_data::state
enum : int16
{
	_sliding_door_is_open,
	_sliding_door_is_absolutely_open,
	_sliding_door_is_active,
	_sliding_door_is_closed
};

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

enum sliding_door_sound_t : int16
{
	_sliding_door_sound_opening,
	_sliding_door_sound_unknown_1,
	_sliding_door_sound_closing,
	_sliding_door_sound_unknown_3,
	_sliding_door_sound_player_touched_active,
	_sliding_door_sound_obstructed_closing,
	_sliding_door_sound_player_touched_unchangeable
};

struct door_sound_data
{
	int16 field_0;
	int16 field_2;
	int16 field_4;
	int16 field_6;
	int16 field_8;
	int16 field_A;
	int16 field_C;
	int16 field_E;
	int16 field_10;
	int16 field_12;
	int16 field_14;
	int16 field_16;
	int16 field_18;
	int16 field_1A;
	int16 field_1C;
	int16 field_1E;
	int16 field_20;
	int16 field_22;
	int16 field_24;
	int16 field_26;
	int16 field_28;
	int16 field_2A;
	int16 field_2C;
	int16 field_2E;
	int16 field_30;
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
	int16 side_indexes[2];
	/*	
		door_texture_side is an index into side_indexes 
		it's used by adjust_door_texture to determine which side's primary
		texture should be adjusted
	*/
	int16 door_texture_side;
	int16 field_28;
	int16 field_2A;
	int16 field_2C;
	int16 field_2E;
	int16 field_30;
	int16 endpoint_x_offset;
	int16 endpoint_y_offset;
	int16 field_36;
	int16 field_38;
	int16 field_3A;
	int16 field_3C;
	int16 field_3E;
	int8 other_flags;
	int8 field_41;
	int16 field_42;
	int16 state;
	bool abool;
	int8 field_47;
	int16 tag;
};

extern vector<swinging_door_data> SwingingDoorList;
extern vector<sliding_door_data> SlidingDoorList;

#define		swinging_doors	(&SwingingDoorList[0])
#define		sliding_doors	(&SlidingDoorList[0])

/*	prototypes	*/

void update_doors();
void update_swinging_doors();

/*	needed for translate_monster	*/
bool door_is_open(int16 door_index);

/*	needed for initialize_control_panels_for_level	*/
bool door_is_active(int16 door_index);

/*	needed for translate_monster	*/
bool door_is_absolutely_open(int16 door_index);

/*	these need extern for update_action_key	*/
void player_touch_sliding_door(int16 unused, int16 door_index, bool change);
void player_touch_swinging_door(int16 door_index, int16 object_index);

/*	needed for handle_tag_flipping		*/
bool try_and_change_tagged_sliding_door_states(int16, bool state);

/*	extern for try_and_toggle_control_panel, change_panel_state and translate_monster	*/
bool try_and_change_sliding_door_state(int16 door_index, bool state);

/*	needed for translate_monster	*/
bool sliding_door_can_be_opened(int16 door_index, bool unknown);

/*	needed for changed_polygon	*/
void hit_door_trigger(int16 door_index, uint16 unknown);
