#include "rtti.h"
#include <foundation/debug.h>
#include <string.h>

static constexpr uint32_t MEMORY_BUDGET = 1024 * 1024;
static uint8_t __memory[MEMORY_BUDGET] = {};
static uint32_t __offset = 0;

// ---
static uint8_t* AllocateData( uint32_t size )
{
    SYS_ASSERT( __offset + size <= MEMORY_BUDGET );
    const uint32_t local_offset = __offset;
    __offset += size;
    return __memory + local_offset;
}

static inline bool IsCString( const std::type_info& ti )
{
    const std::type_info& cstr_ti = typeid(const char*);
    return cstr_ti == ti;
}

static inline bool IsOffsetValid( uint16_t off )
{
    return off != UINT16_MAX;
}
static inline uint8_t* ValuePtr( const uint16_t& offset )
{
    return TYPE_OFFSET_GET_POINTER( uint8_t, offset );
}

static inline uint32_t DataSize( const RTTIAttr& attr, const void* data )
{
    return IsCString( attr._type_info ) ? ((uint32_t)strlen( (const char*)data ) + 1) : attr._size;
}

static void InitializeValue( uint16_t* output_offset, const void* value, uint32_t data_size )
{
    uint8_t* ptr = AllocateData( data_size );
    memcpy( ptr, value, data_size );
    output_offset[0] = TYPE_POINTER_GET_OFFSET( output_offset, ptr );
}
static void SetOrInitValue( uint16_t* offset_value, const void* data, uint32_t size )
{
    if( IsOffsetValid( *offset_value ) )
    {
        memcpy( (void*)ValuePtr( *offset_value ), data, size );
    }
    else
        InitializeValue( offset_value, data, size );
}

// ---
RTTIAttr* RTTI::AllocateAttribute( uint32_t size )
{
    return (RTTIAttr*)AllocateData( size );
}

RTTIAttr* RTTIAttr::SetDefaultData( const void* value, uint32_t data_size )
{
#if ASSERTION_ENABLED
    if( !IsCString( _type_info ) )
        SYS_ASSERT( data_size == _size );
#endif
    const uint32_t size = DataSize( *this, value );
    SetOrInitValue( &_offset_default_value, value, size );
    return this;
}

RTTIAttr* RTTIAttr::SetMinData( const void* value, uint32_t data_size )
{
#if ASSERTION_ENABLED
    if( !IsCString( _type_info ) )
        SYS_ASSERT( data_size == _size );
#endif
    const uint32_t size = DataSize( *this, value );
    SetOrInitValue( &_offset_min_value, value, size );
    return this;
}

RTTIAttr* RTTIAttr::SetMaxData( const void* value, uint32_t data_size )
{
#if ASSERTION_ENABLED
    if( !IsCString( _type_info ) )
        SYS_ASSERT( data_size == _size );
#endif
    const uint32_t size = DataSize( *this, value );
    SetOrInitValue( &_offset_max_value, value, size );
    return this;
}

const char* RTTIAttr::TypeName() const
{
    return _type_info.name();
}
const char* RTTIAttr::Name() const
{
    return TYPE_OFFSET_GET_POINTER( const char, _offset_attr_name );
}
const uint8_t* RTTIAttr::DefaultPtr() const
{
    return IsOffsetValid(_offset_default_value) ? TYPE_OFFSET_GET_POINTER( uint8_t, _offset_default_value ) : nullptr;
}
const uint8_t* RTTIAttr::MinPtr() const
{
    return IsOffsetValid( _offset_min_value ) ? TYPE_OFFSET_GET_POINTER( uint8_t, _offset_min_value ) : nullptr;
}
const uint8_t* RTTIAttr::MaxPtr() const
{
    return IsOffsetValid( _offset_max_value ) ? TYPE_OFFSET_GET_POINTER( uint8_t, _offset_max_value ) : nullptr;
}

RTTI_EXPORT void* BXLoad_rtti( BXIAllocator* allocator )
{
    memset( __memory, 0x00, MEMORY_BUDGET );
    return __memory;
}

RTTI_EXPORT void BXUnload_rtti( void* plugin, BXIAllocator* allocator )
{
    SYS_ASSERT( plugin == __memory );
}

