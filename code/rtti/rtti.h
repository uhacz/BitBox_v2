#pragma once

#include <plugin/plugin_interface.h>
#include <foundation/type.h>
#include <typeinfo>

struct BXIAllocator;

#define RTTI_DECLARE_TYPE( name ) \
    static const char* TypeName() { return #name; };\
    static const RTTIAttr* __attributes[];\
    static const uint32_t __nb_attributes;\
    static void* __Creator( BXIAllocator* allocator )
//
#define RTTI_DEFINE_TYPE( name, ... )\
    const RTTIAttr* name::__attributes[] = __VA_ARGS__;\
    const uint32_t name::__nb_attributes = (uint32_t)sizeof_array( name::__attributes );\
    void* name::__Creator( BXIAllocator* allocator ) { return BX_NEW( allocator, name ); }\
    struct __RTTI_Initializator_name\
    {\
        __RTTI_Initializator_name()\
        {\
            RTTI::RegisterType( #name, RTTITypeInfo( name::__attributes, name::__nb_attributes, name::__Creator ) );\
        }\
    };\
    static __RTTI_Initializator_name __rtti_initializator_name = __RTTI_Initializator_name()
    

#define RTTI_ATTR( type, field, name ) RTTI::Create( &type::field, name )


#ifdef BX_DLL_rtti
#define RTTI_EXPORT __declspec(dllexport)
#else
#define RTTI_EXPORT __declspec(dllimport)
#endif

struct RTTI_EXPORT RTTIAttr
{
    // --- setters
    RTTIAttr* SetDefaultData( const void* data, uint32_t data_size, const std::type_info& data_type_info );
    RTTIAttr* SetMinData( const void* data, uint32_t data_size, const std::type_info& data_type_info );
    RTTIAttr* SetMaxData( const void* data, uint32_t data_size, const std::type_info& data_type_info );

    template< typename T >
    RTTIAttr* SetDefault( const T& value ) 
    { 
        _is_pointer_default = std::is_pointer< std::decay<decltype(value)>::type >::value;
        return SetDefaultData( &value, (uint32_t)sizeof(T), typeid( std::decay<decltype(value)>::type )); 
    }

    template< typename T >
    RTTIAttr* SetMin( const T& value ) { return SetMinData( &value, (uint32_t)sizeof( T ), typeid(std::decay<T>::type) ); }
    
    template< typename T >
    RTTIAttr* SetMax( const T& value ) { return SetMaxData( &value, (uint32_t)sizeof( T ), typeid(std::decay<T>::type) ); }

    // --- getters
    const char*    TypeName() const;
    const char*    Name() const;
    const uint8_t* DefaultPtr() const;
    const uint8_t* MinPtr() const;
    const uint8_t* MaxPtr() const;
    const uint8_t* ValuePtr( const void* obj ) const;

    template< typename T >
    const T& Default() const
    {
        const uint8_t* ptr = DefaultPtr();
        SYS_ASSERT( typeid(std::decay<T>::type).hash_code() == _defaults_storage_hashcode );
        return (_is_pointer_default) ? (const T&)ptr : (const T&)(*(T*)ptr);
    }
    template< typename T >
    const T& Min() const
    {
        SYS_ASSERT( _is_pointer_default == 0 );
        SYS_ASSERT( typeid(std::decay<T>::type).hash_code() == _defaults_storage_hashcode );
        return (const T&)(*(T*)MinPtr());
    }
    template< typename T >
    const T& Max() const
    {
        SYS_ASSERT( _is_pointer_default == 0 );
        const uint8_t* ptr = MaxPtr();
        SYS_ASSERT( typeid(std::decay<T>::type).hash_code() == _defaults_storage_hashcode );
        return (const T&)(*(T*)ptr);
    }

    template< typename T >
    const T& Value( const void* obj ) const
    {
        SYS_ASSERT( typeid(std::decay<T>::type) == _type_info );
        const uint8_t* ptr = ValuePtr( obj );
        return *(const T*)(ptr);
    }

    // --- can't touch this
    const std::type_info& _type_info;
    const uint32_t _offset : 24;
    const uint32_t _size : 8;

    uint16_t _offset_default_value;
    uint16_t _offset_min_value;
    uint16_t _offset_max_value;
    uint16_t _offset_attr_name;

    size_t _defaults_storage_hashcode;

    union 
    {
        uint32_t _flags;
        struct  
        {
            uint32_t _is_pointer : 1;
            uint32_t _is_pointer_default : 1;
            uint32_t _is_pod : 1;
            uint32_t _is_float : 1;
            uint32_t _is_integral : 1;
            uint32_t _is_enum : 1; 
            uint32_t _is_signed : 1;
        };
    };
};

typedef void*(*RTTIObjectCreator)( BXIAllocator* allocator );
struct RTTI_EXPORT RTTITypeInfo
{
    const RTTIAttr* const* attributes;
    const uint32_t nb_attributes;
    RTTIObjectCreator creator;

    RTTITypeInfo();
    RTTITypeInfo( const RTTIAttr* const* attribs, uint32_t nb_attribs, RTTIObjectCreator creator );
};

struct RTTI_EXPORT RTTI
{
    static void RegisterType( const char* name, const RTTITypeInfo& info );
    static const RTTITypeInfo* FindType( const char* name );

    static RTTIAttr* AllocateAttribute(uint32_t size);

    template< typename P, typename T >
    static RTTIAttr* Create( T P::*M, const char* name )
    {
        SYS_ASSERT(typeid(T) != typeid(const char*) && "c strings are not supported. Use string_t instead");

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
        attr->_defaults_storage_hashcode = 0;

        attr->_flags = 0;
        attr->_is_pointer  = std::is_pointer<T>::value;
        //attr->_is_pointer_default : 1;
        attr->_is_pod      = std::is_pod<T>::value;
        attr->_is_float    = std::is_floating_point<T>::value;
        attr->_is_integral = std::is_integral<T>::value;
        attr->_is_enum     = std::is_enum<T>::value;
        attr->_is_signed   = std::is_signed<T>::value;

        return attr;
    }

    template< typename T >
    static const RTTIAttr* Find( const char* name )
    {
        return _FindAttr( T::__attributes, T::__nb_attributes, name );
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


    template< typename T >
    static uint32_t Serialize( uint8_t* buffer, uint32_t buffer_capacity, const T& obj )
    {
        return _Serialize( buffer, buffer_capacity, T::__attributes, T::__nb_attributes, &obj );
    }
    
    template< typename T >
    static uint32_t Unserialize( T* obj, const uint8_t* buffer, uint32_t buffer_size, BXIAllocator* allocator )
    {
        return _Unserialize( obj, T::__attributes, T::__nb_attributes, buffer, buffer_size, allocator );
    }

    static uint32_t _Serialize  ( uint8_t* buffer, uint32_t buffer_capacity, const RTTIAttr** attributes, uint32_t nb_attributes, const void* obj );
    static uint32_t _Unserialize( void* obj, const RTTIAttr** attributes, uint32_t nb_attributes, const uint8_t* buffer, uint32_t buffer_size, BXIAllocator* allocator );

    static const RTTIAttr* _FindAttr( const RTTIAttr** attributes, uint32_t nb_attributes, const char* name );
};

#define BX_RTTI_PLUGIN_NAME "rtti"

extern "C" {
    RTTI_EXPORT void* BXLoad_rtti( BXIAllocator* allocator );
    RTTI_EXPORT void  BXUnload_rtti( void* plugin, BXIAllocator* allocator );
}