#pragma once

#include <foundation/containers.h>

struct BXIAllocator;

template< typename T, uint32_t INDEX_BITS >
union ECSID
{
    static constexpr uint32_t INDEX_BITS_VALUE = INDEX_BITS;
    T hash = 0;
    
    struct
    {
        T index : INDEX_BITS;
        T id : (sizeof(T)*8 - INDEX_BITS); // aka generation
    };
};

using ECSEntityID = ECSID<uint32_t, 10>;
using ECSComponentID = ECSID<uint32_t, 12>;

struct ECSComponentBlob
{};

struct ECSImpl;
struct ECS
{
    ECSEntityID CreateEntity();
    void DestroyEntity( ECSEntityID id );

    void RegisterComponent( const char* type_name, size_t type_hash_code, uint32_t struct_size );

    ECSComponentID CreateComponent( size_t type_hash_code );
    void DestroyComponent( ECSComponentID id );

    uint8_t* Component( ECSComponentID id );
    Blob Components( size_t type_hash_code );

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
inline array_span_t<T> Components( ECS* ecs ) 
{ 
    return ToArraySpan<T>( ecs->Components( typeid(T).hash_code() ) ); 
}

