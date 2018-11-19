#pragma once

#include "entity_id.h"
#include <foundation/containers.h>

struct BXIAllocator;
struct ECSImpl;

using ECSRawComponent = void;
using ECSRawComponentSpan = array_span_t<ECSRawComponent*>;
struct ECS
{
    ECSEntityID CreateEntity();
    ECSEntityID MarkForDestroy( ECSEntityID id );

    void RegisterComponent( const char* type_name, size_t type_hash_code, uint32_t struct_size );

    ECSComponentID CreateComponent( size_t type_hash_code );
    void MarkForDestroy( ECSComponentID id );

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
template< typename T>
inline void RegisterComponent( ECS* ecs, const char* type_name )
{ 
    ecs->RegisterComponent( type_name, typeid(T).hash_code(), (uint32_t)sizeof( T ) ); 
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

