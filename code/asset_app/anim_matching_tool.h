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

class ANIMMatchingTool : public TOOLInterface
{
public:
    void StartUp( CMNEngine* e, const char* src_folder, BXIAllocator* allocator ) override;
    void ShutDown( CMNEngine* e ) override;
    void Tick( CMNEngine* e, const TOOLContext& ctx, float dt ) override;

    void LoadAnimation( CMNEngine* e, const char* filename );

private:
    bool HasSelectedClip() const;

    void TickController();
    void DrawController( const mat44_t& basis );

private:


    BXIAllocator* _allocator = nullptr;

    ANIMatchDatabase* _db = nullptr;
    ANIMAtchContext* _ctx = nullptr;

    u32 _selected_clip = -1;
    u32 _selected_joint = -1;
    i32 _selected_frame = 0;
    bool _isolate_frame = false;

    struct Controller
    {
        vec3_t input_dir{ 0 };
        f32 input_speed = 0.f;

        vec3_t vel{ 0 };
    
    }_controller;
    
};


#include <foundation/type_compound.h>
#include <foundation/math/vmath_type.h>
//
//
//
struct ANIMSkel;
struct ANIMClip;

// 1. dla kazdego jointa znajdz najlepsze entry pos,rot
// 2. ze znalezionych wybierz najbardzie pasujace vel.
// 3. 
struct ANIMatchEntry
{
    f32 phase;
    u32 frame;
    u16 clip_index;
    u16 joint_index;
};

struct ANIMatchEntry3 : ANIMatchEntry
{
    vec3_t value;
};
struct ANIMatchEntry4 : ANIMatchEntry
{
    vec4_t value;
};


struct ANIMatchDatabase
{
    array_t<ANIMatchEntry3> pos;
    array_t<ANIMatchEntry4> rot;
    array_t<ANIMatchEntry3> vel;

    array_t<ANIMatchEntry3> trajectory_vel;
    array_t<ANIMatchEntry3> trajectory_acc;
        
    const ANIMSkel* skel;
    array_t<const ANIMClip*> clips;

    struct SkelMetadata
    {
        //string_t filename;
        srl_file_t* file;
    };

    struct ClipMetadata
    {
        FSName filename;
        srl_file_t* file;
    };

    SkelMetadata skel_metadata;
    array_t<ClipMetadata> clip_metadata;

    BXIAllocator* _allocator;

    union
    {
        u32 all = 0;
        struct  
        {
            u32 sort_data : 1;
        };
    }_flags;
};

struct ANIMAtchContext
{
    static constexpr u32 CLIP_NONE = UINT32_MAX;
    static constexpr u32 ENTRY_NONE = UINT32_MAX;
    
    u32 current_clip = CLIP_NONE;
    u32 current_entry_index = ENTRY_NONE;

    ANIMSimplePlayer player;
    array_t<ANIMJoint> world_joints;

    BXIAllocator* _allocator;
};

namespace anim_match
{
    ANIMAtchContext* CreateContext( const ANIMSkel* skel, BXIAllocator* allocator );
    ANIMatchDatabase* CreateDatabase( BXIAllocator* allocator );

    void DestroyContext( ANIMAtchContext** hctx );
    void DestroyDatabase( ANIMatchDatabase** hdb );

    void LoadSkel( ANIMatchDatabase* ctx, BXIFilesystem* fs, const char* filename );
    void LoadClip( ANIMatchDatabase* db, BXIFilesystem* fs, const char* filename );

    void Update( ANIMatchDatabase* db );
    void Update( ANIMAtchContext* ctx, const ANIMatchDatabase* db, const vec3_t& velocity, float dt );
    void DebugDraw( const mat44_t& basis, const ANIMatchDatabase* db, u32 clip_index, u32 joint_index, u32 clip_frame, bool isolated_frame );
}//





