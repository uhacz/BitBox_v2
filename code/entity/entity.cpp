#include "entity.h"
#include <foundation/array.h>
#include <foundation/id_array.h>
#include <foundation/id_table.h>
#include <foundation/thread/mutex.h>
#include <foundation/buffer.h>
#include "memory/tlsf_allocator.h"
#include "foundation/container_soa.h"

static constexpr uint32_t MAX_ENTITIES = 64;
static constexpr uint32_t MAX_COMPONENTS = 2048;
static constexpr uint32_t ENTITY_DEFAULT_NB_COMPONENTS = 8;

static constexpr uint32_t ENTITY_STORAGE_MEMORY_BUDGET = 1024 * 1024;

static constexpr ENTCComponent::TYPE COMP_TYPE_CUSTOM = ENTCComponent::RESERVED;

union ENTComponentData
{
    ENTComponent* custom_impl;
    ENTCComponent::ID system_id;
};
struct ENTComponentStorage
{
    ENTComponentData    data[MAX_COMPONENTS] = {};
    ENTCComponent::TYPE type[MAX_COMPONENTS] = {};
    ENTComponentID      component_id[MAX_COMPONENTS] = {};
    ENTEntityID         entity_id[MAX_COMPONENTS] = {};
};

using ENTComponentIDArray = array_t<ENTComponentID>;
void PrepareComponentArray( ENTComponentIDArray* ca, BXIAllocator* allocator )
{
    SYS_ASSERT( ca->capacity == 0 );
    ca->allocator = allocator;
    array::reserve( *ca, ENTITY_DEFAULT_NB_COMPONENTS );
}

static inline bool operator == ( const ENTComponentID& a, const ENTComponentID& b )
{
    return a.i == b.i;
}

void ComponentIDArrayPushBack( ENTComponentIDArray* ca, ENTComponentID comp_id, BXIAllocator* allocator )
{
    if( ca->capacity == 0 )
        PrepareComponentArray( ca, allocator );

    const uint32_t found = array::find( ca->begin(), ca->size, comp_id );
    if( found == array::npos )
    {
        array::push_back( *ca, comp_id );
    }
}
void ComponentIDArrayRemove( ENTComponentIDArray* ca, ENTComponentID comp_id )
{
    if( ca->size == 0 )
        return;

    const uint32_t found = array::find( ca->begin(), ca->size, comp_id );
    if( found != array::npos )
    {
        array::erase( *ca, found );
    }
}

struct ENTEntityStorage
{
    ENTComponentIDArray system;
    ENTComponentIDArray custom;
};

void EntityComponentAdd( ENTEntityStorage* storage, ENTComponentID comp_id, ENTCComponent::TYPE type, BXIAllocator* allocator )
{
    if( type == COMP_TYPE_CUSTOM )
        ComponentIDArrayPushBack( &storage->custom, comp_id, allocator );
    else
        ComponentIDArrayPushBack( &storage->system, comp_id, allocator );
}
void EntityComponentRemove( ENTEntityStorage* storage, ENTComponentID comp_id, ENTCComponent::TYPE type )
{
    if( type == COMP_TYPE_CUSTOM )
        ComponentIDArrayRemove( &storage->custom, comp_id );
    else
        ComponentIDArrayRemove( &storage->system, comp_id );
}

struct ENTPendingComponent
{
    ENTEntityID entity_id;
    ENTComponentID comp_id;
    ENTCComponent::TYPE comp_type;
    union 
    {
        uint16_t flags;
        struct  
        {
            uint16_t to_add : 1;
        };
    }; 
};

struct ENT::ENTSystem
{
    
    using EntityIDAllocator = id_array_t<MAX_ENTITIES>;
    using ComponentIDAllocator = id_table_t<MAX_COMPONENTS>;
    using PendingComponents = array_t<ENTPendingComponent>;

    mutex_t           entity_lock;
    EntityIDAllocator entity_id_alloc;
    ENTEntityStorage  entity_storage[MAX_ENTITIES];

    mutex_t              component_lock;
    ComponentIDAllocator comp_id_alloc;
    ENTComponentStorage  comp_storage;
        
    mutex_t pending_lock;
    PendingComponents pending_components;

    TLSFAllocator entity_storate_allocator;
    BXIAllocator* main_allocator;
};
namespace
{
    void ProcessPendingComponents( ENT::ENTSystem* ent )
    {
        scope_mutex_t guard( ent->pending_lock );

        while( !array::empty( ent->pending_components ) )
        {
            ENTPendingComponent pending = array::back( ent->pending_components );
            array::pop_back( ent->pending_components );

            const bool entity_valid = id_array::has( ent->entity_id_alloc, { pending.entity_id.i } );
            const bool component_valid = id_table::has( ent->comp_id_alloc, { pending.comp_id.i } );
        
            if( entity_valid && component_valid )
            {
                if( pending.to_add )
                {
                    
                }
            }
            else if( entity_valid )
            {

            }
            else
            {
                SYS_ASSERT( false );
            }
        }

    }
}
// ---

ENTEntityID ENT::CreateEntity()
{
    scope_lock_t<mutex_t> guard( _ent->entity_lock );
    id_t id = id_array::create( _ent->entity_id_alloc );
    return { id.hash };
}

void ENT::DestroyEntity( ENTEntityID entity_id )
{
    {
        scope_lock_t<mutex_t> guard( _ent->entity_lock );
        if( id_array::has( _ent->entity_id_alloc, { entity_id.i } ) )
        {
            id_t new_id = id_array::invalidate( _ent->entity_id_alloc, { entity_id.i } );
            entity_id.i = new_id.hash;
        }
        else
        {
            return;
        }
    }

    ENTPendingComponent pending;
    memset( &pending, 0x00, sizeof( pending ) );

    pending.entity_id = entity_id;
    {
        scope_mutex_t guard( _ent->pending_lock );
        array::push_back( _ent->pending_components, pending );
    }
}

void ENT::Step( ENTEStepPhase::E phase, uint64_t dt_us )
{

}


// ---
ENT* ENT::StartUp( BXIAllocator* allocator )
{
    uint32_t memory_size = sizeof( ENT ) + sizeof( ENTSystem );
    void* ent_memory = BX_MALLOC( allocator, memory_size, 8 );
    ENT* ent = (ENT*)ent_memory;
    ent->_ent = new (ent + 1) ENTSystem();

    {
        void* entity_storage_memory = BX_MALLOC( allocator, ENTITY_STORAGE_MEMORY_BUDGET, 8 );
        TLSFAllocator::Create( &ent->_ent->entity_storate_allocator, entity_storage_memory, ENTITY_STORAGE_MEMORY_BUDGET );
    }

    ent->_ent->main_allocator = allocator;

    return ent;
}

void ENT::ShutDown( ENT** ent )
{
    ENT* impl = ent[0];
    if( !impl )
        return;

    BXIAllocator* main_alloc = impl->_ent->main_allocator;
    TLSFAllocator::Destroy( &impl->_ent->entity_storate_allocator );

    impl->_ent->~ENTSystem();
    BX_DELETE0( main_alloc, ent[0] );
}

// ---
ENTComponent::~ENTComponent()
{}

void ENTComponent::Initialize( ENTSystemInfo* )
{}

void ENTComponent::Deinitialize( ENTSystemInfo* )
{}

void ENTComponent::Attach( ENTSystemInfo* )
{}

void ENTComponent::Detach( ENTSystemInfo* )
{}

void ENTComponent::ParallelStep( ENTSystemInfo*, uint64_t dt_us )
{}

void ENTComponent::SerialStep( ENTSystemInfo*, ENTComponent* parent, uint64_t dt_us )
{}

int32_t ENTComponent::Serialize( ENTSerializeInfo* info )
{
    return 0;
}
