#include "node.h"
#include "node_system_impl.h"
#include "../memory/memory.h"
#include "../foundation/array.h"
#include "../foundation/string_util.h"
#include "../foundation/debug.h"
#include "../foundation/thread/rw_spin_lock.h"

RTTI_DEFINE_EMPTY_TYPE( NODEComp );

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


NODE* NODESystem::CreateNode( const char* node_name, NODE* parent )
{
    NODE* node = impl->CreateNode( node_name );
    if( node )
    {
        impl->LinkNode( parent, node );
    }

    return node;
}

void NODESystem::DestroyNode( NODE** hnode )
{
    if( !hnode[0] )
        return;

    impl->ScheduleDestroyNode( hnode[0] );
    hnode[0] = nullptr;
}

void NODESystem::SetName( NODE* node, const char* name )
{
    impl->SetName( node, name );
}

void NODESystem::LinkNode( NODE* parent, NODE* node )
{
    impl->LinkNode( parent, node );
}

void NODESystem::UnlinkNode( NODE* child )
{
    impl->UnlinkNode( child );
    impl->LinkNode( impl->_root, child );
}

void NODESystem::Tick( NODESystemContext* ctx, f32 dt )
{
    impl->DestroyPendingNodes( ctx );


}

void NODESystem::StartUp( NODESystem** system, BXIAllocator* allocator )
{
    u32 memory_size = 0;
    memory_size += sizeof( NODESystem );
    memory_size += sizeof( NODESystemImpl );

    void* memory = BX_MALLOC( allocator, memory_size, 16 );
    NODESystem* sys = (NODESystem*)memory;
    sys->impl = new(sys + 1) NODESystemImpl();
    sys->impl->StartUp( allocator );
    
    sys->impl->_root = sys->impl->CreateNode( "_root_" );

    system[0] = sys;
}

void NODESystem::ShutDown( NODESystem** system, NODEInitContext* ctx )
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

