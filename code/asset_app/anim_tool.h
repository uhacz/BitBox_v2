#pragma once

#include <foundation/type.h>
#include <foundation/blob.h>
#include <foundation/string_util.h>
#include <foundation/containers.h>
#include <foundation/math/vmath_type.h>

#include <filesystem/filesystem_plugin.h>
#include <asset_compiler/anim/anim_compiler.h>
#include <util/file_system_name.h>

struct BXIAllocator;
struct ANIMSimplePlayer;
struct ANIMJoint;

struct CMNEngine;
struct TOOLContext;

struct ANIMTool
{
    void StartUp( const char* src_folder, const char* dst_folder, BXIAllocator* allocator );
    void ShutDown();

    void Tick( CMNEngine* e, const TOOLContext& ctx, float dt );
    void DrawMenu();

    BXIAllocator* _allocator = nullptr;

    tool::anim::Skeleton* _in_skel = nullptr;
    tool::anim::Animation* _in_anim = nullptr;
    tool::anim::ImportParams _import_params = {};
    
    blob_t _skel_blob = {};
    blob_t _clip_blob = {};

    ANIMSimplePlayer* _player = nullptr;
    ANIMJoint* _joints_ms = nullptr;
    array_t<mat44_t> _matrices_ms;
    float _time_scale = 1.f;

    string_t _src_folder;
    string_t _dst_folder;
    string_t _current_src_file;
    FSName _current_dst_file_skel;
    FSName _current_dst_file_clip;

    BXFileHandle _hfile = {};
    
    string_buffer_t _src_file_list;
    string_buffer_t _dst_file_list;

    // editor stuff
    array_t<int16_t> _selected_joints;

    union 
    {
        uint32_t all = 0;
        struct  
        {
            uint32_t refresh_src_files : 1;
            uint32_t refresh_dst_files : 1;
            uint32_t request_load : 1;
            uint32_t loading_io : 1;
            uint32_t show_import_dialog : 1;
            uint32_t save_skel : 1;
            uint32_t save_clip : 1;
        };
    } _flags;

};
