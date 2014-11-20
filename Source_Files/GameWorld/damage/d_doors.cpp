/*
D_DOORS.CPP
	Original code by Paranoid Productions. 
	Reverse engineering done by Asylum with help from Hopper.
	...am I doing it right?
*/

#include "cseries.h"

#include "world.h"
#include "map.h"
#include "/damage/d_doors.h"
#include "/damage/d_emux86.h"

#define		copy_worldpoint3d(dest, src)	(dest->x = src->x, dest->y = src->y, dest->z = src->z)
#define		RIGHTANGLE(angle)				((angle + 128) & 0x1FF)

enum swinging_door_flags
{
	SW_DOOR_SNAP_TO_VERTEX = 1,
};

struct swinging_door_data
{
	short textured_objects[4];
	short center_object, top_object, bottom_object;
	
	world_point3d p0, p1, p2, p3;
	
	short owner_polygon;
	short definition_index;
	short flags;
	short field_2C, field_2E;
	
	short radius, height, length, width, yaw;
	
	short field_3A, field_3C, field_3E, field_40, field_42;
	bool a_boolean, door_open;
};

static void find_center_of_door(swinging_door_data *door, world_point2d *p);

static void translate_door_object(swinging_door_data *door_, short door_object, world_point3d *translation);
static void calculate_doors_points(swinging_door_data *door, world_point2d *hinge);
static void calculate_hinge_point(swinging_door_data *door, world_point2d *obj, world_point3d *hinge);
static bool door_is_obstructed(short permutation);
static void adjust_endpoints(short permutation);

#define		SignbitMul4(x)		(x86Emu::Signbit<int>(x) * 4)
/*
	finds the center of a swinging door
	if something isn't working right, check this one
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
	translate_point2d((world_point2d *)&door->p2, door->length, door->yaw);
	
	copy_worldpoint3d(&door->p1, &door->p0);
	translate_point2d((world_point2d *)&door->p1, door->width, v4);
	

	copy_worldpoint3d(&door->p3, &door->p2);
	translate_point2d((world_point2d *)&door->p3, door->width, v4);
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
	for(int i = 0; i < polygon->vertex_count; ++i)
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
	hinge->x += (hinge->x >= polygon->center.x) ? -2 : 2;
	hinge->y += (hinge->y >= polygon->center.y) ? -2 : 2;
}

/*
	checks to see if something is in the way of a sliding door
*/
static bool door_is_obstructed(short permutation)
{
	sliding_door_data *sliding_door = &sliding_doors[permutation];
	short object_index = map_polygons[sliding_door->polygon_index].first_object;

	while(object_index != NONE)
	{
		object_data *object = &objects[object_index];
		int flags_masked = object->flags & 7;
		if(flags_masked != 2 && flags_masked != 1 && flags_masked != 5)	
		{
			object_index = object->next_object;
			continue;
		}

		short xoffset = sliding_door->endpoint_x_offset;
		short yoffset = sliding_door->endpoint_y_offset;
		endpoint_data *endpoints = &map_endpoints[sliding_door->endpoint_data_index0];
		if(xoffset)
		{
			if(xoffset <= 0 && offset + endpoints->vertex.x <= object->location.x + 256)
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
static void adjust_endpoints(short permutation)
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
