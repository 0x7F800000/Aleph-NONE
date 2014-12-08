#pragma once

namespace alephType
{
	/*
		like the offsetof macro, but evaluated at compile-time
	*/
	template< typename classType, typename memberType, memberType classType::*member > 
	static constexpr size_t offset_of()
	{
		return reinterpret_cast<size_t>( &reinterpret_cast<T*>(nullptr)->*M );
	}
};
