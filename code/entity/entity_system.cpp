#include "entity_system.h"
#include "entity_iterator.h"

#include <memory/memory.h>
#include <memory/pool.h>
#include <foundation/containers.h>
#include <foundation/static_array.h>
#include <foundation/id_table.h>
#include <foundation/string_util.h>
#include <foundation/hashmap.h>
#include <algorithm>
#include "foundation/thread/rw_spin_lock.h"
#include "foundation/bitset.h"

static constexpr uint32_t MAX_COMP_TYPES = 1 << 8;
static constexpr uint32_t MAX_ENT = 1 << ECSEntityID::INDEX_BITS_VALUE;
static constexpr uint32_t MAX_COMP = 1 << ECSComponentID::INDEX_BITS_VALUE;
static constexpr uint32_t MAX_COMP_PER_ENTITY = 1 << 7;
static constexpr uint16_t INVALID_TYPE_INDEX = UINT16_MAX;

struct ECSComponentTypeInfo
{
    string_t name;
    uint32_t index = INVALID_TYPE_INDEX;
    ECSComponentTypeDesc desc;

    void Initialize( uint16_t i, const char* n, const ECSComponentTypeDesc& d, BXIAllocator* allocator )
    {
        string::create( &name, n, allocator );
        index = i;
        desc = d;
    }
    void Uninitialize()
    {
        string::free( &name );
    }

    bool IsPOD() const { return !desc.Ctor && !desc.Dtor; }
};


struct ECSComponentStorage
{
    dynamic_pool_t _pool;
    array_t<ECSRawComponent*> _components;
    array_t<ECSRawComponent*> _pending_components;

    void StartUp( uint32_t element_size, uint32_t initial_num_elements, uint32_t alignment, BXIAllocator* allocator )
    {
        _pool = dynamic_pool_t::create( allocator, element_size, alignment, initial_num_elements );
    }
    void ShutDown()
    {
        dynamic_pool_t::destroy( &_pool );
    }

    ECSRawComponent* Allocate()
    {
        ECSRawComponent* comp = _pool.alloc();
        array::push_back( _pending_components, comp );
        return comp;
    };
    void Free( ECSRawComponent* comp )
    {
        const uint32_t index = array::find( _components, comp );
        SYS_ASSERT( index != array::npos );

        array::erase_swap( _components, index );
        _pool.free( comp );
    }

    void Flush()
    {
        for( ECSRawComponent* comp : _pending_components )
        {
            array::push_back( _components, comp );
        }
        array::clear( _pending_components );
    }
};

struct ECSEntityComponents
{
    ECSComponentID _components[MAX_ENT][MAX_COMP_PER_ENTITY];
    uint8_t _num_components[MAX_ENT];

    ECSEntityComponents()
    {
        memset( _components, 0x00, sizeof( _components ) );
        memset( _num_components, 0x00, sizeof( _num_components ) );
    }

    uint32_t Index( ECSEntityID eid, ECSComponentID cid )
    {
        const uint32_t n = _num_components[eid.index];
        for( uint32_t i = 0; i < n; ++i )
        {
            if( _components[eid.index][i].hash == cid.hash )
                return i;
        }
        return UINT32_MAX;
    }

    void Sort( ECSEntityID eid )
    {
        auto span = Components( eid );
        std::sort( span.begin(), span.end() );
    }

    void Add( ECSEntityID eid, const ECSComponentID* cid, uint32_t count )
    {
        SYS_ASSERT( _num_components[eid.index] + count <= MAX_COMP_PER_ENTITY );
        
        const uint32_t first_index = _num_components[eid.index];
        _num_components[eid.index] += count;
        
        for( uint32_t i = 0; i < count; ++i )
        {
            _components[eid.index][first_index+i] = cid[i];
        }
    }
    
    void Remove( ECSEntityID eid, const ECSComponentID* cid, uint32_t count )
    {
        const uint32_t eindex = eid.index;
        SYS_ASSERT( _num_components[eindex] >= count );
        
        for( uint32_t i = 0; i < count; ++i )
        {
            const uint32_t index = Index( eid, cid[i] );
            if( index != UINT32_MAX )
            {
                _components[eindex][index] = _components[eindex][--_num_components[eindex]];
                _components[eindex][_num_components[eindex]].hash = 0;
            }
        }
    }
    
    ECSComponentIDSpan Components( ECSEntityID id )
    {
        return ECSComponentIDSpan( _components[id.index], _num_components[id.index] );
    }

};

struct ECSEntityTree
{
    static_array_t<ECSEntityID, MAX_ENT> parent         { MAX_ENT };
    static_array_t<ECSEntityID, MAX_ENT> first_child    { MAX_ENT };
    static_array_t<ECSEntityID, MAX_ENT> next_slibling  { MAX_ENT };

    ECSEntityTree()
    {
        array::zero( parent );
        array::zero( first_child );
        array::zero( next_slibling );
    }
};

struct ECSImpl
{
    using EntityIdAlloc         = id_table_t<MAX_ENT, ECSEntityID>;
    using ComponentIdAlloc      = id_table_t<MAX_COMP, ECSComponentID>;

    BXIAllocator* allocator = nullptr;
    
    uint32_t                                             num_registered_comp = 0;
    hash_t<ECSComponentTypeInfo*>                        comp_type_info_map;
    static_array_t<ECSComponentTypeInfo, MAX_COMP_TYPES> comp_type_info;
    static_array_t<ECSComponentStorage, MAX_COMP_TYPES>  comp_storage;
    static_array_t<rw_spin_lock_t, MAX_COMP_TYPES>       comp_type_lock;

    ECSEntityComponents                         entity_components;
    static_array_t<rw_spin_lock_t, MAX_ENT>     entity_local_lock;
    ECSEntityTree                               entity_tree;
    static_array_t<ECSEntityID, MAX_ENT>        entity_live_id;
    bitset_t<MAX_ENT>                           entity_dead_mask; // stores indices from id
        
    hash_t<ECSComponentID>                      comp_ptr_to_id_map;
    static_array_t<uint16_t, MAX_COMP>          comp_type_index;
    static_array_t<ECSRawComponent*, MAX_COMP>  comp_address;
    static_array_t<ECSEntityID, MAX_COMP>       comp_owner;
    bitset_t<MAX_COMP>                          comp_dead_mask;

    EntityIdAlloc entity_id_alloc;
    ComponentIdAlloc comp_id_alloc;

    rw_spin_lock_t entity_global_lock;
    rw_spin_lock_t entity_dead_lock;

    rw_spin_lock_t comp_ptr_to_id_lock;
    rw_spin_lock_t comp_lock;
    rw_spin_lock_t comp_dead_lock;

    void StartUp( BXIAllocator* allocator );
    void ShutDown();
};

void ECSImpl::StartUp( BXIAllocator* allocator )
{
    this->allocator = allocator;

    array::resize( comp_address, MAX_COMP );
    array::resize( comp_owner, MAX_COMP );
    array::resize( entity_local_lock, MAX_ENT );

    for( uint32_t i = 0; i < MAX_COMP; ++i )
    {
        new( &entity_local_lock[i] ) rw_spin_lock_t();
    }

    for( uint32_t i = 0; i < MAX_COMP; ++i )
    {
        comp_owner[i].hash = 0;
        comp_address[i] = {};
    }
}


void ECSImpl::ShutDown()
{
    while( !array::empty( comp_type_info ) )
    {
        array::back( comp_type_info ).Uninitialize();
        array::pop_back( comp_type_info );
    }

    hash::clear( comp_type_info_map );

    while( !array::empty( comp_storage ) )
    {
        array::back( comp_storage ).ShutDown();
        array::pop_back( comp_storage );
    }
}

static inline bool IsAliveImpl( const ECSImpl* impl, ECSComponentID id )
{
    return id_table::has( impl->comp_id_alloc, id );
}
static inline bool IsAliveImpl( const ECSImpl* impl, ECSEntityID id )
{
    return id_table::has( impl->entity_id_alloc, id );
}

ECS* ECS::StartUp( BXIAllocator* allocator )
{
    uint32_t memory_size = 0;
    memory_size += sizeof( ECS );
    memory_size += sizeof( ECSImpl );

    void* memory = BX_MALLOC( allocator, memory_size, 16 );
    ECS* ecs = (ECS*)memory;
    ecs->impl = new (ecs + 1) ECSImpl();
    ecs->impl->StartUp( allocator );
    return ecs;
}

void ECS::ShutDown( ECS** ecs )
{
    if( !ecs[0] )
        return;

    ecs[0]->Update();
    ecs[0]->impl->ShutDown();
    ecs[0]->impl->~ECSImpl();
    BXIAllocator* allocator = ecs[0]->impl->allocator;
    BX_FREE0( allocator, ecs[0] );
}

ECSEntityID ECS::CreateEntity()
{
    scoped_write_spin_lock_t guard( impl->entity_global_lock );

    ECSEntityID id = id_table::create( impl->entity_id_alloc );
    
    SYS_ASSERT( array::find( impl->entity_live_id.begin(), impl->entity_live_id.size, id ) == array::npos );
    array::push_back( impl->entity_live_id, id );
    return id;
}
void DestroyEntity( ECSImpl* impl, ECSEntityID id )
{
    scoped_write_spin_lock_t guard( impl->entity_global_lock );

    if( !IsAliveImpl( impl, id ) )
        return;

    const uint32_t index = array::find( impl->entity_live_id.begin(), impl->entity_live_id.size, id );
    if( index == array::npos )
        return;

    //Unlink( id );
    array::erase_swap( impl->entity_live_id, index );
    id_table::destroy( impl->entity_id_alloc, id );
}


void ECS::MarkForDestroy( ECSEntityID id )
{
    id_table::invalidate( impl->entity_id_alloc, id );
    
    scoped_write_spin_lock_t guard( impl->entity_dead_lock );
    bitset::set( impl->entity_dead_mask, id.index );
}

bool ECS::IsAlive( ECSEntityID id ) const
{
    return ::IsAliveImpl( impl, id );
}

bool ECS::IsAlive( ECSComponentID id ) const
{
    return IsAliveImpl( impl, id );
}

void ECS::RegisterComponent( const char* type_name, const ECSComponentTypeDesc& desc )
{
    if( hash::has( impl->comp_type_info_map, desc.hash_code ) )
        return;

    ECSComponentTypeInfo& cinfo = array::emplace_back( impl->comp_type_info );
    cinfo.Initialize( impl->num_registered_comp++, type_name, desc, impl->allocator );
   
    hash::set( impl->comp_type_info_map, desc.hash_code, &cinfo );

    array::push_back( impl->comp_storage, ECSComponentStorage() );
    SYS_ASSERT( ( array::size( impl->comp_storage ) - 1 ) == cinfo.index );
    ECSComponentStorage& storage = array::back( impl->comp_storage );
    storage.StartUp( cinfo.desc.size, cinfo.desc.pool_chunk_size, 16, impl->allocator );
}

ECSNewComponent ECS::CreateComponent( size_t type_hash_code )
{
    const ECSComponentTypeInfo* info = hash::get( impl->comp_type_info_map, type_hash_code, (ECSComponentTypeInfo*)0 );
    if( !info )
        return ECSNewComponent::Null();

    ECSComponentID id = ECSComponentID::Null();

    {
        scoped_write_spin_lock_t guard( impl->comp_lock );
        id = id_table::create( impl->comp_id_alloc );
    }
    SYS_ASSERT( id != ECSComponentID::Null() );

    const uint32_t index = id.index;
    const uint32_t type_index = info->index;
    
    ECSRawComponent* comp = nullptr;
    {
        scoped_write_spin_lock_t comp_type_guard( impl->comp_type_lock[type_index] );
        comp = impl->comp_storage[type_index].Allocate();
    }

    if( info->desc.Ctor )
    {
        info->desc.Ctor( comp );
    }

    {
        scoped_write_spin_lock_t guard( impl->comp_ptr_to_id_lock );
        SYS_ASSERT( hash::has( impl->comp_ptr_to_id_map, (uint64_t)comp ) == false );
        hash::set( impl->comp_ptr_to_id_map, (uint64_t)comp, id );
    }
    
    impl->comp_type_index[index] = type_index;
    impl->comp_address[id.index] = comp;

    return { comp, id };
}

void DestroyComponent( ECSImpl* impl, ECSComponentID id )
{
    if( !IsAliveImpl( impl, id ) )
        return;

    id = id_table::invalidate( impl->comp_id_alloc, id );

    const uint32_t type_index = impl->comp_type_index[id.index];
    ECSRawComponent* comp = impl->comp_address[id.index];

    const ECSComponentTypeInfo& info = impl->comp_type_info[type_index];
    if( info.desc.Dtor )
    {
        info.desc.Dtor( comp );
    }

    {
        scoped_write_spin_lock_t guard( impl->comp_ptr_to_id_lock );
        SYS_ASSERT( hash::has( impl->comp_ptr_to_id_map, (uint64_t)comp ) == true );
        hash::remove( impl->comp_ptr_to_id_map, (uint64_t)comp );
    }

    impl->comp_address[id.index] = nullptr;
    impl->comp_type_index[id.index] = INVALID_TYPE_INDEX;

    ECSComponentStorage& storage = impl->comp_storage[type_index];
    impl->comp_type_lock[type_index].lock_write();
    storage.Free( comp );
    impl->comp_type_lock[type_index].unlock_write();

    {
        scoped_write_spin_lock_t guard( impl->comp_lock );
        id_table::destroy( impl->comp_id_alloc, id );
    }
}

void ECS::MarkForDestroy( ECSComponentID id )
{
    if( !IsAliveImpl( impl, id ) )
        return;

    Unlink( &id, 1 );
    id_table::invalidate( impl->comp_id_alloc, id );
    
    scoped_write_spin_lock_t guard( impl->comp_dead_lock );
    bitset::set( impl->comp_dead_mask, id.index );
}

ECSEntityID ECS::Owner( ECSComponentID id ) const
{
    if( !IsAliveImpl( impl, id ) )
        return ECSEntityID::Null();

    return impl->comp_owner[id.index];
}

ECSComponentID ECS::Lookup( const ECSRawComponent* pointer ) const
{
    if( !pointer )
        return ECSComponentID::Null();

    scoped_read_spin_lock_t guard( impl->comp_ptr_to_id_lock );
    return hash::get( impl->comp_ptr_to_id_map, (uint64_t)pointer, ECSComponentID::Null() );
}

ECSComponentID ECS::Lookup( ECSEntityID id, size_t type_hash_code ) const
{
    if( !IsAliveImpl( impl, id ) )
        return ECSComponentID::Null();

    const ECSComponentTypeInfo* type_info = hash::get( impl->comp_type_info_map, type_hash_code, (ECSComponentTypeInfo*)0 );
    SYS_ASSERT( type_info != nullptr );

    const uint32_t type_index = type_info->index;

    ECSComponentIDSpan span = impl->entity_components.Components( id );
    for( ECSComponentID cid : span )
    {
        if( IsAliveImpl( impl, cid ) )
        {
            if( impl->comp_type_index[cid.index] == type_index )
            {
                return cid;
            }
        }
    }

    return ECSComponentID::Null();
}

ECSRawComponent* ECS::Component( ECSComponentID id ) const
{
    if( !IsAliveImpl( impl, id ) )
        return nullptr;

    return impl->comp_address[id.index];
}

ECSRawComponent* ECS::ComponentSafe( ECSComponentID id, size_t type_id ) const
{
    if( !IsAliveImpl( impl, id ) )
        return nullptr;

#ifdef ASSERTION_ENABLED
    const uint32_t type_index = impl->comp_type_index[id.index];
    SYS_ASSERT( type_id == impl->comp_type_info[type_index].desc.hash_code );
#endif
    return impl->comp_address[id.index];
}

ECSRawComponentSpan ECS::Components( size_t type_hash_code ) const
{
    const ECSComponentTypeInfo* info = hash::get( impl->comp_type_info_map, type_hash_code, (ECSComponentTypeInfo*)0 );
    SYS_ASSERT( info != nullptr );

    ECSComponentStorage& storage = impl->comp_storage[info->index];

    return ECSRawComponentSpan( storage._components.begin(), storage._components.size );
}

ECSComponentIDSpan ECS::Components( ECSEntityID id ) const
{
    if( !IsAliveImpl( impl, id ) )
        return ECSComponentIDSpan();

    return impl->entity_components.Components( id );
}

void ECS::Link( ECSEntityID eid, const ECSComponentID* cid, uint32_t cid_count )
{
    if( !IsAliveImpl( impl, eid ) )
        return;

    static_array_t<ECSComponentID, 128> valid_components;

    for( uint32_t i = 0; i < cid_count; ++i )
    {
        if( IsAliveImpl( impl, cid[i] ) )
        {
            SYS_ASSERT( impl->comp_owner[cid[i].index].hash == 0 );
            impl->comp_owner[cid[i].index] = eid;
            array::push_back( valid_components, cid[i] );
        }
    }

    {
        scoped_write_spin_lock_t guard( impl->entity_local_lock[eid.index] );

        ECSEntityComponents& ec = impl->entity_components;
        ec.Add( eid, valid_components.begin(), valid_components.size );
        ec.Sort( eid );
#if ASSERTION_ENABLED == 1
        auto span = ec.Components( eid );
        SYS_ASSERT( std::unique( span.begin(), span.end() ) == span.end() );
#endif
    }
}

void ECS::Unlink( const ECSComponentID* cid, uint32_t cid_count )
{
    ECSEntityComponents& ec = impl->entity_components;

    static_array_t<ECSEntityID, 128> owners;

    for( uint32_t i = 0; i < cid_count; ++i )
    {
        ECSComponentID comp = cid[i];
        if( IsAliveImpl( impl, comp ) )
        {
            ECSEntityID& owner = impl->comp_owner[comp.index];
            if( owner.hash != 0 )
            {
                scoped_write_spin_lock_t guard( impl->entity_local_lock[owner.index] );
                ec.Remove( owner, &comp, 1 );
                array::push_back( owners, owner );
            }
            owner.hash = 0;
        }
    }

    std::sort( owners.begin(), owners.end() );
    ECSEntityID* unique_end = std::unique( owners.begin(), owners.end() );
    ECSEntityID* it = owners.begin();
    while( it != unique_end )
    {
        scoped_write_spin_lock_t guard( impl->entity_local_lock[it->index] );
        ec.Sort( *it );
        ++it;
    }
}

//void ECS::Link( ECSEntityID parent, ECSEntityID child )
//{
//    if( !IsAliveImpl( impl, parent ) || !IsAliveImpl( impl, child ) )
//        return;
//
//    Unlink( child );
//
//    ECSEntityTree& tree = impl->entity_tree;
//
//
//    ECSEntityID first_child = tree.first_child[parent.index];
//    tree.first_child[parent.index] = child;
//    tree.next_slibling[child.index] = first_child;
//    tree.parent[child.index] = parent;
//}
//
//void ECS::Unlink( ECSEntityID child )
//{
//    if( !IsAliveImpl( impl, child ) )
//        return;
//
//    ECSEntityTree& tree = impl->entity_tree;
//
//    ECSEntityID parent = tree.parent[child.index];
//    if( !IsAliveImpl( impl, parent ) )
//        return;
//
//    if( tree.first_child[parent.index] == child )
//    {
//        tree.first_child[parent.index] = tree.next_slibling[child.index];
//    }
//    else
//    {
//        ECSEntityID prev_slibling = tree.first_child[parent.index];
//        while( IsAliveImpl( impl, prev_slibling ) )
//        {
//            if( tree.next_slibling[prev_slibling.index] == child )
//                break;
//        
//            prev_slibling = tree.next_slibling[prev_slibling.index];
//        }
//        
//        if( IsAliveImpl( impl, prev_slibling ) )
//        {
//            tree.next_slibling[prev_slibling.index] = tree.next_slibling[child.index];
//        }
//    }
//
//    tree.parent[child.index] = ECSEntityID::Null();
//}

void ECS::Update()
{
    { // flush pending components
        for( uint32_t i = 0; i < impl->num_registered_comp; ++i )
        {
            scoped_write_spin_lock_t guard( impl->comp_type_lock[i] );
            impl->comp_storage[i].Flush();
        }
    }

    { // release the dead
        {
            scoped_write_spin_lock_t guard( impl->entity_dead_lock );
            for( bitset::const_iterator<bitset_t<MAX_ENT>> it( impl->entity_dead_mask ); it.ok(); it.next() )
            {
                ECSEntityID id = id_table::id( impl->entity_id_alloc, it.index() );
                ECSComponentIDSpan components = Components( id );
                {
                    for( ECSComponentID comp_id : components )
                    {
                        MarkForDestroy( comp_id );
                    }
                }
                DestroyEntity( impl, id );
            }
            bitset::clear_all( impl->entity_dead_mask );
        }
        {
            scoped_write_spin_lock_t guard( impl->comp_dead_lock );
            for( bitset::const_iterator<bitset_t<MAX_COMP>> it( impl->comp_dead_mask ); it.ok(); it.next() )
            {
                ECSComponentID id = id_table::id( impl->comp_id_alloc, it.index() );
                DestroyComponent( impl, id );
            }
            bitset::clear_all( impl->comp_dead_mask );
        }
    }
}


ECSEntityIterator::ECSEntityIterator( ECS* ecs, ECSEntityID id )
    : _impl( ecs->impl )
    , _current_id( id )
{}
bool ECSEntityIterator::IsValid() const
{
    return IsAliveImpl( _impl, _current_id );
}
void ECSEntityIterator::GoToSlibling()
{
    SYS_ASSERT( IsValid() );
    _current_id = _impl->entity_tree.next_slibling[_current_id.index];
}
void ECSEntityIterator::GoToChild()
{
    SYS_ASSERT( IsValid() );
    _current_id = _impl->entity_tree.first_child[_current_id.index];
}
void ECSEntityIterator::GoToParent()
{
    SYS_ASSERT( IsValid() );
    _current_id = _impl->entity_tree.parent[_current_id.index];
}

