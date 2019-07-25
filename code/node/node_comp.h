#pragma once

#include "../foundation/type.h"

struct NODEComp;

struct NODEComp__TypeInfo;
void NODECompRegisterType( const NODEComp__TypeInfo* tinfo );

struct NODEComp__TypeInfo
{
    using Constructor = NODEComp * (void* address);
    using Destructor = void( NODEComp* comp );
    const u64 type_hash_code;
    const char* name;
    const u32 alignment;
    const u32 size;
    const u32 num_chunks;

    Constructor* const constructor;
    Destructor* const destructor;

    NODEComp__TypeInfo( u64 t_hashcode, const char* n, u32 align, u32 siz, u32 nchunks, Constructor ctor, Destructor dtor )
        : type_hash_code( t_hashcode ), name( n ), alignment( align ), size( siz ), num_chunks( nchunks ), constructor( ctor ), destructor( dtor )
    {
        NODECompRegisterType( this );
    }
};

#define NODE_COMP_DECLARE() \
    static NODEComp__TypeInfo __type_info; \
    static NODEComp* Construct( void* address ); \
    static void Destruct( NODEComp* comp ); \
    static const char* TypeNameStatic() { return __type_info.name; } \
    template< typename T > static bool IsA() { return __type_info.type_hash_code == typeid(T).hash_code(); } \
    virtual u64 TypeHashCode() const { return __type_info.type_hash_code; } \
    virtual const char* TypeName() const { return __type_info.name; }

#define NODE_COMP_DEFINE( type_name ) \
    NODEComp* type_name::Construct( void* address ) { return new(address) type_name(); } \
    void type_name::Destruct( NODEComp* comp ) { ((type_name*)comp)->~type_name(); } \
    NODEComp__TypeInfo type_name::__type_info = NODEComp__TypeInfo( typeid(type_name).hash_code(), #type_name, ALIGNOF(type_name), sizeof(type_name), 32, type_name::Construct, type_name::Destruct )

