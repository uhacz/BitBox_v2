#include "entity.h"
#include <foundation/array.h>
#include <foundation/id_array.h>
#include <foundation/id_table.h>
#include <foundation/thread/mutex.h>
#include <foundation/buffer.h>
#include "memory/tlsf_allocator.h"
#include "foundation/container_soa.h"
#include "rtti/rtti.h"

static constexpr uint32_t MAX_ENTITIES = 64;
static constexpr uint32_t MAX_COMPONENTS = 2048;
static constexpr uint32_t ENTITY_DEFAULT_NB_COMPONENTS = 8;

static constexpr uint32_t ENTITY_STORAGE_MEMORY_BUDGET = 1024 * 1024;

static constexpr ENTCComponent::TYPE COMP_TYPE_CUSTOM = ENTCComponent::RESERVED;

//union ENTComponentData
//{
//    ENTIComponent* custom_impl;
//    ENTCComponent::ID system_id;
//};
struct ENTComponentStorage
{
    ENTIComponent*      impl[MAX_COMPONENTS] = {};
    //ENTCComponent::TYPE type[MAX_COMPONENTS] = {};
    ENTComponentID      component_id[MAX_COMPONENTS] = {};
    ENTEntityID         entity_id[MAX_COMPONENTS] = {};
};

//using ENTComponentIDArray = array_t<ENTComponentID>;
//void PrepareComponentArray( ENTComponentIDArray* ca, BXIAllocator* allocator )
//{
//    SYS_ASSERT( ca->capacity == 0 );
//    ca->allocator = allocator;
//    array::reserve( *ca, ENTITY_DEFAULT_NB_COMPONENTS );
//}

static inline bool operator == ( const ENTComponentID& a, const ENTComponentID& b )
{
    return a.i == b.i;
}

template< typename T >
void IDArrayPushBack( array_t<T>* arr, const T& value, BXIAllocator* allocator )
{
    if( arr->capacity == 0 )
    {
        arr->allocator = allocator;
        array::reserve( *arr, ENTITY_DEFAULT_NB_COMPONENTS );
    }

    const uint32_t found = array::find( arr->begin(), arr->size, value );
    if( found == array::npos )
    {
        array::push_back( *arr, value );
    }
}

template< typename T >
void IDArrayRemove( array_t<T>* arr, const T& value )
{
    if( arr->size == 0 )
        return;

    const uint32_t found = array::find( arr->begin(), arr->size, value );
    if( found != array::npos )
    {
        array::erase( *arr, found );
    }
}

//void ComponentIDArrayPushBack( ENTComponentIDArray* ca, ENTComponentID comp_id, BXIAllocator* allocator )
//{
//    if( ca->capacity == 0 )
//        PrepareComponentArray( ca, allocator );
//
//    const uint32_t found = array::find( ca->begin(), ca->size, comp_id );
//    if( found == array::npos )
//    {
//        array::push_back( *ca, comp_id );
//    }
//}
//void ComponentIDArrayRemove( ENTComponentIDArray* ca, ENTComponentID comp_id )
//{
//    if( ca->size == 0 )
//        return;
//
//    const uint32_t found = array::find( ca->begin(), ca->size, comp_id );
//    if( found != array::npos )
//    {
//        array::erase( *ca, found );
//    }
//}

struct ENTEntityStorage
{
    //ENTComponentIDArray system;
    array_t<ENTCComponent::TYPE> system_type;
    array_t<ENTCComponent::ID > system_id;
    array_t<ENTComponentID> custom;
};

//void EntityComponentAdd( ENTEntityStorage* storage, ENTComponentID comp_id, ENTCComponent::TYPE type, BXIAllocator* allocator )
//{
//    if( type == COMP_TYPE_CUSTOM )
//        ComponentIDArrayPushBack( &storage->custom, comp_id, allocator );
//    else
//        ComponentIDArrayPushBack( &storage->system, comp_id, allocator );
//}
//void EntityComponentRemove( ENTEntityStorage* storage, ENTComponentID comp_id, ENTCComponent::TYPE type )
//{
//    if( type == COMP_TYPE_CUSTOM )
//        ComponentIDArrayRemove( &storage->custom, comp_id );
//    else
//        ComponentIDArrayRemove( &storage->system, comp_id );
//}

struct ENTPendingComponent
{
    ENTEntityID entity_id;
    ENTComponentID comp_id;
    ENTComponentExtID comp_ext_id;
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
    using ComponentIDArray = array_t<ENTComponentID>;

    mutex_t           entity_lock;
    EntityIDAllocator entity_id_alloc;
    ENTEntityStorage  entity_storage[MAX_ENTITIES];

    mutex_t              component_lock;
    ComponentIDAllocator comp_id_alloc;
    ENTComponentStorage  comp_storage;
        
    mutex_t pending_lock;
    PendingComponents pending_components;
    ComponentIDArray new_components;

    void* entity_storage_memory;
    TLSFAllocator entity_storage_allocator;
    BXIAllocator* main_allocator;
};
namespace
{
    ENTIComponent* AllocateComponent( ENT::ENTSystem* sys, const RTTITypeInfo* type_inf )
    {
        ENTIComponent* new_comp = (ENTIComponent*)(*type_inf->creator)( sys->main_allocator );
        return new_comp;
    }
    void FreeComponent( ENT::ENTSystem* sys, ENTIComponent* comp )
    {
        BX_DELETE( sys->main_allocator, comp );
    }
    
    ENTIComponent* GetCustomComponent( ENT::ENTSystem* sys, ENTComponentID id )
    {
        const id_t iid = { id.i };
        SYS_ASSERT( id_table::has( sys->comp_id_alloc, iid ) );
        //SYS_ASSERT( sys->comp_storage.type[iid.index] == COMP_TYPE_CUSTOM );

        return sys->comp_storage.impl[iid.index]; // .custom_impl;
    }
    //ENTCComponent::ID GetSystemComponent( ENT::ENTSystem* sys, ENTComponentID id )
    //{
    //    const id_t iid = { id.i };
    //    SYS_ASSERT( id_table::has( sys->comp_id_alloc, iid ) );
    //    SYS_ASSERT( sys->comp_storage.type[iid.index] != COMP_TYPE_CUSTOM );

    //    return sys->comp_storage.data[iid.index].system_id;
    //}

    void DeinitializeAndDestroyComponent( ENT::ENTSystem* ent, ENTComponentID comp_id, ENTSystemInfo* system_info )
    {
        ENTIComponent* impl = GetCustomComponent( ent, comp_id );
        impl->Detach( system_info );
        impl->Deinitialize( system_info );
        FreeComponent( ent, impl );
    }

    void ProcessPendingComponents( ENT::ENTSystem* ent, ENTSystemInfo* system_info )
    {
        scope_mutex_t guard( ent->pending_lock );

        while( !array::empty( ent->pending_components ) )
        {
            ENTPendingComponent pending = array::back( ent->pending_components );
            array::pop_back( ent->pending_components );

            const bool entity_valid = id_array::has( ent->entity_id_alloc, { pending.entity_id.i } );
            const bool component_valid = id_table::has( ent->comp_id_alloc, { pending.comp_id.i } );
            const bool component_ext_valid = pending.comp_ext_id.type != ENTCComponent::INVALID && pending.comp_ext_id.id != 0;
        
            if( entity_valid && component_valid )
            {
                const id_t eid = { pending.entity_id.i };
                const id_t cid = { pending.comp_id.i };
                
                ENTEntityStorage& storage = ent->entity_storage[eid.index];
                if( pending.to_add )
                {
                    IDArrayPushBack( &storage.custom, pending.comp_id, &ent->entity_storage_allocator );
                    array::push_back( ent->new_components, pending.comp_id );
                }
                else
                {
                    DeinitializeAndDestroyComponent( ent, pending.comp_id, system_info );
                    IDArrayRemove( &storage.custom, pending.comp_id );
                }
            }
            else if( entity_valid && component_ext_valid )
            {
                const id_t eid = { pending.entity_id.i };
                ENTEntityStorage& storage = ent->entity_storage[eid.index];
                if( pending.to_add )
                {
                    IDArrayPushBack( &storage.system_id, pending.comp_ext_id.id, &ent->entity_storage_allocator );
                    IDArrayPushBack( &storage.system_type, pending.comp_ext_id.type, &ent->entity_storage_allocator );
                }
                else
                {
                    IDArrayRemove( &storage.system_id, pending.comp_ext_id.id );
                    IDArrayRemove( &storage.system_type, pending.comp_ext_id.type );
                }
            }
            else if( entity_valid )
            {
                SYS_ASSERT( pending.to_add == 0 );
                
                const id_t eid = { pending.entity_id.i };
                ENTEntityStorage& storage = ent->entity_storage[eid.index];

                ENTComponentStorage& comp_storage = ent->comp_storage;

                for( uint32_t i = 0; i < storage.custom.size; ++i )
                {
                    DeinitializeAndDestroyComponent( ent, storage.custom[i], system_info );
                }

                array::destroy( storage.custom );
                array::destroy( storage.system_id );
                array::destroy( storage.system_type );
                
                const id_array_destroy_info_t destroy_info = id_array::destroy( ent->entity_id_alloc, eid );
                ent->entity_storage[destroy_info.copy_data_to_index] = ent->entity_storage[destroy_info.copy_data_from_index];
            }
            else
            {
                SYS_ASSERT( false );
            }
        }
    }

    void InitializeComponents( ENT::ENTSystem* ent, ENTSystemInfo* system_info )
    {
        for( uint32_t i = 0; i < ent->new_components.size; ++i )
        {
            ENTComponentID comp_id = ent->new_components[i];
            ENTIComponent* impl = GetCustomComponent( ent, comp_id );
            impl->Initialize( system_info );
        }
    }

    void AttachComponents( ENT::ENTSystem* ent, ENTSystemInfo* system_info )
    {
        for( uint32_t i = 0; i < ent->new_components.size; ++i )
        {
            ENTComponentID comp_id = ent->new_components[i];
            ENTIComponent* impl = GetCustomComponent( ent, comp_id );
            impl->Attach( system_info );
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

namespace
{
    void AddPendingComponent( ENT::ENTSystem* sys, const ENTPendingComponent& pending )
    {
        scope_mutex_t guard( sys->pending_lock );
        array::push_back( sys->pending_components, pending );
    }
}

void ENT::AttachComponent( ENTEntityID eid, ENTComponentExtID ext_id )
{
    //id_t id = {};
    //{
    //    scope_mutex_t guard( _ent->component_lock );
    //    id = id_table::create( _ent->comp_id_alloc );
    //}

    //ENTComponentStorage& cstorage = _ent->comp_storage;
    //cstorage.component_id[id.index].i = id.hash;
    //cstorage.data[id.index].system_id = cid;
    //cstorage.type[id.index] = type;
    //cstorage.entity_id[id.index] = eid;

    ENTPendingComponent pending = {};
    pending.entity_id = eid;
    pending.comp_id.i = 0;
    pending.comp_ext_id = ext_id;
    pending.to_add = 1;

    AddPendingComponent( _ent, pending );

    //return { id.hash };
}

void ENT::DetachComponent( ENTEntityID eid, ENTComponentExtID ext_id )
{
    ENTPendingComponent pending = {};
    pending.entity_id = eid;
    pending.comp_id.i = 0;
    pending.comp_ext_id = ext_id;
    pending.to_add = 0;

    AddPendingComponent( _ent, pending );
}

ENTComponentID ENT::CreateComponent( ENTEntityID eid, const char* type_name )
{
    const RTTITypeInfo* type_info = RTTI::FindType( type_name );
    if( !type_info )
        return { 0 };

    ENTIComponent* impl = AllocateComponent( _ent, type_info );
    if( !impl )
        return { 0 };

    id_t id = {};
    {
        scope_mutex_t guard( _ent->component_lock );
        id = id_table::create( _ent->comp_id_alloc );
    }

    ENTComponentStorage& cstorage = _ent->comp_storage;
    cstorage.component_id[id.index].i = id.hash;
    cstorage.impl[id.index] = impl;
    //cstorage.type[id.index] = COMP_TYPE_CUSTOM;
    cstorage.entity_id[id.index] = eid;

    ENTPendingComponent pending = {};
    pending.entity_id = eid;
    pending.comp_id.i = id.hash;
    pending.to_add = 1;

    AddPendingComponent( _ent, pending );

    return { id.hash };
}

void ENT::DestroyComponent( ENTEntityID eid, ENTComponentID cid )
{
    ENTPendingComponent pending = {};
    pending.entity_id = eid;
    pending.comp_id  = cid;
    pending.to_add = 0;

    AddPendingComponent( _ent, pending );
}

void ENT::Step( ENTSystemInfo* system_info, uint64_t dt_us )
{

    ProcessPendingComponents( _ent, system_info );
    InitializeComponents( _ent, system_info );
    AttachComponents( _ent, system_info );
    array::clear( _ent->new_components );


    const uint32_t nb_entities = id_array::size( _ent->entity_id_alloc );
    {// parallel step
        for( uint32_t ie = 0; ie < nb_entities; ++ie )
        {
            ENTEntityStorage& estorage = _ent->entity_storage[ie];
            for( uint32_t ic = 0; ic < estorage.custom.size; ++ic )
            {
                ENTIComponent* impl = GetCustomComponent( _ent, estorage.custom[ic] );
                impl->ParallelStep( system_info, dt_us );
            }
        }
    }

    {// serial step
        for( uint32_t ie = 0; ie < nb_entities; ++ie )
        {
            ENTEntityStorage& estorage = _ent->entity_storage[ie];
            for( uint32_t ic = 0; ic < estorage.custom.size; ++ic )
            {
                ENTIComponent* parent = nullptr;
                ENTIComponent* impl = GetCustomComponent( _ent, estorage.custom[ic] );
                impl->SerialStep( system_info, parent, dt_us );
            }
        }
    }
}


// ---
ENT* ENT::StartUp( BXIAllocator* allocator )
{
    uint32_t memory_size = sizeof( ENT ) + sizeof( ENTSystem );
    void* ent_memory = BX_MALLOC( allocator, memory_size, 8 );
    ENT* ent = (ENT*)ent_memory;
    ent->_ent = new (ent + 1) ENTSystem();

    {
        ent->_ent->entity_storage_memory = BX_MALLOC( allocator, ENTITY_STORAGE_MEMORY_BUDGET, 8 );
        TLSFAllocator::Create( &ent->_ent->entity_storage_allocator, ent->_ent->entity_storage_memory, ENTITY_STORAGE_MEMORY_BUDGET );
    }

    ent->_ent->main_allocator = allocator;

    return ent;
}

void ENT::ShutDown( ENT** ent, ENTSystemInfo* system_info )
{
    ENT* impl = ent[0];
    if( !impl )
        return;

    ProcessPendingComponents( impl->_ent, system_info );

    BXIAllocator* main_alloc = impl->_ent->main_allocator;
    TLSFAllocator::Destroy( &impl->_ent->entity_storage_allocator );
    BX_FREE0( main_alloc, impl->_ent->entity_storage_memory );
    
    impl->_ent->~ENTSystem();
    BX_DELETE0( main_alloc, ent[0] );
}

// ---
ENTIComponent::~ENTIComponent()
{}

void ENTIComponent::Initialize( ENTSystemInfo* )
{}

void ENTIComponent::Deinitialize( ENTSystemInfo* )
{}

void ENTIComponent::Attach( ENTSystemInfo* )
{}

void ENTIComponent::Detach( ENTSystemInfo* )
{}

void ENTIComponent::ParallelStep( ENTSystemInfo*, uint64_t dt_us )
{}

void ENTIComponent::SerialStep( ENTSystemInfo*, ENTIComponent* parent, uint64_t dt_us )
{}

int32_t ENTIComponent::Serialize( ENTSerializeInfo* info )
{
    return 0;
}
