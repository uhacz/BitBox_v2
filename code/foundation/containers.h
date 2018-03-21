#pragma once

#include "type.h"
#include "debug.h"
#include <memory/memory.h>


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

template< class T >
struct array_span_t
{
    array_span_t( T* b, uint32_t size )
        : _begin( b ), _size( size )
    {}

    array_span_t( T* b, T* e )
        : _begin( b ), _size( (uint32_t)(ptrdiff_t)(e - b) )
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


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace dense_container
{
    static constexpr uint8_t MAX_STREAMS = 32;
}//

template< uint32_t MAX >
struct BIT_ALIGNMENT_16 dense_container_t
{
    BXIAllocator*   _allocator = nullptr;
    id_array_t<MAX> _ids;

    uint32_t _num_streams = 0;
    uint8_t  _streams_stride[dense_container::MAX_STREAMS] = {};
    uint32_t _names_hash    [dense_container::MAX_STREAMS] = {};
    uint8_t* _streams       [dense_container::MAX_STREAMS] = {};

    static constexpr uint32_t max() { return MAX; }

    uint32_t size() const { return id_table::size( _ids ); }
    uint32_t data_index( id_t id ) const { return id_array::index( _ids, id ); }
    
    template< uint32_t STREAM_INDEX, typename T >
    const T& get( id_t id ) const
    {
        SYS_ASSERT( STREAM_INDEX < _num_streams );
        SYS_ASSERT( sizeof( T ) == _streams_stride[STREAM_INDEX] );
        const uint32_t index = id_array::index( _ids, id );
        return (const T&)((T*)_streams[STREAM_INDEX])[index];
    }

    template< uint32_t STREAM_INDEX, typename T >
    void set( id_t id, const T& value )
    {
        SYS_ASSERT( STREAM_INDEX < _num_streams );
        SYS_ASSERT( sizeof( T ) == _streams_stride[STREAM_INDEX] );
        const uint32_t index = id_array::index( _ids, id );
        ((T*)_streams[STREAM_INDEX])[index] = value;
    }

    template< typename T >
    bool set( id_t id, const char* stream_name, const T& value )
    {
        const uint32_t stream_index = find_stream<T>( stream_name );
        if( stream_index < _num_streams )
        {
            const uint32_t index = id_array::index( _ids, id );
            ((T*)_streams[stream_index])[index] = value;
            return true;
        }
        return false;
    }

    template< uint32_t STREAM_INDEX, typename T >
    array_span_t<T> stream()
    {
        SYS_ASSERT( STREAM_INDEX < _num_streams );
        T* data = (T*)_streams[STREAM_INDEX];
        return array_span_t<T>( data, id_array::size( _ids ) );
    }

    template< typename T >
    array_span_t<T> stream( const char* name )
    {
        const uint32_t stream_index = find_stream<T>( name );
        if( stream_index < _num_streams )
            return array_span_t<T>( (T*)_streams[stream_index], id_array::size( _ids ) );

        return array_span_t<T>( nullptr, nullptr );
    }

private:
    template< typename T >
    uint32_t find_stream( const char* name )
    {
        const uint32_t hashed_name = dense_container::GenerateHashedName<T>( name );
        for( uint32_t i = 0; i < _num_streams; ++i )
        {
            if( hashed_name == _names_hash[i] )
                return i;
        }
        return UINT32_MAX;
    }
};

