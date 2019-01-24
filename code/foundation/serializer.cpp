#include "serializer.h"

SRLInstance SRLInstance::CreateReader( uint32_t v, const void* data, uint32_t data_size, BXIAllocator* allocator )
{
    SRLInstance srl;
    srl.version = v;
    data_buffer::create( &srl.data, (void*)data, data_size );
    data_buffer::seek_write( &srl.data, data_size );
    srl.allocator = allocator;
    srl.is_writting = 0;

    return srl;
}

SRLInstance SRLInstance::CreateWriterStatic( uint32_t v, const void* data, uint32_t data_size, BXIAllocator* allocator )
{
    SRLInstance srl;
    srl.version = v;
    data_buffer::create( &srl.data, (void*)data, data_size );
    srl.allocator = allocator;
    srl.is_writting = 1;

    return srl;
}

SRLInstance SRLInstance::CreateWriterDynamic( uint32_t v, uint32_t data_size, BXIAllocator* allocator )
{
    SRLInstance srl;
    srl.version = v;
    data_buffer::create( &srl.data, data_size, allocator );
    srl.allocator = allocator;
    srl.is_writting = 1;

    return srl;
}

// property
void* srl_property_t::value_raw( void* instance ) const
{
    return (uint8_t*)instance + value_offset;
}

void srl_property_t::set_value_raw( void* instance, const void* data, uint32_t size ) const
{
    SYS_ASSERT( size == value_size );
    memcpy( value_raw( instance ), data, size );
}

void srl_property_t::set_value_raw( void* instance, uint32_t index, const void* data, uint32_t size ) const
{
    SYS_ASSERT( size == value_size );
    SYS_ASSERT( index < num_elements );
    uint8_t* base = (uint8_t*)value_raw( instance );
    uint32_t offset = index * value_size;
    memcpy( base + offset, data, size );
}

const srl_property_t* srl_property::find( const char* name, const srl_property_t* props, uint32_t count )
{
    const hashed_string_t hash = hashed_string( name );
    return find( hash, props, count );
}

const srl_property_t* srl_property::find( hashed_string_t hash, const srl_property_t* props, uint32_t count )
{
    for( uint32_t i = 0; i < count; ++i )
    {
        if( hash == props[i].name )
            return props + i;
    }
    return nullptr;
}

// file
uint32_t srl_file::calc_header_size( uint32_t num_properties )
{
    return sizeof( srl_file_t ) + num_properties * sizeof( srl_property_t );
}
void srl_file::serialize_properties_and_data( srl_file_t* output, const void* instance, uint32_t instance_memory_size, const srl_property_t* properties, uint32_t num_properties )
{
    srl_property_t* output_properties = const_cast<srl_property_t*>(output->properties());
    for( uint32_t i = 0; i < num_properties; ++i )
        output_properties[i] = properties[i];

    uint8_t* dst_data = (uint8_t*)output + calc_header_size( num_properties );
    memcpy( dst_data, instance, instance_memory_size );
}
