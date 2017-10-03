#pragma once

#include "type.h"
#include "memory/memory.h"

template< typename T >
struct array_t
{
    uint32_t size;
    uint32_t capacity;
    BXIAllocator* allocator;
    T* data;
    
    explicit array_t( BXIAllocator* alloc )
        : size( 0 ), capacity( 0 ), allocator( alloc ), data( 0 ) 
    {}

    ~array_t()
    {
        BX_FREE0( allocator, data );
    }

          T &operator[]( int i) { return data[i]; }
    const T &operator[]( int i) const { return data[i]; }

    T* begin() { return data; }
    T* end  () { return data + size; }

    const T* begin() const { return data; }
    const T* end  () const { return data + size; }
};

template< typename T >
struct queue_t
{
    array_t<T> data;
    uint32_t size;
    uint32_t offset;

    explicit queue_t( BXIAllocator* alloc )
        : data( alloc ) , size( 0 ) , offset( 0 )
    {}
    ~queue_t()
    {}

          T& operator[]( int i )        { return data[(i + offset) % data.size]; }
    const T& operator[]( int i ) const  { return data[(i + offset) % data.size]; }
};

template<typename T> 
struct hash_t
{
public:
    hash_t( BXIAllocator* a )
        : _hash( a )
        , _data( a )
    {}

    struct Entry {
        uint64_t key;
        uint32_t next;
        T value;
    };

    array_t<uint32_t> _hash;
    array_t<Entry> _data;
};


#define BX_INVALID_ID UINT16_MAX
union id_t
{
    uint32_t hash;
    struct{
        uint16_t id;
        uint16_t index;
    };
};
inline bool operator == ( id_t a, id_t b ) { return a.hash == b.hash; }

inline id_t make_id( uint32_t hash ){
    id_t id = { hash };
    return id;
}

template <uint32_t MAX, typename Tid = id_t >
struct id_array_t
{
    id_array_t() : _freelist( BX_INVALID_ID ) , _next_id( 0 ) , _size( 0 )
    {
        for( uint32_t i = 0; i < MAX; i++ )
        {
            _sparse[i].id = BX_INVALID_ID;
        }
    }

    //T& operator[]( uint32_t index );
    //const T& operator[]( uint32_t index ) const;

    uint16_t _freelist;
    uint16_t _next_id;
    uint16_t _size;

    Tid _sparse[MAX];
    uint16_t _sparse_to_dense[MAX];
    uint16_t _dense_to_sparse[MAX];
    //T _objects[MAX];
};

template <uint32_t MAX, typename Tid = id_t>
struct id_table_t
{
    id_table_t() : _freelist( BX_INVALID_ID ) , _next_id( 0 ) , _size( 0 )
    {
        for( uint32_t i = 0; i < MAX; i++ )
        {
            _ids[i].id = BX_INVALID_ID;
        }
    }

    uint16_t _freelist;

    uint16_t _next_id;
    uint16_t _size;

    Tid _ids[MAX];
};

template< typename T > T makeInvalidHandle()
{
    T h = { 0 };
    return h;
}
