/*
DOORS.CPP
	Original code by Paranoid Productions. 
	Reverse engineering done by Asylum with help from Hopper.
	...am I doing it right?
*/

#define		DISABLE_DOOR_SFX

#include "cseries.h"

#include "map.h"
#include "FilmProfile.h"
#include "interface.h"
#include "monsters.h"
#include "preferences.h"
#include "projectiles.h"
#include "effects.h"
#include "player.h"
#include "platforms.h"
#include "lightsource.h"
#include "media.h"
#include "scenery.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <list>
#include <type_traits>
#include <cassert>
#include "framework/emulation/x86/emux86.hpp"

#include "swinging_door_definitions.hpp"
#include "doors.hpp"

#define		sameType(t1, t2)	(std::is_same<t1, t2>::value)
#define		isPtr(t)		(std::is_pointer<t>::value)

std::vector<swinging_door_data> SwingingDoorList(MAX_SWINGING_DOORS);
std::vector<sliding_door_data> SlidingDoorList(MAX_SLIDING_DOORS);

template<typename T1, typename T2> 
void copyWorldPoint(T1 destination, T2 src)
{
	static_assert( isPtr(T1) && isPtr(T2), "must be a pointer");
		
	static_assert(( sameType(T1, world_point3d*) || sameType(T1, world_point2d*))
		&&
		( sameType(T2, world_point3d*) || sameType(T2, world_point2d*)),
		"copyWorldPoint requires that both types be world_point* types"
		);
	
	const world_point2d* source 	= reinterpret_cast<world_point2d*>(src);
	world_point2d* dest 		= reinterpret_cast<world_point2d*>(dest);
	
	if( sameType(T1, world_point2d) )
	{
		dest->x = source->x;
		dest->y = source->y;
	}
	else if( sameType(T1, world_point3d) )
	{
		dest->x = source->x;
		dest->y = source->y;
		if(sameType(T1, T2))
			reinterpret_cast<world_point3d*>(dest)->z 
			= reinterpret_cast<world_point3d*>(src)->z;
	}
}

#define		copy_worldpoint3d(dest, src)	copyWorldPoint<decltype(dest), decltype(src)>(dest, src)


#define		RIGHTANGLE(angle)				((angle + 128) & 0x1FF)

enum swinging_door_flags
{
	SW_DOOR_SNAP_TO_VERTEX = 1,
};

static void find_center_of_door(swinging_door_data *door, world_point2d *p);

static void translate_door_object(swinging_door_data *door_, int16 door_object, world_point3d *translation);
static void calculate_doors_points(swinging_door_data *door, world_point2d *hinge);
static void calculate_hinge_point(swinging_door_data *door, world_point2d *obj, world_point3d *hinge);
static bool door_is_obstructed(int16 permutation);
static void adjust_endpoints(int16 permutation);

static void swing_points(swinging_door_data* door, angle theta);
static void play_swinging_door_sound(ix index, int16 sound_type, int16 def);

static void reverse_direction_of_door(const ix index);
static bool polygon_contains_swinging_door(const int16 polygon_index, int16 * door_index);

static void get_swinging_door_dimensions(const ix swinging_door_index, world_distance * radius, world_distance * height);

static void calculate_doors_moving_points(sliding_door_data *door);
static void calculate_moving_lines(sliding_door_data *door);
static int16 door_associated_with_polygon(int16 polygon_index);
static void activate_nearby_doors(int16 caller_index, bool state);

static void open_door(int16 door_index);
static void close_door(int16 door_index);

static void adjust_door_texture(sliding_door_data *door, int16 adjustment);
static void play_door_sound(int16 door_index, sliding_door_sound_t sound_code);

void update_doors()
{
	//stub :C
}

void update_swinging_doors()
{
	//stub :C
}

#define		SignbitMul4(x)		(x86Emu::Signbit<int>(x) * 4)
/*
	finds the center of a swinging door
	if something isn't working right, check this one
	
	This is SO beyond me. For some reason this code sequence keeps popping up:
	a sbb instruction, then a bit shift >> 31, which would be extracting the sign bit
	then it multiplies the result of the shift
	
	that sequence really messes with hexrays, causing it to claim that there's a bitwise shift
	by 60 bits or some ridiculous number
*/
static void find_center_of_door(swinging_door_data *door, world_point2d *p)
{
	
	int v3 = door->p2.x + door->p0.x + door->p1.x;
	int v4 = door->p3.y + door->p2.y + door->p0.y + door->p1.y;
	
	p->x = (door->p3.x + v3 - SignbitMul4(door->p3.x + v3) + SignbitMul4(door->p3.x + v3)) * 4;
	p->y = (v4 - ((SignbitMul4(v4) + SignbitMul4(v4)))) * 4;
}

/*
	Sets up all four corners of a swinging door
*/
static void calculate_doors_points(swinging_door_data *door, world_point2d *hinge)
{
	polygon_data *polygon = &map_polygons[door->owner_polygon];
	angle v4 = RIGHTANGLE(door->yaw);

	door->p0.x = hinge->x;
	door->p0.y = hinge->y;
	door->p0.z = polygon->floor_height;
	
	copy_worldpoint3d(&door->p2, &door->p0);
	translate_point2d(reinterpret_cast<world_point2d *>(&door->p2), 
			door->length, door->yaw);
	
	copy_worldpoint3d(&door->p1, &door->p0);
	translate_point2d(reinterpret_cast<world_point2d *>(&door->p1), 
			door->width, v4);
	
	copy_worldpoint3d(&door->p3, &door->p2);
	translate_point2d(reinterpret_cast<world_point2d *>(&door->p3), 
				door->width, v4);
}

/*
	calculates the hinge of a swinging door, snapping the map object coordinates to the nearest vertex 
	to keep placement neat
*/
static void calculate_hinge_point(swinging_door_data *door, world_point2d *obj, world_point3d *hinge)
{
	polygon_data *polygon = &map_polygons[door->owner_polygon];
	
	hinge->x = obj->x;
	hinge->y = obj->y;
	hinge->z = polygon->floor_height;
	
	//door does not need to be snapped to the nearest vertex. we're done
	if(!(door->flags & SW_DOOR_SNAP_TO_VERTEX))
		return;

	world_distance last_vertex_distance = NONE;
	
	//correct the map object placement
	for(ix i = 0; i < polygon->vertex_count; ++i)
	{
		endpoint_data *endpoint = &map_endpoints[polygon->endpoint_indexes[i]];
		
		//calculate the distance from the map object to the current vertex
		world_distance current_vertex_distance = distance2d(obj, &endpoint->vertex);
		
		//if this is the first vertex or this vertex is closer than the previous closest vertex
		if(last_vertex_distance == NONE || current_vertex_distance < last_vertex_distance)
		{
			//then we save the distance for comparison with the next vertex
			last_vertex_distance = current_vertex_distance;
			//and snap the hinge to the vertex
			hinge->x = endpoint->vertex.x;
			hinge->y = endpoint->vertex.y;
		}
	}
	hinge->x += hinge->x >= polygon->center.x ? -2 : 2;
	hinge->y += hinge->y >= polygon->center.y ? -2 : 2;
}

/*
	checks to see if something is in the way of a sliding door
*/
static bool door_is_obstructed(int16 permutation)
{
	sliding_door_data *sliding_door = &sliding_doors[permutation];
	int16 object_index = map_polygons[sliding_door->polygon_index].first_object;

	while(object_index != NONE)
	{
		object_data *object = get_object_data(object_index);
		int flags_masked = object->flags & 7;
		if(flags_masked != 2 && flags_masked != 1 && flags_masked != 5)	
		{
			object_index = object->next_object;
			continue;
		}

		int16 xoffset = sliding_door->endpoint_x_offset;
		int16 yoffset = sliding_door->endpoint_y_offset;
		endpoint_data *endpoints = &map_endpoints[sliding_door->endpoint_data_index0];
		if(xoffset)
		{
			if(xoffset <= 0 && xoffset + endpoints->vertex.x <= object->location.x + 256)
				return true;
			else if ( endpoints->vertex.x + xoffset >= object->location.x - 256 )
				return true;
		}
		else if(
		(yoffset <= 0 && yoffset + endpoints->vertex.y <= object->location.y + 256) 
		||
				endpoints->vertex.y + yoffset >= object->location.y - 256)
			return true;
		
		object_index = object->next_object;
	}
	return false;
}
/*
	adjust a sliding door's endpoints
*/
#define		IS_LINE_SOLID(line)				(line->flags & SOLID_LINE_BIT)
#define		IS_LINE_TRANSPARENT(line)		(line->flags & TRANSPARENT_LINE_BIT)
static void adjust_endpoints(int16 permutation)
{
	sliding_door_data *door = &sliding_doors[permutation];
	line_data *l0 = &map_lines[door->line_data_index0];
	line_data *l1 = &map_lines[door->line_data_index1];
	line_data *l2 = &map_lines[door->line_data_index2];
	line_data *l3 = &map_lines[door->line_data_index3];
	line_data *l4 = &map_lines[door->line_data_index4];
	endpoint_data *end0 = &map_endpoints[door->endpoint_data_index0];
	endpoint_data *end1 = &map_endpoints[door->endpoint_data_index1];
	
	SET_ENDPOINT_SOLIDITY(		end0,	IS_LINE_SOLID(l0)		||	IS_LINE_SOLID(l1)		||	IS_LINE_SOLID(l3)		);
	SET_ENDPOINT_TRANSPARENCY(	end0,	IS_LINE_TRANSPARENT(l0)	&&	IS_LINE_TRANSPARENT(l1)	&&	IS_LINE_TRANSPARENT(l3)	);
	
	SET_ENDPOINT_SOLIDITY(		end1,	IS_LINE_SOLID(l0)		||	IS_LINE_SOLID(l2)		||	IS_LINE_SOLID(l4)		);
	SET_ENDPOINT_TRANSPARENCY(	end1,	IS_LINE_TRANSPARENT(l0)	&&	IS_LINE_TRANSPARENT(l2)	&&	IS_LINE_TRANSPARENT(l4)	);
}

/*
	this one had a lot of gotos, so its a bit weird looking.
*/
static void play_swinging_door_sound(ix index, int16 sound_type, int16 def)
{
	int16 v2;
	int16 sound;
	
	const swinging_door_data * door = &swinging_doors[index];
	const swinging_door_definition *definition = &swinging_door_definitions[door->definition_index];
	
	auto polygon = get_object_data(door->center_object)->polygon;
	
	auto playsound = [polygon](int16 s) 
	{
		play_polygon_sound(polygon, s);
		
		#ifndef	DISABLE_DOOR_SFX
		cause_ambient_sound_source_update();
		#endif	
	};
	
	if (sound_type < 1 )
	{
		if( !sound_type )
			def = definition->sound0;
		if( def == NONE )
			return;
		playsound(def);
		return;
	}
	if(sound_type > 1)
	{
		if ( sound_type == 2 )
		{
			sound = definition->sound1;
			if ( sound == NONE )
				return;
			playsound(sound);
			return;
		}
		if(def == NONE)
			return;
		playsound(def);
		return;
	}
	sound = definition->sound2;
	if(sound != NONE)
	{
		playsound(sound);
		return;
	}
}

/*	simple one	*/
static void get_swinging_door_dimensions(const ix swinging_door_index, 
	world_distance * radius, world_distance * height)
{
	const swinging_door_data * swinging_door = &swinging_doors[ swinging_door_index ];
	*radius = swinging_door->radius;
	*height = swinging_door->height;
}

static bool polygon_contains_swinging_door(const int16 polygon_index, int16 * door_index)
{
	bool contains_swinging_door = false;
	*door_index = NONE;
	Object *object;
	
	for( auto i = map_polygons[ polygon_index ].first_object; i != NONE; i = object->next_object )
	{
		object = get_object_data(i);
		
		if( (object->flags & 7) == 6 )
		{
			*door_index = i;
			contains_swinging_door = true;
			break;
		}
	}
	return contains_swinging_door;
}

static void reverse_direction_of_door(const ix index)
{
	int16 v5; 
	
	swinging_door_data *door = &swinging_doors[index];
	auto direction = door->related_to_direction1;

	if ( direction == 2 )
		door->related_to_direction1 = 3;
	else if ( direction == 3 )
		door->related_to_direction1 = 2;
	
	auto v4 = door->related_to_direction2;
	
	if(v4)
		v5 = v4 < 0 ? -1 : 1;
	else
		v5 = 0;
		
	auto v6 = v5;
	
	auto v7 = door->related_to_direction0 < 0 ? -door->related_to_direction0 : door->related_to_direction0;
	auto v8 = door->related_to_direction2 < 0 ? -door->related_to_direction2 : door->related_to_direction2;

	//absolute value of direction0 - absolute value of direction 2
	auto v9 = v7 - v8;
	
	door->related_to_direction2 = v6 > 0 ? -v9 : v9;
}

static void swing_points(swinging_door_data* door, angle theta)
{
	theta &= 0x1FF;
	
	rotate_point2d((world_point2d*) &door->p1, (world_point2d*)&door->p0, theta);
	rotate_point2d((world_point2d*) &door->p2, (world_point2d*)&door->p0, theta);
	rotate_point2d((world_point2d*) &door->p3, (world_point2d*)&door->p0, theta);
}

void calculate_moving_lines(sliding_door_data *door)
{
	polygon_data *poly1 = &map_polygons[door->polygon_index1];
	int16 v14 = 0;
	polygon_data *poly2 = &map_polygons[door->polygon_index];
	
	do
	{
		int16 v13 = door->endpoint_indexes[v14];
		door->line_indexes[v14] = NONE;
		int v5 = 0;
		while( v5 < poly1->vertex_count )
		{
			if( v5 == door->line_data_index0 )
				++v5;
			else
			{
				line_data *v11 = &map_lines[ poly1->line_indexes[v5] ];
				if ( v13 == v11->endpoint_indexes[0] || v13 == v11->endpoint_indexes[1] )
				{
					door->line_indexes[v14] = poly1->line_indexes[v5];
					break;
				}
				++v5;
			}
		}
		int v6 = 0;
		while ( v6 < poly2->vertex_count )
		{
			auto v7 = poly2->line_indexes[ v6 ];
			
			if ( v7 == poly1->line_indexes[door->line_data_index0] )
				++v6;
			else
			{
				line_data *v12 = &map_lines[v7];
				
				if( v13 == v12->endpoint_indexes[0] || v13 == v12->endpoint_indexes[1] )
				{
					door->line_indexes[v14 + 2] = poly2->line_indexes[ v6 ];
					break;
				}
				
				++v6;
			}
		}
		++v14;
	}
	while ( v14 < 2 );
	
	line_data *v8 = &map_lines[door->line_indexes[0]];
	line_data *v9 = &map_lines[door->line_indexes[1]];
	
	if( v8->clockwise_polygon_side_index == NONE )
		door->field_22 = v8->counterclockwise_polygon_side_index;
	else
		door->field_22 = v8->clockwise_polygon_side_index;
	
	if( v9->clockwise_polygon_side_index == NONE )
		door->field_24 = v9->counterclockwise_polygon_side_index;
	else
		door->field_24 = v9->clockwise_polygon_side_index;
}

/*
	this one looks a bit off.
	need to step through the original one
*/
static void calculate_doors_moving_points(sliding_door_data *door)
{
	int v2 = 0;
	polygon_data *v3 = &map_polygons[door->polygon_index1];
	
	while( v2 < v3->vertex_count )
	{
		//oh hexrays
		polygon_data* v4 = (polygon_data*) ((uint8 *)v3 + 2 * v2);
		
		if( find_adjacent_polygon(door->polygon_index1, v4->line_indexes[0]) == door->polygon_index )
		{
			line_data* line = &map_lines[v4->line_indexes[0]];
			door->line_data_index0 = v2;
			door->endpoint_indexes[0] = line->endpoint_indexes[0];
			door->endpoint_indexes[1] = line->endpoint_indexes[1];
		}
		++v2;
	}
}

bool door_is_open(int16 door_index)
{
	assert(door_index >= 0 && door_index < sliding_door_count);
	
	const sliding_door_data* door = &sliding_doors[door_index];
	return door->state == _sliding_door_is_absolutely_open || door->state == _sliding_door_is_open;
}

bool door_is_active(int16 door_index)
{
	int16 doorState;
	
	assert(door_index >= 0 && door_index < sliding_door_count );

	const sliding_door_data* door = &sliding_doors[door_index];
	return door->state == _sliding_door_is_open || doorState == _sliding_door_is_active;
}

bool door_is_absolutely_open(int16 door_index)
{
	  return sliding_doors[door_index].state == _sliding_door_is_absolutely_open;
}

static int16 door_associated_with_polygon(int16 polygon_index)
{
	for( ix i = 0;  i < dynamic_world->sliding_door_count; ++i )
	{
		sliding_door_data* door = &sliding_doors[i];
		
		if(door->polygon_index == polygon_index || door->polygon_index1 == polygon_index )
			return i;
	}
	return NONE;
}

void activate_nearby_doors(int16 caller_index, bool state)
{
	int16 associated_door_index; 

	sliding_door_data *caller 	= &sliding_doors[caller_index];
	int16 polygon_index 		= caller->polygon_index;
	ix line_index 			= 0;
	polygon_data *caller_polygon 	= &map_polygons[caller->polygon_index];
	
	//todo: this was obviously originally a for loop
	while( line_index < caller_polygon->vertex_count )
	{
		auto adjacent_polygon = find_adjacent_polygon(polygon_index, 
						caller_polygon->line_indexes[line_index]);
						
		if( adjacent_polygon == NONE || adjacent_polygon == caller->polygon_index1
		|| (associated_door_index = door_associated_with_polygon(adjacent_polygon), associated_door_index == NONE) )
		{
			++line_index;
			continue;
		}
		
		sliding_door_data *target_door = &sliding_doors[associated_door_index];
		if( target_door->flags & 0x40 && target_door->other_flags & 2 )
		{
			++line_index;
			continue;
		}
		
		if( state )
		{
			if ( target_door->state == _sliding_door_is_active )
			{
				target_door->state = _sliding_door_is_open;
				++line_index;
			}
			else
			{
				if ( target_door->state != _sliding_door_is_closed || target_door->other_flags & 1 )
				{
					++line_index;
					continue;
				}
				open_door(associated_door_index);
				++line_index;
			}
		}
		else
		{
			if ( target_door->state != _sliding_door_is_open )
			{
				if ( target_door->state != _sliding_door_is_absolutely_open || target_door->other_flags & 1 )
				{
					++line_index;
					continue;
				}
				close_door(associated_door_index);
				++line_index;
			}
			else
			{
				target_door->state = _sliding_door_is_active;
				++line_index;
			}
		}
	}
}

static void open_door(int16 door_index)
{
	sliding_door_data *door = &sliding_doors[door_index];
	
	door->state = _sliding_door_is_open;
	
	map_lines[ door->line_indexes[2] ].flags &= 0xBFFF;
	map_lines[ door->line_indexes[3] ].flags &= 0xBFFF; 
	
	adjust_endpoints(door_index);
	play_door_sound(door_index, 0);
	
	if ( door->flags & 2 )
	{
		door->other_flags |= 1;
		activate_nearby_doors(door_index, true);
		door->other_flags &= 0xFE;
	}
	
	door->other_flags |= 2;
	assume_correct_switch_position(9, door->polygon_index1, true);
}

static void close_door(int16 door_index)
{
	sliding_door_data *door = &sliding_doors[door_index];
	door->state 		= _sliding_door_is_active;
	
	if( door_is_obstructed(door_index) )
	{
		play_door_sound(door_index, 5);
		door->abool = true;
	}
	else
	{
		map_lines[ door->line_indexes[0] ].flags |= 0x4000;
		map_lines[ door->line_indexes[1] ].flags |= 0x4000;
		adjust_endpoints(door_index);
		play_door_sound(door_index, 2);
		door->abool = false;
	}
	if ( door->flags & 2 )
	{
		door->other_flags |= 1;
		activate_nearby_doors(door_index, false);
		door->other_flags &= 0xFE;
	}
	door->other_flags |= 2;
	assume_correct_switch_position(9, door->polygon_index1, true);
}

void player_touch_sliding_door(int16 unused, int16 door_index, bool change)
{
	sliding_door_data *door = &sliding_doors[door_index];

	if( door->flags & 8 && (!change || door->flags & 4) )
	{
		if( door->flags & 0x40 && door->other_flags & 2 )
		{
			play_door_sound(door_index, 6);
			return;
		}
		switch( door->state )
		{
		case _sliding_door_is_open:

			door->state = _sliding_door_is_active;
			
			play_door_sound(door_index, 4);
			if ( door->flags & 2 )
			{
				door->other_flags |= 1;
				activate_nearby_doors(door_index, false);
				door->other_flags &= 0xFE;
			}
			door->other_flags |= 2;
			break;
		case _sliding_door_is_absolutely_open:
			close_door(door_index);
			door->other_flags |= 2;
			break;
			
		case _sliding_door_is_active:
			door->state = _sliding_door_is_open;
			play_door_sound(door_index, 4);
			if ( door->flags & 2 )
			{
				door->other_flags |= 1;
				activate_nearby_doors(door_index, true);
				door->other_flags &= 0xFE;
			}
			door->other_flags |= 2;
			break;
			
		case _sliding_door_is_closed:
			open_door(door_index);
			door->other_flags |= 2;
			break;
			
		default:
			door->other_flags |= 2;
			break;
		}

	}
	else
		play_door_sound(door_index, 6);
}

bool try_and_change_tagged_sliding_door_states(int16 tag, bool state)
{
	if(!tag)
		return false;
		
	bool changed_state 	= false;
	sliding_door_data *door = sliding_doors;
	int door_index 		= 0;
	
	while( door_index < sliding_door_count )
	{
		if( door->tag == tag )
		{
			if( try_and_change_sliding_door_state(door_index, state) )
				changed_state = true;
		}
		++door_index;
		++door;
	}
	return changed_state;
}

bool try_and_change_sliding_door_state(int16 door_index, bool state)
{
	sliding_door_data *door = &sliding_doors[door_index];
	
	if( state && door->flags & 8 )
	{
		if( !(door->flags & 0x40) || !(door->other_flags & 2) )
		{
			player_touch_sliding_door(NONE, door_index, false);
			return true;
		}
	}
	return false;
}

static void adjust_door_texture(sliding_door_data *door, int16 adjustment)
{
	auto side_index = door->side_indexes[ door->door_texture_side ];
	
	if ( side_index == NONE )
		return
	side_data *side = &map_sides[side_index];

	if ( side->type == 0 || side->type == 1 || side->type == 4 )
		side->primary_texture.x0 -= adjustment;
}

static void play_door_sound(int16 door_index, sliding_door_sound_t sound_code)
{
	sliding_door_data *door = &sliding_doors[door_index];
	door_sound_data *sounds = &door_sounds[door->sound_index];
	
	int16 sound = NONE;
	
	switch ( sound_code )
	{
		
	case _sliding_door_sound_opening:
		sound = sounds->field_0;
		break;
		
	case _sliding_door_sound_unknown_1:
		sound = sounds->field_2;
		break;
		
	case _sliding_door_sound_unknown_3:
		sound = sounds->field_6;
		break;
		
	case _sliding_door_sound_closing:
		sound = sounds->field_4;
		break;
		
	case _sliding_door_sound_obstructed_closing:
		sound = sounds->field_8;
		break;
		
	case _sliding_door_sound_player_touched_unchangeable:
		sound = sounds->field_A;
		break;
		
	case _sliding_door_sound_player_touched_active:
		sound = sounds->field_C;
		break;
		
	default:
		//could this ever happen?
		assert(false);
	}
	if(sound == NONE)
		return;
	play_polygon_sound(door->polygon_index1, sound);
		
#ifndef	DISABLE_DOOR_SFX
	cause_ambient_sound_source_update();
#endif
}

bool sliding_door_can_be_opened(int16 door_index, bool unknown)
{
	sliding_door_data *door = &sliding_doors[door_index];

	if( door->flags & 4 )
	{
		if( unknown && door->flags & 0x10 || !unknown && door->flags & 8 )
			return true;
	}
	return false;
}

void hit_door_trigger(int16 door_index, uint16 unknown)
{
	sliding_door_data *door = &sliding_doors[door_index];
	
	if( door->flags & 0x40 && door->other_flags & 2 || unknown < 0x14 )
		return;
	
	if ( unknown > 0x14 )
	{
		if ( unknown == 21 && door->flags & 8 && door->state == _sliding_door_is_absolutely_open )
			close_door(door_index);
	}
	else
	{
		if ( door->flags & 8 && door->state == _sliding_door_is_closed )
			open_door(door_index);
	}
}

bool polygon_is_neighbor_to_door(int16 polygon_index, int16 *indexOut)
{
	assert(indexOut);
	*indexOut = NONE;
	for( ix i = 0; i < sliding_door_count; ++i )
	{
		if( sliding_doors[i].polygon_index == polygon_index )
		{
			*indexOut = i;
			break;
		}
	}
	return *indexOut != NONE;
}

int16 get_neighbor_polygon_index(int16 door_index)
{
	return sliding_doors[door_index].polygon_index;
}

int16 get_door_moving_sound(int16 door_index)
{
	return door_sounds[sliding_doors[door_index].sound_index].moving_sound;
}
