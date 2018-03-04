#pragma once

#include <foundation/type.h>
#include "dll_interface.h"

struct ENTEntityID    { uint32_t i; };
struct ENTComponentID { uint32_t i; };



namespace ent_ext_id
{
    using ID = uint16_t;
    static constexpr ID GFX_CAMERA = 1;
    static constexpr ID GFX_MESH   = 2;
    static constexpr ID PHX        = 100;
    static constexpr ID ANIM       = 200;
};

struct ENTSystemInfo
{};

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

using ENTComponentData = void;

struct ENTComponentVTable
{
    using Initialize   = void( ENTComponentData*, ENTSystemInfo* );
    using Deinitialize = void( ENTComponentData*, ENTSystemInfo* );
    using Attach       = void( ENTComponentData*, ENTSystemInfo* );
    using Detach       = void( ENTComponentData*, ENTSystemInfo* );
    using Step         = void( ENTComponentData*, ENTSystemInfo* info, uint64_t dt_us );

    Initialize*   init   = nullptr;
    Deinitialize* deinit = nullptr;
    Attach*       attach = nullptr;
    Detach*       detach = nullptr;
    Step*         step[ENTEStepPhase::_COUNT_] = {};
};

struct ENT_EXPORT ENT
{
    ENTEntityID CreateEntity();
    void        DestroyEntity();

    ENTComponentID AttachComponent( ENTEntityID eid, ent_ext_id::ID extid, uint32_t cid );
    ENTComponentID AttachComponent( ENTEntityID eid, const ENTComponentVTable& vtable );
    void           DetachComponent( ENTEntityID eid, ENTComponentID cid );



    void Step( ENTEStepPhase::E phase, uint64_t dt_us );
    
    typedef struct ENTImpl* _ent;
};

