#pragma once

#define	__accessordecl(thing, type, field) \
inline type get##thing()\
{\
	return field; \
}\
inline type set##thing(const type newval)\
{\
	return field = newval;\
}\
inline bool is##thing(const type isval)\
{\
  return get##thing() == isval;\
}
