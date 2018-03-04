#pragma once

#include <foundation/plugin/plugin_interface.h>
#include <foundation/type.h>
#include <typeinfo>

#ifdef BX_DLL_rtti
#define RTTI_EXPORT __declspec(dllexport)
#else
#define RTTI_EXPORT __declspec(dllimport)
#endif

#define RTTI_VALIDATE_GETTER( ptr, size )\
    SYS_ASSERT( ptr != nullptr );\
    SYS_ASSERT( size == _size )

struct RTTI_EXPORT RTTIAttr
{
    // --- setters
    RTTIAttr* SetDefaultData( const void* data, uint32_t data_size );
    RTTIAttr* SetMinData( const void* data, uint32_t data_size );
    RTTIAttr* SetMaxData( const void* data, uint32_t data_size );

    template< typename T >
    RTTIAttr* SetDefault( const T& value ) { return SetDefaultData( &value, (uint32_t)sizeof( T ) ); }

    template< typename T >
    RTTIAttr* SetMin( const T& value ) { return SetMinData( &value, (uint32_t)sizeof( T ) ); }
    
    template< typename T >
    RTTIAttr* SetMax( const T& value ) { return SetMaxData( &value, (uint32_t)sizeof( T ) ); }

    // --- getters
    const char*    TypeName() const;
    const char*    Name() const;
    const uint8_t* DefaultPtr() const;
    const uint8_t* MinPtr() const;
    const uint8_t* MaxPtr() const;

    template< typename T >
    const T& Default() const
    {
        const uint8_t* ptr = DefaultPtr();
        RTTI_VALIDATE_GETTER( ptr, sizeof( T ) );
        return (const T&)(*(T*)ptr);
    }
    template< typename T >
    const T& Min() const
    {
        RTTI_VALIDATE_GETTER( MinPtr(), sizeof( T ) );
        return (const T&)(*(T*)MinPtr());
    }
    template< typename T >
    const T& Max() const
    {
        const uint8_t* ptr = MaxPtr();
        RTTI_VALIDATE_GETTER( ptr, sizeof( T ) );
        return (const T&)(*(T*)ptr);
    }

    template< typename T >
    const T& Value( const void* obj ) const
    {
        RTTI_VALIDATE_GETTER( obj, sizeof( T ) );
        const uint8_t* base = (uint8_t*)obj;
        return *(const T*)(base + _offset);
    }

    // --- can't touch this
    const std::type_info& _type_info;
    const uint32_t _offset : 24;
    const uint32_t _size : 8;
    uint16_t _offset_default_value;
    uint16_t _offset_min_value;
    uint16_t _offset_max_value;
    uint16_t _offset_attr_name;
};

struct RTTI_EXPORT RTTI
{
    static RTTIAttr*      AllocateAttribute(uint32_t size);

    template< typename P, typename T >
    static RTTIAttr* Create( T P::*M, const char* name )
    {
        const uint32_t name_len = (uint32_t)strlen( name );

        uint32_t mem_size = 0;
        mem_size += sizeof( RTTIAttr );
        mem_size += name_len + 1;

        const uint32_t offset = (uint32_t)(uintptr_t)( &(((P*)0)->*M) );

        RTTIAttr* attr = AllocateAttribute( mem_size );
        attr = new (attr) RTTIAttr{ typeid(T), offset, sizeof( T ) };
        
        char* attr_name = (char*)(attr + 1);
        memcpy( attr_name, name, name_len );
        attr_name[name_len] = 0;
        attr->_offset_attr_name = TYPE_POINTER_GET_OFFSET( &attr->_offset_attr_name, attr_name );

        attr->_offset_default_value = UINT16_MAX;
        attr->_offset_min_value = UINT16_MAX;
        attr->_offset_max_value = UINT16_MAX;
        return attr;
    }

    template< typename T >
    static const RTTIAttr* Find( const char* name )
    {
        for( uint32_t i = 0; i < T::__nb_attributes; ++i )
            if( !strcmp( name, T::__attributes[i]->Name() ) )
                return T::__attributes[i];
        
        return nullptr;
    }

    template< typename F, typename T >
    static bool Value( F* dst, const T& obj, const char* attr_name )
    {
        const RTTIAttr* attr = Find<T>( attr_name );
        if( attr )
        {
            dst[0] = attr->Value<F>( &obj );
            return true;
        }

        return false;
    }

    
};

#define BX_RTTI_PLUGIN_NAME "rtti"

extern "C" {
    RTTI_EXPORT void* BXLoad_rtti( BXIAllocator* allocator );
    RTTI_EXPORT void  BXUnload_rtti( void* plugin, BXIAllocator* allocator );
}