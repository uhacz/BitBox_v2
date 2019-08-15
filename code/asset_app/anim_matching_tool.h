#pragma once

#include "tool_context.h"
#include "util/file_system_name.h"
#include "anim/anim_player.h"
#include <foundation/math/vmath_type.h>

struct BXIAllocator;
struct ANIMSimplePlayer;
struct ANIMJoint;
struct ANIMatchDatabase;
struct ANIMAtchContext;
struct ANIMatchDatabaseDebugInfo;

class ANIMMatchingTool : public TOOLInterface
{
public:
    void StartUp( CMNEngine* e, const char* src_folder, BXIAllocator* allocator ) override;
    void ShutDown( CMNEngine* e ) override;
    void Tick( CMNEngine* e, const TOOLContext& ctx, float dt ) override;

    void LoadAnimation( CMNEngine* e, const char* filename );

private:
    void TickController( float dt );
    void DrawController( const mat44_t& basis );

private:


    BXIAllocator* _allocator = nullptr;

    ANIMatchDatabase* _db = nullptr;
    ANIMAtchContext* _ctx = nullptr;
    ANIMatchDatabaseDebugInfo* _dbg = nullptr;

    struct Controller
    {
        vec3_t input_dir{ 0 };
        f32 input_speed = 0.f;

        quat_t rot = quat_t::identity();
        vec3_t vel{ 0 };
    }_controller;
    
};







