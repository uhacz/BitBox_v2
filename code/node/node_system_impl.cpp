#include "node_system_impl.h"
#include "node.h"
#include "../foundation/array.h"
#include "../foundation/string_util.h"
#include "../foundation/c_array.h"
#include "../foundation/hashmap.h"
#include "../foundation/hash.h"
#include "node_serialize.h"
#include "node_comp.h"

inline u32 CalcHash( const guid_t& guid )
{
    return murmur3_hash32( &guid, sizeof( guid_t ), 2166136261 );
}

void NODEContainerImpl::StartUp( BXIAllocator* alloc )
{
    _allocator = alloc;
    _node_allocator = alloc;
    _name_allocator = alloc;
}

void NODEContainerImpl::ShutDown()
{
}

NODE* NODEContainerImpl::AllocateNode()
{
    void* memory = BX_MALLOC( _node_allocator, sizeof( NODE ), sizeof( void* ) );
    memset( memory, 0x00, sizeof( NODE ) );
    NODE* node = new(memory) NODE();
    c_array::reserve( node->_children, 4, _allocator );
    c_array::reserve( node->_components, 4, _allocator );

    return node;
}

void NODEContainerImpl::FreeNode( NODE* node )
{
    if( node )
    {
        string::free_and_null( &node->_name, _name_allocator );
        c_array::destroy( node->_components );
        c_array::destroy( node->_children );
    }
    BX_FREE( _node_allocator, node );
}

NODE* NODEContainerImpl::FindParent( const NODEComp* comp )
{
    return nullptr;
}

NODE* NODEContainerImpl::FindNode( const guid_t& guid )
{
    NODE* null_node = nullptr;
    scoped_read_spin_lock_t guard( _node_lock );
    return hash::get( _guid_lookup, guid, null_node );
}

NODE* NODEContainerImpl::GetNode( u32 runtime_index )
{
    scoped_read_spin_lock_t guard( _node_lock );
    SYS_ASSERT( runtime_index < array::size( _node_lookup ) );
    return _node_lookup[runtime_index];
}

void NODEContainerImpl::SetName( NODE* node, const char* name )
{
    if( !name )
    {
        string::free_and_null( &node->_name, _name_allocator );
    }
    else
    {
        node->_name = string::duplicate( node->_name, name, _name_allocator );
    }
}

void NODEContainerImpl::DestroyPendingNodes( NODESystemContext* ctx )
{
    array_t<NODE*> pending_nodes;

    {
        scoped_write_spin_lock_t lck( _node_to_destroy_lock );
        pending_nodes = std::move( _node_to_destroy );
    }


    scoped_write_spin_lock_t lck( _node_lock );
    for( NODE* node : pending_nodes )
    {
        DestroyNode( ctx, node );
    }
}

void NODEContainerImpl::DestroyNode( NODESystemContext* ctx, NODE* node )
{
    u32 num_nodes_in_branch = 0;
    u32 num_comps_in_branch = 0;

    helper::TraverseTree( node, [&num_nodes_in_branch]( const NODE* nod )
    {
        num_nodes_in_branch += 1;
    } );
    helper::TraverseTree( node, [&num_comps_in_branch]( const NODE* nod )
    {
        num_comps_in_branch += nod->Components().size();
    } );

    array_t<NODE*> nodes_to_destroy;
    array_t<NODEComp*> comps_to_destroy;

    array::reserve( nodes_to_destroy, num_nodes_in_branch );
    array::reserve( comps_to_destroy, num_comps_in_branch );

    helper::TraverseTree( node, [&nodes_to_destroy]( NODE* node )
    {
        array::push_back( nodes_to_destroy, node );
    } );

    helper::TraverseTree( node, [&comps_to_destroy]( NODE* node )
    {
        for( NODEComp* comp : node->Components() )
        {
            array::push_back( comps_to_destroy, comp );
        }
    } );

    NODEAttachContext detach_ctx;
    NODEInitContext uninit_ctx;
    InitContext( &detach_ctx, ctx );
    InitContext( &uninit_ctx, ctx );

    while( !array::empty( comps_to_destroy ) )
    {
        NODEComp* comp = array::back( comps_to_destroy );
        array::pop_back( comps_to_destroy );

        NODE* parent_node = FindParent( comp );

        comp->OnDetach( parent_node, &detach_ctx );
        comp->OnUninitialize( parent_node, &uninit_ctx );

        NODECompFree( comp );
    }

    while( !array::empty( nodes_to_destroy ) )
    {
        NODE* nod = array::back( nodes_to_destroy );
        array::pop_back( nodes_to_destroy );

        UnlinkNode( node );

        SetName( nod, nullptr );
        u32 node_index = nod->Index();

        _node_lookup[node_index] = (NODE*)(uintptr_t)_node_free_list;
        _node_free_list = node_index;

        SYS_ASSERT( hash::has( _guid_lookup, nod->Guid() ) == false );

        FreeNode( nod );
    }
}

NODE* NODEContainerImpl::CreateNode( const guid_t& guid, const char* node_name )
{
    NODE* node = nullptr;
    {
        scoped_write_spin_lock_t lck( _node_lock );

        node = AllocateNode();

        if( node )
        {
            node->_guid = guid;
            SetName( node, node_name );
            AddToLookup( node );
        }
    }
    return node;
}

void NODEContainerImpl::ScheduleDestroyNode( NODE* node )
{
    {
        scoped_write_spin_lock_t lck( _node_lock );
        RemoveFromLookup( node );
    }
    {
        scoped_write_spin_lock_t lck( _node_to_destroy_lock );
        array::push_back( _node_to_destroy, node );
    }
}

void NODEContainerImpl::AddToLookup( NODE* node )
{
    const guid_t& guid = node->Guid();

    u32 node_index = UINT32_MAX;

    if( _node_free_list != UINT32_MAX )
    {
        node_index = _node_free_list;
        _node_free_list = (u32)(uintptr_t)_node_lookup[node_index];
    }
    else
    {
        node_index = array::push_back( _node_lookup, (NODE*)nullptr );
    }

    _node_lookup[node_index] = node;
    node->_index = node_index;

    SYS_ASSERT( hash::has( _guid_lookup, guid ) == false );
    hash::set( _guid_lookup, guid, node );
}

void NODEContainerImpl::RemoveFromLookup( NODE* node )
{
    helper::TraverseTree( node, [this]( NODE* node )
    {
        SYS_ASSERT( node->_index < array::size( _node_lookup ) );
        SYS_ASSERT( _node_lookup[node->_index] == node );
        SYS_ASSERT( hash::has( _guid_lookup, node->_guid ) );

        _node_lookup[node->_index] = nullptr;
        hash::remove( _guid_lookup, node->_guid );
    } );
}

bool NODEContainerImpl::LinkNode( NODE* parent, NODE* node )
{
    if( !parent )
    {
        parent = _root;
    }

    if( node->_parent == parent )
    {
        return true;
    }

    const NODESpan children = parent->Children();
    auto it = std::find( children.begin(), children.end(), node );
    if( it != children.end() )
    {
        return false;
    }

    node->_parent = parent;
    c_array::push_back( parent->_children, node );

    return true;
}

void NODEContainerImpl::UnlinkNode( NODE* child )
{
    if( !child->_parent )
        return;

    const NODESpan children = child->_parent->Children();
    auto it = std::find( children.begin(), children.end(), child );
    SYS_ASSERT( it != children.end() );
 
    const u32 index = (u32)((uintptr_t)(it - children.begin()));
    c_array::erase( child->_parent->_children, index );
    
    child->_parent = nullptr;
}

void NODEContainerImpl::ScheduleSerialize( const char* filename, bool read )
{
    NODESerializeToken* token = BX_NEW( _allocator, NODESerializeToken );
    token->filename.AppendRelativePath( filename );
    token->is_reader = read;

    scoped_write_spin_lock_t guard( _serialization_lock );

    SYS_ASSERT( !_serialization_token );
    _serialization_token = token;
}

void NODEContainerImpl::ProcessSerialization( NODESystemContext* ctx )
{
    NODESerializeToken* token = nullptr;
    {
        scoped_write_spin_lock_t serialization_guard( _serialization_lock );
        std::swap( token, _serialization_token );
    }
    if( !token )
        return;

    NODETextSerializer serializer;
    if( token->is_reader )
    {
        if( ReadFromFile( &serializer, token->filename.RelativePath(), ctx->fsys, _allocator ) )
        {
            {
                scoped_write_spin_lock_t nodes_guard( _node_lock );
                RemoveFromLookup( _root );
                DestroyNode( ctx, _root );
            }
            _root = ReadNodeTree( this, serializer.xml_root );
        }
    }
    else
    {
        InitWriter( &serializer );
        WriteNodeTree( serializer.xml_root, _root );
        WtiteToFile( token->filename.RelativePath(), serializer, ctx->fsys );
    }

    BX_DELETE( _allocator, token );

}
