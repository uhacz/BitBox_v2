#pragma once

#include "type.h"
#include "debug.h"

struct BufferChunker
{
	BufferChunker( void* mem, int mem_size )
		: current( (unsigned char*)mem ), end( (unsigned char*)mem + mem_size )
	{}

	template< typename T >
	T* Add( int count = 1, int alignment = ALIGNOF( T ) )
	{
		unsigned char* result = (unsigned char*)TYPE_ALIGN( current, alignment );

		unsigned char* next = result + count * sizeof( T );
		current = next;
		return (T*)result;
	}
	unsigned char* AddBlock( int size, int alignment = 4 )
	{
		unsigned char* result = (unsigned char*)TYPE_ALIGN( current, alignment );

		unsigned char* next = result + size;
        SYS_ASSERT( next <= end );
		current = next;
		return result;
	}

	void Check()
	{
		SYS_ASSERT( current == end );
	}

	unsigned char* current;
	unsigned char* end;
};