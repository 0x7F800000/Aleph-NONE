#pragma once
/*
	Helper code for emulating certain complex x86 instructions
	These should eventually be phased out.
	
	Mostly referred to the code at
	https://code.google.com/p/open-last-chaos/source/browse/trunk/Messenger/src/defs.h?r=2
	for writing the implementation of these
	
*/
#define		bitsizeof(type)		(sizeof(type) * 8)

namespace x86Emu
{
	//left bit rotation
	static inline int __ROL__(int v, unsigned int count)
	{
		count %= 32;
		int h = v >> (32 - count);
		return (v << count) | h;
	}
	//right bit rotation
	static inline int __ROR__(int v, unsigned int count)
	{
		count %= 32;
		int l = v << (32 - count);
		return (v >> count) | l;
	}
	static inline unsigned char __MKCSHL__(int v, unsigned int count)
	{
		return (v >> (32 - (count % 32) )) & 1;
	}
	static inline unsigned char __MKCSHR__(int v, unsigned int count)
	{
		return (value >> (count - 1)) & 1;
	}
	
	template <typename T> T Signbit(T val)
	{
		return (val >> (bitsizeof(T) - 1)) & 1;
	}
};
