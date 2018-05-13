#pragma once

#include <foundation/type.h>
#include "dll_interface.h"
#include <gfx/gfx.h>

struct BXIAllocator;

namespace ENTCComponent
{
    using ID = uint64_t;
    using TYPE = uint32_t;
    static constexpr TYPE RESERVED   = 0xFFFF;
    static constexpr TYPE INVALID    = 0;
    static constexpr TYPE GFX_CAMERA = 1;
    static constexpr TYPE GFX_MESH   = 2;
    static constexpr TYPE PHX        = 100;
    static constexpr TYPE ANIM       = 200;
};
struct ENTEntityID { uint32_t i; };
struct ENTComponentID { uint32_t i; };

struct ENTComponentExtID
{
    ENTCComponent::TYPE type;
    ENTCComponent::ID   id;
    
    ENTComponentExtID()
        : type( ENTCComponent::INVALID ), id( 0 ) {}
    ENTComponentExtID( ENTCComponent::TYPE t, ENTCComponent::ID i )
        : type( t ), id( i ) {}
};
inline bool operator == ( const ENTComponentExtID& a, const ENTComponentExtID& b )
{
    return a.type == b.type && a.id == b.id;
}



struct ENT;

struct ENTSystemInfo
{
    ENT* ent = nullptr;
    GFX* gfx = nullptr;
    RSM* rsm = nullptr;

    BXIAllocator* default_allocator = nullptr;
    BXIAllocator* scratch_allocator = nullptr;

    GFXSceneID gfx_scene_id;
};

struct ENTSerializeInfo
{
};

struct ENT_EXPORT ENTIComponent
{
    virtual ~ENTIComponent();

    virtual void Initialize( ENTEntityID entity_id, ENTSystemInfo* sys );
    virtual void Deinitialize( ENTEntityID entity_id, ENTSystemInfo* sys );
    virtual void Attach( ENTEntityID entity_id, ENTSystemInfo* sys );
    virtual void Detach( ENTEntityID entity_id, ENTSystemInfo* sys );
    virtual void ParallelStep( ENTEntityID entity_id, ENTSystemInfo* sys, uint64_t dt_us );
    virtual void SerialStep( ENTEntityID entity_id, ENTIComponent* parent, ENTSystemInfo* sys, uint64_t dt_us );
    virtual int32_t Serialize( ENTEntityID entity_id, ENTSerializeInfo* sys );
};

struct ENT_EXPORT ENT
{
    ENTEntityID CreateEntity();
    void        DestroyEntity( ENTEntityID entity_id );
    bool        IsAlive( ENTEntityID entity_id );

    void           AttachComponent( ENTEntityID eid, ENTComponentExtID ext_id );
    void           DetachComponent( ENTEntityID eid, ENTComponentExtID ext_id );
    ENTComponentID CreateComponent( ENTEntityID eid, const char* type_name );
    void           DestroyComponent( ENTEntityID eid, ENTComponentID cid );

    array_span_t<ENTComponentExtID> GetSystemComponents( ENTEntityID eid );
    array_span_t<ENTComponentID> GetCustomComponents( ENTEntityID eid );
    ENTIComponent* ResolveComponent( ENTComponentID cid );

    void Step( ENTSystemInfo* system_info, uint64_t dt_us );
    
    static ENT* StartUp( BXIAllocator* allocator );
    static void ShutDown( ENT** ent, ENTSystemInfo* system_info );

    struct ENTSystem;
    ENTSystem* _ent;
};

