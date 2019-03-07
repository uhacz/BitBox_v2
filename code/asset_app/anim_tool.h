#pragma once

#include <foundation/type.h>
#include <foundation/blob.h>
#include <foundation/string_util.h>
#include <foundation/containers.h>
#include <foundation/math/vmath_type.h>

#include <filesystem/filesystem_plugin.h>
#include <asset_compiler/anim/anim_compiler.h>
#include <util/file_system_name.h>
#include "tool_context.h"
#include "common/common.h"
#include "foundation/serializer.h"

struct BXIAllocator;
struct ANIMSimplePlayer;
struct ANIMJoint;


struct ANIMTool : TOOLInterface
{
    void StartUp( CMNEngine* e, const char* src_folder, BXIAllocator* allocator ) override;
    void ShutDown( CMNEngine* e ) override;
    void Tick( CMNEngine* e, const TOOLContext& ctx, float dt ) override;

    void DrawMenu( CMNEngine* e, const TOOLContext& ctx );

    BXIAllocator* _allocator = nullptr;

    tool::anim::Skeleton* _in_skel = nullptr;
    tool::anim::Animation* _in_anim = nullptr;
    tool::anim::ImportParams _import_params = {};
    
    srl_file_t* _skel_file = nullptr;
    srl_file_t* _clip_file = nullptr;

    ANIMSimplePlayer* _player = nullptr;
    ANIMJoint* _joints_ms = nullptr;
    array_t<mat44_t> _matrices_ms;
    float _time_scale = 1.f;

    ECSComponentID _anim_desc_component;
    ECSComponentID _mesh_comp;

    common::FolderContext _root_src_folder_ctx;
    string_t _current_src_file;

    FSName _current_dst_file_skel;
    FSName _current_dst_file_clip;

    BXFileHandle _hfile = {};
    
    // editor stuff
    array_t<int16_t> _selected_joints;

    union 
    {
        uint32_t all = 0;
        struct  
        {
            uint32_t request_load : 1;
            uint32_t loading_io : 1;
            uint32_t show_import_dialog : 1;
            uint32_t save_skel : 1;
            uint32_t save_clip : 1;
        };
    } _flags;

};

struct ANIMClipPoses
{

};