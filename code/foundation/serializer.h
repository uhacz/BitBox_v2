#pragma once

#include "data_buffer.h"
#include "blob.h"

#define SRL_ADD( version_added, field )\
    if( srl->version >= version_added )\
    {\
        Serialize( srl, &(field) );\
    }

#define SRL_REM( version_added, version_removed, field )\
    if( srl->version >= version_added && srl->version < version_removed )\
    {\
        Serialize( srl, &(field) );\
    }

struct SRLInstance
{
    uint32_t version;
    
    data_buffer_t data;
    BXIAllocator* allocator;

    uint32_t is_writting;

    static SRLInstance CreateReader( uint32_t v, const void* data, uint32_t data_size, BXIAllocator* allocator );

    static SRLInstance CreateWriterStatic( uint32_t v, const void* data, uint32_t data_size, BXIAllocator* allocator );
    static SRLInstance CreateWriterDynamic( uint32_t v, uint32_t data_size, BXIAllocator* allocator );
};


template<typename T>
inline void Serialize( SRLInstance* srl, T* p )
{
    SYS_STATIC_ASSERT( std::is_trivially_destructible<T>::value );
    SYS_STATIC_ASSERT( !std::is_pointer<T>::value );

    if( srl->is_writting )
    {
        data_buffer::write( &srl->data, p, sizeof( T ), 1 );
    }
    else
    {
        data_buffer::read( p, sizeof( T ), 1, &srl->data );
    }
}

#include <foundation/string_util.h>
template<>
inline void Serialize<string_t>( SRLInstance* srl, string_t* p )
{
    if( srl->is_writting )
    {
        const uint32_t len = string::length( p->c_str() );
        data_buffer::write( &srl->data, &len, sizeof( uint32_t ), 1 );
        data_buffer::write( &srl->data, p->c_str(), len, 1 );
    }
    else
    {
        uint32_t len = 0;
        data_buffer::read( &len, sizeof( uint32_t ), 1, &srl->data );
        string::reserve( p, len, srl->allocator );
        data_buffer::read( p->c_str(), len, 1, &srl->data );
    }
}

#include "hashed_string.h"
#include <memory.h>

// property
struct srl_property_t
{
    hashed_string_t name;
    uint32_t value_offset;
    uint32_t value_size;
    uint16_t num_elements;
    union
    {
        uint16_t all = 0;
        struct
        {
            uint16_t is_pointer : 1;
            uint16_t is_integral : 1;
            uint16_t is_float : 1;
            uint16_t is_signed : 1;
            uint16_t is_array : 1;
        };
    } flags;

    void* value_raw( void* instance ) const;
    void set_value_raw( void* instance, const void* data, uint32_t size ) const;
    void set_value_raw( void* instance, uint32_t index, const void* data, uint32_t size ) const;

    template<typename T>
    T* value_ptr( void* instance ) const
    {
        SYS_ASSERT( value_size == sizeof( std::remove_pointer<T>::type ) );
        return (T*)value_raw( instance );
    }

    template< typename T >
    void set_value( void* instance, const T& value ) const
    {
        set_value_raw( instance, &value, sizeof( T ) );
    }  

    template< typename T >
    void set_value( void* instance, uint32_t index, const T& value ) const
    {
        set_value_raw( instance, index, &value, sizeof( T ) );
    }
};

namespace srl_property
{
    template< typename T >
    inline srl_property_t create( const char* n, uint32_t value_offset )
    {
        srl_property_t prop = {};
        prop.name         = hashed_string( n );
        prop.value_offset = value_offset;
        prop.value_size   = sizeof( std::remove_extent<T>::type );
        prop.num_elements = sizeof( T ) / sizeof( std::remove_extent<T>::type );

        prop.flags.is_pointer  = std::is_pointer<T>::value;
        prop.flags.is_float    = std::is_floating_point<T>::value;
        prop.flags.is_integral = std::is_integral<T>::value;
        prop.flags.is_signed   = std::is_signed<T>::value;
        prop.flags.is_array    = std::is_array<T>::value;

        return prop;
    }

    const srl_property_t* find( const char* name, const srl_property_t* props, uint32_t count );
    const srl_property_t* find( hashed_string_t hash, const srl_property_t* props, uint32_t count );
}

// file
struct srl_file_t
{
    uint32_t tag = 0;
    uint32_t version = 0;
    uint32_t num_properties = 0;
    uint32_t size = 0;

    const srl_property_t* properties() { return (srl_property_t*)(this + 1); }

    template< typename T >
    const T* data() const
    {
        SYS_ASSERT( num_properties != 0 );
        SYS_ASSERT( version == T::VERSION );
        SYS_ASSERT( tag == T::TAG );
        return (T*)( (uint8_t*)this + srl_file::calc_header_size( num_properties ) );
    }
};

namespace srl_file
{
    uint32_t calc_header_size( uint32_t num_properties );
    
    template< typename T >
    inline uint32_t calc_header_size()
    {
        return calc_header_size( T::__props._count );
    }


    void serialize_properties_and_data( srl_file_t* output, const void* instance, uint32_t instance_memory_size, const srl_property_t* properties, uint32_t num_properties );

    template< typename T >
    inline void serialize( srl_file_t* output, const T* instance, uint32_t instance_memory_size )
    {
        output->tag = T::TAG;
        output->version = T::VERSION;
        output->num_properties = T::__props._count;
        output->size = calc_header_size<T>() + instance_memory_size;
        serialize_properties_and_data( output, instance, instance_memory_size, T::__props._array, T::__props._count );
    }

    template< typename T >
    inline srl_file_t* serialize( const T* instance, size_t instance_memory_size, BXIAllocator* allocator )
    {
        uint32_t file_blob_size = srl_file::calc_header_size<T>();
        file_blob_size += (uint32_t)instance_memory_size;

        srl_file_t* output = (srl_file_t*)BX_MALLOC( allocator, file_blob_size, sizeof( void* ) );
        serialize( output, instance, (uint32_t)instance_memory_size );

        return output;
    }


    template< typename T >
    inline srl_file_t* serialize( const blob_t& blob, BXIAllocator* allocator )
    {
        return serialize( (T*)blob.raw, blob.size, allocator );
    }
}//

#define SRL_PROPERTY( name ) const srl_property_t name = srl_property::create<decltype(__this_type::name)>( #name, offsetof( __this_type, name ) )

#define SRL_TYPE( type, properties_block ) \
        using __this_type = type; \
        static const uint32_t __this_size;\
        struct __props_t {\
        properties_block;\
        const srl_property_t* _array;\
        const uint32_t _count;\
        __props_t()\
            : _array( (srl_property_t*)this )\
            , _count( offsetof( __props_t, _array ) / sizeof( srl_property_t ) )\
        {\
            for( uint32_t i = 0; i < _count; ++i ) \
                for( uint32_t j = i + 1; j < _count; ++j ) \
                    SYS_ASSERT( _array[i].name != _array[j].name ); \
        }\
        const srl_property_t* find( const char* name ) const { return srl_property::find( name, _array, _count ); }\
        const srl_property_t* find( hashed_string_t hash ) const { return srl_property::find( hash, _array, _count ); }\
        };\
        static const __props_t __props

#define SRL_TYPE_DEFINE( type ) \
const uint32_t type::__this_size = sizeof(type); \
const type::__props_t type::__props = type::__props_t()