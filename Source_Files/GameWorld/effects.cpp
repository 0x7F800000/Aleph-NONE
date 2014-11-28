/*
EFFECTS.CPP

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

Friday, May 27, 1994 10:40:13 AM

Saturday, May 28, 1994 2:13:08 AM
	hopefully effects can be used for sparks.  most effects disappear when their animations
	terminate.
Friday, September 30, 1994 5:48:25 PM (Jason)
	hopefully.  ha.  added sound-only effects.
Wednesday, February 1, 1995 12:58:17 AM  (Jason')
	teleporting item effects.

Feb 6, 2000 (Loren Petrich):
	Added access to size of effect-definition structure

Aug 30, 2000 (Loren Petrich):
	Added stuff for unpacking and packing
*/

#include "cseries.h"
#include "map.h"
#include "interface.h"
#include "effects.h"
#include "SoundManager.h"
#include "lua_script.h"

#include "Packing.h"

/*
	ryan reports get_object_data() failing on effect->data after a teleport effect terminates
*/

/* import effect definition constants, structures and globals */
#include "effect_definitions.h"

// Moved the definition over to map.cpp

static effect_definition *get_effect_definition(const short type);

/* ---------- code */

Effect *get_effect_data(const short effect_index)
{
	Effect *effect = GetMemberWithBounds(effects,effect_index,MAXIMUM_EFFECTS_PER_MAP);
	
	vassert(effect, csprintf(temporary, "effect index #%d is out of range", effect_index));
	vassert(SLOT_IS_USED(effect), csprintf(temporary, "effect index #%d (%p) is unused", effect_index, effect));
	
	return effect;
}

class Effect& Effect::Get(const ix index)
{
	assert(index < MAXIMUM_EFFECTS_PER_MAP);
	assert(index >= 0);
	return EffectList[index];
}

// LP change: moved down here because it refers to effect definitions
effect_definition *get_effect_definition(const short type)
{
	return GetMemberWithBounds(effect_definitions,type,NUMBER_OF_EFFECT_TYPES);
}


short new_effect(world_point3d *origin, short polygon_index, short type, angle facing)
{
	
	if (polygon_index == NONE)
		return NONE;

	effect_definition *definition = get_effect_definition(type);
	
	// LP change: idiot-proofing
	if (!definition) 
		return NONE;
	
	if (definition->flags & _sound_only)
	{
		shape_animation_data *animation = 
			get_shape_animation_data(
				BUILD_DESCRIPTOR(definition->collection, definition->shape)
				);
			
		if (!animation) 
			return NONE;
		
		play_world_sound(polygon_index, origin, animation->first_frame_sound);
		return NONE;
	}

	for (ix effectIndex = 0; effectIndex < MAXIMUM_EFFECTS_PER_MAP; ++effectIndex)
	{
		Effect &effect = Effect::Get(effectIndex);
		if( !SLOT_IS_FREE(&effect) )
			continue;
		
		auto object_index = new_map_object3d(origin, polygon_index, 
			BUILD_DESCRIPTOR(definition->collection, definition->shape), 
			facing);
		
		if (object_index == NONE)
			break;
		
		Object &object = Object::Get(object_index);
		
		effect.type		= type;
		effect.flags		= 0;
		effect.object_index	= object_index;
		effect.data		= 0;
		effect.delay		= definition->delay 
					? global_random( definition->delay )
					: 0;
		MARK_SLOT_AS_USED(&effect);
		
		SET_OBJECT_OWNER(&object, _object_is_effect);
		object.sound_pitch = definition->sound_pitch;
		
		if (effect.delay) 
			SET_OBJECT_INVISIBILITY(&object, true);
		if (definition->flags & _media_effect) 
			SET_OBJECT_IS_MEDIA_EFFECT(&object);
			
		if(effectIndex == MAXIMUM_EFFECTS_PER_MAP) 
			return NONE;
		return effectIndex;
	}
	return NONE;
}

/* assumes t==1 tick */
void update_effects()
{
	Effect *effect;
	short effect_index;
	
	for (effect_index= 0, effect= effects; effect_index<MAXIMUM_EFFECTS_PER_MAP; ++effect_index, ++effect)
	{
		if (!SLOT_IS_USED(effect))
			continue;
		Object *object 			= get_object_data(effect->object_index);
		effect_definition *definition	= get_effect_definition(effect->type);
		// LP change: idiot-proofing
		if (!definition) 
			continue;
		
		if (effect->delay)
		{
			/* 
				handle invisible, delayed effects 
			*/
			if (!--effect->delay)
			{
				SET_OBJECT_INVISIBILITY(object, false);
				play_object_sound(effect->object_index, definition->delay_sound);
			}
			continue;
		}

		/* 
			update our object's animation 
		*/
		animate_object(effect->object_index);
		
		/* 
			if the effect's animation has terminated and we're supposed to deactive it, do so 
		*/
		if (((GET_OBJECT_ANIMATION_FLAGS(object)&_obj_last_frame_animated)&&(definition->flags&_end_when_animation_loops)) ||
			((GET_OBJECT_ANIMATION_FLAGS(object)&_obj_transfer_mode_finished)&&(definition->flags&_end_when_transfer_animation_loops)))
		{
			remove_effect(effect_index);
			
			/* if we're supposed to make another item visible, do so */
			if (definition->flags & _make_twin_visible)
			{
				Object *object = get_object_data(effect->data);
				
				SET_OBJECT_INVISIBILITY(object, false);
			}
		}
	}
}

void remove_effect(int16 effect_index)
{
	Effect &effect = Effect::Get(effect_index);
	remove_map_object(effect.object_index);
	
	//lua hook
	L_Invalidate_Effect(effect_index);
	
	MARK_SLOT_AS_FREE(&effect);
}

void remove_all_nonpersistent_effects()
{
	for(ix effectIndex = 0; effectIndex < MAXIMUM_EFFECTS_PER_MAP; ++effectIndex)
	{
		Effect &effect = Effect::Get(effectIndex);
		
		if (!SLOT_IS_USED(&effect))
			continue;
		
		const effect_definition *definition = get_effect_definition(effect.type);
		// LP change: idiot-proofing
		if (!definition) 
			continue;

		if (definition->flags & (_end_when_animation_loops | _end_when_transfer_animation_loops) )
			remove_effect(effectIndex);
	}
}

void mark_effect_collections(short effect_type, bool loading)
{
	if (effect_type == NONE)
		return;
	
	const effect_definition *definition = get_effect_definition(effect_type);
	// LP change: idiot-proofing
	if (!definition) 
		return;

	/* 
		mark the effect collection 
	*/
	if(loading)
		mark_collection_for_loading(definition->collection);
	else
		mark_collection_for_unloading(definition->collection);
}

void teleport_object_out(short object_index)
{
	Object &object = Object::Get(object_index);
	
	if( object.isInvisible() )
		return;

	auto effect_index = new_effect(&object.location, 
					object.polygon, 
					_effect_teleport_object_out, 
					object.facing);
	
	if (effect_index == NONE)
		return;
	
	Effect &effect		= Effect::Get(effect_index);
	Object &effectObject 	= Object::Get(effect.object_index);
	
	// make the effect look like the object
	effectObject.shape		= object.shape;
	effectObject.sequence		= object.sequence;
	effectObject.transfer_mode	= _xfer_fold_out;
	effectObject.transfer_period	= TELEPORTING_MIDPOINT;
	effectObject.transfer_phase	= 0;
	effectObject.flags		|= object.flags & ( _object_is_enlarged | _object_is_tiny );
	
	// make the object invisible
	SET_OBJECT_INVISIBILITY(&object, true);

	play_object_sound(effect.object_index, Sound_TeleportOut()); /* teleport in sound, at destination */
}

/*
	if the given object isn't already teleporting in, do so
*/
void teleport_object_in(short object_index)
{
	for(ix effectIndex = 0; effectIndex < MAXIMUM_EFFECTS_PER_MAP; ++effectIndex)
	{
		Effect &effect = Effect::Get(effectIndex);
		
		if( !SLOT_IS_USED(&effect) )
			continue;
		
		if (effect.type == _effect_teleport_object_in && effect.data == object_index)
		{
			object_index = NONE;
			break;
		}
	}
	
	if (object_index == NONE)
		return;
	
	Object &object = Object::Get(object_index);

	auto effect_index = new_effect(&object.location, 
				object.polygon, 
				_effect_teleport_object_in, 
				object.facing
				);
	
	if (effect_index == NONE)
		return;
	Effect &effect			= Effect::Get(effect_index);
	
	effect.data			= object_index;
	
	Object &effectObject		= Object::Get(effect.object_index);
	effectObject.shape		= object.shape;
	effectObject.transfer_mode	= _xfer_fold_in;
	effectObject.transfer_period	= TELEPORTING_MIDPOINT;
	effectObject.transfer_phase	= 0;
	effectObject.flags		|= object.flags & ( _object_is_enlarged | _object_is_tiny );
}


/* ---------- private code */


uint8 *unpack_effect_data(uint8 *Stream, effect_data* Objects, size_t Count)
{
	uint8* S = Stream;
	effect_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->type);
		StreamToValue(S,ObjPtr->object_index);
		
		StreamToValue(S,ObjPtr->flags);
		
		StreamToValue(S,ObjPtr->data);
		StreamToValue(S,ObjPtr->delay);
		
		S += 11*2;
	}
	
	assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_effect_data));
	return S;
}

uint8 *pack_effect_data(uint8 *Stream, effect_data* Objects, size_t Count)
{
	uint8* S = Stream;
	effect_data* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->type);
		ValueToStream(S,ObjPtr->object_index);
		
		ValueToStream(S,ObjPtr->flags);
		
		ValueToStream(S,ObjPtr->data);
		ValueToStream(S,ObjPtr->delay);
		
		S += 11*2;
	}
	
	assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_effect_data));
	return S;
}


uint8 *unpack_effect_definition(uint8 *Stream, size_t Count)
{
	return unpack_effect_definition(Stream,effect_definitions,Count);
}

uint8 *unpack_effect_definition(uint8 *Stream, effect_definition *Objects, size_t Count)
{
	uint8* S = Stream;
	effect_definition* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S,ObjPtr->collection);
		StreamToValue(S,ObjPtr->shape);
		
		StreamToValue(S,ObjPtr->sound_pitch);
				
		StreamToValue(S,ObjPtr->flags);
		StreamToValue(S,ObjPtr->delay);
		StreamToValue(S,ObjPtr->delay_sound);
	}
	
	assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_effect_definition));
	return S;
}

uint8* unpack_m1_effect_definition(uint8* Stream, size_t Count)
{
	uint8* S = Stream;
	effect_definition* ObjPtr = effect_definitions;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		StreamToValue(S, ObjPtr->collection);
		StreamToValue(S, ObjPtr->shape);
		ObjPtr->sound_pitch = FIXED_ONE;
		StreamToValue(S, ObjPtr->flags);
		ObjPtr->delay = 0;
		ObjPtr->delay_sound = NONE;
	}

	return S;
}


uint8 *pack_effect_definition(uint8 *Stream, size_t Count)
{
	return pack_effect_definition(Stream,effect_definitions,Count);
}

uint8 *pack_effect_definition(uint8 *Stream, effect_definition *Objects, size_t Count)
{
	uint8* S = Stream;
	effect_definition* ObjPtr = Objects;
	
	for (size_t k = 0; k < Count; k++, ObjPtr++)
	{
		ValueToStream(S,ObjPtr->collection);
		ValueToStream(S,ObjPtr->shape);
		
		ValueToStream(S,ObjPtr->sound_pitch);
				
		ValueToStream(S,ObjPtr->flags);
		ValueToStream(S,ObjPtr->delay);
		ValueToStream(S,ObjPtr->delay_sound);
	}
	
	assert((S - Stream) == static_cast<ptrdiff_t>(Count*SIZEOF_effect_definition));
	return S;
}

void init_effect_definitions()
{
	memcpy(effect_definitions, original_effect_definitions, sizeof(effect_definitions));
}
