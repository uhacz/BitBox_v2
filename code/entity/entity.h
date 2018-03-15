#pragma once

#include <foundation/type.h>
#include "dll_interface.h"

struct ENTEntityID    { uint32_t i; };
struct ENTComponentID { uint32_t i; };



namespace ent_ext_comp
{
    using ID = uint32_t;
    using TYPE = uint16_t;
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

struct ENTComponent
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
    void        DestroyEntity();

    ENTComponentID CreateComponent( ENTEntityID eid, ent_ext_comp::TYPE type, uint32_t cid );
    ENTComponentID CreateComponent( ENTEntityID eid, const char* type_name );
    void           DestroyComponent( ENTEntityID eid, ENTComponentID cid );

    void Step( ENTEStepPhase::E phase, uint64_t dt_us );
    
    static ENT* StartUp();
    static void ShutDown( ENT** ent );

    typedef struct ENTSystem* _ent;
};

