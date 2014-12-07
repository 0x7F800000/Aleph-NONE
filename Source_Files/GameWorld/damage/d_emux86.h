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
	
	
	class x86Flags
	{
	public:
		#define		__declflag(flag, name)	inline void set##name##(bool status) \
			{	flag = (status) ? 1 : 0;	}\
			inline bool ##name##()	{	return bool(flag);	}
			
		x86Flags() {}
		
		__declflag(carry, Carry)
		__declflag(parity, Parity)
		__declflag(adjust, Adjust)
		__declflag(zero, Zero)
		__declflag(sign, Sign)
		__declflag(trap, Trap)
		__declflag(direction, Direction)
		__declflag(overflow, Overflow)
		
		inline size_t getFlags()
		{
			return __flags;
		}
		inline size_t setFlags(size_t set)
		{
			return __flags = set;
		}
		
	private:
	
		#define	__declbit(name)	size_t name : 1
		
		union
		{
			struct
			{
				__declbit(carry),
				__declbit(reserved0),
				__declbit(parity),
				__declbit(reserved1),
				__declbit(adjust),
				__declbit(reserved2),
				__declbit(zero),
				__declbit(sign),
				__declbit(trap),
				__declbit(interruptEnable),
				__declbit(direction),
				__declbit(overflow)
			};	//the rest of the flags are unneeded
			size_t __flags;
		};
	};
	static x86Flags flags;
	
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
					
		#define		__declopr(type, ...)	template<typename T, bool low = true, __VA_ARGS__> type
		
		#define		UNSIGNED_OPR_PROLOG(fullreg, castreg)\
			LVARMASK();\
			size_t fullreg = uval & mask; \
			ADJUST_BYTE_FOR_OP(fullreg);\
			T castreg = fullreg
			
		#define		UNSIGNED_OPR_EPILOG(fullreg, castreg)\
			fullreg = ADJUST_BYTE_FOR_REPACKING(castreg);\
			uval = maskedRegOprU(fullreg);\
			return castreg
			
		#define		SIGNED_OPR_PROLOG(fullreg, castreg)\
			LVARMASK();\
			decltype(this->sval) fullreg = uval & mask; \
			ADJUST_BYTE_FOR_OP(fullreg);\
			T castreg = fullreg
			
		#define		SIGNED_OPR_EPILOG(fullreg, castreg)\
			fullreg = ADJUST_BYTE_FOR_REPACKING(castreg);\
			sval = maskedRegOprS(fullreg);\
			return castreg	
			
		#define		ARGUMENT_IS_REGISTER(argument)	\
		( std::is_same< decltype(this), decltype(argument) >::value)
		
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
		#define	__declshift(fname, castname, operation, signedness, capital_signedness)\
		__declopr(T) fname(signedness factor)\
		{\
			##capital_signedness##_OPR_PROLOG(fullbits, castname);\
			operation;\
			##capital_signedness##_OPR_EPILOG(fullbits, castname);\
		}
		
		//unsigned shift right
		__declshift(ushr, casted, casted >>= factor, unsigned, UNSIGNED)
		//unsigned shift left
		__declshift(ushl, casted, casted <<= factor, unsigned, UNSIGNED)
		
		//signed shift right
		__declshift(sshr, casted, casted >>= factor, signed, SIGNED)
		//signed shift left
		__declshift(sshl, casted, casted <<= factor, signed, SIGNED)
		
		__declopr(T) Value()
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			UNSIGNED_OPR_EPILOG(fullbits, casted);
		}
		__declopr(void) compare(T against)
		{
			
		}
		
		/*
			sets the reg/one of its sub regs to a Value and extends the sign to the whole register
			if extendSign is true
		*/
		__declopr(T, bool extendSign = false) Value(T newValue)
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			if( sizeof(T) == sizeof(int16) && extendSign)
			{
				if(std::is_unsigned<T>::value)
					uval = static_cast< decltype(uval) >(newValue);
				else if(std::is_signed<T>::value)
					sval = static_cast< decltype(sval) >(newValue);
				else
					assert(false);
				return newValue;
			}
			UNSIGNED_OPR_EPILOG(fullbits, casted);
		}
		
		__declopr(T) bor(T bits)
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			casted |= bits;
			UNSIGNED_OPR_EPILOG(fullbits, casted);
		}
		
		__declopr(T) band(T bits)
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			casted &= bits;
			UNSIGNED_OPR_EPILOG(fullbits, casted);
		}
		
		__declopr(T) bxor(T bits)
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			casted ^= bits;
			UNSIGNED_OPR_EPILOG(fullbits, casted);
		}
		
		__declopr(T) bnot()
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			casted = ~casted;
			UNSIGNED_OPR_EPILOG(fullbits, casted);
		}
		__declopr(T) neg()
		{
			SIGNED_OPR_PROLOG(fullbits, casted);
			casted = -casted;
			SIGNED_OPR_EPILOG(fullbits, casted);
		}
		
		__declopr(bool) test(T testVal)
		{
			UNSIGNED_OPR_PROLOG(fullbits, casted);
			
			if( !ARGUMENT_IS_REGISTER(testVal) )
				return casted & testVal;
			T otherVal = testVal.Value<T, low>();
			return casted & otherVal;
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
	};
};
