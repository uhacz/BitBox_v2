#pragma once

#include <foundation/type.h>
#include "dll_interface.h"

struct BXIAllocator;

struct ENTEntityID    { uint32_t i; };
struct ENTComponentID { uint32_t i; };



namespace ENTCComponent
{
    using ID = uint32_t;
    using TYPE = uint16_t;
    static constexpr TYPE RESERVED   = 0xFFFF;
    static constexpr TYPE GFX_CAMERA = 1;
    static constexpr TYPE GFX_MESH   = 2;
    static constexpr TYPE PHX        = 100;
    static constexpr TYPE ANIM       = 200;
};

struct ENT;
struct GFX;

struct ENTSystemInfo
{
    ENT* ent = nullptr;
    GFX* gfx = nullptr;
};

struct ENTSerializeInfo
{
};

struct ENT_EXPORT ENTIComponent
{
    virtual ~ENTIComponent();

    virtual void Initialize( ENTSystemInfo* );
    virtual void Deinitialize( ENTSystemInfo* );
    virtual void Attach( ENTSystemInfo* );
    virtual void Detach( ENTSystemInfo* );
    virtual void ParallelStep( ENTSystemInfo* , uint64_t dt_us );
    virtual void SerialStep( ENTSystemInfo*, ENTIComponent* parent, uint64_t dt_us );
    virtual int32_t Serialize( ENTSerializeInfo* info );
};

struct ENT_EXPORT ENT
{
    ENTEntityID CreateEntity();
    void        DestroyEntity( ENTEntityID entity_id );

    ENTComponentID CreateComponent( ENTEntityID eid, ENTCComponent::TYPE type, uint32_t cid );
    ENTComponentID CreateComponent( ENTEntityID eid, const char* type_name );
    void           DestroyComponent( ENTEntityID eid, ENTComponentID cid );

    void Step( ENTSystemInfo* system_info, uint64_t dt_us );
    
    static ENT* StartUp( BXIAllocator* allocator );
    static void ShutDown( ENT** ent, ENTSystemInfo* system_info );

    struct ENTSystem;
    ENTSystem* _ent;
};

