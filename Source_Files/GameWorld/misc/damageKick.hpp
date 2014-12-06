#pragma once

// How much external velocity is imparted by some damage?
struct damage_kick_definition
{
	int16 base_value;
	float delta_vitality_multiplier;
	bool is_also_vertical;

	// if non-zero, will enable vertical_component if
	// delta_vitality is greater than threshold
	int16 vertical_threshold;

	// whether monsters die a hard death, or in flames
	int16 death_action;
};


/* ---------- definitions */
#ifndef   DONT_REPEAT_DAMAGE_KICK_DEFINITIONS
// LP: implements commented-out damage-kick code
struct damage_kick_definition damage_kick_definitions[NUMBER_OF_DAMAGE_TYPES] = 
{
	{0, 1, true, 0, _monster_is_dying_hard}, // _damage_explosion,
	{0, 3, true, 0, _monster_is_dying_soft}, // _damage_electrical_staff,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_projectile,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_absorbed,
	{0, 1, false, 0, _monster_is_dying_flaming}, // _damage_flame,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_hound_claws,
	{0, 1, false, 0, _monster_is_dying_flaming}, // _damage_alien_projectile,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_hulk_slap,
	{0, 3, true, 0, _monster_is_dying_soft}, // _damage_compiler_bolt,
	{0, 0, false, 100, _monster_is_dying_soft}, // _damage_fusion_bolt,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_hunter_bolt,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_fist,
	{250, 0, false, 0, _monster_is_dying_soft}, // _damage_teleporter,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_defender,
	{0, 3, true, 0, _monster_is_dying_soft}, // _damage_yeti_claws,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_yeti_projectile,
	{0, 1, false, 0, _monster_is_dying_hard}, // _damage_crushing,
	{0, 1, false, 0, _monster_is_dying_flaming}, // _damage_lava,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_suffocation,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_goo,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_energy_drain,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_oxygen_drain,
	{0, 1, false, 0, _monster_is_dying_soft}, // _damage_hummer_bolt,
	{0, 0, true, 0, _monster_is_dying_soft} // _damage_shotgun_projectile,
};
#endif
