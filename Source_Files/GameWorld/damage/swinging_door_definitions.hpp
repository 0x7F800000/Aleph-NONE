#pragma once

struct swinging_door_definition
{
	int16 field_0;	//most of these first few fields are constant
	int16 field_2;	//across all of the definitions...
	int16 field_4;	//no code seems to ever access them, either
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
	int16 textured_object_0_definition_index;
	int16 textured_object_1_definition_index;
	int16 oops;
	int16 field_26;
	int16 field_28;
	int16 sound0;
	int16 sound2;
	int16 sound1;
};

#ifndef DONT_REPEAT_DEFINITIONS

static const swinging_door_definition swinging_door_definitions[10] =
{
	{
		0,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		9,
		9,
		10,
		11,
		11,
		12,
		13,
		0
	},
	{
		0,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		12,
		12,
		13,
		14,
		14,
		149,
		151,
		152
	},
	{
		2,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		4,
		4,
		5,
		6,
		6,
		27,
		25,
		40
	},
	{
		3,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		15,
		15,
		16,
		17,
		17,
		12,
		13,
		0
	},
	{
		4,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		18,
		18,
		19,
		20,
		20,
		213,
		214,
		40
	},
	{
		5,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		21,
		21,
		22,
		23,
		23,
		213,
		214,
		122
	},
	{
		6,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		24,
		24,
		25,
		26,
		26,
		149,
		151,
		152
	},
	{
		7,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		27,
		27,
		28,
		29,
		29,
		12,
		13,
		0
	},
	{
		3,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		16,
		16,
		19,
		20,
		20,
		27,
		25,
		40
	},
	{
		2,
		0,
		0,
		1024,
		768,
		128,
		384,
		4,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		7,
		7,
		8,
		6,
		6,
		12,
		13,
		0
	}
};



#endif

