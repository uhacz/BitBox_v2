#include "node.h"
#include "node_system_impl.h"
#include "node_serialize.h"

#include "../memory/memory.h"
#include "../foundation/array.h"
#include "../foundation/string_util.h"
#include "../foundation/debug.h"
#include "../foundation/thread/rw_spin_lock.h"


void InitContext( NODEAttachContext* out, const NODESystemContext* sys_ctx )
{
    *(NODESystemContext*)out = *sys_ctx;
}
void InitContext( NODEInitContext* out, const NODESystemContext* sys_ctx )
{
    *(NODESystemContext*)out = *sys_ctx;
}
void InitContext( NODETickContext* out, const NODESystemContext* sys_ctx )
{
    *(NODESystemContext*)out = *sys_ctx;
}

NODE::NODE()
{}

NODE::~NODE()
{}

NODESpan NODE::Children() const
{
    return to_array_span( _children->begin(), _children->size );
}

NODECompSpan NODE::Components() const
{
    return to_array_span(_components->begin(), _components->size );
}


NODE* NODEContainer::CreateNode( const char* node_name, NODE* parent )
{
    return CreateNode( GenerateGUID(), node_name, parent );
}

NODE* NODEContainer::CreateNode( const guid_t& guid, const char* node_name, NODE* parent /*= nullptr */ )
{
    NODE* node = impl->CreateNode( guid, node_name );
    if( node )
    {
        impl->LinkNode( parent, node );
    }

    return node;
}

void NODEContainer::DestroyNode( NODE** hnode )
{
    if( !hnode[0] )
        return;

    impl->ScheduleDestroyNode( hnode[0] );
    hnode[0] = nullptr;
}

void NODEContainer::SetName( NODE* node, const char* name )
{
    impl->SetName( node, name );
}

void NODEContainer::LinkNode( NODE* parent, NODE* node )
{
    impl->UnlinkNode( node );
    impl->LinkNode( parent, node );
}

void NODEContainer::UnlinkNode( NODE* child )
{
    if( child->Parent() != GetRoot() )
    {
        impl->UnlinkNode( child );
        impl->LinkNode( impl->_root, child );
    }
}

NODEComp* NODEContainer::CreateComponent( NODE* parent, const char* type_name )
{
    return impl->CreateComponent( parent, type_name );
}

void NODEContainer::DestroyComponent( NODEComp** comp )
{
    impl->ScheduleDestroyComponent( comp[0] );
    comp[0] = nullptr;
}

NODE* NODEContainer::FindNode( const NODEGuid& guid )
{
    return impl->FindNode( guid );
}

NODE* NODEContainer::GetNode( u32 runtime_index )
{
    return impl->GetNode( runtime_index );
}

NODE* NODEContainer::GetRoot()
{
    return impl->_root;
}

void NODEContainer::Tick( NODESystemContext* ctx, f32 dt )
{
    NODEInitContext init_ctx;
    InitContext( &init_ctx, ctx );

    NODEAttachContext attach_ctx;
    InitContext( &attach_ctx, ctx );

    impl->UninitializePendingComponents( &init_ctx );
    impl->DetachPendingComponents( &attach_ctx );
    impl->DestroyPendingNodes( ctx );

    impl->InitializePendingComponents( &init_ctx );
    impl->AttachPendingComponents( &attach_ctx );

    impl->ProcessSerialization( ctx );
}


void NODEContainer::Serialize( const char* filename )
{
    impl->ScheduleSerialize( filename, false );
}

void NODEContainer::Unserialize( const char* filename )
{
    impl->ScheduleSerialize( filename, true );
}

void NODEContainer::StartUp( NODEContainer** system, BXIAllocator* allocator )
{
    u32 memory_size = 0;
    memory_size += sizeof( NODEContainer );
    memory_size += sizeof( NODEContainerImpl );

    void* memory = BX_MALLOC( allocator, memory_size, 16 );
    NODEContainer* sys = (NODEContainer*)memory;
    sys->impl = new(sys + 1) NODEContainerImpl();
    sys->impl->StartUp( allocator );
    
    sys->impl->_root = sys->impl->CreateNode( GenerateGUID(), "_root_" );

    system[0] = sys;
}

void NODEContainer::ShutDown( NODEContainer** system, NODEInitContext* ctx )
{
    if( !system[0] )
        return;

    system[0]->impl->ScheduleDestroyNode( system[0]->impl->_root );
    system[0]->impl->DestroyPendingNodes( ctx );

    BXIAllocator* allocator = system[0]->impl->_allocator;

    system[0]->impl->ShutDown();
    InvokeDestructor( system[0]->impl );

    BX_FREE0( allocator, system[0] );
}
