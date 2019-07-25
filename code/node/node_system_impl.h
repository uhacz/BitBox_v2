#pragma once

#include "../foundation/containers.h"
#include "../foundation/thread/rw_spin_lock.h"
#include "../util/guid.h"
#include "../util/file_system_name.h"

struct BXIAllocator;
struct BXIFilesystem;
struct NODE;
struct NODEComp;

struct NODESystemContext;
struct NODEInitContext;
struct NODEAttachContext;
struct NODETickContext;

NODEComp* NODECompAlloc( const char* type_name );
NODEComp* NODECompAlloc( u64 type_hash_code );
void NODECompFree( NODEComp* comp );

template< typename T >
inline T* NODECompAllc()
{
    return (T*)NODECompAlloc( typeid(T).hash_code() );
}

struct NODESerializeToken
{
    FSName filename = {};
    u32 is_reader = 0;
};

static constexpr u32 MAX_ATTACHED_COMPONENTS = 1024 * 16;

struct NODEContainerImpl
{
    BXIAllocator* _allocator;
    BXIAllocator* _node_allocator;
    BXIAllocator* _name_allocator;

    NODE* _root = nullptr;

    hash_t<NODE*>          _comp_lookup; // component -> node
    hash_t<NODE*, guid_t>  _guid_lookup; // guid -> node
    array_t<NODE*>         _node_lookup;
    array_t<NODE*>         _node_to_destroy;

    using CompBitmask = bitset_t < MAX_ATTACHED_COMPONENTS >;
    using CompStorage = static_array_t<NODEComp*, MAX_ATTACHED_COMPONENTS>;
    CompStorage _comp_storage;
    CompBitmask _comp_attached_bitmask;
    CompBitmask _comp_to_attach_bitmask;
    CompBitmask _comp_to_detach_bitmask;

    NODESerializeToken* _serialization_token = nullptr;

    u32 _node_free_list = UINT32_MAX;

    rw_spin_lock_t _node_lock;
    rw_spin_lock_t _comp_lock;
    rw_spin_lock_t _comp_scene_lock;
    rw_spin_lock_t _node_to_destroy_lock;
    rw_spin_lock_t _serialization_lock;

    void StartUp( BXIAllocator* alloc );
    void ShutDown();

    NODE* AllocateNode();
    void FreeNode( NODE* node );

    NODE* FindParent( const NODEComp* comp );
    NODE* FindNode( const guid_t& guid );
    NODE* GetNode( u32 runtime_index );
    void SetName( NODE* node, const char* name );

    void DestroyPendingNodes( NODESystemContext* ctx );
    void DestroyNode( NODESystemContext* ctx, NODE* node );

    NODE* CreateNode( const guid_t& guid, const char* node_name );
    void ScheduleDestroyNode( NODE* node );
    void AddToLookup( NODE* node );
    void RemoveFromLookup( NODE* node );

    bool LinkNode( NODE* parent, NODE* node );
    void UnlinkNode( NODE* child );

    void ScheduleSerialize( const char* filename, bool read );
    void ProcessSerialization( NODESystemContext* ctx );
};


namespace helper
{
    template< typename TNode, typename TCallback >
    static void TraverseTree( TNode* node, TCallback&& callback )
    {
        callback( node );
        for( NODE* child : node->Children() )
        {
            TraverseTree<TNode, TCallback>( child, std::forward<TCallback>( callback ) );
        }
    }
}
