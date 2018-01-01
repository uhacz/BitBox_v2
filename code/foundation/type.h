#pragma once

#include <stdint.h>
#include <float.h>

#ifdef _MSC_VER
#pragma warning( disable: 4512 )
#endif

#ifdef _MSC_VER
#pragma warning( disable: 4351 )
#endif

typedef float float32_t;
typedef double float64_t;

union TypeReinterpert
{
    uint32_t u;
    int32_t i;
    float32_t f;

    explicit TypeReinterpert( uint32_t v ) : u( v ) {}
    explicit TypeReinterpert( int32_t v ) : i( v ) {}
    explicit TypeReinterpert( float32_t v ) : f( v ) {}
};



#define BIT_OFFSET(n) (1<<n)
#define PTR_TO_UINT32( p )   ((uint32_t)(uintptr_t) (uint32_t*) (p))
#define UINT32_TO_PTR( ui )  ((void *)(uint32_t*)((uint32_t)ui))

#define TYPE_OFFSET_GET_POINTER(type,offset) ( (offset)? (type*)((intptr_t)(&offset)+(intptr_t)(offset)) : (type*)0 )
#define TYPE_POINTER_GET_OFFSET(base, address) ( (base) ? (PTR_TO_UINT32(address) - PTR_TO_UINT32(base)) : 0 )

#ifdef _MSC_VER
#define BIT_ALIGNMENT( alignment )	__declspec(align(alignment))	
#else
#define BIT_ALIGNMENT( alignment ) __attribute__ ((aligned(alignment)))
#endif


#define BIT_ALIGNMENT_16 BIT_ALIGNMENT(16)
#define BIT_ALIGNMENT_64 BIT_ALIGNMENT(64)
#define BIT_ALIGNMENT_128 BIT_ALIGNMENT(128)

// "alignment" must be a power of 2
#define TYPE_IS_ALIGNED(value, alignment)                       \
	(                                                           \
		!(((intptr_t)(value)) & (((intptr_t)(alignment)) - 1U))         \
	) 


#define TYPE_ALIGN(value, alignment)                                \
	(                                                               \
		(((intptr_t) (value)) + (((intptr_t) (alignment)) - 1U))			\
		& ~(((intptr_t) (alignment)) - 1U)								\
	)



#define MAKE_STR(x) MAKE_STR_(x)
#define MAKE_STR_(x) #x

template <typename T, size_t N>
size_t sizeof_array( const T( &)[N] )
{
    return N;
}

inline bool is_pow2( int n )
{
    return ( n&( n - 1 ) ) == 0;
}
template< typename T >
inline T TYPE_NOT_FOUND()
{
    return std::numeric_limits<T>::max();
}