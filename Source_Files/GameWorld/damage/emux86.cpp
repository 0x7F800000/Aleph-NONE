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

using namespace x86Emu;

x86Flags flags;

x86Register EAX;
x86Register EBX;
x86Register ECX;
x86Register EDX;
x86Register EDI;
x86Register ESI;
x86Register EBP;


