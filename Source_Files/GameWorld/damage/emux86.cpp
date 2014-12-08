/*
	EMUX86.CPP
	
	Code for emulating complex code sequences in Damage Inc that won't decompile well
	and probably never will
	
	with time this will eventually be phased out
*/

#include "cseries.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <type_traits>
#include "emux86.hpp"


void fooo()
{
	x86Emu::EAX.compare<int16, false>(0xFFFF);
}
