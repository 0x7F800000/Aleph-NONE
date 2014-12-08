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



x86Emu::x86Flags x86Emu::flags;

x86Emu::x86Register x86Emu::EAX;
x86Emu::x86Register x86Emu::EBX;
x86Emu::x86Register x86Emu::ECX;
x86Emu::x86Register x86Emu::EDX;
x86Emu::x86Register x86Emu::EDI;
x86Emu::x86Register x86Emu::ESI;
x86Emu::x86Register x86Emu::EBP;

void fooo()
{
	x86Emu::EAX.compare<int16, false>(0xFFFF);
}
