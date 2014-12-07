#pragma once
/*
	Helper code for emulating certain complex x86 instructions
	These should eventually be phased out.
	
	referred to the code at
	https://code.google.com/p/open-last-chaos/source/browse/trunk/Messenger/src/defs.h?r=2
	for implementing some of these
	
*/
#include <type_traits>

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
	class x86Register
	{
	public:
		bool env64()
		{
			return sizeof(void*) == 8;
		}
		
		template<typename T> T operator *()
		{
			if( env64() )
				return *(T*)value;
			return *(T*) (value & 0xFFFFFFFF);
		}
		#define		LVARMASK()	unsigned mask = maskForType<T, low>()
		#define		ISBYTE()	(sizeof(T) == 1)
		
		#define		maskedRegOprU(operation) 	((T)operation) | (uval & (~mask))
		#define		maskedRegOprS(operation) 	((T)operation) | (sval & (~mask))
		#define		INTEGRAL()			(std::is_integral<T>::value)
		
		#define		ASSERT_TYPE_IS_POINTER(fname)	static_assert(std::is_pointer<T>::value, "Pointer to non-integral type required in x86Register::"#fname#".")
		#define		SIZEOF_REFERENCED_TYPE()	(sizeof(std::remove_pointer<T>::type))
		#define		UNPACK_BYTE_REG(v)		if(!low && ISBYTE() ) v >>= highByteShift
		#define		ADJUST_BYTE_FOR_REPACKING(v)	\
				!low && ISBYTE() ? \
				static_cast<std::make_unsigned<decltype(v)>::type>(v) << highByteShift \
				: v
					
		#define		__declopr(type)		template<typename T, bool low = true> type
		
		#define		UNSIGNED_OPR_PROLOG(fullreg, castreg, )\
			LVARMASK();\
			size_t fullreg = uval & mask; \
			ADJUST_BYTE_FOR_OP(fullreg);\
			T castreg = fullreg
			
		#define		UNSIGNED_OPR_EPILOG(fullreg, castreg)\
			fullreg = ADJUST_BYTE_FOR_REPACKING(castreg);\
			uval = maskedRegOprU(fullreg);\
			return castreg
			
		#define		SIGNED_OPR_PROLOG(fullreg, castreg, )\
			LVARMASK();\
			decltype(this->sval) fullreg = uval & mask; \
			ADJUST_BYTE_FOR_OP(fullreg);\
			T castreg = fullreg
			
		#define		SIGNED_OPR_EPILOG(fullreg, castreg)\
			fullreg = ADJUST_BYTE_FOR_REPACKING(castreg);\
			sval = maskedRegOprS(fullreg);\
			return castreg	
			
		__declopr(T) increment()
		{
			if( INTEGRAL() )
			{
				UNSIGNED_OPR_PROLOG(fullbits, casted);
				++casted;
				UNSIGNED_OPR_EPILOG(fullbits, casted);
			}
			//not an integral type. no masking needed
			ASSERT_TYPE_IS_POINTER("increment");
			uval += SIZEOF_REFERENCED_TYPE();
			return T(uval);
		}
		
		__declopr(T) decrement()
		{
			if( INTEGRAL() )
			{
				UNSIGNED_OPR_PROLOG(fullbits, casted);
				--casted;
				UNSIGNED_OPR_EPILOG(fullbits, casted);
			}
			//not an integral type. no masking needed
			ASSERT_TYPE_IS_POINTER("increment");
			uval -= SIZEOF_REFERENCED_TYPE();
			return T(uval);
		}
		
		//unsigned shift right
		__declopr(T) ushr(unsigned factor)
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			casted >>= factor;
			UNSIGNED_OPR_EPILOG(fullbits, casted);
		}
		
		//unsigned shift left
		__declopr(T) ushl(unsigned factor)
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			casted <<= factor;
			UNSIGNED_OPR_EPILOG(fullbits, casted);
		}
		//signed shift right
		__declopr(T) sshr(signed factor)
		{
			SIGNED_OPR_PROLOG(fullbits, casted);
			casted >>= factor;
			SIGNED_OPR_EPILOG(fullbits, casted);
		}
		//signed shift left
		__declopr(T) sshl(signed factor)
		{
			SIGNED_OPR_PROLOG(fullbits, casted);
			casted <<= factor;
			SIGNED_OPR_EPILOG(fullbits, casted);
		}
		__declopr(unsigned) maskForType()
		{
			static_assert( std::is_integral<T>::value, "maskForType requires an integral typename" );
			static_assert( sizeof(T) <= 4, "maskForType cannot use 64 bit values" );
			
			if( sizeof(T) == sizeof(uint8) )
				return low ? lowByteRegMask : highByteRegMask;
				
			else if( sizeof(T) == sizeof(uint16) )
				return lowWordRegMask;
				
			return dwordRegMask;
		}
	private:
		static const size_t lowByteRegMask 	= 0x00FF;
		static const size_t highByteRegMask 	= 0xFF00;
		static const size_t lowWordRegMask	= 0xFFFF;
		static const size_t dwordRegMask	= 0xFFFFFFFF;
		static const size_t highByteShift	= 8;
		
		union
		{
			size_t uval;
			ptrdiff_t sval;
		};
	}
};
