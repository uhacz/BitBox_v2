#pragma once

#include <string.h>
#include "type.h"


unsigned murmur3_32x86_hash( const void* key, unsigned int size8, unsigned int seed );
void murmur3_128x64_hash( void* out, const void* key, unsigned int size8, unsigned int seed );

__forceinline unsigned murmur3_hash32( const void* key, unsigned int size8, unsigned int seed )
{
    return murmur3_32x86_hash( key, size8, seed );	
}
__forceinline void murmur3_hash128( void* out, const void* key, unsigned size8, unsigned seed )
{
	murmur3_128x64_hash( out, key, size8, seed );
}

inline unsigned int elf_hash_string(const char* str, unsigned int prev_value )
{
	unsigned int hash = prev_value;
	unsigned int x    = prev_value;

	char c = *str;
	while( c )
	{
		hash = (hash << 4) + c;
		c = * (++str);
		if((x = hash & 0xF0000000L) != 0)
		{
			hash ^= (x >> 24);
		}
		hash &= ~x;
	}

	return hash;
}

inline unsigned int elf_hash_data(const void* data, unsigned size, unsigned int prev_value )
{
	unsigned int hash = prev_value;
	unsigned int x    = prev_value;

    char* str = (char*)data;

    for( unsigned i = 0; i < size; ++i )
    {
        char c = *str;
        hash = (hash << 4) + c;
		c = * (++str);
		if((x = hash & 0xF0000000L) != 0)
		{
			hash ^= (x >> 24);
		}
		hash &= ~x;
    }

	return hash;
}


inline unsigned char LRC_hash( const unsigned char* buf, unsigned n )
{
	unsigned char sum = 0;
	while( n > 0 )
	{
		sum += *buf++;
		--n;
	}
	return ((~sum) + 1 );
}

inline unsigned char pearson_hash( const unsigned char* buf, unsigned n )
{
	static unsigned char lut[] = 
	{
		49, 118,  63, 252,  13, 155, 114, 130, 137,  40, 210,  62, 219, 246, 136, 221,
		174, 106,  37, 227, 166,  25, 139,  19, 204, 212,  64, 176,  70,  11, 170,  58,
		146,  24, 123,  77, 184, 248, 108, 251,  43, 171,  12, 141, 126,  41,  95, 142,
		167,  46, 178, 235,  30,  75,  45, 208, 110, 230, 226,  50,  32, 112, 156, 180,
		205,  68, 202, 203,  82,   7, 247, 217, 223,  71, 116,  76,   6,  31, 194, 183,
		15, 102,  97, 215, 234, 240,  53, 119,  52,  47, 179,  99, 199,   8, 101,  35,
		65, 132, 154, 239, 148,  51, 216,  74,  93, 192,  42,  86, 165, 113,  89,  48,
		100, 195,  29, 211, 169,  38,  57, 214, 127, 117,  59,  39, 209,  88,   1, 134,
		92, 163,   0,  66, 237,  22, 164, 200,  85,   9, 190, 129, 111, 172, 231,  14,
		181, 206, 128,  23, 187,  73, 149, 193, 241, 236, 197, 159,  55, 125, 196,  60,
		161, 238, 245,  94,  87, 157, 122, 158, 115, 207,  17,  20, 145, 232, 107,  16,
		21, 185,  33, 225, 175, 253,  81, 182,  67, 243,  69, 220, 153,   5, 143,   3,
		26, 213, 147, 222, 105, 188, 229, 191,  72, 177, 250, 135, 152, 121, 218,  44,
		120, 140, 138,  28,  84, 186, 198, 131,  54,   2,  56,  78, 173, 151,  83,  27,
		255, 144, 249, 189, 104,   4, 168,  98, 162, 150, 254, 242, 109,  34, 133, 224,
		228,  79, 103, 201, 160,  90,  18,  61,  10, 233,  91,  80, 124,  96, 244,  36
	};

	unsigned char h = 0;
	while( n > 0 )
	{
		h = lut[ h ^ *buf++ ];
		--n;
	}

	return h;
}
