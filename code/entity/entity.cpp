#include "entity.h"
#include <foundation/id_array.h>
#include <foundation/thread/mutex.h>
#include <foundation/buffer.h>

struct ENTSystemComponent
{
    ent_ext_comp::TYPE* external_type;
    ent_ext_comp::ID*   external_id;
    ENTComponentID*     component_id;
    uint32_t count;
    uint32_t capacity;
};

struct ENTCustomComponent
{
    ENTComponent*        component;
    ENTComponentID*      component_id;
    uint32_t             count;
    uint32_t             capacity;
};

static ENTSystemComponent* AllocateSystemComponent( uint32_t capacity, BXIAllocator* allocator )
{
    uint32_t memory_size = sizeof( ENTSystemComponent );
    memory_size += capacity * sizeof( *ENTSystemComponent::external_type );
    memory_size += capacity * sizeof( *ENTSystemComponent::external_id );
    memory_size += capacity * sizeof( *ENTSystemComponent::component_id );

    void* memory = BX_MALLOC( allocator, memory_size, ALIGNOF( ENTSystemComponent ) );
    BufferChunker chunker( memory, memory_size );
    ENTSystemComponent* comp = chunker.Add<ENTSystemComponent>();
    comp->external_type = chunker.Add<ent_ext_comp::TYPE>( capacity );
    comp->external_id = chunker.Add<ent_ext_comp::ID>( capacity );
    comp->component_id = chunker.Add<ENTComponentID>( capacity );
    comp->count = 0;
    comp->capacity = 0;

    chunker.Check();

    return comp;
};


struct ENTPendingComponent
{
    ENTEntityID entity_id;
    ENTComponentID comp_id;
    union 
    {
        uint32_t flags;
        struct  
        {
            uint32_t system : 1;
            uint32_t to_add : 1;
        };
    }; 
    union
    {
        struct
        {
            ent_ext_comp::TYPE external_type;
            ent_ext_comp::ID   external_id;
        };
        struct  
        {
            ENTComponent* comp;
        };
    };

};

struct ENTSystemData
{
    ENTSystemComponent* system_comp = nullptr;
    ENTCustomComponent* custom_comp = nullptr;
    ENTEntityID*        entity_id = nullptr;
};


struct ENTSystem
{
    static constexpr uint32_t MAX_ENTITIES = 64;
    static constexpr uint32_t MAX_COMPONENTS = 2048;
    using EntityIDAllocator = id_array_t<MAX_ENTITIES>;
    using ComponentIDAllocator = id_array_t<MAX_COMPONENTS>;
    using PendingComponents = array_t<ENTPendingComponent>;

    mutex_t           _entity_lock;
    EntityIDAllocator _entity_id_alloc;

    mutex_t              _component_lock;
    ComponentIDAllocator _comp_id_alloc;

    ENTSystemData     _entity_data;



    mutex_t _pending_lock;
    PendingComponents _pending_components;
};

// ---

ENTEntityID ENT::CreateEntity()
{
    return { 0 };
}

void ENT::DestroyEntity()
{

}

ENT* ENT::StartUp()
{
    return nullptr;
}

void ENT::ShutDown( ENT** ent )
{

}
