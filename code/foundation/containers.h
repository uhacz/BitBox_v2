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
    
    explicit array_t( BXIAllocator* alloc = BXDefaultAllocator() )
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

// ---
#include <tuple>

template<class... _Types> class tuple;

// empty tuple
template<> struct tuple<> {};

// recursive tuple definition
template<class _This, class... _Rest>
struct tuple<_This, _Rest...> : tuple<_Rest...>
{
    _This _Myfirst;
};

// tuple_element
template<size_t _Index, class _Tuple> struct tuple_element;

// select first element
template<class _This, class... _Rest>
struct tuple_element<0, tuple<_This, _Rest...>>
{
    typedef _This& type;
    typedef tuple<_This, _Rest...> _Ttype;
};

// recursive tuple_element definition
template <size_t _Index, class _This, class... _Rest>
struct tuple_element<_Index, tuple<_This, _Rest...>> : tuple_element<_Index - 1, tuple<_Rest...> >
{};

template<size_t _Index, class... _Types> 
inline typename tuple_element<_Index, tuple<_Types...>>::type tuple_get( tuple<_Types...>& _Tuple )
{
    typedef typename tuple_element<_Index, tuple<_Types...>>::_Ttype _Ttype;
    return (((_Ttype&)_Tuple)._Myfirst);
}

template <uint32_t MAX, class... Ts>
struct dense_container_t : public tuple<Ts...>
{
    dense_container_t() = default;

    using tuple_t = tuple<Ts...>;
    static constexpr size_t NUM_STREAMS = std::tuple_size<tuple_t>::value;
    id_array_t<MAX> _id;

    template< uint32_t SI, class T >
    const T& get( id_t id ) const
    {
        const uint32_t index = id_array::index( _id, id );
        const auto& stream = tuple_get<SI>( *this );
        return stream[index];
    }
    template< uint32_t SI, class T>
    void set( id_t id, const T& data )
    {
        const uint32_t index = id_array::index( _id, id );
        auto& stream = tuple_get<SI>( *this );
        stream[index] = data;
    }

    bool has( id_t id ) const { return id_array::has( _id, id ); }

    template< uint32_t SI >
    auto stream()
    {
        auto& begin = tuple_get<SI>( *this );
        using value_type = std::remove_reference<decltype(*begin)>::type;
        return array_span_t< value_type >( begin, id_array::size( _id ) );
    }
};
