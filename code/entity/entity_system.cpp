#include "entity_system.h"
#include "entity_iterator.h"

#include <memory/memory.h>
#include <foundation/containers.h>
#include <foundation/static_array.h>
#include <foundation/id_table.h>
#include <foundation/string_util.h>
#include <foundation/hashmap.h>
#include <algorithm>

static constexpr uint32_t MAX_COMP_TYPES = 1 << 8;
static constexpr uint32_t MAX_ENT = 1 << ECSEntityID::INDEX_BITS_VALUE;
static constexpr uint32_t MAX_COMP = 1 << ECSComponentID::INDEX_BITS_VALUE;
static constexpr uint32_t MAX_COMP_PER_ENTITY = 1 << 7;

struct ECSComponentInfo
{
    string_t type_name;
    uint32_t struct_size = 0;
    uint32_t type_index = UINT32_MAX;
    uint32_t granularity = 16;
};

struct ECSComponentStorage
{
    uint8_t* _data = nullptr;
    uint32_t _size = 0;
    uint32_t _capacity = 0;
    uint32_t _element_size = 0;
    uint32_t _element_alignment = 0;

    BXIAllocator* _allocator = nullptr;

    struct RemoveResult
    {
        uint32_t removed = UINT32_MAX;
        uint32_t last = UINT32_MAX;

        bool Succeed() const { return removed != UINT32_MAX; }
    };

    void StartUp( uint32_t element_size, uint32_t initial_capacity_in_bytes, uint32_t alignment, BXIAllocator* allocator )
    {
        SYS_ASSERT( _data == nullptr );
        _allocator = allocator;
        _data = (uint8_t*)BX_MALLOC( allocator, initial_capacity_in_bytes, alignment );
        _capacity = initial_capacity_in_bytes;
        _size = 0;

        _element_size = element_size;
        _element_alignment = alignment;
        
    }
    void ShutDown()
    {
        BX_FREE( _allocator, _data );
    }

    uint8_t* Pointer( uint32_t offset )
    {
        SYS_ASSERT( offset + _element_size <= _size );
        return _data + offset;
    }
    const uint8_t* Pointer( uint32_t offset ) const
    {
        SYS_ASSERT( offset + _element_size <= _size );
        return _data + offset;
    }
    
    void Grow( uint32_t new_capacity )
    {
        SYS_NOT_IMPLEMENTED;
    }

    uint32_t PushBack( const void* data = nullptr )
    {
        if( _size + _element_size > _capacity )
        {
            const uint32_t new_capacity = (_capacity) ? _capacity * 2 : 8;
            Grow( new_capacity );
        }
        
        const uint32_t offset = _size;
        if( data )
        {
            memcpy( _data + offset, data, _element_size );
        }
        _size += _element_size;

        return offset;
    }

    RemoveResult RemoveAt( uint32_t offset )
    {
        SYS_ASSERT( (offset % _element_size) == 0 );
        
        RemoveResult result;
        if( _size )
        {
            result.removed = offset;
            result.last = _size - _element_size;

            if( _size != result.last )
            {
                memcpy( _data + offset, _data + result.last, _element_size );
            }
            _size -= _element_size;
        }
        
        return result;
    }
};

union ECSComponentAddress
{
    uint64_t hash = UINT64_MAX;
    struct
    {
        uint32_t data_offset;
        uint32_t type_index;
    };
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
    
    array_span_t<ECSComponentID> Components( ECSEntityID id )
    {
        return array_span_t<ECSComponentID>( _components[id.index], _num_components[id.index] );
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
    using ComponentAddressArray = static_array_t<ECSComponentAddress, MAX_COMP>;
    using ComponentOwnerArray   = static_array_t<ECSEntityID, MAX_COMP>;
    using ComponentAddressMap   = hash_t<ECSComponentID>;
    using ComponentInfoMap      = hash_t<ECSComponentInfo>;
    using ComponentStorageArray = static_array_t<ECSComponentStorage, MAX_COMP_TYPES>;
    

    BXIAllocator* allocator = nullptr;
    
    ComponentInfoMap comp_info_map;
    ComponentAddressMap comp_address_map;
    ComponentAddressArray comp_address;
    ComponentOwnerArray comp_owner;
    ComponentStorageArray comp_storage;
    uint32_t num_registered_comp = 0;

    ECSEntityComponents entity_components;
    ECSEntityTree entity_tree;

    EntityIdAlloc entity_id_alloc;
    ComponentIdAlloc comp_id_alloc;


    void StartUp( BXIAllocator* allocator );
    void ShutDown();
};

void ECSImpl::StartUp( BXIAllocator* allocator )
{
    this->allocator = allocator;

    array::resize( comp_address, MAX_COMP );
    array::resize( comp_owner, MAX_COMP );

    for( uint32_t i = 0; i < MAX_COMP; ++i )
    {
        comp_owner[i].hash = 0;
        comp_address[i] = {};
    }
}


void ECSImpl::ShutDown()
{
    for( auto* it = hash::begin( comp_info_map ); it != hash::end( comp_info_map ); ++it )
    {
        string::free( (string_t*)&it->value.type_name );
    }

    while( !array::empty( comp_storage ) )
    {
        array::back( comp_storage ).ShutDown();
        array::pop_back( comp_storage );
    }
}

static inline bool IsAlive( const ECSImpl* impl, ECSComponentID id )
{
    return id_table::has( impl->comp_id_alloc, id );
}
static inline bool IsAlive( const ECSImpl* impl, ECSEntityID id )
{
    return id_table::has( impl->entity_id_alloc, id );
}


static inline ECSComponentID AllocateComponentID( ECSImpl* impl )
{
    return id_table::create( impl->comp_id_alloc );
}
static inline void FreeComponentID( ECSImpl* impl, ECSComponentID id )
{
    id_table::destroy( impl->comp_id_alloc, id );
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

    ecs[0]->impl->ShutDown();
    ecs[0]->impl->~ECSImpl();
    BXIAllocator* allocator = ecs[0]->impl->allocator;
    BX_FREE0( allocator, ecs[0] );
}

ECSEntityID ECS::CreateEntity()
{
    ECSEntityID id = id_table::create( impl->entity_id_alloc );
    return id;
}

ECSEntityID ECS::MarkForDestroy( ECSEntityID id )
{
    return id_table::invalidate( impl->entity_id_alloc, id );
}

void ECS::DestroyEntity( ECSEntityID id )
{
    Unlink( id );
    id_table::destroy( impl->entity_id_alloc, id );
}

void ECS::RegisterComponent( const char* type_name, size_t type_hash_code, uint32_t struct_size )
{
    if( hash::has( impl->comp_info_map, type_hash_code ) )
        return;

    ECSComponentInfo cinfo;
    cinfo.struct_size = struct_size;
    cinfo.type_index = impl->num_registered_comp++;
    string::create( &cinfo.type_name, type_name, impl->allocator );
    hash::set( impl->comp_info_map, type_hash_code, cinfo );

    impl->comp_storage[cinfo.type_index].StartUp( cinfo.struct_size, cinfo.granularity * cinfo.struct_size, 16, impl->allocator );
}

ECSComponentID ECS::CreateComponent( size_t type_hash_code )
{
    const ECSComponentInfo& info = hash::get( impl->comp_info_map, type_hash_code, ECSComponentInfo() );
    if( info.type_index == UINT32_MAX )
        return ECSComponentID();

    ECSComponentID id = AllocateComponentID( impl );
    const uint32_t index = id.index;
    ECSComponentAddress& address = impl->comp_address[index];
    address.data_offset = impl->comp_storage[info.type_index].PushBack();
    address.type_index = info.type_index;

    SYS_ASSERT( hash::has( impl->comp_address_map, address.hash ) == false );
    hash::set( impl->comp_address_map, address.hash, id );

    return id;
}

void ECS::DestroyComponent( ECSComponentID id )
{
    if( !IsAlive( impl, id ) )
        return;

    const ECSComponentAddress& address = impl->comp_address[id.index];

    ECSComponentStorage& storage = impl->comp_storage[address.type_index];
    const ECSComponentStorage::RemoveResult result = storage.RemoveAt( address.data_offset );
    SYS_ASSERT( result.Succeed() );

    {
        ECSComponentAddress last_address;
        last_address.type_index = address.type_index;
        last_address.data_offset = result.last;

        SYS_ASSERT( hash::has( impl->comp_address_map, last_address.hash ) );
        const ECSComponentID last_id = hash::get( impl->comp_address_map, last_address.hash, ECSComponentID() );
       
        impl->comp_address[last_id.index] = address;
        impl->comp_address[id.index] = ECSComponentAddress();
        hash::set( impl->comp_address_map, address.hash, last_id );
        hash::remove( impl->comp_address_map, last_address.hash );

    }

    FreeComponentID( impl, id );
}

uint8_t* ECS::Component( ECSComponentID id )
{
    if( !IsAlive( impl, id ) )
        return nullptr;

    const ECSComponentAddress& address = impl->comp_address[id.index];
    ECSComponentStorage& storage = impl->comp_storage[address.type_index];
    return storage.Pointer( address.data_offset );
}

Blob ECS::Components( size_t type_hash_code )
{
    const ECSComponentInfo& info = hash::get( impl->comp_info_map, type_hash_code, ECSComponentInfo() );
    SYS_ASSERT( info.type_index != UINT32_MAX );

    ECSComponentStorage& storage = impl->comp_storage[info.type_index];

    Blob blob;
    blob.data = storage._data;
    blob.size = storage._size;
    return blob;
}

void ECS::Link( ECSEntityID eid, const ECSComponentID* cid, uint32_t cid_count )
{
    if( !IsAlive( impl, eid ) )
        return;

    static_array_t<ECSComponentID, 128> valid_components;

    for( uint32_t i = 0; i < cid_count; ++i )
    {
        if( IsAlive( impl, cid[i] ) )
        {
            SYS_ASSERT( impl->comp_owner[cid[i].index].hash == 0 );
            impl->comp_owner[cid[i].index] = eid;
            array::push_back( valid_components, cid[i] );
        }
    }

    ECSEntityComponents& ec = impl->entity_components;
    ec.Add( eid, valid_components.begin(), valid_components.size );
    ec.Sort( eid );
#if ASSERTION_ENABLED == 1
    auto span = ec.Components( eid );
    SYS_ASSERT( std::unique( span.begin(), span.end() ) == span.end() );
#endif
}

void ECS::Unlink( const ECSComponentID* cid, uint32_t cid_count )
{
    ECSEntityComponents& ec = impl->entity_components;

    static_array_t<ECSEntityID, 128> owners;

    for( uint32_t i = 0; i < cid_count; ++i )
    {
        ECSComponentID comp = cid[i];
        if( IsAlive( impl, comp ) )
        {
            ECSEntityID& owner = impl->comp_owner[comp.index];
            SYS_ASSERT( owner.hash != 0 );

            ec.Remove( owner, &comp, 1 );
            array::push_back( owners, owner );

            owner.hash = 0;
        }
    }

    std::sort( owners.begin(), owners.end() );
    ECSEntityID* unique_end = std::unique( owners.begin(), owners.end() );
    ECSEntityID* it = owners.begin();
    while( it != unique_end )
    {
        ec.Sort( *it );
        ++it;
    }
}

void ECS::Link( ECSEntityID parent, ECSEntityID child )
{

}

void ECS::Unlink( ECSEntityID child )
{

}

ECSEntityIterator::ECSEntityIterator( ECS* ecs, ECSEntityID id )
    : _impl( ecs->impl )
    , _current_id( id )
{}
bool ECSEntityIterator::IsValid() const
{
    return IsAlive( _impl, _current_id );
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

