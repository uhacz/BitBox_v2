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

struct ENTSystemInfo
{};

struct ENTSerializeInfo
{
};

namespace ENTEStepPhase
{
    enum E : uint8_t
    {
        PRE_STEP = 0,
        STEP,
        POST_STEP,
        _COUNT_,
    };
}//

struct ENT_EXPORT ENTComponent
{
    virtual ~ENTComponent();

    virtual void Initialize( ENTSystemInfo* );
    virtual void Deinitialize( ENTSystemInfo* );
    virtual void Attach( ENTSystemInfo* );
    virtual void Detach( ENTSystemInfo* );
    virtual void ParallelStep( ENTSystemInfo* , uint64_t dt_us );
    virtual void SerialStep( ENTSystemInfo*, ENTComponent* parent, uint64_t dt_us );
    virtual int32_t Serialize( ENTSerializeInfo* info );
};

struct ENT_EXPORT ENT
{
    ENTEntityID CreateEntity();
    void        DestroyEntity( ENTEntityID entity_id );

    ENTComponentID CreateComponent( ENTEntityID eid, ENTCComponent::TYPE type, uint32_t cid );
    ENTComponentID CreateComponent( ENTEntityID eid, const char* type_name );
    void           DestroyComponent( ENTEntityID eid, ENTComponentID cid );

    void Step( ENTEStepPhase::E phase, uint64_t dt_us );
    
    static ENT* StartUp( BXIAllocator* allocator );
    static void ShutDown( ENT** ent );

    struct ENTSystem;
    ENTSystem* _ent;
};

