#include "node.h"
#include "../memory/memory.h"
#include "../foundation/array.h"
#include "../foundation/string_util.h"
#include "../foundation/debug.h"

RTTI_DEFINE_EMPTY_TYPE( NODEComp );

NODE::NODE()
{}

NODE::~NODE()
{}

//void NODE::Initialize( NODEInitContext* ctx )
//{
//    _implementation->OnInitialize( this, ctx, &_flags );
//}
//
//void NODE::Uninitialize( NODEInitContext* ctx )
//{
//    _implementation->OnUninitialize( this, ctx );
//}
//
//bool NODE::Attach( NODEAttachContext* ctx )
//{
//    bool result = _implementation->OnAttach( this, ctx );
//    return result;
//}
//
//void NODE::Detach( NODEAttachContext* ctx )
//{
//    _implementation->OnDetach( this, ctx );
//}
//
//void NODE::Tick( NODETickContext* ctx )
//{
//    _implementation->OnTick( this, ctx );
//}
//
//void NODE::Serialize( NODETextSerializer* serializer )
//{
//    _implementation->OnSerialize( this, serializer );
//}
//
//void NODE::SetName( const char* name )
//{
//   _name = string::duplicate( _name, name, _allocator );
//}

const NODESpan NODE::Children() const
{
    return to_array_span( _children, _num_children );
}

const NODECompSpan NODE::Components() const
{
    return to_array_span(_components, _num_components );
}

//
//void NODE::AddChild( NODE* node )
//{
//    SYS_ASSERT( array::find( _tree.children, node ) == array::npos );
//    array::push_back( _tree.children, node );
//}
//
//void NODE::RemoveChild( NODE* node )
//{
//    const u32 pos = array::find( _tree.children, node );
//    if( pos != array::npos )
//    {
//        array::erase( _tree.children, pos );
//    }
//}

//
//
//

static inline NODE* AllocateNode( BXIAllocator* allocator )
{
    void* memory = BX_MALLOC( allocator, sizeof( NODE ), sizeof( void* ) );
    memset( memory, 0x00, sizeof( NODE ) );
    return new(memory) NODE();
}
static inline void FreeNode( NODE* node, BXIAllocator* allocator )
{
    BX_DELETE( allocator, node );
}

struct NODESystem::Impl
{
    BXIAllocator* allocator;

    NODE* _root = nullptr;
    hash_t
};


