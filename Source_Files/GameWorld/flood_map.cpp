/*
FLOOD_MAP.CPP

	Copyright (C) 1991-2001 and beyond by Bungie Studios, Inc.
	and the "Aleph One" developers.
 
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

Sunday, June 5, 1994 3:25:22 PM

Friday, July 15, 1994 12:54:35 PM
	added visited_polygon array for quickly determining whether we have visited a polygon yet.
Monday, September 5, 1994 2:31:12 PM
	static memory allocated is 512 bytes (visited polygons) and 4k (flood nodes).
Wednesday, October 26, 1994 3:18:59 PM (Jason)
	added _flagged_breadth_first.

Jan 30, 2000 (Loren Petrich):
	Did some typecasts

Feb. 4, 2000 (Loren Petrich):
	Changed halt() to assert(false) for better debugging
*/

/*
	on very small maps, choose_random_flood_node() may not terminate
	for performance, maybe we should allow the caller to specify breadth-first instead of best-first.
*/

#include "cseries.h"
#include "map.h"
#include "flood_map.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>


/* ---------- constants */

#define MAXIMUM_FLOOD_NODES 255
#define UNVISITED NONE

/* ---------- structures */

#define NODE_IS_EXPANDED(n) ((n)->flags&(uint16)0x8000)
#define NODE_IS_UNEXPANDED(n) (!NODE_IS_EXPANDED(n))
#define MARK_NODE_AS_EXPANDED(n) ((n)->flags|=(uint16)0x8000)

struct node_data /* 16 bytes */
{
	static struct node_data& Get(const ix index);
	uint16 flags;
	
	int16 parent_node_index; /* node index of the node we came from to get here; only used for backtracking */
	int16 polygon_index; /* index of this polygon */
	int32 cost; /* the cost to evaluate this entry */

	int16 depth;

	int32 user_flags;
};

/* ---------- globals */

static int16 node_count 		= 0;
static int16 last_node_index_expanded 	= NONE;

static node_data *nodes 		= nullptr;
static int16 *visited_polygons 		= nullptr;

/* ---------- private prototypes */

static void add_node(int16 parent_node_index, int16 polygon_index, int16 depth, int32 cost, int32 user_flags);

/* ---------- code */

void allocate_flood_map_memory()
{
	// Made reentrant because this must be called every time a map is loaded
	if (nodes) 
		delete [] nodes;
	nodes = new node_data[MAXIMUM_FLOOD_NODES];
	if (visited_polygons) 
		delete [] visited_polygons;
	visited_polygons = new int16[MAXIMUM_POLYGONS_PER_MAP];
	assert(nodes && visited_polygons);
}

struct node_data& node_data::Get(const ix index)
{
	assert(nodes && index < MAXIMUM_FLOOD_NODES);
	return nodes[index];
}

/* 
	returns next polygon index or NONE if there are no more polygons left cheaper than maximum_cost 
*/
int16 flood_map(int16 first_polygon_index, int32 maximum_cost, cost_proc_ptr cost_proc, int16 flood_mode,
		void *caller_data)
{
	int16 lowest_cost_node_index, node_index;
	node_data *node;
	int16 polygon_index;
	int32 lowest_cost;

	/* initialize ourselves if first_polygon_index!=NONE */
	if(first_polygon_index != NONE)
	{
		/* clear the visited polygon array */
		objlist_set(visited_polygons, NONE, MAXIMUM_POLYGONS_PER_MAP);
		
		node_count = 0;
		last_node_index_expanded = NONE;
		
		int32 userFlags = 0;
		
		if(flood_mode == _flagged_breadth_first)
			userFlags = *static_cast<int32*>(caller_data);
			
		add_node(NONE, first_polygon_index, 0, 0, userFlags);
	}
	
	switch (flood_mode)
	{
		case _best_first:
			/* find the unexpanded node with the lowest cost */
			lowest_cost= maximum_cost, lowest_cost_node_index= NONE;
			for (node = nodes, node_index = 0; node_index<node_count; ++node_index, ++node)
			{
				if (NODE_IS_UNEXPANDED(node) && node->cost < lowest_cost)
				{
					lowest_cost_node_index	= node_index;
					lowest_cost		= node->cost;
				}
			}
			break;
		
		case _breadth_first:
		case _flagged_breadth_first:
			/* find the next unexpanded node in the list under maximum_cost */
			node_index = isNONE(last_node_index_expanded) ? 0 : last_node_index_expanded + 1;
			
			for (node = nodes+node_index; node_index < node_count; ++node_index, ++node)
			{
				if (node->cost < maximum_cost) 
					break;
			}
			if (node_index == node_count)
			{
				lowest_cost_node_index 	= NONE;
				lowest_cost 		= maximum_cost;
			}
			else
			{
				lowest_cost_node_index 	= node_index;
				lowest_cost 		= node->cost;
			}
			break;
		
		case _depth_first:
			/* implementation left to the caller (c.f., zen() in fareast.c) */
			assert(false);
		default:
			assert(false);
	}

	
	if( isNONE(lowest_cost_node_index) )
		return NONE;
	/* 
		if we found a node, mark it as expanded and add it's adjacent non-solid polygons 
		to the search tree 
	*/
	/* 
		for flood_depth() and reverse_flood_map(), remember which node we successfully expanded last 
	*/
	last_node_index_expanded = lowest_cost_node_index;

	/* get pointer to lowest cost node */
	assert(lowest_cost_node_index >= 0 && lowest_cost_node_index < node_count);
	node = &nodes[lowest_cost_node_index];

	polygon_data *polygon = get_polygon_data(node->polygon_index);
	assert(!POLYGON_IS_DETACHED(polygon));

	/* mark node as expanded */
	MARK_NODE_AS_EXPANDED(node);

	for(ix i = 0; i < polygon->vertex_count; ++i)		
	{
		auto destination_polygon_index = polygon->adjacent_polygon_indexes[i];
		
		const bool condition = 
			destination_polygon_index != NONE &&
			(maximum_cost != INT32_MAX || 
			visited_polygons[destination_polygon_index] == UNVISITED);
			
		if (!condition)
			continue;
	
		auto new_user_flags 	= node->user_flags;
		auto cost 		= polygon->area;
		
		if(cost_proc)
		{
			decltype(caller_data) costProcArg = caller_data;
			
			if(flood_mode == _flagged_breadth_first)
				costProcArg = static_cast<decltype(caller_data)>(&new_user_flags);
				
			cost = cost_proc(
					node->polygon_index, 
					polygon->line_indexes[i], 
					destination_polygon_index,
					costProcArg
				);
		}
		
		/* polygons with zero or negative costs are not added to the node list */
		if(cost > 0) 
			add_node(lowest_cost_node_index, destination_polygon_index, 
			node->depth + 1, lowest_cost+cost, new_user_flags);
	
	}
	
	polygon_index = node->polygon_index;
	
	if( flood_mode == _flagged_breadth_first ) 
		*static_cast<int32*>(caller_data) = node->user_flags;

	return polygon_index;
}

/* 
	walks backwards from the last node expanded, returning polygons as it goes; returns NONE
	when there are no more polygons to return.  this is useful for pathfinding: when
	flood_map() returns the destination polygon index, calling reverse_flood_map() will return
	the polygons traversed to reach the destination) 
*/
int16 reverse_flood_map()
{
	if (isNONE(last_node_index_expanded))
		return NONE;
	
	assert(last_node_index_expanded >= 0 && last_node_index_expanded < node_count);
	node_data *node = nodes + last_node_index_expanded;

	last_node_index_expanded = node->parent_node_index;
	auto polygon_index = node->polygon_index;

	return polygon_index;
}

/* returns depth (in polygons) at last_node_index_expanded */
int16 flood_depth()
{
	assert(last_node_index_expanded >= 0 && last_node_index_expanded < node_count);

	return isNONE( last_node_index_expanded ) ? 0 : nodes[ last_node_index_expanded ].depth;
}

#define MAXIMUM_BIASED_RETRIES 10

/* when looking for a random path, always choose a random node.  if bias is not NULL, then try
	and choose a destination in that direction */
void choose_random_flood_node(world_vector2d *bias)
{
	world_point2d origin;
	
	assert(node_count >= 1);
	find_center_of_polygon(nodes[0].polygon_index, &origin);
	
	if(node_count <= 1)
		return;
	
	bool suitable;
	short retries = MAXIMUM_BIASED_RETRIES;
	
	do
	{
		do
		{
			last_node_index_expanded = global_random(node_count);
		}
		while( NODE_IS_UNEXPANDED( nodes + last_node_index_expanded ) );

		/* if we have no bias, this node is automatically suitable if it has been expanded;
			if we have a bias, this node is only suitable if it is in the same general
			direction (by sign of dot product) as the bias */
		suitable = true;
		if(bias && --retries >=0)
		{
			const node_data *node = nodes + last_node_index_expanded;
			world_point2d destination;
			
			find_center_of_polygon(node->polygon_index, &destination);
			if (bias->i * (destination.x - origin.x) + bias->j * (destination.y - origin.y) < 0) 
				suitable = false;
		}
	}
	while (!suitable);

}

/* ---------- private code */

/* checks to see if the given node is already in the node list */
static void add_node(int16 parent_node_index, int16 polygon_index, int16 depth, int32 cost, int32 user_flags)
{
	if (node_count >= MAXIMUM_FLOOD_NODES)
		return;

	/* 
		see if this polygon already exists in the node list anywhere 
	*/
	assert( polygon_index >= 0 && polygon_index < dynamic_world->polygon_count );
	
	auto node_index 		= visited_polygons[polygon_index];
	const bool nodeIsUnvisited	= node_index == UNVISITED;
	
	if(nodeIsUnvisited)
		node_index = node_count;
		
	node_data &node = node_data::Get(node_index);
	
	if( !nodeIsUnvisited )
	{
		if(NODE_IS_EXPANDED(&node) || node.cost <= cost)
			return;
	}
	
	if (node_index == node_count)
		++node_count;
	
	node.flags 			= 0;
	node.parent_node_index 		= parent_node_index;
	node.polygon_index 		= polygon_index;
	node.depth 			= depth;
	node.cost 			= cost;
	node.user_flags 		= user_flags;
	
	assert(polygon_index >= 0 && polygon_index < dynamic_world->polygon_count);
	visited_polygons[polygon_index] = node_index;
}
