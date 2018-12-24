#pragma once

#include "entity_id.h"
#include <typeinfo>
#include <foundation/containers.h>

struct BXIAllocator;
struct ECSImpl;

using ECSRawComponent = void;
using ECSRawComponentSpan = array_span_t<ECSRawComponent*>;
using ECSComponentIDSpan  = array_span_t<ECSComponentID>;

struct ECSComponentTypeDesc
{
    void( *Ctor )(ECSRawComponent* raw) = nullptr;
    void( *Dtor )(ECSRawComponent* raw) = nullptr;
    
    size_t hash_code = 0;
    uint16_t size = 0;
    uint16_t pool_chunk_size = 64;
};

struct ECS
{
    ECSEntityID CreateEntity();
    void MarkForDestroy( ECSEntityID id );
    bool IsAlive( ECSEntityID id ) const;

    void RegisterComponent( const char* name, const ECSComponentTypeDesc& desc );

    ECSComponentID CreateComponent( size_t type_hash_code );
    void MarkForDestroy( ECSComponentID id );
    bool IsAlive( ECSComponentID id ) const;

    ECSEntityID Owner( ECSComponentID id ) const;
    ECSComponentID Lookup( const ECSRawComponent* pointer ) const;
    ECSRawComponent* Component( ECSComponentID id ) const;
    ECSRawComponentSpan Components( size_t type_hash_code ) const;
    ECSComponentIDSpan  Components( ECSEntityID id ) const;

    //void Link( ECSEntityID parent, ECSEntityID child );
    //void Unlink( ECSEntityID child );

    void Link( ECSEntityID eid, const ECSComponentID* cid, uint32_t cid_count );
    void Unlink( const ECSComponentID* cid, uint32_t cid_count );

    void Update();

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

