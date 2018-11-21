#pragma once

#include "entity_id.h"
#include <foundation/containers.h>

struct BXIAllocator;
struct ECSImpl;

using ECSRawComponent = void;
using ECSRawComponentSpan = array_span_t<ECSRawComponent*>;

struct ECSComponentTypeDesc
{
    void( *Ctor )(ECSRawComponent* raw) = nullptr;
    void( *Dtor )(ECSRawComponent* raw) = nullptr;
    
    size_t hash_code = 0;
    uint16_t size = 0;
    uint16_t pool_chunk_size = 64;
};
using ECSComponentDestructor = void( ECSRawComponent* raw );


struct ECS
{
    ECSEntityID CreateEntity();
    ECSEntityID MarkForDestroy( ECSEntityID id );

    void RegisterComponent( const char* name, const ECSComponentTypeDesc& desc );

    ECSComponentID CreateComponent( size_t type_hash_code );
    void MarkForDestroy( ECSComponentID id );

    ECSEntityID Owner( ECSComponentID id );
    ECSComponentID Lookup( const ECSRawComponent* pointer );
    ECSRawComponent* Component( ECSComponentID id );
    ECSRawComponentSpan Components( size_t type_hash_code );

    void Link( ECSEntityID parent, ECSEntityID child );
    void Unlink( ECSEntityID child );

    void Link( ECSEntityID eid, const ECSComponentID* cid, uint32_t cid_count );
    void Unlink( const ECSComponentID* cid, uint32_t cid_count );



    //
    ECSImpl *impl = nullptr;
    static ECS* StartUp( BXIAllocator* allocator );
    static void ShutDown( ECS** ecs );
};

//
// helpers
//

#define ECS_NON_POD_COMPONENT( type ) \
    static void _Ctor( ECSRawComponent* raw ) { new(raw) type(); } \
    static void _Dtor( ECSRawComponent* raw ) { ((type*)raw)->~type(); }

template< typename T>
inline void RegisterComponent( ECS* ecs, const char* type_name )
{ 
    ECSComponentTypeDesc desc = {};
    desc.hash_code = typeid(T).hash_code();
    desc.size = (uint32_t)sizeof( T );

    ecs->RegisterComponent( type_name, desc );
}

template< typename T>
inline void RegisterComponentNoPOD( ECS* ecs, const char* type_name )
{
    ECSComponentTypeDesc desc = {};
    desc.hash_code = typeid(T).hash_code();
    desc.size = (uint32_t)sizeof( T );
    desc.Ctor = T::_Ctor;
    desc.Dtor = T::_Dtor;

    ecs->RegisterComponent( type_name, desc );
}

template< typename T>
inline ECSComponentID CreateComponent( ECS* ecs ) 
{ 
    return ecs->CreateComponent( typeid(T).hash_code() );
}

template< typename T>
inline T* Component( ECS* ecs, ECSComponentID id ) 
{ 
    return (T*)ecs->Component( id ); 
}

template< typename T>
inline array_span_t<T*> Components( ECS* ecs ) 
{ 
    ECSRawComponentSpan raw_span = ecs->Components( typeid(T).hash_code() );
    return array_span_t<T*>( (T**)raw_span.begin(), raw_span.size() ); 
}

