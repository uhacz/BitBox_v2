#pragma once

#include "type.h"
#include "debug.h"
#include <memory/memory.h>

//
// dynamic array
//
template< typename T >
struct array_t
{
    uint32_t size;
    uint32_t capacity;
    BXIAllocator* allocator;
    T* data;
    
    explicit array_t( BXIAllocator* alloc = BXDefaultAllocator() )
        : size( 0 ), capacity( 0 ), allocator( alloc ), data( 0 ) 
    {
        SYS_ASSERT( alloc != nullptr );
    }

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

//
// static array
//
template< typename T, uint32_t MAX >
struct static_array_t
{
    T data[MAX];
    uint32_t size = 0;

    T &operator[]( int i ) { return data[i]; }
    const T &operator[]( int i ) const { return data[i]; }

    T* begin() { return data; }
    T* end() { return data + size; }

    const T* begin() const { return data; }
    const T* end() const { return data + size; }
};

//
// array span
//
template< class T >
struct array_span_t
{
    array_span_t( T* b, uint32_t size )
        : _begin( b ), _size( size )
    {}

    array_span_t( T* b, T* e )
        : _begin( b ), _size( (uint32_t)(ptrdiff_t)(e - b) )
    {}

    explicit array_span_t( const array_t<T>& arr )
        : _begin( arr.begin() ), _size( arr.size )
    {}

    template< uint32_t MAX>
    explicit array_span_t( const static_array_t<T, MAX>& arr )
        : _begin( arr.begin() ), _size( arr.size )
    {}

    T* begin() { return _begin; }
    T* end() { return _begin + _size; }

    const T* begin() const { return _begin; }
    const T* end()   const { return _begin + _size; }

    uint32_t size() const { return _size; }

    T& operator[] ( uint32_t i ) { return _begin[i]; }
    const T& operator[] ( uint32_t i ) const { return _begin[i]; }

private:
    T* _begin = nullptr;
    uint32_t _size = 0;
};

template< typename T >
struct queue_t
{
    array_t<T> data;
    uint32_t size;
    uint32_t offset;

    explicit queue_t( BXIAllocator* alloc = BXDefaultAllocator() )
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
    hash_t( BXIAllocator* a = BXDefaultAllocator() )
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

struct id_array_destroy_info_t
{
    uint16_t copy_data_from_index;
    uint16_t copy_data_to_index;
};

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
    
    uint32_t capacity() const { return MAX; }

    uint16_t _freelist;
    uint16_t _next_id;
    uint16_t _size;

    Tid _sparse[MAX];
    uint16_t _sparse_to_dense[MAX];
    uint16_t _dense_to_sparse[MAX];
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

struct id_allocator_dense_t
{
    uint32_t capacity;
    uint32_t size;
    uint16_t free_index;

    id_t*     sparse_id;
    uint16_t* sparse_to_dense_index;
    uint16_t* dense_to_sparse_index;

    BXIAllocator* allocator;

    struct delete_info_t
    {
        // all indices here are dense

        uint16_t copy_data_from_index;
        union 
        {
            uint16_t copy_data_to_index;
            uint16_t removed_at_index;
        };
    };
};
