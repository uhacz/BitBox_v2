#include "node.h"
#include "../memory/memory.h"
#include "../foundation/array.h"
#include "../foundation/string_util.h"
#include "../foundation/debug.h"
#include "../foundation/thread/rw_spin_lock.h"

RTTI_DEFINE_EMPTY_TYPE( NODEComp );

NODE::NODE()
{}

NODE::~NODE()
{}

const NODESpan NODE::Children() const
{
    return to_array_span( _children, _num_children );
}

const NODECompSpan NODE::Components() const
{
    return to_array_span(_components, _num_components );
}

struct NODESystem::Impl
{
    BXIAllocator* allocator;
    BXIAllocator* node_allocator;
    BXIAllocator* comp_allocator;
    BXIAllocator* name_allocator;

    NODE* root = nullptr;

    hash_t<NODE*> comp_lookup; // component -> node
    array_t<NODE*> node_lookup;
    array_t<NODE*> node_to_destroy;
    
    u32 node_free_list = UINT32_MAX;

    rw_spin_lock_t node_lock;
    rw_spin_lock_t comp_lock;
    rw_spin_lock_t node_to_destroy_lock;



    void StartUp( BXIAllocator* alloc )
    {
        allocator = alloc;
    }
    void ShutDown()
    {}
};

static inline NODE* AllocateNode( NODESystem::Impl* impl )
{
    void* memory = BX_MALLOC( impl->node_allocator, sizeof( NODE ), sizeof( void* ) );
    memset( memory, 0x00, sizeof( NODE ) );
    return new(memory) NODE();
}
static inline void FreeNode( NODESystem::Impl* impl, NODE* node )
{
    BX_FREE( impl->node_allocator, node );
}

static inline NODEComp* AllocateComponent( NODESystem::Impl* impl )
{
    return nullptr;
}
static inline void FreeComponent( NODESystem::Impl* impl, NODEComp* comp )
{
    BX_DELETE( impl->comp_allocator, comp );
}


NODE* NODESystem::CreateNode( const char* node_name )
{
    NODE* node = nullptr;
    {
        scoped_write_spin_lock_t lck( impl->node_lock );
        
        node = AllocateNode( impl );
        
        u32 node_index = UINT32_MAX;

        if( impl->node_free_list != UINT32_MAX )
        {
            node_index = impl->node_free_list;
            impl->node_free_list = (u32)(uintptr_t)impl->node_lookup[node_index];
        }
        else
        {
            node_index = array::push_back( impl->node_lookup, (NODE*)nullptr );
        }

        impl->node_lookup[node_index] = node;
        node->_index = node_index;
    }

    if( node )
    {
        node->_guid = GenerateGUID();
        SetName( node, node_name );
    }
    return node;
}

namespace helper
{
    template< typename TNode, typename TCallback >
    static void TraverseTree( TNode* node, TCallback&& callback )
    {
        callback( node );
        for( const NODE* child : node->Children() )
        {
            TraverseTree( child, std::forward( callback ) );
        }
    }

    static void DestroyPendingNodes( NODESystem::Impl* impl, NODESystemContext* ctx )
    {
        array_t<NODE*> pending_nodes;

        {
            scoped_write_spin_lock_t lck( impl->node_to_destroy_lock );
            pending_nodes = std::move( impl->node_to_destroy );
        }


        scoped_write_spin_lock_t lck( impl->node_lock );
        for( NODE* node : pending_nodes )
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

            while( !array::empty( comps_to_destroy ) )
            {
                NODEComp* comp = array::back( comps_to_destroy );
                array::pop_back( comps_to_destroy );

                NODE* parent_node = FindParent( comp );

                comp->OnDetach( parent_node, detach_ctx );
                comp->OnUninitialize( parent_node, uninitialize_ctx );

                FreeComponent( impl, comp );
            }

            while( !array::empty( nodes_to_destroy ) )
            {
                NODE* nod = array::back( nodes_to_destroy );
                array::pop_back( nodes_to_destroy );

                SetName( nod, nullptr );
                u32 node_index = nod->Index();

                impl->node_lookup[node_index] = (NODE*)(uintptr_t)impl->node_free_list;
                impl->node_free_list = node_index;

                FreeNode( impl, nod );
            }
        }

        NODE* node = hnode[0];
        
    }
}//



void NODESystem::DestroyNode( NODE** hnode )
{
    if( !hnode[0] )
        return;

    scoped_write_spin_lock_t lck( impl->node_to_destroy_lock );
    array::push_back( impl->node_to_destroy, hnode[0] );
    hnode[0] = nullptr;

    
}

void NODESystem::StartUp( NODESystem** system, BXIAllocator* allocator )
{
    u32 memory_size = 0;
    memory_size += sizeof( NODESystem );
    memory_size += sizeof( NODESystem::Impl );

    void* memory = BX_MALLOC( allocator, memory_size, 16 );
    NODESystem* sys = (NODESystem*)memory;
    sys->impl = new(sys + 1) NODESystem::Impl();
    sys->impl->StartUp( allocator );
    system[0] = sys;
}

void NODESystem::ShutDown( NODESystem** system )
{
    if( !system[0] )
        return;

    BXIAllocator* allocator = system[0]->impl->allocator;

    system[0]->impl->ShutDown();
    InvokeDestructor( system[0]->impl );

    BX_FREE0( allocator, system[0] );
}

