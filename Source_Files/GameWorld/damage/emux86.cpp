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

x86Flags x86Emu::flags;

x86Register X86Emu::EAX;
x86Register X86Emu::EBX;
x86Register X86Emu::ECX;
x86Register X86Emu::EDX;
x86Register X86Emu::EDI;
x86Register X86Emu::ESI;
x86Register X86Emu::EBP;


