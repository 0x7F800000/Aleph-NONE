#pragma once

namespace alephType
{
	/*
		like the offsetof macro, but evaluated at compile-time
	*/
	template< typename classType, typename memberType, memberType classType::*member > 
	static constexpr size_t offset_of()
	{
		return reinterpret_cast<size_t>( &static_cast<classType*>(nullptr)->*member );
	}
	
	void initTypeInfo();
	
	template< size_t memberOffset, typename memberTypename> struct alephMemberInfo
	{
		const std::type_info& getInfo()
		{
			return typeid(memberTypename);
		}
		const size_t offset = memberOffset;
	};
	
};
