#pragma once

#include "../foundation/type.h"

namespace ecs
{
    static constexpr u32 MAX_ENTITIES_BITS = 16;
    static constexpr u32 MAX_ENTITIES = 1 << MAX_ENTITIES_BITS;

    static constexpr u32 MAX_COMPONENTS_BITS = 18;
    static constexpr u32 MAX_COMPONENTS = 1 << MAX_COMPONENTS_BITS;


    union EntityId
    {
        u32 hash;
        struct
        {
            u32 index : MAX_ENTITIES_BITS;
            u32 generation : 32 - MAX_ENTITIES_BITS;
        };
    };

    union ComponentId
    {
        u32 hash;
        struct
        {
            u32 index : MAX_COMPONENTS_BITS;
            u32 generation : 32 - MAX_COMPONENTS_BITS;
        };
    };

    struct ComponentType
    {
        const char* name;
        u64 hash_code;
        u32 size;
        u32 alignment;
    
        template< typename T > static ComponentType Define( const char* name )
        {
            ComponentType result =
            {
                name,
                typeid( T ).hash_code(),
                sizeof( T ),
                ALIGNOF( T )
            };
            return result;

        }
    };

    void RegisterType( const ComponentType& ct );

    EntityId CreateEntity();
    void DestroyEntity( EntityId id );

    ComponentId CreateComponent( u64 type_hash_code );
    template< typename T >
    ComponentId CreateComponent() { return CreateComponent( typeid(T).hash_code() ); }
    
    void DestroyComponent( ComponentId id );


    template< typename T >
    struct ComponentView
    {
        array_span_t< T > data;
        array_span_t< u32 > indices;
    };
    
    template< typename T >
    struct ComponentViewRO : ComponentView< const T >
    {
        const T& Get( ComponentId id ) const { return (indices.size) ? data[indices[id.index]] : data[id.index]; }
    };

    template< typename T >
    struct ComponentViewRW : ComponentView<T>
    {
        void Set( ComponentId id, const T& value )
        {
            (indices.size) ? 
                data[indices[id.index]] = value : 
                data[id.index] = value;
        }
    };


}


